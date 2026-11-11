#include <kheap.h>
#include <sem.h>
#include <vmm.h>

Mutex(heap_t) g_kheap;

static void kheap_rollback(bitmap_allocator_t *ba, uint64_t pml4, uint64_t count) {
	if (count > 0) {
		vmm_unmap_range(ba, pml4, KHEAP_BASE, count * VMM_PAGE_SIZE_4K, 1);
	}
}

kheap_status_t kheap_init(bitmap_allocator_t *ba, uint64_t pml4, heap_t *kheap) {
	if ((ba == NULL) || (kheap == NULL)) { return KHEAP_ERR_NULL; }
	pml4 = pml4 & VMM_ADDR_MASK_4K;
	uint64_t pages = KHEAP_SIZE / VMM_PAGE_SIZE_4K;

	vmm_status_t st = vmm_map_range(ba, pml4, KHEAP_BASE, 0, KHEAP_SIZE, VMM_RW | VMM_XD);
	if (st != VMM_OK) {
		return (st == VMM_ERR_OUT_OF_MEM) ? KHEAP_ERR_PMM : KHEAP_ERR_VMM;
	}

	if (heap_init(kheap, (uint8_t *)KHEAP_BASE, KHEAP_SIZE) != HEAP_OK) {
		kheap_rollback(ba, pml4, pages);
		return KHEAP_ERR_HEAP;
	}

	return KHEAP_OK;
}

void *kmalloc(uint64_t n) {
	heap_t *h = MUTEX_LOCK(g_kheap);
	void *p = heap_alloc(h, n);
	MUTEX_UNLOCK(g_kheap);
	return p;
}

heap_status_t kfree(void *p) {
	heap_t *h = MUTEX_LOCK(g_kheap);
	heap_status_t st = heap_free(h, p);
	MUTEX_UNLOCK(g_kheap);
	return st;
}
