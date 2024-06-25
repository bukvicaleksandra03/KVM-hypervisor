#include <stddef.h>
#include <stdint.h>

static void outb(uint16_t port, uint8_t value) {
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

static uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {

	/*
		INSERT CODE BELOW THIS LINE
	*/

	const char *p;
	uint8_t val;
	uint16_t port = 0xE9;

	for (p = "Hello, world!\n"; *p; ++p)
		outb(0xE9, *p);

	for (int i = 0; i < 10; i++) {
		val = inb(0xE9);
		outb(0xE9, val);
	}

	/*
		INSERT CODE ABOVE THIS LINE
	*/

	for (;;)
		asm("hlt");
}
