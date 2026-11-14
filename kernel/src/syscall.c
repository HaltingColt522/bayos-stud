#include <syscall.h>
#include <io.h>
#include <stdint.h>
#include <printk.h>
#include <stddef.h>
#include <vmm.h>
#include <errno.h>
#include <stdbool.h>
#include <sched.h>
#include <kheap.h>

extern void _syscall_handler;

void syscall_init(void) {
	// enable syscall / sysret on cpu
	wrmsr(0xC0000080, rdmsr(0xC0000080) | 1);
	// set the kernel and user code and data selector
	wrmsr(0xC0000081, (0x8ULL << 32) | (0x20ULL << 48));
	// set the syscall entry address
	wrmsr(0xC0000082, (uint64_t)&_syscall_handler);
	// clear these RLFLAGS bits on syscall entry
	// TF, IF, DF, AC
	wrmsr(0xC0000084, (1 << 9) | (1 << 10) | (1 << 8) | (1 << 18));

	// set fsbase and gsbase to 0x2000
	// this is somehow needed by mlibc
	// for rtdl
	wrmsr(0xC0000100, 0x2000);
	wrmsr(0xC0000101, 0x2000);
}

errno_t vmm_to_errno(vmm_status_t st) {
	switch (st) {
	case VMM_OK:
		return 0;
	case VMM_ERR_OUT_OF_MEM:
	case VMM_ERR_PMM:
		return ENOMEM;
	default:
		printk(QEMU_SERIAL | FB, "[FAIL] unexpected VMM error\n");
		return EINVAL;
	}
}

bool is_user_ptr(uintptr_t ptr, uint64_t len) {
	if (ptr == 0) { return false; }
	if (len > VMM_LOWER_HALF_END - ptr) { return false; }
	return true;
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len);
static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len);
static uint64_t anon_allocate_syscall(size_t size, uint64_t *dk);

uint64_t syscall_handler(uint64_t index, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
	switch (index) {
	case 0:
		return read_syscall(arg0, (uint8_t *)arg1, arg2);
	case 1:
		return write_syscall(arg0, (uint8_t *)arg1, arg2);
	case 2:
		return anon_allocate_syscall(arg0, (uint64_t *)arg1);
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
	if (size == 0) { return EINVAL; }
	if (!is_user_ptr((uint64_t)dk, 8)) { return EFAULT; }
	if (size > USER_PROC_STACK_ADDRESS_LOWER_BOUND) { return ENOMEM; }

	/* round up */
	size = (size + 4095) & ~((uint64_t)VMM_PAGE_SIZE_4K - 1);

	process_t *proc = get_current_process();
	if (size > (USER_PROC_STACK_ADDRESS_LOWER_BOUND - proc->mmap_next)) { return ENOMEM; }

	anon_block_t *node = (anon_block_t *)kmalloc(sizeof(anon_block_t));
	if (node == NULL) { return ENOMEM; }

	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	vmm_status_t st = vmm_map_range(ba, proc->cr3 & VMM_ADDR_MASK_4K,
	                                proc->mmap_next, 0, size, VMM_RW | VMM_XD | VMM_US);
	MUTEX_UNLOCK(g_ba);

	if (st != VMM_OK) {
		kfree(node);
		return vmm_to_errno(st);
	}

	node->start = proc->mmap_next;
	node->len = size;
	node->next = proc->anon_head;


	proc->anon_head = node;

	*dk = node->start;
	proc->mmap_next += size;

	return 0;
}
