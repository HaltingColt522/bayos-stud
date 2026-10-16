#include <printk.h>
#include <stddef.h>

static inline void qemu_serial_putchar(int c) {
	__asm__ volatile("outb %b0, %w1" : : "a"((uint8_t)c), "Nd"(0xe9) : "memory");
}

static framebuffer_t *default_fb = NULL;

void early_printk_init(framebuffer_t *fb) {
	default_fb = fb;
}

static inline void early_putchar(int c) {
	if (!default_fb) {
		return;
	}
	framebuffer_putchar(default_fb, c);
}

static void putchar(printk_output_type_e type, int c) {
	if (type & QEMU_SERIAL) {
		qemu_serial_putchar(c);
	}

	if (type & EARLY) {
		early_putchar(c);
	}
}

static inline int format_spec_x(printk_output_type_e type, uint64_t val) {
	char digits[] = "0123456789abcdef";
	char buffer[sizeof(val) * 2];
	int i = 0;
	int count = 0;

	if (val == 0) {
		putchar(type, (int)'0');
		return 1;
	}

	while (val > 0) {
		buffer[i++] = digits[val % 16];
		val /= 16;
	}

	while (i > 0) {
		putchar(type, buffer[--i]);
		count++;
	}

	return count;
}

static inline int format_spec_s(printk_output_type_e type, uint8_t *str) {
	int count = 0;

	char *c = str;
	while (*c) {
		putchar(type, (int)*c);

		count++;
		c++;
	}

	return count;
}

static inline int format_spec_d(printk_output_type_e type, int val) {
	int number = val;
	int count = 0;

	if (number < 0) {
		count++;
		putchar(type, (int)'-');
		number = -number;
	}

	if (number >= 10) {
		count += format_spec_d(type, number / 10);
	}

	putchar(type, (int)('0' + number % 10));
	count++;
	return count;
}

int printk(printk_output_type_e type, const char *fmt, ...) {
	if (!fmt) {
		return 0;
	}

	int count = 0;

	va_list args;
	va_start(args, fmt);

	char *c = fmt;
	while (*c) {
		if (*c == '%') {
			c++;

			switch (*c) {
			case 'x':
				count += format_spec_x(type, va_arg(args, uint64_t));
				break;
			case '%':
				putchar(type, (int)*c);
				count++;
				break;
			case 'c':
				putchar(type, va_arg(args, int));
				count++;
				break;
			case 's':
				count += format_spec_s(type, va_arg(args, uint8_t *));
				break;
			case 'd':
				count += format_spec_d(type, va_arg(args, int));
				break;
			}
		} else {
			putchar(type, (int)*c);
			count++;
		}
		c++;
	}

	va_end(args);
	return count;
}
