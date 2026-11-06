#ifndef _ALLOC_H
#define _ALLOC_H

#include <stdint.h>
#include <stddef.h>

#define BLOCK_FREE 0x46524545 /* FREE */
#define BLOCK_USED 0x55534544 /* USED */

typedef struct block {
	uint64_t size;
	uint64_t magic;
	struct block *prev;
	struct block *next;
} block_t;

_Static_assert(sizeof(block_t) % 16 == 0, "block_t size faulty (!= 16)");

typedef struct {
	uint8_t *base;
	uint64_t size;
	block_t *head;
} heap_t;

#define HEAP_ALIGN 16
#define BLOCK_HDR  sizeof(block_t)
#define HEAP_MIN_SPLIT  (BLOCK_HDR + HEAP_ALIGN)

typedef enum {
	HEAP_OK = 0,
	HEAP_ERR_NULL = 1,
	HEAP_ERR_NOT_ALIGNED = 2,
	HEAP_ERR_SIZE = 3,
	HEAP_ERR_NOT_IN_RANGE = 4,
	HEAP_ERR_DOUBLE_FREE = 5,
	HEAP_ERR_CORRUPT = 6,
} heap_status_t;

heap_status_t heap_init(heap_t *heap, uint8_t *base, uint64_t size);
void *heap_alloc(heap_t *heap, uint64_t n);
heap_status_t heap_free(heap_t *heap, void *p);

#endif // !_ALLOC_H
