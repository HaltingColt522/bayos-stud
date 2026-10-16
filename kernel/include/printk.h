#ifndef PRINTK_H
#define PRINTK_H

#include <stdarg.h>
#include <framebuffer.h>

typedef enum {
	QEMU_SERIAL = 1,
	EARLY = 2,
} printk_output_type_e;

int printk(printk_output_type_e type, const char *fmt, ...);

void early_printk_init(framebuffer_t *fb);

#endif // PRINTK_H
