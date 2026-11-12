#include <fpu.h>
#include <stdint.h>

void fpu_init(void) {
	uint64_t cr0, cr4;

	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 &= ~((1ULL << 2) | (1ULL << 3));
	cr0 |=  (1ULL << 1) | (1ULL << 5);
	asm volatile("mov %0, %%cr0" :: "r"(cr0));

	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= (1ULL << 9) | (1ULL << 10);
	asm volatile("mov %0, %%cr4" :: "r"(cr4));

	asm volatile("fninit");

	uint32_t mxcsr = 0x1F80;
	asm volatile("ldmxcsr %0" :: "m"(mxcsr));
}
