#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
	printf("Hello, from userspace!\n");

	pid_t pid = fork();
	if (pid == 0) {
		execve("/usr/bin/shell", NULL, NULL);
	}

	int res;
	pid = waitpid(pid, &res, 0);
	printf("Shell with pid: %d exited. Returned %d\n", pid, res);

	return 0;
}
