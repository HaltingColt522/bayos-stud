#ifndef MANPAGE_H
#define MANPAGE_H

#include <stdio.h>
#include <string.h>

typedef struct {
	const char *name;
	const char *desc;
	const char *usage;
} manpage_entry_t;

static inline void print_manpage(const manpage_entry_t *m) {
	printf("NAME\n\t%s\n\nSYNOPSIS\n\t%s\n\nDESCRIPTION\n\t%s\n",
	       m->name, m->usage, m->desc);
}

static inline int handle_manpage(const manpage_entry_t *m, int argc, char **argv) {
	if ((argc == 2) && !(strcmp(argv[1], "--help"))) { print_manpage(m); return 1; }
	return 0;
}

#endif // !MANPAGE_H
