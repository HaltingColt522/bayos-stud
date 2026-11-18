#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

extern int listprog(const char *path, uint8_t *buf, uint64_t len, uint64_t *out);

int main(void) {
	uint64_t cap = 512, out = 0;
	uint8_t *buf = malloc(cap);
	if (!buf) { printf("help: malloc fehlgeschlagen\n"); return 1; }

	int err = listprog("/usr/bin/", buf, cap, &out);

	if (err == 0) { write(1, buf, out); }
	else if (err == ERANGE) { printf("help: Puffer zu klein (%lu noetig, %lu vorhanden)\n", out, cap); }
	else { printf("help: listprog fehlgeschlagen (errno %d)\n", err); }

	free(buf);
	return err != 0;
}
