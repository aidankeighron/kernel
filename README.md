# kernel
Basic kernel project


Create object files:
`nasm -f elf32 kernel.asm -o kasm.o`

Assemble object files:
`gcc -m32 -c kernel.c -o kc.o`

Generate executable:
`ld -m elf_i386 -T link.ld -o kernel kasm.o kc.o`