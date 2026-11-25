#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include "framebuffer.h"
#include "manpage.h"

#define FRAMES 60

static const manpage_entry_t man = {
	.name = "mandelbrot",
	.desc = "\
Displays a rendering of the mandelbrot set.\n\
\n\
ARGUMENTS\n\
\tSTEP is a positive integer. It can be set to a higher number to increase \
performance. It will make the Picture pixelated.\n\
\tZOOM is a positive integer. It specifies the maximal zoom factor.\n\
\tPRESET is a integer [0..2] which specifies the preset to be zoomed in.\n\
\tX Y is the alternative to PRESET if you want to parse custom coordinates to be \
zoomed in.",
	.usage = "mandelbrot [STEP [ZOOM [PRESET | X Y]]]"
};

static const double presets[][2] = {
	{ -0.743643887037151,   0.131825904205330  },
	{  0.2549870375144766,  0.0005679790528465 },
	{ -0.16,                1.0405             },
};

static uint32_t palette(int it, int max_iter) {
	if (it >= max_iter) { return 0x000000; }
	int t = (it * 4) & 0xFF;
	uint8_t r = (t * 9) & 0xFF;
	uint8_t g = (t * 5) & 0xFF;
	uint8_t b = 128 + t / 2;
	return ((uint32_t)r << 16) | (g << 8) | b;
}

static int quit_pressed(void) {
	char k;
	ssize_t r = read(0, &k, 1);
	return r == 1 && (k == 'q' || k == 'Q');
}

int main(int argc, char **argv) {
	if (handle_manpage(&man, argc, argv)) { return 0; }
	int step = 4;
	double zoom_end = 1000.0, cx = presets[0][0], cy = presets[0][1];

	if (argc > 1) { step = atoi(argv[1]); }
	if (argc > 2) { zoom_end = strtod(argv[2], NULL); }
	if (argc == 4) {
		int p = atoi(argv[3]);
		if (p < 0 || p >= (int)(sizeof(presets) / sizeof(presets[0]))) { p = 0; }
		cx = presets[p][0];
		cy = presets[p][1];
	} else if (argc >= 5) {
		cx = strtod(argv[3], NULL);
		cy = strtod(argv[4], NULL);
	}

	if (step < 1) { step = 1; }
	if (step > 64) { step = 64; }
	if (zoom_end < 1.0) { zoom_end = 1.0; }
	if (zoom_end > 1e12) { zoom_end = 1e12; }

	fb_info_t info;
	if (fb_info(&info) != 0) { printf("fb_info failed\n"); return 1; }
	int w = info.width, h = info.height;

	uint32_t *img = malloc((size_t)w * h * 4);
	if (!img) { printf("out of memory\n"); return 1; }

	int fl = fcntl(0, F_GETFL);
	fcntl(0, F_SETFL, fl | O_NONBLOCK);

	for (int f = 0; f < FRAMES; f++) {
		double zoom = pow(zoom_end, (double)f / (FRAMES - 1));
		double scale = 3.0 / w / zoom;
		int max_iter = 128 + (int)(40.0 * log2(zoom));

		for (int py = 0; py < h; py += step) {
			double ci = cy + (py - h / 2) * scale;
			for (int px = 0; px < w; px += step) {
				double cr = cx + (px - w / 2) * scale;
				double zr = 0, zi = 0;
				int it = 0;
				while (it < max_iter && zr * zr + zi * zi <= 4.0) {
					double t = zr * zr - zi * zi + cr;
					zi = 2 * zr * zi + ci;
					zr = t;
					it++;
				}
				uint32_t col = palette(it, max_iter);
				for (int by = 0; by < step && py + by < h; by++)
					for (int bx = 0; bx < step && px + bx < w; bx++) {
						img[(py + by) * w + px + bx] = col;
					}
			}
			if (quit_pressed()) { goto done; }
		}
		fb_blit(img, 0, 0, w, h);
	}

	while (!quit_pressed()) { usleep(50000); }

done:
	fcntl(0, F_SETFL, fl);
	free(img);
	write(1, "\033[2J\033[H", 7);
	return 0;
}
