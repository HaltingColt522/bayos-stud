#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <interrupts.h>
#include <file_desc.h>

#define USER_MMAP_BASE 0x100000000000ULL

#define MAX_FDS		0x10
#define FD_STDIN	0x0
#define FD_STDOUT	0x1
/* for future use */
#define FD_STDERR	0x2

void sched_init(void);

typedef enum {
	READY,
	RUNNING,
	BLOCKED,
	DEAD,
} process_status_e;

typedef struct {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rbx;
	uint64_t rax;

	iret_frame_t iret;
} __attribute__((packed)) cpu_status_t;

typedef struct anon_block {
	uintptr_t start;
	uint64_t len;
	struct anon_block *next;
} anon_block_t;

typedef struct process_t {
	uint64_t cr3;
	process_status_e status;
	cpu_status_t *context;

	uint8_t fpu_state[512] __attribute__((aligned(16)));

	struct process_t *next;
	struct process_t *wait_next;

	int pid;

	// user space process would not have this field
	uint8_t *kernel_stack;

	// user space exclusive members
	uintptr_t mmap_next;
	anon_block_t *anon_head;

	file_t *fds[MAX_FDS];
	int ret_value;

	pipe_t *fds[MAX_FDS];
} process_t;

// yield control to scheduler
// ends timeslice of process
void yield(void);

// add process to the process list
void insert_process(process_t *p);
// remove a process from the list
void remove_process_from_list(process_t *proc);
// mark process ready for deletion
// should only be called after all resources were freed
void mark_current_proc_as_dead(void);

// returns the pointer to the currently active
// process
process_t *get_current_process(void);
// returns a pointer to the process with pid
process_t *get_process_from_pid(int);

// generates a unique pid
int get_unique_pid(void);

#define USER_PROC_STACK_SIZE (0x200000)
#define USER_PROC_STACK_ADDRESS_UPPER_BOUND (0x7fffffffe000)
#define USER_PROC_STACK_ADDRESS_LOWER_BOUND (USER_PROC_STACK_ADDRESS_UPPER_BOUND - USER_PROC_STACK_SIZE)

#endif // SCHED_H
