#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define DEFAULT_DIGITS 100
#define MAX_DIGITS     100000

int main(int argc, char **argv) {
	long digits = DEFAULT_DIGITS;

	if (argc > 1) {
		char *end;
		errno = 0;
		digits = strtol(argv[1], &end, 10);

		if (errno != 0 || end == argv[1] || *end != '\0' || digits < 1 || digits > MAX_DIGITS) {
			fprintf(stderr, "usage: %s [digits (1-%d)]\n", argv[0], MAX_DIGITS);
			return 1;
		}
	}

	int n = (int)digits + 10;
	int len = (n * 10) / 3 + 1;

	int *a = malloc(len * sizeof(int));
	if (!a)
		return -1;

	for (int i = 0; i < len; i++)
		a[i] = 2;

	int nines = 0, predigit = 0;
	int printed = 0;

	printf("pi = ");

	for (int j = 0; j < n; j++) {
		int q = 0;
		for (int i = len - 1; i >= 0; i--) {
			int x = 10 * a[i] + q * (i + 1);
			a[i] = x % (2 * i + 1);
			q = x / (2 * i + 1);;
		}
		a[0] = q % 10;
		q /= 10;

		if (q == 9) {
			nines++;
		} else if (q == 10) {
			printf("%d", predigit + 1);
			printed++;
			for (int k = 0; k < nines; k++) {
				printf("0");
				printed++;
			}
			predigit = 0;
			nines = 0;
		} else {
			if (j > 0) {
				printf("%d", predigit);
				printed++;
				if (printed == 1)
					printf(".");
			}
			predigit = q;
			for (int k = 0; k < nines; k++) {
				printf("9");
				printed++;
			}
			nines = 0;
		}

		if (printed >= digits)
			break;
	}

	printf("\n");
	free(a);
	return 0;
}
