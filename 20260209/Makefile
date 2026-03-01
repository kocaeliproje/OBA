all:
	nasm -f elf32 boot.s -o boot.o
	gcc -m32 -c gdt.c -o gdt.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	gcc -m32 -c idt.c -o idt.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	gcc -m32 -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o kernel.o gdt.o idt.o

run:
	qemu-system-i386 -kernel kernel.bin

clean:
	rm *.o kernel.bin
