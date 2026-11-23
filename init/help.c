#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

extern int listprog(const char *path, uint8_t *buf, uint64_t len, uint64_t *out);

int main(int argc, char **argv) {
	if (argc == 1) {
		uint64_t cap = 512, out = 0;
		uint8_t *buf = malloc(cap);
		if (!buf) { fprintf(stderr, "help: malloc failed\n"); return 1; }

		int err = listprog("/usr/bin/", buf, cap, &out);

		if (err == 0) { write(1, buf, out); }
		else if (err == ERANGE) { printf("help: buffer too small (%lu needed, %lu got)\n", out, cap); }
		else { printf("help: listprog failed (errno %d)\n", err); }

		free(buf);
		return err != 0;
	} else {
		if ((argv[1][0] == '\0') || (strchr(argv[1], '/') != NULL)) {
			fprintf(stderr, "%s: invalid command\n", argv[0]);
			return 1;
		}

		if (!(strcmp(argv[1], "init")) || !(strcmp(argv[1], "help"))) {
			fprintf(stderr, "%s: no entry for '%s'\n", argv[0], argv[1]);
			return 1;
		}

		/* clear terminal */
		write(1, "\033[2J\033[H", 7);

		const char *prefix = "/usr/bin/";
		uint64_t len = strlen(prefix) + strlen(argv[1]) + 1;
		char *path = malloc(len);

		if (path == NULL) {
			return 1;
		}

		snprintf(path, len, "%s%s", prefix, argv[1]);

		pid_t pid = fork();
		if (pid < 0) {
			fprintf(stderr, "%s: fork failed!\n", argv[0]);
			free(path);
			return 1;
		}

		if (pid == 0) {
			char *child_argv[] = { (char *)argv[1], "--help", NULL };
			char *envp[] = { NULL };
			execve(path, child_argv, envp);

			perror(path);
			_exit(127);
		}

		int st = 0;
		if (waitpid(pid, &st, 0) < 0) {
			fprintf(stderr, "%s: waitpid failed\n", argv[0]);
			free(path);
			return 1;
		}

		if (st == 127) {
			free(path);
			return 127;
		}

		const char *hint = "\n-- q to quit --\n";
		write(1, hint, strlen(hint));

		char c = 0;
		for (;;) {
			int64_t n = read(0, &c, 1);
			if (n <= 0) { break; }
			if ((c == 'q') || (c == 'Q')) { break; }
		}

		/* clear terminal */
		write(1, "\033[2J\033[H", 7);

		free(path);

		return 0;
	}
}
