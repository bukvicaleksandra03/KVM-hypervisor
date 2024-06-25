
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>
#include <pthread.h>

#define PDE64_PRESENT 1
#define PDE64_RW (1U << 1)
#define PDE64_USER (1U << 2)
#define PDE64_PS (1U << 7)

// CR4
#define CR4_PAE (1U << 5)

// CR0
#define CR0_PE 1u
#define CR0_PG (1U << 31)

#define EFER_LME (1U << 8)
#define EFER_LMA (1U << 10)

#define MEM_SIZE_DEFAULT (4 * 1024 * 1024) // 4MB default memory size
#define PAGE_SIZE_DEFAULT (4 * 1024)       // 4KB default page size

struct vm
{
    int kvm_fd;
    int vm_fd;
    int vcpu_fd;
    char *mem;
    struct kvm_run *kvm_run;
};

int init_vm(struct vm *vm, size_t mem_size)
{
    struct kvm_userspace_memory_region region;
    int kvm_run_mmap_size;

    vm->kvm_fd = open("/dev/kvm", O_RDWR);
    if (vm->kvm_fd < 0)
    {
        perror("open /dev/kvm");
        return -1;
    }

    vm->vm_fd = ioctl(vm->kvm_fd, KVM_CREATE_VM, 0);
    if (vm->vm_fd < 0)
    {
        perror("KVM_CREATE_VM");
        return -1;
    }

    vm->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (vm->mem == MAP_FAILED)
    {
        perror("mmap mem");
        return -1;
    }

    region.slot = 0;
    region.flags = 0;
    region.guest_phys_addr = 0;
    region.memory_size = mem_size;
    region.userspace_addr = (unsigned long)vm->mem;
    if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &region) < 0)
    {
        perror("KVM_SET_USER_MEMORY_REGION");
        return -1;
    }

    vm->vcpu_fd = ioctl(vm->vm_fd, KVM_CREATE_VCPU, 0);
    if (vm->vcpu_fd < 0)
    {
        perror("KVM_CREATE_VCPU");
        return -1;
    }

    kvm_run_mmap_size = ioctl(vm->kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (kvm_run_mmap_size <= 0)
    {
        perror("KVM_GET_VCPU_MMAP_SIZE");
        return -1;
    }

    vm->kvm_run = mmap(NULL, kvm_run_mmap_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED, vm->vcpu_fd, 0);
    if (vm->kvm_run == MAP_FAILED)
    {
        perror("mmap kvm_run");
        return -1;
    }

    return 0;
}

static void setup_64bit_code_segment(struct kvm_sregs *sregs)
{
    struct kvm_segment seg = {
        .base = 0,
        .limit = 0xffffffff,
        .present = 1, // Prisutan ili učitan u memoriji
        .type = 11,   // Code: execute, read, accessed
        .dpl = 0,     // Descriptor Privilage Level: 0 (0, 1, 2, 3)
        .db = 0,      // Default size - ima vrednost 0 u long modu
        .s = 1,       // Code/data tip segmenta
        .l = 1,       // Long mode - 1
        .g = 1,       // 4KB granularnost
    };

    sregs->cs = seg;

    seg.type = 3; // Data: read, write, accessed
    sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;
}

