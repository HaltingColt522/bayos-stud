#ifndef SYSCALL_H
#define SYSCALL_H

#define WRITE_CHUNK	256
#define READ_CHUNK	256
#define LISTPROG_MAX	0x10000
#define PS_CMD_MAX	32

#include <stdint.h>

typedef struct {
	uint64_t pid;
	uint64_t tty;
	uint64_t time;
	char s;
	char _pad[7];
	char cmd[PS_CMD_MAX];
} ps_proc_t;

void syscall_init(void);

#endif // SYSCALL_H
