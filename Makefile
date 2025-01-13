OUT := kernel

all: build run

clean:
	rm -f kasm.o
	rm -f kc.o
	rm -f kernel

build:
	nasm -f elf32 kernel.asm -o kasm.o
	gcc -fno-stack-protector -m32 -c kernel.c -o kc.o
	ld -m elf_i386 -T link.ld -o ${OUT} kasm.o kc.o

run:
	qemu-system-i386 -kernel kernel