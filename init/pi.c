#include <stdio.h>
#include <stdlib.h>

#define DIGITS 100

int main(void) {
	int n = DIGITS + 10;
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

		if (printed >= DIGITS)
			break;
	}

	printf("\n");
	free(a);
	return 0;
}
