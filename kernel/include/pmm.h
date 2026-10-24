#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <limine.h>
#include <sem.h>

// TODO: performance improvements to speed up searching the bitmap. For example keeping track of the last allocated page and checking if the one after it is free, and only if it is not scanning the whole list
#define OFFSET_OF_DATA_IN_AREA 24
typedef struct {
	uint64_t areas_count;
	// to make this viariable length
	// we would need the virtual allocator
	// before the bitmap allocator
	// this can be done but is much more
	// effort, so i decided to do it like this
	// 0x100 should suffice for most memory layouts anyways
	struct {
		void *first_frame;
		uint64_t pages_count;
		uint64_t used;
		uint8_t data[];
	} *areas[0x100];
} bitmap_allocator_t;

void bitmap_allocator_init(struct limine_memmap_response *mmap, uint64_t offset, bitmap_allocator_t *ba);

uint64_t allocate_frame(bitmap_allocator_t *ba);
void free_frame(bitmap_allocator_t *ba, uint64_t page);

DEFINE_MUTEX_TYPE(bitmap_allocator_t);
extern Mutex(bitmap_allocator_t) g_ba;

#endif // PMM_H
