#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "manpage.h"

#define MAX_ARGS 32

static const manpage_entry_t man = {
	.name = "shell",
	.desc = "Creates a new instance of BayOS Shell. A list of available programs, \
functions or utilies can be printed with <help>.",
	.usage = "shell"
};

static int tokenize(char *line, char **argv) {
	int argc = 0;
	for (char *tok = strtok(line, " \t\n"); tok && argc < MAX_ARGS - 1; tok = strtok(NULL, " \t\n")) {
		argv[argc++] = tok;
	}
	argv[argc] = NULL;
	return argc;
}

int main(int argc, char **argv) {
	if (handle_manpage(&man, argc, argv)) { return 0; }

	printf("Bayos Shell\n");

	char line[256];
	char *argv_sh[MAX_ARGS];
	char *envp[] = { NULL };

	for (;;) {
		printf("> ");
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin)) {
			break;
		}

		int argc_sh = tokenize(line, argv_sh);
		if (argc_sh == 0) {
			continue;
		}

		if (strcmp(argv_sh[0], "exit") == 0) {
			break;
		}

		pid_t pid = fork();
		if (pid == 0) {
			char path[256];
			const char *exec_path = argv_sh[0];
			/* programs can now be called either with (e.g) /usr/bin/pi or just pi.
			 * if the path doesn't start with '/', commands will be searched in
			 * /usr/bin/. if the path starts with '/', it will take the absolute path */
			if (strchr(argv_sh[0], '/') == NULL) {
				int n = snprintf(path, sizeof(path), "/usr/bin/%s", argv_sh[0]);

				if (n < 0 || (size_t)n >= sizeof(path)) {
					fprintf(stderr, "Path too long\n");
					_exit(127);
				}

				exec_path = path;
			}

			execve(exec_path, argv_sh, envp);
			perror(exec_path);
			_exit(127);
		}

		int status;
		waitpid(pid, &status, 0);
	}
	return 0;
}
