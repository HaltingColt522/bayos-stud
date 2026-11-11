#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <pmm.h>

#define PF_X (1ULL << 0) /* executable */
#define PF_W (1ULL << 1) /* writable */
#define PF_R (1ULL << 2) /* readable */

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYN 2
#define PT_INTERP 3
#define PT_NOTE 4

typedef enum {
	ELF_OK = 0,
	ELF_ERR_SIZE = 1,
	ELF_ERR_MAGIC = 2,
	ELF_ERR_ARCH = 3,
	ELF_ERR_ENDIAN = 4,
	ELF_ERR_TYPE = 5,
	ELF_ERR_ISET = 6,
	ELF_ERR_OFFSET = 7,
	ELF_ERR_LITTLE_MEM = 8,
	ELF_ERR_NO_DATA = 9,
	ELF_ERR_FILE_RANGE = 10,
	ELF_ERR_OVERFLOW = 11,
	ELF_ERR_VMM = 12,
	ELF_ERR_NO_MEM = 13,
	ELF_ERR_ENTRY = 14,
	ELF_ERR_NULL = 15,
	ELF_ERR_RANGE = 16,
	ELF_ERR_OCCUPIED = 17,
} elf_status_t;

typedef struct __attribute__((packed)) {
	uint32_t magic;	/* 0x7F'E''L''F' */
	uint8_t arch;	/* 1 = 32 bit, 2 = 64 bit */
	uint8_t endian;	/* 1 = little endian, 2 = big endian */
	uint8_t h_vers;	/* ELF hdr version */
	uint8_t osabi;
	uint64_t pad;	/* unused space, padding */
	uint16_t type;	/* 1 = relocatable, 2 = executable, 3 = shared, 4 = core */
	uint16_t iset;	/* instruction set, for x86_64 0x3E */
	uint32_t vers;	/* ELF version */
	uint64_t eoff;	/* progr entry offset */
	uint64_t htoff;	/* progr hdr table offset */
	uint64_t shoff;	/* section hdr table offset */
	uint32_t flags;
	uint16_t ehdrs;	/* ELF hdr size */
	uint16_t phesz; /* size of an entry in program hdr table */
	uint16_t phect; /* number of entries in program hdr table */
	uint16_t shesz; /* size of an entry in section hdr table */
	uint16_t shect; /* number of entries in section hdr table */
	uint16_t sindx; /* section index to section hdr string table */
}
elf64_ehdr; /* ELF hdr itself */

typedef struct __attribute__((packed)) {
	/* 0 = ignore, 1 = load, 2 = dynamic, 3 = interp, 4 = note
	 * more see: https://wiki.osdev.org/ELF#Program_header */
	uint32_t type;
	uint32_t flags; /* 1 = executable, 2 = writable, 4 = readable */
	uint64_t p_offset;	/* offset in the file where data to this seg can be found */
	uint64_t p_vaddr;	/* start of virt memory to put this segment */
	uint64_t p_paddr;	/* reserved for phys addr of segment */
	uint64_t p_filesz;	/* size of segment in file */
	uint64_t p_memsz;	/* size of segment in memory */
	uint64_t align;		/* required alignment */
}
elf64_phdr_entry; /* program header entry */

elf_status_t elf_load(bitmap_allocator_t *ba, uint64_t pml4, uint8_t *elf, uint64_t size, uint64_t *entry);

#endif // !ELF_H
