#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

extern int listprog(const char *path, uint8_t *buf, uint64_t len, uint64_t *out);
extern uint64_t listprocs(void);

typedef struct {
	char s;
	uint64_t pid;
	uint64_t tty;
	uint64_t time;
	/* does NOT move this char array!!! in C, arrays of unknown length need to be
	 * the last member of the struct */
	char *cmd;
} ps_proc_t;

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
int main(void) {
	uint64_t proc_cnt = listprocs();
	printf("Number of processes: %ld\n", proc_cnt);

	uint64_t cap = 512, out = 0;
	uint8_t *buf = (uint8_t *)malloc(cap);
	if (!buf) { fprintf(stderr, "help: malloc failed\n"); return 1; }

	int err = listprog("/usr/bin/", buf, cap, &out);
	if ((err != 0) || (out > cap)) { free(buf); return 1; }

	buf[out] = '\0';
	uint64_t longest = (uint64_t)longest_line_length((const char *)buf);

	ps_proc_t *ps_lst = calloc(proc_cnt, sizeof(*ps_lst));
	if (!ps_lst) {
		free(buf);
		return 1;
	}

	for (uint64_t i = 0; i < proc_cnt; i++) {
		ps_lst[i].cmd = malloc(longest + 1);
		if (!ps_lst[i].cmd) {
			for (uint64_t j = 0; j < i; j++) { free(ps_lst[j].cmd); }
			free(ps_lst);
			free(buf);
			return 1;
		}
	}

	/* fill ps_lst[i] for usage, too lazy for now, will care abt it tmrrw */

	for (uint64_t i = 0; i < proc_cnt; i++) { free(ps_lst[i].cmd); }

	free(ps_lst);
	free(buf);

	return 0;
}
