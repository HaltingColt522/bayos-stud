#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 32

static int tokenize(char *line, char **argv) {
	int argc = 0;
	for (char *tok = strtok(line, " \t\n"); tok && argc < MAX_ARGS - 1; tok = strtok(NULL, " \t\n")) {
		argv[argc++] = tok;
	}
	argv[argc] = NULL;
	return argc;
}

int main(void) {
	printf("Bayos Shell\n");

	char line[256];
	char *argv[MAX_ARGS];
	char *envp[] = { NULL };

	for (;;) {
		printf("> ");
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin)) {
			break;
		}

		int argc = tokenize(line, argv);
		if (argc == 0) {
			continue;
		}

		if (strcmp(argv[0], "exit") == 0) {
			break;
		}

		pid_t pid = fork();
		if (pid == 0) {
			char path[256];
			const char *exec_path = argv[0];
			/* programs can now be called either with (e.g) /usr/bin/pi or just pi.
			 * if the path doesn't start with '/', commands will be searched in
			 * /usr/bin/. if the path starts with '/', it will take the absolute path */
			if (strchr(argv[0], '/') == NULL) {
				int n = snprintf(path, sizeof(path), "/usr/bin/%s", argv[0]);

				if (n < 0 || (size_t)n >= sizeof(path)) {
					fprintf(stderr, "Path too long\n");
					_exit(127);
				}

				exec_path = path;
			}

			execve(exec_path, argv, envp);
			perror(exec_path);
			_exit(127);
		}

		int status;
		waitpid(pid, &status, 0);
	}
	return 0;
}
