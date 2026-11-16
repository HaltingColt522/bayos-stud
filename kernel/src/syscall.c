#include <syscall.h>
#include <initramfs.h>
#include <io.h>
#include <stdint.h>
#include <printk.h>
#include <stddef.h>
#include <vmm.h>
#include <errno.h>
#include <stdbool.h>
#include <sched.h>
#include <kheap.h>
#include <memory.h>
#include <string.h>
#include <elf.h>

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
	if (ptr >= VMM_LOWER_HALF_END) { return false; }
	return true;
}

bool user_range_ok(uint64_t cr3, uint64_t ptr, uint64_t len) {
	if (!is_user_ptr(ptr, len)) { return false; }
	uint64_t phys;
	if (vmm_translate(cr3, ptr, &phys) != VMM_OK) { return false; }
	if (vmm_translate(cr3, ptr + len - 1, &phys) != VMM_OK) { return false; }
	return true;
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len, size_t *out);
static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len, size_t *out);
static uint64_t anon_allocate_syscall(size_t size, uint64_t *dk);
static uint64_t anon_free_syscall(uint64_t ptr, uint64_t size);
static uint64_t fork_syscall();
static uint64_t execve_syscall(const char *path, char **argv, char **evnp);
static uint64_t waitpid_syscall(int pid, int *status, int flags);

uint64_t syscall_handler(uint64_t index, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
	switch (index) {
	case 0:
		return read_syscall(arg0, (uint8_t *)arg1, arg2, (uint64_t *)arg3);
	case 1:
		return write_syscall(arg0, (uint8_t *)arg1, arg2, (uint64_t *)arg3);
	case 2:
		return anon_allocate_syscall(arg0, (uint64_t *)arg1);
	case 4:
		return anon_free_syscall(arg0, arg1);
	case 5:
		return fork_syscall();
	case 6:
		return execve_syscall((const char *)arg0, (char **)arg1, (char **)arg2);
	case 7:
		return waitpid_syscall((int)arg0, (int *)arg1, (int)arg2);
	default:
		printk(QEMU_SERIAL, "Unimplemented syscall called: %x\n", index);
		return ENOSYS;
	}
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len, size_t *out) {
	process_t *proc = get_current_process();

	if (proc == NULL) { return EFAULT; }
	if (fd >= MAX_FDS) { return EBADF; }

	file_t *file = proc->fds[fd];
	if ((file == NULL) || (file->ops == NULL) || (file->ops->write == NULL)) { return EBADF; }

	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;
	if (!user_range_ok(cr3, (uint64_t)out, sizeof(*out))) { return EFAULT; }
	if (len == 0) { *out = 0; return 0; }
	if (!is_user_ptr((uint64_t)buf, len)) { return EFAULT; }

	uint64_t total = 0;
	errno_t err = 0;

	uint8_t kbuf[WRITE_CHUNK];

	while (total < len) {
		uint64_t chunk = (len - total < WRITE_CHUNK) ? (len - total) : WRITE_CHUNK;
		if (!user_range_ok(cr3, (uint64_t)buf + total, chunk)) { err = EFAULT; break; }
		memcpy(kbuf, buf + total, chunk);
		int64_t n = file->ops->write(file, kbuf, chunk);
		if (n < 0) { err = -n; break; }
		total += n;
		if ((uint64_t)n < chunk) { break; }
	}

	if ((total == 0) && (err != 0)) { return err; }
	*out = total;

	return 0;
}

static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len, size_t *out) {
	process_t *proc = get_current_process();

	if (proc == NULL) { return EFAULT; }
	if (fd >= MAX_FDS) { return EBADF; }

	file_t *file = proc->fds[fd];
	if ((file == NULL) || (file->ops == NULL) || (file->ops->read == NULL)) { return EBADF; }

	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;
	if (!user_range_ok(cr3, (uint64_t)out, sizeof(*out))) { return EFAULT; }
	if (len == 0) { *out = 0; return 0; }

	uint64_t chunk = (len > READ_CHUNK) ? READ_CHUNK : len;

	uint8_t kbuf[READ_CHUNK];

	if (!is_user_ptr((uint64_t)buf, chunk) || !user_range_ok(cr3, (uint64_t)buf, chunk)) { return EFAULT; }
	int64_t n = file->ops->read(file, kbuf, chunk);

	if (n < 0) { return -n; }
	memcpy(buf, kbuf, (uint64_t)n);

	*out = n;
	return 0;
}

