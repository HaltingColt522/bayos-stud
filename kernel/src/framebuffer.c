#include <framebuffer.h>
#include <memory.h>

Mutex(framebuffer_t) g_fb;

extern char _binary_zap_vga16_psf_start;
extern char _binary_zap_vga16_psf_end;

#define PSF_FONT_MAGIC 0x436

typedef struct {
	uint16_t magic;
	uint8_t font_mode;
	uint8_t char_size;
} psf_header_t;

#define LINE_SPACING 2
#define LETTER_SPACING 0
#define BORDER_PADDING 1

#define CHAR_RASTER_HEIGHT 16
#define CHAR_RASTER_WIDTH 8
#define BACKUP_CHAR 32

#define TAB_SIZE 8

typedef enum {
	ST_NORMAL,
	ST_ESC,
	ST_CSI,
} esc_state_t;

static esc_state_t esc_state = ST_NORMAL;
static int esc_param = 0;

static inline uint64_t width(framebuffer_t *fb) {
	return fb->info.width;
}

static inline uint64_t height(framebuffer_t *fb) {
	return fb->info.height;
}

static inline uint64_t pitch(framebuffer_t *fb) {
	return fb->info.pitch;
}

static inline void carriage_return(framebuffer_t *fb) {
	fb->x_pos = BORDER_PADDING;
}

static inline void newline(framebuffer_t *fb) {
	fb->y_pos += CHAR_RASTER_HEIGHT + LINE_SPACING;
	carriage_return(fb);
}

static inline void clear(framebuffer_t *fb) {
	uint64_t size = pitch(fb) * height(fb);
	fb->x_pos = BORDER_PADDING;
	fb->y_pos = BORDER_PADDING;
	memset(fb->buffer, 0, size);
}

static inline void write_pixel(framebuffer_t *fb, uint64_t x, uint64_t y, uint32_t color) {
	uint32_t *arr = fb->buffer;
	arr[x + (y * (fb->info.pitch / 4))] = color;
}

static void write_char(framebuffer_t *fb, unsigned char c, uint32_t color) {
	int bytes_per_glyph = (CHAR_RASTER_WIDTH * CHAR_RASTER_HEIGHT) / 8;

	uint8_t *glyph = (uint8_t *)&_binary_zap_vga16_psf_start +
	                 sizeof(psf_header_t) +
	                 c * bytes_per_glyph;

	for (int y = 0; y < CHAR_RASTER_HEIGHT; y++) {
		uint8_t pixel_row = glyph[y];
		for (int x = 0; x < CHAR_RASTER_WIDTH; x++) {
			if (pixel_row & (1 << (7 - x))) {
				write_pixel(fb, fb->x_pos + x, fb->y_pos + y, color);
			}
		}
	}

	fb->x_pos += CHAR_RASTER_WIDTH + LETTER_SPACING;
}

static void tab(framebuffer_t *fb) {
	for (int i = 0; i < TAB_SIZE; i++) {
		write_char(fb, ' ', 0xffffffff);
	}
}

int framebuffer_putchar(framebuffer_t *fb, int c) {
	switch (esc_state) {
	case ST_ESC:
		if (c == '[') { esc_state = ST_CSI; esc_param = 0; }
		else { esc_state = ST_NORMAL; }
		return 0;
	case ST_CSI:
		if (c >= '0' && c <= '9') {
			esc_param = esc_param * 10 + (c - '0');
			return 0;
		}
		if (c == 'J' && esc_param == 2) { clear(fb); }
		else if (c == 'H') { fb->x_pos = BORDER_PADDING; fb->y_pos = BORDER_PADDING; }
		esc_state = ST_NORMAL;
		return 0;
	case ST_NORMAL:
		/* 0x1B == ESC */
		if (c == 0x1B) { esc_state = ST_ESC; return 0;}
		break;
	}
	switch (c) {
	case '\n':
		newline(fb);
		break;
	case '\r':
		carriage_return(fb);
		break;
	case '\t':
		tab(fb);
		break;
	default:
		uint64_t new_xpos = fb->x_pos + CHAR_RASTER_WIDTH;
		if (new_xpos >= width(fb)) {
			newline(fb);
		}
		uint64_t new_ypos = fb->y_pos + CHAR_RASTER_HEIGHT + BORDER_PADDING;
		if (new_ypos >= height(fb)) {
			clear(fb);
		}
		write_char(fb, (char)c, 0xffffffff);
	}
	return 0;
}

void framebuffer_init(struct limine_framebuffer *info, framebuffer_t *fb) {
	fb->info.width = info->width;
	fb->info.height = info->height;
	fb->info.bpp = info->bpp;
	fb->info.pitch = info->pitch;
	fb->buffer = info->address;
	clear(fb);
}

