#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
	printf("Hello, from userspace!\n");

	pid_t pid = fork();
	if (pid == 0) {
		execve("/usr/bin/fork", NULL, NULL);
		return -1;
	}

	int res;
	pid = waitpid(pid, &res, 0);
	printf("Child process with pid: %d is done. Returned %d\n", pid, res);

	return 0;
}
