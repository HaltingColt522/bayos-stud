#ifndef _KHEAP_H
#define _KHEAP_H

/* index 384 in PML4, should be free, far enough away from HHDM and kernel */
#define KHEAP_BASE 0xFFFFC00000000000
/* kernel heap size 2MiB */
#define KHEAP_SIZE 0x200000

#include <alloc.h>
#include <pmm.h>
#include <sem.h>

DEFINE_MUTEX_TYPE(heap_t);
extern Mutex(heap_t) g_kheap;

typedef enum {
	KHEAP_OK = 0,
	KHEAP_ERR_NULL = 1,
	KHEAP_ERR_PMM = 2,
	KHEAP_ERR_VMM = 3,
	KHEAP_ERR_HEAP = 4,
} kheap_status_t;

kheap_status_t kheap_init(bitmap_allocator_t *ba, uint64_t pml4, heap_t *kheap);
void *kmalloc(uint64_t n);
heap_status_t kfree(void *p);

#endif // !_KHEAP_H
