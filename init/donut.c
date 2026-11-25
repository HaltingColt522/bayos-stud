#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "manpage.h"

static const manpage_entry_t man = {
	.name = "donut",
	.desc = "Displays the known animated donut.c.",
	.usage = "donut"
};

/* note: this donut.c is ported from
 * https://github.com/sleepymalc/Donut.c/blob/master/source_code/donut.c
 * with a few changes for syscalls, non-blocking reading to quit and manpage */

int k;
const float theta_spacing = 0.07;
const float phi_spacing = 0.02;

int main(int argc, char **argv) {
	if (handle_manpage(&man, argc, argv)) { return 0; }

	int fl = fcntl(0, F_GETFL);
	fcntl(0, F_SETFL, fl | O_NONBLOCK);

	float A = 0;
	float B = 0;
	float i, j;
	float zbuff[1760];  // Z buffer of 80 * 22
	char b[1760];       // chars of 80 * 22

	printf("\x1b[2J");  // clear screen
	for (;;) {
		char qch = 0;
		int64_t r = read(0, &qch, 1);
		if (r == 1) {
			if (qch == 'q' || qch == 'Q') { break; }
		} else if (r < 0 && errno != EAGAIN) {
			break;              /* echter Fehler */
		}

		memset(b, 32, 1760);
		memset(zbuff, 0, 7040);
		// theta goes around the cross-sectional circle of a torus
		for (j = 0; 6.28 > j; j += theta_spacing)
			// phi goes around the center of revolution of a torus
			for (i = 0; 6.28 > i; i += phi_spacing) {
				float c = sin(i);
				float d = cos(j);
				float e = sin(A);
				float f = sin(j);
				float g = cos(A);
				float h = d + 2;
				float D = 1 / (c * h * e + f * g + 5);
				float l = cos(i);
				float m = cos(B);
				float n = sin(B);
				float t = c * h * g - f * e;
				int x = 40 + 30 * D * (l * h * m - t * n);
				int y = 12 + 15 * D * (l * h * n + t * m);
				int o = x + 80 * y;
				int N = 8 * ((f * e - c * d * g) * m - c * d * e - f * g -
				             l * d * n);
				if (22 > y && y > 0 && x > 0 && 80 > x && D > zbuff[o]) {
					zbuff[o] = D;
					b[o] = ".,-~:;=!*#$@"[N > 0 ? N : 0];
				}
			}
		printf("\x1b[H");
		for (k = 0; 1761 > k; k++) {
			putchar(k % 80 ? b[k] : 10);
		}
		A += 0.04;
		B += 0.02;

		const char *hint = "\n-- q to quit --\n";
		write(1, hint, strlen(hint));


		usleep(1000 * 10);
	}

	fcntl(0, F_SETFL, fl);
	/* clear terminal */
	write(1, "\033[2J\033[H", 7);
}