static uint64_t anon_allocate_syscall(size_t size, uint64_t *dk) {
	if (size == 0) { return EINVAL; }
	if (!is_user_ptr((uint64_t)dk, sizeof(*dk))) { return EFAULT; }
	if (size > USER_PROC_STACK_ADDRESS_LOWER_BOUND) { return ENOMEM; }

	/* round up */
	size = (size + 4095) & ~((uint64_t)VMM_PAGE_SIZE_4K - 1);

	process_t *proc = get_current_process();
	if (proc == NULL) { return EFAULT; }
	if (size > (USER_PROC_STACK_ADDRESS_LOWER_BOUND - proc->mmap_next)) { return ENOMEM; }

	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;
	uint64_t phys;
	if (vmm_translate(cr3, (uint64_t)dk, &phys) != VMM_OK) { return EFAULT; }
	if (vmm_translate(cr3, (uint64_t)dk + sizeof(*dk) - 1, &phys) != VMM_OK) { return EFAULT; }

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

static uint64_t anon_free_syscall(uint64_t ptr, uint64_t size) {
	if ((size == 0) || ((ptr % VMM_PAGE_SIZE_4K) != 0)) { return EINVAL; }
	if (size > USER_PROC_STACK_ADDRESS_LOWER_BOUND) { return EINVAL; }

	/* round up */
	size = (size + 4095) & ~((uint64_t)VMM_PAGE_SIZE_4K - 1);

	process_t *proc = get_current_process();
	if (proc == NULL) { return EFAULT; }

	anon_block_t *prev = NULL;
	anon_block_t *cur = proc->anon_head;

	while ((cur != NULL) && !((cur->start == ptr) && (cur->len == size))) {
		prev = cur;
		cur = cur->next;
	}

	if (cur == NULL) { return EINVAL; }

	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	vmm_status_t st = vmm_unmap_range(ba, proc->cr3 & VMM_ADDR_MASK_4K, ptr, size, 1);
	MUTEX_UNLOCK(g_ba);

	if (prev != NULL) {
		prev->next = cur->next;
	} else {
		proc->anon_head = cur->next;
	}

	kfree(cur);

	return vmm_to_errno(st);
}

static void anon_list_free(anon_block_t *head)
{
	while (head) {
		anon_block_t *next = head->next;
		kfree(head);
		head = next;
	}
}

static uint64_t fork_syscall(void)
{
	process_t *proc = get_current_process();

	// now we pretend an interrupt happened right here
	uint64_t old_rsp, rip_tmp;
	cpu_status_t context;

	asm volatile(
		"leaq 1f(%%rip), %[rip]\n\t"
		"movq %%rsp, %[old]\n\t"
		"movq %[top], %%rsp\n\t"

		"pushq $0x10\n\t"
		"pushq %[old]\n\t"
		"pushfq\n\t"
		"pushq $0x8\n\t"
		"pushq %[rip]\n\t"

		"pushq %%rax\n\t"
		"pushq %%rbx\n\t"
		"pushq %%rcx\n\t"
		"pushq %%rdx\n\t"
		"pushq %%rbp\n\t"
		"pushq %%rsi\n\t"
		"pushq %%rdi\n\t"
		"pushq %%r8\n\t"
		"pushq %%r9\n\t"
		"pushq %%r10\n\t"
		"pushq %%r11\n\t"
		"pushq %%r12\n\t"
		"pushq %%r13\n\t"
		"pushq %%r14\n\t"
		"pushq %%r15\n\t"

		"movq %[old], %%rsp\n\t"
		"1:\n\t"
		: [old] "=&r"(old_rsp), [rip] "=&r"(rip_tmp)
		: [top] "r"((uint64_t)&context + sizeof(cpu_status_t))
		: "memory", "cc"
	);

	if (get_current_process() != proc)
		return 0;

	// copy the address space
	uint64_t new_cr3;
	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	vmm_status_t st = vmm_copy_page_table(ba, proc->cr3, &new_cr3);
	MUTEX_UNLOCK(g_ba);

	if (st != VMM_OK)
		return -ENOMEM;

	process_t *new_proc = kmalloc(sizeof(process_t));
	if (!new_proc)
		goto fail_pt;

	memcpy(new_proc, proc, sizeof(process_t));
	new_proc->cr3 = new_cr3;
	new_proc->pid = get_unique_pid();

	// copy the anon alloc linked list
	anon_block_t *copy_head = NULL;
	anon_block_t **tail = &copy_head;

	for (anon_block_t *cur = proc->anon_head; cur; cur = cur->next) {
		anon_block_t *node = kmalloc(sizeof(*node));
		if (!node) {
			anon_list_free(copy_head);
			kfree(new_proc);
			goto fail_pt;
		}
		node->start = cur->start;
		node->len = cur->len;
		node->next = NULL;

		*tail = node;
		tail = &node->next;
	}
	new_proc->anon_head = copy_head;

	new_proc->context = &context;

	insert_process(new_proc);
	yield();

	return (uint64_t)new_proc->pid;

fail_pt:
	ba = MUTEX_LOCK(g_ba);
	vmm_free_page_table(ba, new_cr3);
	MUTEX_UNLOCK(g_ba);
	return -ENOMEM;
}

static uint64_t execve_syscall(const char *path, char **argv, char **evnp) {
	// TODO: validate path, argv and envp
	// this is kinda hard to do since you
	// need to check for each byte if it is 
	// still in the user half
	// NOTE: argv and envp are ignored for now anyways
	
	uint8_t *elf;
	uint64_t elf_size = initramfs_lookup(path, &elf);
	if (!elf)
		return -ENOENT;

	process_t *proc = get_current_process();
	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;

	// copy the path onto the stack in case it was stored in the elf
	size_t path_len = strlen(path) + 1;
	if (path_len > 0x2000) {
		// it is probably not a good idea to copy this on the path
		return -EFAULT;
	}

	char path_buf[path_len];
	memmove(path_buf, path, path_len);

	// reset our process to a clean state
	// from here on out we cant return anymore, since the address space in gone
	// we should leave a little for the stack though
	// now i left 6 GB
	uint64_t *pml4 = phys_to_virt(cr3 & VMM_ADDR_MASK_4K);

	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	for (unsigned i = 0; i < 250; i++) {
		uint64_t e = pml4[i];
		if (!(e & VMM_PRESENT))
			continue;

		vmm_free_level(ba, e & VMM_ADDR_MASK_4K, 3);
		pml4[i] = 0;
	}
	MUTEX_UNLOCK(g_ba);

	__asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");

	anon_list_free(proc->anon_head);
	proc->mmap_next = USER_MMAP_BASE;
	proc->anon_head = NULL;

	// this is somehow needed for rtdl in mlibc
	// i dont really know why though
	ba = MUTEX_LOCK(g_ba);
	vmm_status_t st = vmm_map_range(ba, cr3, 0x1000, 0, 0x2000, VMM_RW | VMM_XD | VMM_US);
	MUTEX_UNLOCK(g_ba);
	if (st != VMM_OK) {
		printk(QEMU_SERIAL, "[FAIL] Cant return from execve syscall\n");
		// if this ever causes a deadlock you should probably make sure the
		// process is cleaned up here
		asm volatile("cli; hlt");
	}
	*(uint64_t *)0x2000 = 0x2000;

	uint64_t entry;
	ba = MUTEX_LOCK(g_ba);
	elf_status_t e_st = elf_load(ba, cr3, elf, elf_size, &entry);
	MUTEX_UNLOCK(g_ba);
	if (e_st != ELF_OK) {
		printk(QEMU_SERIAL, "[FAIL] Cant return from execve syscall\n");
		// if this ever causes a deadlock you should probably make sure the
		// process is cleaned up here
		asm volatile("cli; hlt");
	}

	asm volatile(
		"cli; cld\n\t"
		"xorl %%eax, %%eax\n\t"
		"rep stosq\n\t"
		"movq %%rdx, %%rcx\n\t"
		"movq %%rsi, %%rsp\n\t"
		"movl $0x202, %%r11d\n\t"
		"sysretq\n\t"
		:
		: "D"(USER_PROC_STACK_ADDRESS_LOWER_BOUND), "c"(USER_PROC_STACK_SIZE / 8), "d"(entry), "S"(USER_PROC_STACK_ADDRESS_UPPER_BOUND)
		: "rax", "r11", "memory"
	);

	__builtin_unreachable();
}

static uint64_t waitpid_syscall(int pid, int *status, int flags) {
	// NOTE: flags are ignored for now
	// we just react as soon as the process turns dead
	(void)flags;

	if (status == NULL)
		return -EFAULT;

	process_t *proc = get_process_from_pid(pid);
	if (!proc)
		return -EFAULT;

	while (proc->status != DEAD) {
		yield();
	}

	// everything must be cleaned up before this
	remove_process_from_list(proc);

	*status = proc->ret_value;

	kfree(proc);

	return pid;
}
