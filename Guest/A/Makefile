all: guest.img 
%.img: %.o
	ld -T guest.ld $(^) -o $(@)

%.o: %.c
	$(CC) -m64 -ffreestanding -fno-pic -c -o $@ $^

clean:
	rm -f *.o *.img
