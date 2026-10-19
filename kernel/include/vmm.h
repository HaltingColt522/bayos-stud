#ifndef _VMM_H
#define _VMM_H

#define VMM_ENTRIES (512)

#define VMM_PAGE_SIZE_4K (0x1000ULL)
#define VMM_PAGE_SIZE_2M (0x200000ULL)
#define VMM_PAGE_SIZE_1G (0x40000000ULL)

#define VMM_PRESENT	(1ULL)
#define VMM_RW		(1ULL << 1)
#define VMM_US		(1ULL << 2)
#define VMM_PWT		(1ULL << 3)
#define VMM_PCD		(1ULL << 4)
#define VMM_A		(1ULL << 5)
#define VMM_D		(1ULL << 6)
#define VMM_PAT		(1ULL << 7)
#define VMM_PS		(1ULL << 7)
#define VMM_G		(1ULL << 8)
#define VMM_PAT_HUG	(1ULL << 12)
#define VMM_XD		(1ULL << 63)

#define VMM_ADDR_MASK_4K (0x000ffffffffff000ULL)
#define VMM_ADDR_MASK_2M (0x000fffffffe00000ULL)
#define VMM_ADDR_MASK_1G (0x000fffffc0000000ULL)

#include <stdint.h>
#include <pmm.h>
#include <limine.h>

struct virt_addr_t {
	uint64_t offset;
	uint64_t PT;
	uint64_t PD;
	uint64_t PDP;
	uint64_t PML4;
};

typedef enum {
	VMM_OK = 0,
	VMM_ERR_NOT_MAPPED = 1,
	VMM_ERR_NON_CANONICAL = 2,
	VMM_ERR_UNKNOWN_ERR = 3,
	VMM_ERR_NULL_HHDM = 4,
	VMM_ERR_NULL_BA = 5,
	VMM_ERR_NULL_FRAME = 6,
	VMM_ERR_OUT_OF_MEM = 7,
	VMM_ERR_HUGE_PAGE_CONFLICT = 8,
} vmm_status_t;


extern volatile struct limine_hhdm_request hhdm_request;

vmm_status_t vmm_init(bitmap_allocator_t *ba);
struct virt_addr_t separate_addr(uint64_t addr);
vmm_status_t vmm_translate(uint64_t phys_pml4, uint64_t virt, uint64_t *phys_ret);
vmm_status_t vmm_map(uint64_t phys_pml4, uint64_t virt, uint64_t phys, uint64_t flags);

#endif // !_VMM_H
