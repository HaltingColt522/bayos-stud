#include <alloc.h>

void *heap_alloc(heap_t *heap, uint64_t n) {
	if ((heap == NULL) || (n == 0)) { return NULL; }
	if (n > heap->size) { return NULL; }

	n = (n + HEAP_ALIGN - 1) & ~((uint64_t)HEAP_ALIGN - 1);

	for (block_t *b = heap->head; b; b = b->next) {
		if (b->magic == BLOCK_USED) { continue; }
		/* if current block isn't used nor free, something is corrupted
		 * (note: magic is initialized to BLOCK_FREE) */
		if (b->magic != BLOCK_FREE) { return NULL; }
		if (b->size < n) { continue; }

		if ((b->size - n) >= HEAP_MIN_SPLIT) {
			block_t *remainder = (block_t *)((uint8_t *)b + BLOCK_HDR + n);
			remainder->size = b->size - n - BLOCK_HDR;
			remainder->magic = BLOCK_FREE;
			remainder->prev = b;
			remainder->next = b->next;
			if (b->next != NULL) { b->next->prev = remainder; }
			b->next = remainder;
			b->size = n;
		}

		b->magic = BLOCK_USED;

		return (uint8_t *)b + BLOCK_HDR;
	}

	return NULL;
}

heap_status_t heap_free(heap_t *heap, void *p) {
	if ((heap == NULL) || (p == NULL)) { return HEAP_ERR_NULL; }
	if (((uintptr_t)p % HEAP_ALIGN) != 0) { return HEAP_ERR_NOT_ALIGNED; }
	if ((p < (heap->base + BLOCK_HDR)) || (p >= (heap->base + heap->size))) {
		return HEAP_ERR_NOT_IN_RANGE;
	}

	block_t *b = (block_t *)((uint8_t *)p - BLOCK_HDR);
	if (b->magic == BLOCK_FREE) { return HEAP_ERR_DOUBLE_FREE; }
	if (b->magic != BLOCK_USED) { return HEAP_ERR_CORRUPT; }

	b->magic = BLOCK_FREE;

	/* if next block is also free, both can be united */
	if ((b->next != NULL) && (b->next->magic == BLOCK_FREE)) {
		b->size += BLOCK_HDR + b->next->size;
		b->next = b->next->next;
		if (b->next != NULL) { b->next->prev = b; }
	}

	/* if prev block is free, both can be united */
	if ((b->prev != NULL) && (b->prev->magic == BLOCK_FREE)) {
		b->prev->size += BLOCK_HDR + b->size;
		b->prev->next = b->next;
		if (b->next != NULL) { b->next->prev = b->prev; }
	}

	return HEAP_OK;
}

heap_status_t heap_init(heap_t *heap, uint8_t *base, uint64_t size) {
	if ((heap == NULL) || (base == NULL)) { return HEAP_ERR_NULL; }
	if (((uintptr_t)base % HEAP_ALIGN) != 0) { return HEAP_ERR_NOT_ALIGNED; }

	size = (size / HEAP_ALIGN) * HEAP_ALIGN;

	if (size < HEAP_MIN_SPLIT) { return HEAP_ERR_SIZE; }

	heap->base = base;
	heap->size = size;
	heap->head = (block_t *)base;
	block_t *head = heap->head;

	head->size = size - BLOCK_HDR;
	head->magic = BLOCK_FREE;
	head->prev = NULL;
	head->next = NULL;

	return HEAP_OK;
}
