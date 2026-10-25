#include <vmm.h>
#include <stddef.h>
#include <memory.h>

struct limine_hhdm_response *hhdm_response = NULL;
static uint64_t hhdm_offset = 0;
static bitmap_allocator_t *ba = NULL;

/* takes a virtual addr and returns seperate entries to access individual tables */
struct virt_addr_t separate_addr(uint64_t addr) {
	uint16_t offset, PT, PD, PDP, PML4;
	offset = addr & 0xFFF;
	addr >>= 12;
	PT = addr & 0x1FF;
	addr >>= 9;
	PD = addr & 0x1FF;
	addr >>= 9;
	PDP = addr & 0x1FF;
	addr >>= 9;
	PML4 = addr & 0x1FF;

	return (struct virt_addr_t) {
		.offset = offset,
		.PT = PT,
		.PD = PD,
		.PDP = PDP,
		.PML4 = PML4
	};
}

uint64_t *get_table(uint64_t addr) {
	addr &= VMM_ADDR_MASK_4K;
	return (uint64_t *)(addr + hhdm_offset);
}

/* takes a new frame with allocate_frame, initialize it with zero to prevent
 * faulty walk */
static uint64_t get_new_table(void) {
	uint64_t new_frame = allocate_frame(ba);
	if (new_frame == 0) {
		return 0;
	}

	uint64_t *new_table = get_table(new_frame);
	memset(new_table, 0, VMM_ENTRIES * sizeof(*new_table));

	return (uint64_t)new_frame;
}

vmm_status_t vmm_translate(uint64_t phys_pml4, uint64_t virt, uint64_t *phys_ret) {
	if (!(((int64_t)virt >> 47) == 0 || ((int64_t)virt >> 47) == -1)) {
		return VMM_ERR_NON_CANONICAL;
	}

	struct virt_addr_t sep = separate_addr(virt);
	uint64_t idxs[4] = {sep.PML4, sep.PDP, sep.PD, sep.PT};
	uint64_t *tbl = get_table(phys_pml4);

	for (uint8_t lvl = 4; lvl >= 1; lvl--) {
		uint64_t entry = tbl[ idxs[4 - lvl] ];

		if (!(entry & VMM_PRESENT)) {
			return VMM_ERR_NOT_MAPPED;
		}

		if (lvl == 1) {
			*phys_ret = (entry & VMM_ADDR_MASK_4K) + sep.offset;
			return VMM_OK;
		}

		if ((lvl == 3 || lvl == 2) && (entry & VMM_PS)) {
			uint64_t page_size = (lvl == 2) ? VMM_PAGE_SIZE_2M : VMM_PAGE_SIZE_1G;
			uint64_t addr_mask = (lvl == 2) ? VMM_ADDR_MASK_2M : VMM_ADDR_MASK_1G;
			*phys_ret = (entry & addr_mask) + (virt & (page_size - 1));

			return VMM_OK;
		}

		tbl = get_table(entry);
	}

	return VMM_ERR_UNKNOWN_ERR;
}

vmm_status_t vmm_map(uint64_t phys_pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
	if (!(((int64_t)virt >> 47) == 0 || ((int64_t)virt >> 47) == -1)) {
		return VMM_ERR_NON_CANONICAL;
	}

	if (((virt & (VMM_PAGE_SIZE_4K - 1)) != 0) || ((phys & (VMM_PAGE_SIZE_4K - 1)) != 0)) {
		return VMM_ERR_NOT_ALIGNED;
	}

	if ((flags & VMM_ADDR_MASK_4K) != 0) {
		return VMM_ERR_ADDR_FLAGS;
	}

	if ((phys & ~VMM_ADDR_MASK_4K) != 0) {
		return VMM_ERR_NOT_ALIGNED;
	}

	struct virt_addr_t sep = separate_addr(virt);
	uint64_t idxs[4] = {sep.PML4, sep.PDP, sep.PD, sep.PT};
	uint64_t *tbl = get_table(phys_pml4);

	for (uint8_t lvl = 4; lvl >= 2; lvl--) {
		uint64_t entry = tbl[ idxs[4 - lvl] ];
		if (!(entry & VMM_PRESENT)) {
			uint64_t new_tbl = get_new_table();
			if (new_tbl == 0) {
				return VMM_ERR_OUT_OF_MEM;
			}

			tbl[ idxs[4 - lvl] ] = new_tbl | VMM_PRESENT | VMM_RW | VMM_US;
			tbl = get_table(new_tbl);
		} else if ((entry & VMM_PS)) {
			return VMM_ERR_HUGE_PAGE_CONFLICT;
		} else {
			tbl = get_table(entry);
		}
	}

	if (tbl[ idxs[3] ] & VMM_PRESENT) {
		return VMM_ERR_ALR_MAPPED;
	} else {
		tbl[ idxs[3] ] = phys | flags | VMM_PRESENT;
		return VMM_OK;
	}
}

vmm_status_t vmm_init(bitmap_allocator_t *bitmap_a) {
	if (hhdm_request.response == NULL) {
		return VMM_ERR_NULL_HHDM;
	}
	if (bitmap_a == NULL) {
		return VMM_ERR_NULL_BA;
	}

	hhdm_response = hhdm_request.response;
	hhdm_offset = hhdm_response->offset;
	ba = bitmap_a;

	/* for future use */
	uint64_t cr3;
	__asm__ volatile("mov %%cr3, %0" : "=r"(cr3) : : "memory");

	return VMM_OK;
}
