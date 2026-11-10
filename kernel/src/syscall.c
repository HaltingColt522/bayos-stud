#include <syscall.h>
#include <io.h>
#include <stdint.h>
#include <printk.h>
#include <stddef.h>

extern void _syscall_handler;

void syscall_init(void) {
	// enable syscall / sysret on cpu
	wrmsr(0xC0000080, rdmsr(0xC0000080) | 1);
	// set the kernel and user code and data selector
	wrmsr(0xC0000081, (0x8 << 32) | (0x20 << 48));
	// set the syscall entry address
	wrmsr(0xC0000082, (uint64_t)&_syscall_handler);
	// clear these RLFLAGS bits on syscall entry
	// TF, IF, DF, AC
	wrmsr(0xC0000084, (1 << 9) | (1 << 10) | (1 << 8) | (1 << 18));

	// set fsbase and gsbase to 0x1000
	// this is somehow needed by mlibc
	// for rtdl
	wrmsr(0xC0000100, 0x1000);
	wrmsr(0xC0000101, 0x1000);
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len);
static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len);
static uint64_t anon_allocate_syscall(size_t size, uint64_t *dk);

uint64_t syscall_handler(uint64_t index, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
	switch (index) {
		case 0:
			return read_syscall(arg0, (uint8_t*)arg1, arg2);
		case 1:
			return write_syscall(arg0, (uint8_t*)arg1, arg2);
		case 2:
			return anon_allocate_syscall(arg0, (uint64_t*)arg1);
		default:
			printk(QEMU_SERIAL, "Unimplemented syscall called: %x\n", index);
			return -1;
	}
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len) {
	printk(QEMU_SERIAL, "Write syscall not implemented\n");
	return -1;
}

static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len) {
	printk(QEMU_SERIAL, "Read syscall not implemented\n");
	return -1;
}

static uint64_t anon_allocate_syscall(size_t size, uint64_t *dk) {
	printk(QEMU_SERIAL, "Anon allocate syscall not implemented\n");
	return -1;
}
