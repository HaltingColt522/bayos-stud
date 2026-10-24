#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <framebuffer.h>
#include <printk.h>
#include <interrupts.h>
#include <gdt.h>
#include <pmm.h>
#include <sched.h>

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

	sched_init();
	printk(QEMU_SERIAL | EARLY, "[OK] Scheduler\n");

	// this is where mutexes for global variables are created
	// mutexes are used like so: (example for bitmap_allocator)
	// 	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	// 	... use the frame allocator
	// 	... NEVER take more than one mutex at once
	// 	MUTEX_UNLOCK(g_ba);
	mutex_t mutex_list[2] = {0};
	g_ba.data = &frame_allocator;
	g_ba.lock = mutex_create(&mutex_list[0]);
	g_fb.data = &fb;
	g_fb.lock = mutex_create(&mutex_list[1]);
	printk(QEMU_SERIAL | EARLY, "[OK] Mutexes\n");

	// as soon as the scheduler is started
	// we should only use the global version of things
	// like framebuffer, frame_allocator and so on
	// so printk(EARLY, ... also should not be used anymore
	printk(QEMU_SERIAL | EARLY, "Starting scheduler\n");
	// this line starts the scheduler
	asm volatile("sti");

	for (;;)
		// since we have no use for our timeslice, just yield
	{
		yield();
	}
}
