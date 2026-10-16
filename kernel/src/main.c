#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <framebuffer.h>
#include <printk.h>
#include <interrupts.h>
#include <gdt.h>
#include <pmm.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

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
        asm ("cli; hlt");
}

void _start(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision))
        hcf();

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1)
        hcf();

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

    hcf();
}
