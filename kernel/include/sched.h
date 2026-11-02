#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <interrupts.h>

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

typedef struct process_t {
	uint64_t cr3;
	process_status_e status;
	cpu_status_t *context;

	struct process_t *next;
	struct process_t *wait_next;

	int pid;

	// user space process would not have this field
	uint8_t *kernel_stack;

	// user space exclusive members
} process_t;

// yield control to scheduler
// ends timeslice of process
void yield(void);

// add process to the process list
void insert_process(process_t *p);
// mark process ready for deletion
// should only be called after all resources were freed
void mark_current_proc_as_dead(void);

// returns the pointer to the currently active
// process
process_t *get_current_process(void);

#endif // SCHED_H
