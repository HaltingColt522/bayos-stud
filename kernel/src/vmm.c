#include <vmm.h>
#include <stddef.h>
#include <memory.h>
#include <printk.h>

struct limine_hhdm_response *hhdm_response = NULL;
static uint64_t hhdm_offset = 0;

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
static uint64_t get_new_table(bitmap_allocator_t *ba) {
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

vmm_status_t vmm_map(bitmap_allocator_t *ba, uint64_t phys_pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
	if (!(((int64_t)virt >> 47) == 0 || ((int64_t)virt >> 47) == -1)) {
		return VMM_ERR_NON_CANONICAL;
	}

	if (((virt & (VMM_PAGE_SIZE_4K - 1)) != 0) || ((phys & (VMM_PAGE_SIZE_4K - 1)) != 0)) {
		return VMM_ERR_NOT_ALIGNED;
	}

	if ((flags & VMM_ADDR_MASK_4K) != 0) {
		return VMM_ERR_ADDR_FLAGS;
	}

	struct virt_addr_t sep = separate_addr(virt);
	uint64_t idxs[4] = {sep.PML4, sep.PDP, sep.PD, sep.PT};
	uint64_t *tbl = get_table(phys_pml4);

	for (uint8_t lvl = 4; lvl >= 2; lvl--) {
		uint64_t entry = tbl[ idxs[4 - lvl] ];
		if (!(entry & VMM_PRESENT)) {
			uint64_t new_tbl = get_new_table(ba);
			if (new_tbl == 0) {
				return VMM_ERR_OUT_OF_MEM;
			}

			tbl[ idxs[4 - lvl] ] = new_tbl | VMM_PRESENT | VMM_RW | VMM_US;
			tbl = get_table(new_tbl);
		} else if (entry & VMM_PS) {
			return VMM_ERR_HUGE_PAGE_CONFLICT;
		} else {
			tbl = get_table(entry);
		}
	}

	if (tbl[ idxs[3] ] & VMM_PRESENT) {
		return VMM_ERR_ALR_MAPPED;
	} else {
		tbl[ idxs[3] ] = phys | flags | VMM_PRESENT;
		/* doesn't need to be marked as invalid (like in unmap) because the CPU
		 * only caches present entries, need to invalidate only when entries get
		 * set as not present */
		return VMM_OK;
	}
}

vmm_status_t vmm_unmap(uint64_t phys_pml4, uint64_t virt, uint64_t *phys_ret) {
	if ((phys_pml4 == 0) || (phys_ret == NULL)) {
		return VMM_ERR_NULL_FRAME;
	}
	if (!(((int64_t)virt >> 47) == 0 || ((int64_t)virt >> 47) == -1)) {
		return VMM_ERR_NON_CANONICAL;
	}

	if ((virt & (VMM_PAGE_SIZE_4K - 1)) != 0) {
		return VMM_ERR_NOT_ALIGNED;
	}

	struct virt_addr_t sep = separate_addr(virt);
	uint64_t idxs[4] = {sep.PML4, sep.PDP, sep.PD, sep.PT};
	uint64_t *tbl = get_table(phys_pml4);

	for (uint8_t lvl = 4; lvl >= 2; lvl--) {
		uint64_t entry = tbl[ idxs[4 - lvl] ];

		if (!(entry & VMM_PRESENT)) {
			return VMM_ERR_NOT_MAPPED;
		}

		if (entry & VMM_PS) {
			return VMM_ERR_HUGE_PAGE_CONFLICT;
		}

		tbl = get_table(entry);
	}

	uint64_t leaf = tbl[ idxs[3] ];

	if (!(leaf & VMM_PRESENT)) {
		return VMM_ERR_NOT_MAPPED;
	}

	*phys_ret = leaf & VMM_ADDR_MASK_4K;

	tbl[ idxs[3] ] = 0;

	__asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");

	return VMM_OK;
}

static vmm_status_t vmm_rollback(bitmap_allocator_t *ba, uint64_t phys_pml4, uint64_t virt, uint64_t count, uint64_t free_frames) {
	uint64_t phys;
	vmm_status_t st;
	vmm_status_t result = VMM_OK;

	for (uint64_t i = 0; i < count; i++) {
		uint64_t addr = virt + i * VMM_PAGE_SIZE_4K;

		st = vmm_unmap(phys_pml4, addr, &phys);
		if (st != VMM_OK) {
			printk(QEMU_SERIAL | FB, "[FAIL] unmap in rollback failed at %x with code %d\n",
			       addr, (int)st);
			if (result == VMM_OK) { result = VMM_ERR_ROLLBACK; }
			continue;
		}

		if (free_frames) {
			pmm_status_t fs = free_frame(ba, phys);

			if (fs != PMM_OK) {
				printk(QEMU_SERIAL | FB, "[FAIL] free_frame in rollback failed at %x with code %d\n",
				       (uint64_t)phys, (int)fs);
				if (result == VMM_OK) { result = VMM_ERR_ROLLBACK; }
			}
		}
	}

	return result;
}

static vmm_status_t vmm_validate_virt_range(uint64_t virt, uint64_t size) {
	if ((virt & (VMM_PAGE_SIZE_4K - 1)) != 0) {
		return VMM_ERR_NOT_ALIGNED;
	}

	/* int overflow */
	if ((size - VMM_PAGE_SIZE_4K) > (UINT64_MAX - virt)) {
		return VMM_ERR_OVERFLOW;
	}

	uint64_t last = virt + size - VMM_PAGE_SIZE_4K;
	uint64_t top_first = virt >> 47;
	uint64_t top_last = last >> 47;

	if ((top_first != 0) && (top_first != 0x1FFFF)) {
		return VMM_ERR_NON_CANONICAL;
	}

	if ((top_last != 0) && (top_last != 0x1FFFF)) {
		return VMM_ERR_NON_CANONICAL;
	}

	if (top_first != top_last) {
		return VMM_ERR_NON_CANONICAL;
	}

	return VMM_OK;
}

static vmm_status_t vmm_validate_phys_range(uint64_t phys, uint64_t size) {
	if ((phys & (VMM_PAGE_SIZE_4K - 1)) != 0) {
		return VMM_ERR_NOT_ALIGNED;
	}

	/* bigger than 52 bit */
	if (phys >= (1ULL << 52) ||
	        (size - VMM_PAGE_SIZE_4K) >= (1ULL << 52) - phys) {
		return VMM_ERR_OVERFLOW;
	}

	return VMM_OK;
}

/* maps a range of pages of size size. size needs to be 4KiB-aligned in order to work
 * note: number of pages = size / 4096 */
vmm_status_t vmm_map_range(bitmap_allocator_t *ba, uint64_t phys_pml4, uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags) {
	if ((size == 0) || ((size % VMM_PAGE_SIZE_4K) != 0)) {
		return VMM_ERR_SIZE;
	}

	vmm_status_t st = vmm_validate_virt_range(virt, size);
	if (st != VMM_OK) { return st; }
	st = vmm_validate_phys_range(phys, size);
	if (st != VMM_OK) { return st; }

	uint64_t pages = size / VMM_PAGE_SIZE_4K;

	for (uint64_t i = 0; i < pages; i++) {
		st = vmm_map(ba, phys_pml4, virt + i * VMM_PAGE_SIZE_4K, phys + i * VMM_PAGE_SIZE_4K, flags);
		if (st != VMM_OK) {
			vmm_status_t rb = vmm_rollback(ba, phys_pml4, virt, i, 0);
			if (rb != VMM_OK) {
				return rb;
			}
			return st;
		}
	}

	return VMM_OK;
}

/* unmaps a range of pages; if free_frames is set to != 0, frames will be freed */
vmm_status_t vmm_unmap_range(bitmap_allocator_t *ba, uint64_t phys_pml4, uint64_t virt, uint64_t size, uint64_t free_frames) {
	if ((size == 0) || ((size % VMM_PAGE_SIZE_4K) != 0)) {
		return VMM_ERR_SIZE;
	}

	vmm_status_t st = vmm_validate_virt_range(virt, size);
	if (st != VMM_OK) { return st; }

	uint64_t pages = size / VMM_PAGE_SIZE_4K;

	vmm_status_t result = VMM_OK;
	uint64_t phys;

	for (uint64_t i = 0; i < pages; i++) {
		st = vmm_unmap(phys_pml4, virt + i * VMM_PAGE_SIZE_4K, &phys);

		if (st == VMM_ERR_NOT_MAPPED) { continue; } /* goal achieved, nothing to do */
		if (st != VMM_OK) {
			if (result == VMM_OK) { result = st; }
			continue;
		}

		if (free_frames) {
			pmm_status_t fs = free_frame(ba, phys);
			if (fs != PMM_OK) {
				if (result == VMM_OK) { result = VMM_ERR_PMM; }
				printk(QEMU_SERIAL | FB, "[FAIL] free_frame %x failed (%d)\n",
				       (uint64_t)phys, (int)fs);
			}
		}
	}

	return result;
}

vmm_status_t vmm_copy_range(bitmap_allocator_t *ba, uint64_t src_pml4, uint64_t src_virt, uint64_t dst_pml4,
                            uint64_t dst_virt, uint64_t size, uint64_t flags) {
	if ((size == 0) || ((size % VMM_PAGE_SIZE_4K) != 0)) {
		return VMM_ERR_SIZE;
	}

	vmm_status_t st = vmm_validate_virt_range(src_virt, size);
	if (st != VMM_OK) { return st; }
	st = vmm_validate_virt_range(dst_virt, size);
	if (st != VMM_OK) { return st; }

	if ((flags & VMM_ADDR_MASK_4K) != 0) {
		return VMM_ERR_ADDR_FLAGS;
	}

	if (src_pml4 == dst_pml4) {
		uint64_t diff = (src_virt < dst_virt) ? (dst_virt - src_virt) : (src_virt - dst_virt);
		if (diff < size) {
			return VMM_ERR_OVERLAP;
		}
	}

	uint64_t pages = size / VMM_PAGE_SIZE_4K;

	for (uint64_t i = 0; i < pages; i++) {
		uint64_t src = src_virt + i * VMM_PAGE_SIZE_4K;
		uint64_t dst = dst_virt + i * VMM_PAGE_SIZE_4K;

		/* resolve src */
		uint64_t src_phys;
		st = vmm_translate(src_pml4, src, &src_phys);
		if (st != VMM_OK) {
			vmm_status_t rb = vmm_rollback(ba, dst_pml4, dst_virt, i, 1);
			return (rb != VMM_OK) ? rb : st;
		}

		/* get dst frame */
		uint64_t dst_phys = allocate_frame(ba);
		if (dst_phys == 0) {
			vmm_status_t rb = vmm_rollback(ba, dst_pml4, dst_virt, i, 1);
			return (rb != VMM_OK) ? rb : VMM_ERR_OUT_OF_MEM;
		}

		memcpy((void *)(dst_phys + hhdm_offset), (const void *)(src_phys + hhdm_offset), VMM_PAGE_SIZE_4K);

		/* map dst */
		st = vmm_map(ba, dst_pml4, dst, dst_phys, flags);
		if (st != VMM_OK) {
			free_frame(ba, dst_phys);
			vmm_status_t rb = vmm_rollback(ba, dst_pml4, dst_virt, i, 1);
			return (rb != VMM_OK) ? rb : st;
		}
	}

	return VMM_OK;
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

	return VMM_OK;
}