static void setup_long_mode(struct vm *vm, struct kvm_sregs *sregs, int page_size, int mem_size)
{
    uint64_t page = 0;
    uint64_t pml4_addr = 0x1000; // Adrese su proizvoljne.
    uint64_t *pml4 = (void *)(vm->mem + pml4_addr);

    uint64_t pdpt_addr = 0x2000;
    uint64_t *pdpt = (void *)(vm->mem + pdpt_addr);

    uint64_t pd_addr = 0x3000;
    uint64_t *pd = (void *)(vm->mem + pd_addr);

    uint64_t pt_addr = 0x4000;
    uint64_t *pt = (void *)(vm->mem + pt_addr);

    pml4[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pdpt_addr;
    pdpt[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pd_addr;

    if (page_size == 2 * 1024 * 1024)
    {
        switch (mem_size)
        {
        case 2 * 1024 * 1024:
            pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;
            break;
        case 4 * 1024 * 1024:
            pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;
            pd[1] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;
            break;
        case 8 * 1024 * 1024:
            pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;
            pd[1] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;
            pd[2] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;
            pd[3] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;
            break;
        default:
            perror("Non valid mem_size. In setup_long_mode function.");
            exit(1);
        }
    }
    else
    {
        pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pt_addr;
        pt[0] = page | PDE64_PRESENT | PDE64_RW | PDE64_USER;
        switch (mem_size)
        {
        case 2 * 1024 * 1024:
            pt[511] = 0x6000 | PDE64_PRESENT | PDE64_RW | PDE64_USER;
            break;
        case 4 * 1024 * 1024:
            uint64_t pt1_addr = 0x5000;
            uint64_t *pt1 = (void *)(vm->mem + pt1_addr);
            pd[1] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pt1_addr;
            pt1[511] = 0x6000 | PDE64_PRESENT | PDE64_RW | PDE64_USER;
            break;
        case 8 * 1024 * 1024:
            uint64_t pt3_addr = 0x5000;
            uint64_t *pt3 = (void *)(vm->mem + pt3_addr);
            pd[3] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pt3_addr;
            pt3[511] = 0x6000 | PDE64_PRESENT | PDE64_RW | PDE64_USER;
            break;
        default:
            perror("Non valid mem_size. In setup_long_mode function.");
            exit(1);
        }
    }

    // FOR petlja služi tome da mapiramo celu memoriju sa stranicama 4KB.
    // Zašti je uslov i < 512? Odgovor: jer je memorija veličine 2MB.
    // for(int i = 0; i < 512; i++) {
    // 	pt[i] = page | PDE64_PRESENT | PDE64_RW | PDE64_USER;
    // 	page += 0x1000;
    // }
    // -----------------------------------------------------

    sregs->cr3 = pml4_addr;
    sregs->cr4 = CR4_PAE;              // "Physical Address Extension" mora biti 1 za long mode.
    sregs->cr0 = CR0_PE | CR0_PG;      // Postavljanje "Protected Mode" i "Paging"
    sregs->efer = EFER_LME | EFER_LMA; // Postavljanje  "Long Mode Active" i "Long Mode Enable"

    // Inicijalizacija segmenata procesora.
    setup_64bit_code_segment(sregs);
}

void usage()
{
    printf("Usage: ./mini_hypervisor --memory <2|4|8> --page <4KB|2MB> --guest <guest1.img> <guest2.img> ...\n");
    exit(1);
}

struct thread_data
{
    char *img_name;
    int mem_size;
    int page_size;
};

void *thread_body(void *data_ptr)
{
    struct thread_data *thread_data = (struct thread_data *)data_ptr;
    char *img_name = thread_data->img_name;
    int mem_size = thread_data->mem_size;
    int page_size = thread_data->page_size;

    struct vm vm;
    struct kvm_sregs sregs;
    struct kvm_regs regs;
    int stop = 0;
    int ret = 0;
    FILE *img;

    if (init_vm(&vm, mem_size))
    {
        printf("Failed to init the VM\n");
        exit(-1);
    }

    if (ioctl(vm.vcpu_fd, KVM_GET_SREGS, &sregs) < 0)
    {
        perror("KVM_GET_SREGS");
        exit(-1);
    }

    setup_long_mode(&vm, &sregs, page_size, mem_size);

    if (ioctl(vm.vcpu_fd, KVM_SET_SREGS, &sregs) < 0)
    {
        perror("KVM_SET_SREGS");
        exit(-1);
    }

    memset(&regs, 0, sizeof(regs));
    regs.rflags = 2;
    regs.rip = 0;
    // SP raste nadole
    regs.rsp = mem_size;
    // regs.rsp = 2 << 20;

    if (ioctl(vm.vcpu_fd, KVM_SET_REGS, &regs) < 0)
    {
        perror("KVM_SET_REGS");
        exit(-1);
    }

    img = fopen(img_name, "r");
    if (img == NULL)
    {
        printf("Can not open binary file\n");
        exit(-1);
    }

    char *p = vm.mem;
    while (feof(img) == 0)
    {
        int r = fread(p, 1, 1024, img);
        p += r;
    }
    fclose(img);

    char data;

    while (stop == 0)
    {
        ret = ioctl(vm.vcpu_fd, KVM_RUN, 0);
        if (ret == -1)
        {
            printf("KVM_RUN failed\n");
            exit(-1);
        }

        switch (vm.kvm_run->exit_reason)
        {
        case KVM_EXIT_IO:
            if (vm.kvm_run->io.direction == KVM_EXIT_IO_OUT && vm.kvm_run->io.port == 0xE9)
            {
                char *p = (char *)vm.kvm_run;
                printf("%c", *(p + vm.kvm_run->io.data_offset));
            }
            else if (vm.kvm_run->io.direction == KVM_EXIT_IO_IN && vm.kvm_run->io.port == 0xE9)
            {
                scanf("%c", &data);
                char *data_in = (((char *)vm.kvm_run) + vm.kvm_run->io.data_offset);
                (*data_in) = data;
            }
            break;
        case KVM_EXIT_HLT:
            printf("KVM_EXIT_HLT\n");
            stop = 1;
            break;
        case KVM_EXIT_INTERNAL_ERROR:
            printf("Internal error: suberror = 0x%x\n", vm.kvm_run->internal.suberror);
            stop = 1;
            break;
        case KVM_EXIT_SHUTDOWN:
            printf("Shutdown\n");
            stop = 1;
            break;
        default:
            printf("Exit reason: %d\n", vm.kvm_run->exit_reason);
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    struct thread_data thread_data[10];
    int num_of_threads = 0;

    int mem_size = MEM_SIZE_DEFAULT;
    int page_size = PAGE_SIZE_DEFAULT;

    // Parse command line options
    if (argc < 7 || argc > 15)
    {
        usage();
    }

    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "--memory") == 0 || strcmp(argv[i], "-m") == 0)
        {
            mem_size = atoi(argv[i + 1]);
            if (mem_size != 2 && mem_size != 4 && mem_size != 8)
            {
                printf("Invalid memory size. Allowed values: 2, 4, 8\n");
                usage();
            }
            mem_size *= 1024 * 1024; // Convert to MB
        }
        else if (strcmp(argv[i], "--page") == 0 || strcmp(argv[i], "-p") == 0)
        {
            if (strcmp(argv[i + 1], "4KB") == 0)
            {
                page_size = 4 * 1024;
            }
            else if (strcmp(argv[i + 1], "2MB") == 0)
            {
                page_size = 2 * 1024 * 1024;
            }
            else
            {
                printf("Invalid page size. Allowed values: 4KB, 2MB\n");
                usage();
            }
        }
        else if (strcmp(argv[i], "--guest") == 0 || strcmp(argv[i], "-g") == 0)
        {
            while (++i < argc)
            {
                thread_data[num_of_threads++].img_name = argv[i];
            }
        }
        else
        {
            usage();
        }
    }

    if (num_of_threads == 0)
    {
        printf("Guest images not specified.\n");
        usage();
    }

    pthread_t thread[num_of_threads];
    for (int i = 0; i < num_of_threads; i++)
    {
        thread_data[i].mem_size = mem_size;
        thread_data[i].page_size = page_size;
        pthread_create(&thread[i], NULL, thread_body, &thread_data[i]);
    }

    for (int i = 0; i < num_of_threads; i++)
    {
        pthread_join(thread[i], NULL);
    }

    return 0;
}