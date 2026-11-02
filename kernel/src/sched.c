#include <sched.h>
#include <stddef.h>
#include <io.h>
#include <limine.h>

// globals
// only modified with functions for it
process_t *process_list = NULL;
process_t *current_process = NULL;

// the init process
// will always stay active to make sure the
// process_list is never empty
static process_t init_process;

extern void timer_int_handler;

void sched_init(void) {
	//
	// first we need to set up a timer interrupt, so
	// we can even create a preemptive scheduler
	//
	// i dont wanna bother with acpi right now, so we are just going to use
	// the legacy pit. it is still present on almost all new systems
	// this is why we need to use revision4. revision6 doesnt leave the lapic
	// in a state we can use, so we would need to reconfigure it
	//
	// the timer should trigger an interupt every 10ms
	//
	// but first we need to install an intterupt handler
	// and remap the pic
	//
	idt_set_descriptor(0x20, (uintptr_t)&timer_int_handler, 0x8e, 0);

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT  0x10
#define ICW1_ICW4  0x01
#define ICW4_8086  0x01

	outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
	io_wait();
	outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
	io_wait();

	outb(PIC1_DATA, 0x20);
	io_wait();
	outb(PIC2_DATA, 0x28);
	io_wait();

	outb(PIC1_DATA, 0x04);
	io_wait();
	outb(PIC2_DATA, 0x02);
	io_wait();

	outb(PIC1_DATA, ICW4_8086);
	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();

	outb(PIC1_DATA, ~1);
	io_wait();
	outb(PIC2_DATA, 0xff);
	io_wait();

	// configure the timer
	uint16_t divisor = (uint16_t)(1193182 / 100);

	outb(PIT_COMMAND, 0x36);
	outb(PIT_CHANNEL0, divisor & 0xff);
	outb(PIT_CHANNEL0, (divisor >> 8) & 0xff);

	// now as soon as we enable interrupts, one will fire every 10ms

	// so that our schedule function works, we need to create a initial process,
	// (our) _start function
	init_process.status = RUNNING;
	init_process.pid = 0;
	init_process.cr3 = read_cr3();
	process_list = &init_process;
	current_process = &init_process;
}

//
// this function will schedule a process
// NOTE: this runs with interrupts disabled
//
cpu_status_t *schedule(cpu_status_t *context) {
	current_process->context = context;
	if (current_process->status != DEAD && current_process->status != BLOCKED) {
		current_process->status = READY;
	}

	for (;;) {
		process_t *prev_process = current_process;
		if (current_process->next != NULL) {
			current_process = current_process->next;
		} else {
			current_process = process_list;
		}

		if (current_process != NULL &&
		        (current_process->status == DEAD || current_process->status == BLOCKED)) {
			// do nothing, kernel processes should run forever or take care of their own cleanup
			// user processes should be reaped by wait eventually
		} else {
			current_process->status = RUNNING;
			break;
		}
	}

	return current_process->context;
}

void yield(void) {
	asm volatile("int $0x20");
}

void insert_process(process_t *p) {
	uint64_t flags = save_irqdisable();

	process_t *last = process_list;
	while (last->next != NULL) {
		last = last->next;
	}

	last->next = p;

	irqrestore(flags);
}

void mark_current_proc_as_dead(void) {
	get_current_process()->status = DEAD;

	// never get scheduled again
	yield();
}

process_t *get_current_process(void) {
	uint64_t flags = save_irqdisable();

	process_t *proc = current_process;

	irqrestore(flags);

	return proc;
}
