#include <elf.h>
#include <memory.h>
#include <vmm.h>
#include <pmm.h>
#include <stdbool.h>

static elf_status_t elf_read_ehdr(uint8_t *elf, uint64_t size, elf64_ehdr *eh) {
	if (size < sizeof(elf64_ehdr)) { return ELF_ERR_SIZE; }
	memcpy(eh, elf, sizeof(elf64_ehdr));

	if (eh->magic != 0x464C457F) { return ELF_ERR_MAGIC; }
	if (eh->arch != 0x02) { return ELF_ERR_ARCH; }
	if (eh->endian != 0x01) { return ELF_ERR_ENDIAN; }
	if (eh->type != 0x02) { return ELF_ERR_TYPE; }
	if (eh->iset != 0x3E) { return ELF_ERR_ISET; }
	if (eh->phesz != sizeof(elf64_phdr_entry)) { return ELF_ERR_SIZE; }
	if (eh->htoff > size) { return ELF_ERR_OFFSET; }
	if ((eh->phect * sizeof(elf64_phdr_entry)) > (size - eh->htoff)) { return ELF_ERR_SIZE;}

	return ELF_OK;
}

static elf_status_t elf_read_phdr(uint8_t *elf, elf64_ehdr *eh, uint64_t i, elf64_phdr_entry *ph) {
	memcpy(ph, elf + eh->htoff + i * sizeof(elf64_phdr_entry), sizeof(elf64_phdr_entry));
	return ELF_OK;
}

static void elf_seg_bounds(elf64_phdr_entry *ph, uint64_t *start, uint64_t *end) {
	*start = ph->p_vaddr & ~((uint64_t)VMM_PAGE_SIZE_4K - 1);
	*end = (ph->p_vaddr + ph->p_memsz + 4095) & ~((uint64_t)VMM_PAGE_SIZE_4K - 1);
}

static elf_status_t elf_validate_segment(elf64_phdr_entry *ph, uint64_t elf_size) {
	if (ph->p_filesz > ph->p_memsz) { return ELF_ERR_LITTLE_MEM; }
	if (ph->p_offset > elf_size) { return ELF_ERR_NO_DATA; }
	if (ph->p_filesz > (elf_size - ph->p_offset)) { return ELF_ERR_FILE_RANGE; }
	if ((ph->p_vaddr + ph->p_memsz) < ph->p_vaddr) { return ELF_ERR_OVERFLOW; }

	if (ph->p_vaddr < 4096) { return ELF_ERR_NULL; }
	if ((ph->p_vaddr + ph->p_memsz) > VMM_LOWER_HALF_END) { return ELF_ERR_RANGE; }

	uint64_t start, end;
	elf_seg_bounds(ph, &start, &end);

	vmm_status_t st = vmm_validate_virt_range(start, end - start);
	if (st != VMM_OK) { return ELF_ERR_VMM; }

	return ELF_OK;
}

static uint64_t elf_seg_flags(elf64_phdr_entry *ph) {
	uint64_t flags = 0 | VMM_US;

	if (ph->flags & PF_W) { flags |= VMM_RW; } /*if segment is alr writable, set VMM_RW */
	if (!(ph->flags & PF_X)) { flags |= VMM_XD; }

	return flags;
}

static void rollback_load(bitmap_allocator_t *ba, uint64_t pml4, uint8_t *elf, elf64_ehdr *eh, uint64_t n) {
	elf64_phdr_entry ph;
	for (uint64_t i = 0; i < n; i++) {
		uint64_t start, end, phys;
		elf_read_phdr(elf, eh, i, &ph);
		if ((ph.type != PT_LOAD) || (ph.p_memsz == 0)) { continue; }

		elf_seg_bounds(&ph, &start, &end);

		for (uint64_t vaddr = start; vaddr < end; vaddr += VMM_PAGE_SIZE_4K) {
			if (vmm_translate(pml4, vaddr, &phys) != VMM_OK) { continue; }
			vmm_unmap(pml4, vaddr, &phys);
			free_frame(ba, phys);
		}
	}
}

