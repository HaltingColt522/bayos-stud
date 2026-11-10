#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <framebuffer.h>
#include <printk.h>
#include <interrupts.h>
#include <gdt.h>
#include <pmm.h>
#include <vmm.h>
#include <kheap.h>
#include <sched.h>
#include <pipe.h>
#include <initramfs.h>
#include <memory.h>
#include <io.h>
#include <syscall.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
	.id = LIMINE_FRAMEBUFFER_REQUEST_ID,
	.revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memory_map_request = {
	.id = LIMINE_MEMMAP_REQUEST_ID,
	.revision = 0,
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = {
	.id = LIMINE_HHDM_REQUEST_ID,
	.revision = 0,
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire function.
static void hcf(void) {
	asm("cli; hlt");
}

// this function sets up the syscall interface
// sets up the memory for the userspace program
// , loads it into the, address space, switches cpu to ring 3
// and jumps to the entry (the process becomes the init process)
void setup_enter_userspace_proc(void);

void _start(void) {
	if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
		hcf();
	}

	if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
		hcf();
	}

	framebuffer_t fb;
	framebuffer_init(framebuffer_request.response->framebuffers[0], &fb);

	early_printk_init(&fb);

	printk(QEMU_SERIAL | EARLY, "BAYOS\n");
	printk(QEMU_SERIAL | EARLY, "[OK] Framebuffer\n");

	gdt_init();
	printk(QEMU_SERIAL | EARLY, "[OK] GDT\n");

	exceptions_init();
	printk(QEMU_SERIAL | EARLY, "[OK] Exceptions\n");

	if (memory_map_request.response == NULL) {
		printk(QEMU_SERIAL | EARLY, "[FAIL] no memory map provided by limine\n");
		hcf();
	}

	if (hhdm_request.response == NULL) {
		printk(QEMU_SERIAL | EARLY, "[FAIL] no hhdm mapping provided by limine\n");
		hcf();
	}

	bitmap_allocator_t frame_allocator;
	bitmap_allocator_init(memory_map_request.response, hhdm_request.response->offset, &frame_allocator);
	printk(QEMU_SERIAL | EARLY, "[OK] Bitmap Allocator\n");

	if (vmm_init(&frame_allocator) != VMM_OK) {
		printk(QEMU_SERIAL | EARLY, "[FAIL] initialize VMM failed\n");
		hcf();
	}

	printk(QEMU_SERIAL | EARLY, "[OK] VMM\n");

	uint64_t pml4;
	__asm__ volatile("mov %%cr3, %0" : "=r"(pml4) : : "memory");
	heap_t kheap;
	if (kheap_init(&frame_allocator, pml4, &kheap) != KHEAP_OK) {
		printk(QEMU_SERIAL | EARLY, "[FAIL] initialize kheap failed\n");
		hcf();
	}
	printk(QEMU_SERIAL | EARLY, "[OK] HEAP\n");

	sched_init();
	printk(QEMU_SERIAL | EARLY, "[OK] Scheduler\n");

	// this is where mutexes for global variables are created
	// mutexes are used like so: (example for bitmap_allocator)
	// 	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	// 	... use the frame allocator
	// 	... NEVER take more than one mutex at once
	// 	MUTEX_UNLOCK(g_ba);
	mutex_t mutex_list[3] = {0};
	g_ba.data = &frame_allocator;
	g_ba.lock = mutex_create(&mutex_list[0]);
	g_fb.data = &fb;
	g_fb.lock = mutex_create(&mutex_list[1]);
	g_kheap.data = &kheap;
	g_kheap.lock = mutex_create(&mutex_list[2]);
	printk(QEMU_SERIAL | EARLY, "[OK] Mutexes\n");

	// as soon as the scheduler is started
	// we should only use the global version of things
	// like framebuffer, frame_allocator and so on
	// so printk(EARLY, ... also should not be used anymore
	printk(QEMU_SERIAL | EARLY, "Starting scheduler\n");
	// this line starts the scheduler
	asm volatile("sti");

	setup_enter_userspace_proc();

	for (;;)
		// since we have no use for our timeslice, just yield
	{
		yield();
	}

	hcf();
	__builtin_unreachable();
}

void setup_enter_userspace_proc(void) {
	// first we need to create the process
	process_t *proc = kmalloc(sizeof(process_t));
	if (!proc) {
		printk(QEMU_SERIAL | FB, "[FAIL] Failed to allocate init process struct\n");
		hcf();
	}

	memset(proc, 0, sizeof(process_t));

	// we need a cpu context to enter ring 3
	cpu_status_t enter_ring_3_context = { 0 };

	// fill the process struct
	proc->status = READY;
	// this should be one, since its the first pid we generate
	proc->pid = get_unique_pid();
	proc->kernel_stack = NULL;

	// find the elf file
	uint8_t *init_elf;
	int _init_elf_size = initramfs_lookup("/usr/bin/init", &init_elf);
	if (!init_elf) {
		printk(QEMU_SERIAL | FB, "[FAIL] Initramfs does not contain init process\n");
		hcf();
	}

	// configure the iret for userspace
	enter_ring_3_context.iret.rip = *(uint64_t*)(init_elf + 0x18);
	enter_ring_3_context.iret.cs = 0x33;
	enter_ring_3_context.iret.flags = 0x202;
	enter_ring_3_context.iret.rsp = USER_PROC_STACK_ADDRESS_UPPER_BOUND;
	enter_ring_3_context.iret.ss = 0x2b;

	// write it into the proc struct
	proc->context = &enter_ring_3_context;

	// the init process gets our current address space
	// this also means it can never stop. otherwise we could not have an
	// address space anymore
	uint64_t cr3 = read_cr3();
	proc->cr3 = cr3;

	// before we add our process we need to set up the address space
	// for the init process
	cr3 &= VMM_ADDR_MASK_4K;

	// first allocate the stack
	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	vmm_status_t st = vmm_map_range(ba, cr3, USER_PROC_STACK_ADDRESS_LOWER_BOUND, 0, USER_PROC_STACK_SIZE, VMM_RW | VMM_XD | VMM_US);
	MUTEX_UNLOCK(g_ba);
	if (st != VMM_OK) {
		printk(QEMU_SERIAL | FB, "[FAIL] Failed to map init process stack\n");
		hcf();
	}

	// zero out the stack
	memset((void*)USER_PROC_STACK_ADDRESS_LOWER_BOUND, 0, USER_PROC_STACK_SIZE);

	// this is somehow needed for rtdl in mlibc
	// i dont really know why though
	ba = MUTEX_LOCK(g_ba);
	st = vmm_map_range(ba, cr3, 0x1000, 0, 0x1000, VMM_RW | VMM_XD | VMM_US);
	MUTEX_UNLOCK(g_ba);
	if (st != VMM_OK) {
		printk(QEMU_SERIAL | FB, "[FAIL] Failed to prepare init process\n");
		hcf();
	}
	memset((void*)0x1000, 0, 0x1000);
	*(uint64_t*)0x1000 = 0x1000;

	// TODO: load the elf
	
	syscall_init();

	// finally insert the process
	insert_process(proc);
	// the process is guaranteed to be behind us in the schedule list
	// we yield now to keep the stack
	// the new process will be scheduled before this stack is lost
	// we have to do this to keep the context valid
	yield();
}
