#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "manpage.h"

static const manpage_entry_t man = {
	.name = "man",
	.desc = "Display system documentation and print information about the given \
program, utility or function.",
	.usage = "man <name>"
};


int main(int argc, char **argv) {
	if (handle_manpage(&man, argc, argv)) { return 0; }

	if (argc != 2) {
		fprintf(stderr, "usage: %s <command>\n", argv[0]);
		return 1;
	}

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork failed!\n", argv[0]);
		return 1;
	}

	if (pid == 0) {
		char *child_argv[] = { "help", argv[1], NULL };
		execve("/usr/bin/help", child_argv, NULL);
		perror("/usr/bin/help");
		_exit(127);
	}

	int st = 0;
	if (waitpid(pid, &st, 0) < 0) {
		fprintf(stderr, "%s: waitpid failed!\n", argv[0]);
		return 1;
	}

	return st;
}
