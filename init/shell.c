#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 32

static int tokenize(char *line, char **argv) {
	int argc = 0;
	for (char *tok = strtok(line, " \t\n"); tok && argc < MAX_ARGS - 1; tok = strtok(NULL, " \t\n"))
		argv[argc++] = tok;
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

		if (!fgets(line, sizeof(line), stdin))
			break;

		int argc = tokenize(line, argv);
		if (argc == 0)
			continue;

		if (strcmp(argv[0], "exit") == 0)
			break;

		pid_t pid = fork();
		if (pid == 0) {
			execve(argv[0], argv, envp);
			perror(argv[0]);
			_exit(127);
		}

		int status;
		waitpid(pid, &status, 0);
	}
	return 0;
}
