#include <gdt.h>

static uint8_t kernel_stack[1024] __attribute__((aligned (16)));

/* GDT Descriptor (GDTR) points to the GDT which will be loaded via asm
 * in load_gdt(). See: https://wiki.osdev.org/Global_Descriptor_Table#GDTR */
struct __attribute__((packed)) gdt_gdtr {
	uint16_t limit; /* size of GDT - 1 */
	uint64_t offset; /* linear address; not physical*/
};
_Static_assert(sizeof(struct gdt_gdtr) == 10, "GDTR size faulty(!=10)!");

/* Descriptor for each entry in the GDT*/
struct __attribute__((packed)) gdt_seg_descriptor {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t acc_byte;
	uint8_t limit_high : 4;
	uint8_t flags : 4;
	uint8_t base_high;
};

/* TSS Descrtiptor */
struct __attribute__((packed)) gdt_tss_descriptor {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t acc_byte;
	uint8_t limit_high : 4;
	uint8_t flags : 4;
	uint8_t base_high;
	uint32_t base_long;
	uint32_t reserved;
};

struct __attribute__((packed)) gdt_tss {
	uint32_t reserved0;
	uint64_t rsp[3];
	uint64_t reserved1;
	uint64_t ist[7];
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iopb;
};
_Static_assert(sizeof(struct gdt_tss) == 104, "TSS Segment size faulty (!= 104)!");

/* returns a GDT segment descriptor. Cannot be used for TSS because only access
 * and flags can be modyfied here. */
static struct gdt_seg_descriptor generate_entry(uint8_t access, uint8_t flags) {
	return (struct gdt_seg_descriptor){
		.limit_low = 0x0FFFF,
		.base_low = 0,
		.base_middle = 0,
		.acc_byte = access,
		.limit_high = 0xF,
		.flags = flags,
		.base_high = 0
	};
};

_Static_assert(sizeof(struct gdt_seg_descriptor) == 8, "GDT Segment size faulty (!= 8)!");

static struct gdt_tss_descriptor generate_tss_entry(uint8_t access, uint8_t flags,
		uint64_t base, uint32_t limit) {
	return (struct gdt_tss_descriptor){
		.limit_low = limit & 0xFFFF,
		.base_low = base & 0xFFFF,
		.base_middle = (base >> 16) & 0xFF,
		.acc_byte = access,
		.limit_high = (limit >> 16) & 0xF,
		.flags = flags,
		.base_high = (base >> 24) & 0xFF,
		.base_long = (uint32_t)(base >> 32),
		.reserved = 0x00,
	};
};
_Static_assert(sizeof(struct gdt_tss_descriptor) == 16, "TSS descriptor size faulty (!= 16)!");

/* The GDT itself */
__attribute__((packed)) struct gdt_gdt{
	struct gdt_seg_descriptor null;
	struct gdt_seg_descriptor kernel_code;
	struct gdt_seg_descriptor kernel_data;
	struct gdt_tss_descriptor tss;
	struct gdt_seg_descriptor user_data;
	struct gdt_seg_descriptor user_code;
};
_Static_assert(sizeof(struct gdt_gdt) == 56, "GDT size faulty (!= 56)!");

/* The GDTR itself */
static struct gdt_gdtr gdtr;
static struct gdt_tss tss;
static struct gdt_gdt gdt;

static void load_gdt(void) {
	__asm__ volatile("lgdt %0" : : "m"(gdtr) : "memory");
}

static void reload_segments(void) {
	/* Kernel Data Segment begins at offset 0x10. Data Segments need to point to
	 * 0x10 now */
	__asm__ volatile(
			"mov $0x10, %%ax\n\t"
			"mov %%ax, %%ds\n\t"
			"mov %%ax, %%es\n\t"
			"mov %%ax, %%ss\n\t"
			"mov %%ax, %%fs\n\t"
			"mov %%ax, %%gs"
			: /* no output needed*/
			: /* no inputs needed*/
			: "ax", "memory");

	/* Kernel Code Segment begins at 0x08 */
	__asm__ volatile(
			"pushq $0x08\n\t"
			"lea 1f(%%rip), %%rax\n\t" /* load address from rip(next label 1) into rax */
			"pushq %%rax\n\t"
			"lretq\n\t"
			"1:"
			:
			:
			: "rax", "memory");

	__asm__ volatile(
			"mov $0x18, %%ax\n\t"
			"ltr %%ax"
			:
			:
			: "ax", "memory");
}

int8_t gdt_init(void) {
	/* gdt[0] is the null-descriptor. static is guaranteed to init all fields with 0 */
	/* Kernel Code Segment; Access: 10011011 Flags: 1010 */
	gdt.kernel_code = generate_entry(0x9B, 0xA);
	/* Kernel Data Segment; Access: 10010011 Flags: 1100 */
	gdt.kernel_data = generate_entry(0x93, 0xC);
	/* TSS */
	gdt.tss = generate_tss_entry(0x89, 0x0, (uint64_t)&tss, sizeof(tss) - 1);
	tss.ist[0] = (uint64_t)(kernel_stack + sizeof(kernel_stack));
	/* User Data Segment; Access: */
	gdt.user_data = generate_entry(0xF3, 0xC);
	/* User Code Segment */
	gdt.user_code = generate_entry(0xFB, 0xA);

	gdtr = (struct gdt_gdtr){
		.limit = sizeof(gdt) - 1,
		.offset = (uint64_t)&gdt,
	};

	load_gdt();
	reload_segments();

	return 0;
}
