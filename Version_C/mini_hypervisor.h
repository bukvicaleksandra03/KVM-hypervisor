#ifndef mini_hypervisor_h
#define mini_hypervisor_h

struct vm
{
    int kvm_fd;
    int vm_fd;
    int vcpu_fd;
    char *mem;
    struct kvm_run *kvm_run;
};

#endif