static elf_status_t elf_map_seg(bitmap_allocator_t *ba, uint64_t pml4, uint64_t start, uint64_t end, uint64_t flags) {
	uint64_t phys;
	for (uint64_t vaddr = start; vaddr < end; vaddr += VMM_PAGE_SIZE_4K) {
		vmm_status_t st = vmm_translate(pml4, vaddr, &phys);
		/* pages belongs to neighborsegment */
		if (st == VMM_OK) { continue; }

		phys = allocate_frame(ba);
		if (phys == 0) { return ELF_ERR_NO_MEM; }

		/* should be initialized with zero so no memory contains data from other
		 * stuff */
		memset((void *)(hhdm_request.response->offset + phys), 0, VMM_PAGE_SIZE_4K);

		st = vmm_map(ba, pml4, vaddr, phys, flags);
		if (st != VMM_OK) {
			free_frame(ba, phys);
			return ELF_ERR_VMM;
		}
	}

	return ELF_OK;
}

static elf_status_t elf_copy_seg(uint64_t pml4, uint8_t *elf, elf64_phdr_entry *ph) {
	uint64_t done = 0;
	while (done < ph->p_filesz) {
		uint64_t vaddr = ph->p_vaddr + done;
		uint64_t offset = vaddr % VMM_PAGE_SIZE_4K;
		uint64_t chunk = ((VMM_PAGE_SIZE_4K - offset) < (ph->p_filesz - done)) ?
		                 VMM_PAGE_SIZE_4K - offset : ph->p_filesz - done;

		uint64_t phys;
		vmm_status_t st = vmm_translate(pml4, vaddr & ~(VMM_PAGE_SIZE_4K - 1), &phys);
		if (st != VMM_OK) { return ELF_ERR_VMM; }

		memcpy((void *)(hhdm_request.response->offset + phys + offset),
		       (void *)(elf + ph->p_offset + done), chunk);

		done += chunk;
	}

	return ELF_OK;
}

static elf_status_t elf_check_free(uint64_t pml4, uint64_t start, uint64_t end) {
	uint64_t phys;
	for (uint64_t vaddr = start; vaddr < end; vaddr += VMM_PAGE_SIZE_4K) {
		if (vmm_translate(pml4, vaddr, &phys) == VMM_OK) {
			return ELF_ERR_OCCUPIED;
		}
	}

	return ELF_OK;
}

elf_status_t elf_load(bitmap_allocator_t *ba, uint64_t pml4, uint8_t *elf, uint64_t size, uint64_t *entry) {
	elf64_ehdr eh;
	elf_status_t st = elf_read_ehdr(elf, size, &eh);
	if (st != ELF_OK) { return st; }

	bool entry_ok = false;
	for (uint64_t i = 0; i < eh.phect; i++) {
		elf64_phdr_entry ph;
		elf_read_phdr(elf, &eh, i, &ph);
		if ((ph.type != PT_LOAD) || (ph.p_memsz == 0)) { continue; }

		st = elf_validate_segment(&ph, size);
		if (st != ELF_OK) { return st; }

		uint64_t start, end;
		elf_seg_bounds(&ph, &start, &end);
		st = elf_check_free(pml4, start, end);
		if (st != ELF_OK) { return st; }

		if ((ph.flags & PF_X) && (eh.eoff >= ph.p_vaddr) && (eh.eoff < ph.p_vaddr + ph.p_memsz)) {
			entry_ok = true;
		}
	}

	if (!entry_ok) { return ELF_ERR_ENTRY; }

	for (uint64_t i = 0; i < eh.phect; i++) {
		elf64_phdr_entry ph;
		elf_read_phdr(elf, &eh, i, &ph);
		if ((ph.type != PT_LOAD) || (ph.p_memsz == 0)) { continue; }

		uint64_t flags = elf_seg_flags(&ph);
		uint64_t start, end;
		elf_seg_bounds(&ph, &start, &end);

		st = elf_map_seg(ba, pml4, start, end, flags);
		if (st == ELF_OK) {
			st = elf_copy_seg(pml4, elf, &ph);
		}

		if (st != ELF_OK) {
			rollback_load(ba, pml4, elf, &eh, i + 1);
			return st;
		}
	}

	*entry = eh.eoff;

	return ELF_OK;
}
