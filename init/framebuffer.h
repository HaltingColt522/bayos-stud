#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

typedef struct {
	uint64_t width;
	uint64_t height;
	uint64_t bpp;
	uint64_t pitch;
} fb_info_t;

extern int fb_info(fb_info_t *i);
extern int fb_blit(uint32_t *out, uint64_t x, uint64_t y, uint64_t w, uint64_t h);

#endif
