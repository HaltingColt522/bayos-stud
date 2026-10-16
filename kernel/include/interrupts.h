#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void exceptions_init(void);

typedef struct {
	uint64_t rip;
	uint64_t cs;
	uint64_t flags;
	uint64_t rsp;
	uint64_t ss;
} iret_frame_t;

#endif // INTERRUPTS_H
