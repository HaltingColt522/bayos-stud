#include <pmm.h>
#include <stddef.h>
#include <memory.h>
#include <stdint.h>

static inline uint64_t bitmap_bytes_for(uint64_t page_count) {
	return (page_count + 7) / 8;
}

static inline uint64_t calc_header_pages(uint64_t page_count) {
	uint64_t header_pages = 1;
	uint64_t prev;

	do {
		prev = header_pages;
		uint64_t usable_pages = page_count - header_pages;
		uint64_t needed_bytes = OFFSET_OF_DATA_IN_AREA + bitmap_bytes_for(usable_pages);
		header_pages = (needed_bytes + 0x1000 - 1) / 0x1000;
		if (header_pages == 0) { header_pages = 1; }
	} while (header_pages != prev);

	return header_pages;
}

void bitmap_allocator_init(struct limine_memmap_response *memmap, uint64_t offset, bitmap_allocator_t *ba) {
	uint64_t entry_count = 0;
	for (int i = 0; i < (int)memmap->entry_count; i++) {
		struct limine_memmap_entry *entry = memmap->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= 0x2000) {
			uint64_t total_pages = entry->length / 0x1000;
			uint64_t header_pages = calc_header_pages(total_pages);
			uint64_t usable_pages = total_pages - header_pages;

			ba->areas[entry_count] = (void *)(entry->base + offset);

			memset(ba->areas[entry_count], 0, header_pages * 0x1000);

			ba->areas[entry_count]->first_frame = (void *)(entry->base + header_pages * 0x1000);
			ba->areas[entry_count]->pages_count = usable_pages;
			ba->areas[entry_count]->used = 0;

			entry_count++;
		}
	}
	ba->areas_count = entry_count;
}

static inline int64_t find_unused_frame(uint8_t *bitmap, uint64_t page_count) {
	if (bitmap == NULL || page_count == 0) {
		return -1;
	}

	uint64_t byte_count = (page_count + 7) / 8;

	for (uint64_t i = 0; i < byte_count; i++) {
		for (uint64_t j = 0; j < 8; j++) {
			uint64_t page = i * 8 + j;

			if (page >= page_count) {
				return -1;
			}

			if ((bitmap[i] & (uint8_t)(1U << j)) == 0) {
				return (int64_t)page;
			}
		}
	}

	return -1;
}

uint64_t allocate_frame(bitmap_allocator_t *ba) {
	if (ba == NULL) {
		return 0;
	}
	for (uint64_t i = 0; i < ba->areas_count; i++) {
		typeof(*ba->areas[i]) *area = ba->areas[i];
		if (area == NULL) {
			continue;
		}

		if (area->used >= area->pages_count) {
			continue;
		}

		int index = find_unused_frame(area->data, area->pages_count);

		if (index < 0 || (uint64_t)index >= area->pages_count) {
			continue;
		}

		uint64_t page = (uint64_t)index;

		area->data[page / 8] |= (uint8_t)(1U << (page % 8));
		area->used++;

		return (uint64_t)area->first_frame + (uint64_t)index * 0x1000;
	}
	return 0;
}

pmm_status_t free_frame(bitmap_allocator_t *ba, uint64_t page) {
	for (size_t i = 0; i < ba->areas_count; i++) {
		uint64_t first = (uint64_t)ba->areas[i]->first_frame;
		uint64_t end = first + ba->areas[i]->pages_count * 0x1000;

		if (page >= first && page < end) {
			/* frame isn't aligned correctly */
			if ((page - first) % 0x1000 != 0) {
				return PMM_ERR_UNALIGNED;
			}

			// the page is in this area of memory
			uint64_t index = (page - first) / 0x1000;
			uint64_t array_index = index / 8;
			uint64_t array_index_offset = index % 8;

			uint8_t mask = (uint8_t)(1u << array_index_offset);

			if (!(ba->areas[i]->data[array_index] & mask)) {
				/* frame is alr free */
				return PMM_ERR_DOUBLE_FREE;
			}

			ba->areas[i]->data[array_index] &= (uint8_t)~mask;
			ba->areas[i]->used--;

			return PMM_OK;
		}
	}

	return PMM_ERR_INVALID_ADDR;
}
