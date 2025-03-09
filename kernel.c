#include "keyboard_map.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

#define ENTER_KEY_CODE 0x1C

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

char* command_names[] = {"ls", "echo"};
unsigned int number_of_commands = sizeof(command_names) / sizeof(char*);

// Current cursor location
unsigned int current_location = 0;
// Location of start of user input
unsigned int INPUT_LOCATION = 0;

/* video memory begins at address 0xb8000 */
char *vidptr = (char*)0xb8000;

struct IDT_entry {
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];


void idt_init(void)
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

	/*     Ports
	*	 PIC1	PIC2
	*Command 0x20	0xA0
	*Data	 0x21	0xA1
	*/

	/* ICW1 - begin initialization */
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);

	/* ICW2 - remap offset address of IDT */
	/*
	* In x86 protected mode, we have to remap the PICs beyond 0x20 because
	* Intel have designated the first 32 interrupts as "reserved" for cpu exceptions
	*/
	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);

	/* ICW3 - setup cascading */
	write_port(0x21 , 0x00);
	write_port(0xA1 , 0x00);

	/* ICW4 - environment info */
	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);
	/* Initialization finished */

	/* mask interrupts */
	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);

	/* fill the IDT descriptor */
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}

void kb_init(void)
{
	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21 , 0xFD);
}

void kprint(const char *str)
{
	unsigned int i = 0;
	while (str[i] != '\0') {
		vidptr[current_location++] = str[i++];
		vidptr[current_location++] = 0x07;
	}
}

void kprint_line(const char *str, unsigned int line, unsigned int gap, unsigned int offset) {
	unsigned int i = 0;
	while (str[i] != '\0') {
		unsigned int index = (i+offset)/gap;
		vidptr[line*COLUMNS_IN_LINE*BYTES_FOR_EACH_ELEMENT+index*2] = str[i];
		vidptr[line*COLUMNS_IN_LINE*BYTES_FOR_EACH_ELEMENT+index*2+1] = 0x07;
		i += gap;
	}
}

void kprint_newline(void)
{
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_location = current_location + (line_size - current_location % (line_size));
}

void clear_screen(void)
{
	unsigned int i = 0;
	while (i < SCREENSIZE) {
		vidptr[i++] = '\0';
		vidptr[i++] = 0x07;
	}
}

int is_equal(char* a, char* b) {
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return 0;
		}

		a++;
		a++;
		b++;
	}
	
	return (*a == ' ' || *a == '\0') && *b == '\0';
}

void get_command_type(void) {
	unsigned int start_of_command = INPUT_LOCATION;
	unsigned int command = (2 >> number_of_commands) - 1;
	unsigned int i = 0;
	for (int j = 0; j < number_of_commands; j++) {
		if (is_equal(vidptr+start_of_command, command_names[j])) {
			switch (j)
			{
			case 0:
				// ls
				break;
			case 1:	
				// echo
				kprint_line(vidptr+start_of_command+5*2, 1, 2, 0);
				break;
			case 2:	
				// color
				break;
			default:
				break;
			}
			// kprint_line(command_names[j], j+10, 1, 0);
		}
	}
}

void handle_enter_press(void) {
	get_command_type();
	// Clear input
	unsigned int start_of_command = INPUT_LOCATION;
	for (int i = start_of_command; i < COLUMNS_IN_LINE; i += 2) {
		vidptr[i] = '\0';
		vidptr[i+1] = 0x07;
	}
	current_location = start_of_command;
}


void keyboard_handler_main(void)
{
	unsigned char status;
	char keycode;

	/* write EOI */
	write_port(0x20, 0x20);

	status = read_port(KEYBOARD_STATUS_PORT);
	/* Lowest bit of status will be set if buffer is not empty */
	if (status & 0x01) {
		keycode = read_port(KEYBOARD_DATA_PORT);
		if(keycode < 0)
			return;

		if(keycode == ENTER_KEY_CODE) {
			handle_enter_press();
			return;
		}

		if (keycode == 14) {
			if (current_location > INPUT_LOCATION) {
				vidptr[--current_location] = 0x07;
				vidptr[--current_location] = '\0';
			}
			return;
		}

		vidptr[current_location++] = keyboard_map[(unsigned char) keycode];
		vidptr[current_location++] = 0x07;
	}
}

void kmain(void)
{
	const char *str = "cool kernel>";
	clear_screen();
	kprint(str);
	INPUT_LOCATION = current_location;

	idt_init();
	kb_init();

	while(1);
}