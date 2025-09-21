#include "keyboard_map.h"

#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

unsigned int current_loc = 0;
char *vidptr = (char*)0xb8000;

struct IDT_entry {
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];

#define GRID_W 40
#define GRID_H 20
#define ORIGIN_X 20
#define ORIGIN_Y 2

typedef struct { int x; int y; } Point;

volatile int game_started = 0;
volatile int input_dx = 1;
volatile int input_dy = 0;
char start_buf[5] = {0,0,0,0,0};
unsigned int rng_seed = 2463534242u;
Point snake[GRID_W * GRID_H];
int snake_length = 0;
Point food;
int score = 0;

void idt_init(void)
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);
	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);
	write_port(0x21 , 0x00);
	write_port(0xA1 , 0x00);
	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);
	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;
	load_idt(idt_ptr);
}

void kb_init(void)
{
	write_port(0x21 , 0xFD);
}

void kprint(const char *str)
{
	unsigned int i = 0;
	while (str[i] != '\0') {
		vidptr[current_loc++] = str[i++];
		vidptr[current_loc++] = 0x07;
	}
}

void kprint_newline(void)
{
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void clear_screen(void)
{
	unsigned int i = 0;
	while (i < SCREENSIZE) {
		vidptr[i++] = ' ';
		vidptr[i++] = 0x07;
	}
	current_loc = 0;
}

void put_char_at(char c, int x, int y)
{
	unsigned int pos = (y * COLUMNS_IN_LINE + x) * BYTES_FOR_EACH_ELEMENT;
	vidptr[pos] = c;
	vidptr[pos + 1] = 0x07;
}

void draw_text_at(int x, int y, const char* s)
{
	int i = 0;
	while (s[i] != '\0') {
		put_char_at(s[i], x + i, y);
		i++;
	}
}

unsigned int prng(void)
{
	rng_seed ^= rng_seed << 13;
	rng_seed ^= rng_seed >> 17;
	rng_seed ^= rng_seed << 5;
	return rng_seed;
}

int snake_contains(Point p)
{
	int i = 0;
	for (i = 0; i < snake_length; i++) {
		if (snake[i].x == p.x && snake[i].y == p.y) return 1;
	}
	return 0;
}

void spawn_food(void)
{
	Point p;
	do {
		p.x = (int)(prng() % GRID_W);
		p.y = (int)(prng() % GRID_H);
	} while (snake_contains(p));
	food = p;
}

void render_frame(void)
{
	int y = 0;
	int x = 0;
	for (y = 0; y < GRID_H; y++) {
		for (x = 0; x < GRID_W; x++) {
			put_char_at(' ', ORIGIN_X + x, ORIGIN_Y + y);
		}
	}
	for (y = 0; y < snake_length; y++) {
		put_char_at('o', ORIGIN_X + snake[y].x, ORIGIN_Y + snake[y].y);
	}
	put_char_at('O', ORIGIN_X + snake[snake_length - 1].x, ORIGIN_Y + snake[snake_length - 1].y);
	put_char_at('@', ORIGIN_X + food.x, ORIGIN_Y + food.y);
	char s[32];
	int n = score;
	int i = 0;
	for (i = 0; i < 32; i++) s[i] = 0;
	s[0] = 'S'; s[1] = 'c'; s[2] = 'o'; s[3] = 'r'; s[4] = 'e'; s[5] = ':'; s[6] = ' ';
	i = 0;
	if (n == 0) { s[7] = '0'; s[8] = 0; }
	else {
		int idx = 0; char tmp[16]; int j; for (j = 0; j < 16; j++) tmp[j] = 0;
		while (n > 0) { tmp[idx++] = (char)('0' + (n % 10)); n /= 10; }
		for (j = 0; j < idx; j++) s[7 + j] = tmp[idx - 1 - j];
	}
	draw_text_at(0, 0, s);
}

void delay_ticks(int loops)
{
	volatile int i, j;
	for (i = 0; i < loops; i++) {
		for (j = 0; j < 25000; j++) { }
	}
}

void init_game(void)
{
	int cx = GRID_W / 2;
	int cy = GRID_H / 2;
	snake_length = 3;
	snake[0].x = cx - 2; snake[0].y = cy;
	snake[1].x = cx - 1; snake[1].y = cy;
	snake[2].x = cx; snake[2].y = cy;
	input_dx = 1; input_dy = 0;
	score = 0;
	spawn_food();
	render_frame();
}

void game_loop(void)
{
	int dx = 1;
	int dy = 0;
	while (1) {
		dx = input_dx;
		dy = input_dy;
		Point head = snake[snake_length - 1];
		Point next;
		next.x = head.x + dx;
		next.y = head.y + dy;
		if (next.x < 0 || next.y < 0 || next.x >= GRID_W || next.y >= GRID_H) break;
		if (snake_contains(next)) break;
		if (next.x == food.x && next.y == food.y) {
			snake[snake_length] = next;
			snake_length++;
			score++;
			spawn_food();
		} else {
			int i;
			for (i = 0; i < snake_length - 1; i++) snake[i] = snake[i + 1];
			snake[snake_length - 1] = next;
		}
		render_frame();
		delay_ticks(2);
	}
}

void keyboard_handler_main(void)
{
	unsigned char status;
	char keycode;
	write_port(0x20, 0x20);
	status = read_port(KEYBOARD_STATUS_PORT);
	if (status & 0x01) {
		keycode = read_port(KEYBOARD_DATA_PORT);
		if ((unsigned char)keycode & 0x80) return;
		unsigned char ch = keyboard_map[(unsigned char) keycode];
		if (!ch) return;
		start_buf[0] = start_buf[1];
		start_buf[1] = start_buf[2];
		start_buf[2] = start_buf[3];
		start_buf[3] = start_buf[4];
		start_buf[4] = (char)ch;
		if (start_buf[0]=='s' && start_buf[1]=='t' && start_buf[2]=='a' && start_buf[3]=='r' && start_buf[4]=='t') game_started = 1;
		if (ch=='w') { if (!(input_dy==1)) { input_dx=0; input_dy=-1; } }
		else if (ch=='s') { if (!(input_dy==-1)) { input_dx=0; input_dy=1; } }
		else if (ch=='a') { if (!(input_dx==1)) { input_dx=-1; input_dy=0; } }
		else if (ch=='d') { if (!(input_dx==-1)) { input_dx=1; input_dy=0; } }
	}
}

void kmain(void)
{
	clear_screen();
	kprint("mkeykernel snake\n\n");
	kprint("digite start\n");
	kprint("controles: w a s d\n\n");
	idt_init();
	kb_init();
	while (!game_started) { }
	clear_screen();
	init_game();
	game_loop();
	clear_screen();
	kprint("game over\n");
	kprint("score: ");
	{
		int n = score; char tmp[16]; int idx = 0; int i; for (i = 0; i < 16; i++) tmp[i] = 0;
		if (n == 0) { vidptr[current_loc++] = '0'; vidptr[current_loc++] = 0x07; }
		else { while (n > 0) { tmp[idx++] = (char)('0' + (n % 10)); n /= 10; } for (i = idx-1; i >= 0; i--) { vidptr[current_loc++] = tmp[i]; vidptr[current_loc++] = 0x07; } }
	}
	kprint_newline();
	while (1) { }
}
