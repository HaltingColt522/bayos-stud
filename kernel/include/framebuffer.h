#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <limine.h>

typedef struct {
	struct {
		uint64_t width;
		uint64_t height;
		uint64_t bpp;
		uint64_t pitch;
	} info;

	uint64_t x_pos;
	uint64_t y_pos;
	void *buffer;
} framebuffer_t;

int framebuffer_putchar(framebuffer_t *fb, int c);

void framebuffer_init(struct limine_framebuffer *info, framebuffer_t *fb);

#endif // FRAMEBUFFER_H
