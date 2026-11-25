#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <limine.h>
#include <sem.h>

typedef struct {
	uint64_t width;
	uint64_t height;
	uint64_t bpp;
	uint64_t pitch;
} fb_info_t;

typedef struct {
	fb_info_t info;
	uint64_t x_pos;
	uint64_t y_pos;
	void *buffer;
} framebuffer_t;

int framebuffer_putchar(framebuffer_t *fb, int c);

void framebuffer_init(struct limine_framebuffer *info, framebuffer_t *fb);

DEFINE_MUTEX_TYPE(framebuffer_t);
extern Mutex(framebuffer_t) g_fb;

void fb_get_info(fb_info_t *fbi);
int64_t fb_blit(const uint32_t *src, uint64_t x, uint64_t y, uint64_t w, uint64_t h);

#endif // FRAMEBUFFER_H
