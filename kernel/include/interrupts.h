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
} __attribute__((packed)) iret_frame_t;

void idt_set_descriptor(uint8_t entry, uint64_t isr, uint8_t flags, uint8_t ist);

#endif // INTERRUPTS_H
