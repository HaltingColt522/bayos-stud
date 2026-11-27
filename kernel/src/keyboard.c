#include <keyboard.h>
#include <stdbool.h>
#include <io.h>
#include <printk.h>

static uint8_t kb_buff[0x100];
pipe_t kb_pipe;

static bool shift_l, shift_r, ctrl, altgr, capslck, extended;

static const uint8_t map_normal[128] = {
	[0x02]	=	'1',
	[0x03]	=	'2',
	[0x04]	=	'3',
	[0x05]	=	'4',
	[0x06]	=	'5',
	[0x07]	=	'6',
	[0x08]	=	'7',
	[0x09]	=	'8',
	[0x0A]	=	'9',
	[0x0B]	=	'0',
	/* [0x0C]	=	'ß', need to be added later for UTF-8 support */
	/* [0x0D]	=	'´', need to be added later for UTF-8 support */
	[0x0E]	=	0x7F, /* backspace */
	[0x0F]	=	0x09, /* tab */
	[0x10]	=	'q',
	[0x11]	=	'w',
	[0x12]	=	'e',
	[0x13]	=	'r',
	[0x14]	=	't',
	[0x15]	=	'z',
	[0x16]	=	'u',
	[0x17]	=	'i',
	[0x18]	=	'o',
	[0x19]	=	'p',
	/* [0x1A]	=	'ü', need to be added later for UTF-8 support */
	[0x1B]	=	'+',
	[0x1C]	=	'\n',
	/* [0x1D]	left ctrl */
	[0x1E]	=	'a',
	[0x1F]	=	's',
	[0x20]	=	'd',
	[0x21]	=	'f',
	[0x22]	=	'g',
	[0x23]	=	'h',
	[0x24]	=	'j',
	[0x25]	=	'k',
	[0x26]	=	'l',
	/* [0x27]	=	'ö', need to be added later for UTF-8 support */
	/* [0x28]	=	'ä', need to be added later for UTF-8 support */
	[0x29]	=	'^',
	/* [0x2A]	left shift */
	[0x2B]	=	'#',
	[0x2C]	=	'y',
	[0x2D]	=	'x',
	[0x2E]	=	'c',
	[0x2F]	=	'v',
	[0x30]	=	'b',
	[0x31]	=	'n',
	[0x32]	=	'm',
	[0x33]	=	',',
	[0x34]	=	'.',
	[0x35]	=	'-',
	[0x39]	=	' ',
	/* a lot of (currently) not needed keys, e.g numpad, F-keys, etc */
	[0x56]	=	'<',
	/* unused / (currently) not needed */
};

static const uint8_t map_shift[128] = {
	[0x02]	=	'!',
	[0x03]	=	'"',
	/* [0x04]	=	'§', need to be added later for UTF-8 support */
	[0x05]	=	'$',
	[0x06]	=	'%',
	[0x07]	=	'&',
	[0x08]	=	'/',
	[0x09]	=	'(',
	[0x0A]	=	')',
	[0x0B]	=	'=',
	[0x0C]	=	'?',
	[0x0D]	=	'`',
	[0x0E]	=	0x7F,
	[0x0F]	=	0x09,
	[0x10]	=	'Q',
	[0x11]	=	'W',
	[0x12]	=	'E',
	[0x13]	=	'R',
	[0x14]	=	'T',
	[0x15]	=	'Z',
	[0x16]	=	'U',
	[0x17]	=	'I',
	[0x18]	=	'O',
	[0x19]	=	'P',
	/* [0x1A]	=	'ü', need to be added later for UTF-8 support */
	[0x1B]	=	'*',
	[0x1C]	=	'\n',
	/* [0x1D]	left ctrl */
	[0x1E]	=	'A',
	[0x1F]	=	'S',
	[0x20]	=	'D',
	[0x21]	=	'F',
	[0x22]	=	'G',
	[0x23]	=	'H',
	[0x24]	=	'J',
	[0x25]	=	'K',
	[0x26]	=	'L',
	/* [0x27]	=	'ö', need to be added later for UTF-8 support */
	/* [0x28]	=	'ä', need to be added later for UTF-8 support */
	/* [0x29]	=	'°', need to be added later for UTF-8 support */
	/* [0x2A]	left shift */
	[0x2B]	=	0x27,
	[0x2C]	=	'Y',
	[0x2D]	=	'X',
	[0x2E]	=	'C',
	[0x2F]	=	'V',
	[0x30]	=	'B',
	[0x31]	=	'N',
	[0x32]	=	'M',
	[0x33]	=	';',
	[0x34]	=	':',
	[0x35]	=	'_',
	[0x39]	=	' ',
	/* a lot of (currently) not needed keys, e.g numpad, F-keys, etc */
	[0x56]	=	'>',
	/* unused / (currently) not needed */
};

static const uint8_t map_altgr[128] = {
	[0x08]	=	'{',
	[0x09]	=	'[',
	[0x0A]	=	']',
	[0x0B]	=	'}',
	[0x0C]	=	0x5c,
	[0x10]	=	'@',
	[0x1B]	=	'~',
	[0x56]	=	'|',
};

void keyboard_init(void) {
	/* read bit 0 from port 0x64 (PS/2 for read) => 0 = empty, 1 = full;
	 * empty controller to clear leftovers. if there is a unread byte, a new
	 * IRQ will never appear */
	while (inb(0x64) & 1ULL) { inb(0x60); }
	pipe_create(&kb_pipe, kb_buff, 256);
	idt_set_descriptor(0x21, (uintptr_t)&keyboard_int_handler, 0x8E, 0);
	outb(0x21, inb(0x21) & ~(1ULL << 1ULL));
}

static bool isalpha(unsigned char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static uint8_t lookup(uint8_t key, bool keyUp, bool was_ext) {
	if (keyUp) { return 0; }
	if (was_ext) { return 0; }

	if (altgr) {
		return map_altgr[key];
	}

	uint8_t base = map_normal[key];
	bool shift = shift_l || shift_r;

	if (isalpha(base)) { shift = shift != capslck; }

	uint8_t c = shift ? map_shift[key] : base;

	if (map_normal[key] == 0x7F) { return '\b'; }

	if ((ctrl) && (isalpha(base))) { c = base & 0x1F; }

	return c;
}

void keyboard_irq(void) {
	if ((inb(0x64) & 1ULL) == 0) { return; }
	/* note: scancodes != characters, just keys on the keyboard.
	 * we use scancode set 1 (https://wiki.osdev.org/PS/2_Keyboard#Scan_Code_Set_1)
	 * */
	uint8_t sc = inb(0x60);
	/* 0xFA = ACK doesn't need to be handled */
	if (sc == 0xFA) { return; }
	/* 0xE0: next byte is an extended key */
	if (sc == 0xE0) { extended = true; return; }
	bool keyUp = sc & 0x80;
	uint8_t key = sc & 0x7F;

	bool is_mody = true;
	switch (key) {
	case 0x2A:
		if (!extended) { shift_l = !keyUp; }
		break;
	case 0x36:
		shift_r = !keyUp;
		break;
	case 0x1D:
		ctrl = !keyUp;
		break;
	case 0x38:
		if (extended) { altgr = !keyUp; }
		break;
	case 0x3A:
		if (!keyUp) { capslck = !capslck; }
		break;
	default:
		is_mody = false;
		break;
	}

	if (is_mody) {
		extended = false;
		return;
	}

	if (keyUp) { extended = false; return; }
	bool was_ext = extended;
	key = lookup(key, keyUp, was_ext);

	if (key != 0) { pipe_try_write(&kb_pipe, key); }
	extended = false;
	return;
}
