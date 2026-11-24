#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "manpage.h"

static const manpage_entry_t man = {
	.name = "ps",
	.desc = "\
Displays information about processes.\n\
\n\
PROCESS STATUS CODES\n\
\t0 - READY,\n\
\t1 - RUNNING,\n\
\t2 - BLOCKED,\n\
\t3 - DEAD",
	.usage = "ps"
};


#define PS_CMD_MAX	32
/* pretty small but works for now */
#define PS_MAX_ENT	16

typedef struct {
	uint64_t pid;
	uint64_t tty;
	uint64_t time;
	char s;
	char _pad[7];
	char cmd[PS_CMD_MAX];
} ps_proc_t;

extern int listprog(const char *path, uint8_t *buf, uint64_t len, uint64_t *out);
extern uint64_t listprocs(ps_proc_t *buf, uint64_t max);

size_t longest_line_length(const char *s) {
	size_t longest = 0;
	size_t current = 0;

	while (*s) {
		if (*s == '\n') {
			if (current > longest) {
				longest = current;
			}
			current = 0;
		} else if (*s != '\r') {
			++current;
		}
		++s;
	}

	return current > longest ? current : longest;
}
int main(int argc, char **argv) {
	if (handle_manpage(&man, argc, argv)) { return 0; }
	ps_proc_t *lst = NULL;
	uint64_t total = 0;
	uint64_t cap = PS_MAX_ENT;

	for (;;) {
		ps_proc_t *tmp = (ps_proc_t *)realloc(lst, cap * sizeof(*lst));

		if (!tmp) {
			free(lst);
			fprintf(stderr, "%s: realloc failed!\n", argv[0]);
			return 1;
		}

		lst = tmp;
		total = listprocs(lst, cap);
		if (total < 0) {
			free(lst);
			fprintf(stderr, "%s: error %ld\n", (long) - total);
			return 1;
		}
		if ((uint64_t)total <= cap) { break; }

		cap = (uint64_t)total + 8;
	}

	printf("S\tPID\tTTY\tTIME\tCMD\n");

	for (uint64_t i = 0; i < total; i++) {
		printf("%d\t%d\t%d\t%d\t%s\n", lst[i].s, lst[i].pid, lst[i].tty, lst[i].time, lst[i].cmd);
	}

	free(lst);
	return 0;
}
