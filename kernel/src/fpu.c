#include <fpu.h>
#include <stdint.h>
#include <memory.h>

static inline uint64_t read_cr0(void)  { uint64_t v; asm volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static inline uint64_t read_cr4(void)  { uint64_t v; asm volatile("mov %%cr4, %0" : "=r"(v)); return v; }
static inline void write_cr0(uint64_t v) { asm volatile("mov %0, %%cr0" :: "r"(v) : "memory"); }
static inline void write_cr4(uint64_t v) { asm volatile("mov %0, %%cr4" :: "r"(v) : "memory"); }

#define MXCSR_DEFAULT 0x1F80

static uint8_t fpu_template[512] __attribute__((aligned(16)));

void fpu_init(void) {
	uint64_t cr0 = read_cr0();
	cr0 &= ~(1ULL << 2);
	cr0 |= (1ULL << 1);
	cr0 &= ~(1ULL << 3);
	write_cr0(cr0);

	uint64_t cr4 = read_cr4();
	cr4 |= (1ULL << 9);
	cr4 |= (1ULL << 10);
	write_cr4(cr4);

	asm volatile("fninit");

	uint32_t mxcsr = MXCSR_DEFAULT;
	asm volatile("ldmxcsr %0" :: "m"(mxcsr));

	memset(fpu_template, 0, sizeof fpu_template);
	asm volatile("fxsave %0" : "=m"(fpu_template) :: "memory");
}

void fpu_state_init(void *save_area) {
	memcpy(save_area, fpu_template, 512);
}
