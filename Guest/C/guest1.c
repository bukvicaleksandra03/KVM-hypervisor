#include <stddef.h>
#include <stdint.h>
#include "../../Version_C/file_handling.h"

static void outb(uint16_t port, uint8_t value)
{
    asm("outb %0,%1" : /* empty */ : "a"(value), "Nd"(port) : "memory");
}

static uint8_t inb(uint16_t port)
{
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint8_t f_open(const char *path)
{
    const char *start = path;
    outb(0x0278, FOPEN_CODE);
    uint8_t cnt = 0;
    while (*path)
    {
        cnt++;
        path++;
    }
    outb(0x0278, cnt);

    path = start;
    while (*path)
    {
        outb(0x0278, *path++);
    }

    return inb(0x0278);
}

// maximum of elements which we can write is numOfBytes * numOfElems
uint8_t f_write(void *data_start, int numOfBytes, int numOfElems, uint8_t fd)
{
    if (numOfBytes * numOfElems >= 1 << 8)
        return 0;
    if (numOfBytes * numOfElems == 0)
        return 0;
    outb(0x0278, FWRITE_CODE);
    outb(0x0278, fd);

    uint8_t *ptr = (uint8_t *)data_start;
    uint8_t num = numOfBytes * numOfElems;
    outb(0x0278, num);
    for (int k = 0; k < num; k++)
    {
        outb(0x0278, ptr[k]);
    }

    return inb(0x0278);
}

uint8_t f_read(void *data_start, int numOfBytes, int numOfElems, uint8_t fd)
{
    if (numOfBytes * numOfElems >= 1 << 8)
        return 0;
    if (numOfBytes * numOfElems == 0)
        return 0;
    outb(0x0278, FREAD_CODE);
    outb(0x0278, fd);

    uint8_t *ptr = (uint8_t *)data_start;
    uint8_t num = numOfBytes * numOfElems;
    outb(0x0278, num);
    for (int k = 0; k < num; k++)
    {
        ptr[k] = inb(0x0278);
    }
}

uint8_t f_close(uint8_t fd)
{
    outb(0x0278, FCLOSE_CODE);
    outb(0x0278, fd);

    return inb(0x0278);
}

void
    __attribute__((noreturn))
    __attribute__((section(".start")))
    _start(void)
{

    /*
        INSERT CODE BELOW THIS LINE
    */

    const char *p;
    uint8_t val;
    uint16_t port = 0xE9;

    uint8_t fd = f_open("./guest1file.txt");

    uint8_t fd2 = f_open("./flowers.txt");

    uint8_t buffer[100];

    f_read(buffer, 1, 100, fd2);
    fd = f_write(buffer, 1, 100, fd);
    fd2 = f_write(buffer, 1, 100, fd2);

    f_close(fd);
    f_close(fd2);

    /*
        INSERT CODE ABOVE THIS LINE
    */

    for (;;)
        asm("hlt");
}
