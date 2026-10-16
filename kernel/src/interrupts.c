#include <interrupts.h>
#include <printk.h>

void exception_handler(uint64_t num, uint64_t err, iret_frame_t *frame) {
	printk(QEMU_SERIAL | EARLY, "exception 0x%x occured\nError: 0x%x\n", num, err);

	uint64_t cr2;
	asm volatile("mov %%cr2, %0" : "=r"(cr2));
	printk(QEMU_SERIAL | EARLY,
	       "CR2: 0x%x\nRIP: 0x%x\nCS: 0x%x\nFLAGS: 0x%x\nRSP: 0x%x\nSS: 0x%x\n", cr2, frame->rip, frame->cs, frame->flags, frame->rsp, frame->ss);

	printk(QEMU_SERIAL | EARLY, "halting execution");

	asm volatile("cli; hlt");
}

typedef struct {
	uint16_t isr_low;
	uint16_t kernel_cs;
	uint8_t ist;
	uint8_t attr;
	uint16_t isr_mid;
	uint32_t isr_high;
	uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

__attribute__((aligned(0x10)))
static idt_entry_t idt[0x100];

typedef struct {
	uint16_t size;
	idt_entry_t *idt;
} __attribute__((packed)) idtr_t;

static void idt_set_descriptor(uint8_t entry, uint64_t isr, uint8_t flags, uint8_t ist) {
	idt[entry].isr_low = isr & 0xffff;
	idt[entry].kernel_cs = 0x8;
	idt[entry].ist = ist & 0x7;
	idt[entry].attr = flags;
	idt[entry].isr_mid = (isr >> 16) & 0xffff;
	idt[entry].isr_high = (isr >> 32) & 0xffffffff;
	idt[entry].reserved = 0;
}

extern uint64_t isr_stub_table[32];

void exceptions_init(void) {
	idtr_t idtr = {
		sizeof(idt) - 1,
		idt,
	};

	for (int i = 0; i < 32; i++) {
		if (i == 0x8) {
			idt_set_descriptor(i, isr_stub_table[i], 0x8e, 1);
		} else {
			idt_set_descriptor(i, isr_stub_table[i], 0x8e, 0);
		}
	}

	asm volatile("lidt %0" : : "m"(idtr) : "memory");
}
