#include <syscall.h>
#include <initramfs.h>
#include <fpu.h>
#include <interrupts.h>
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
#include <fcntl.h>
#include <framebuffer.h>

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

static void anon_list_free(anon_block_t *head) {
	while (head) {
		anon_block_t *next = head->next;
		kfree(head);
		head = next;
	}
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len, size_t *out);
static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len, size_t *out);
static uint64_t anon_allocate_syscall(size_t size, uint64_t *dk);
static uint64_t anon_free_syscall(uint64_t ptr, uint64_t size);
static uint64_t fork_syscall();
static uint64_t execve_syscall(const char *path, char **argv, char **envp);
static uint64_t waitpid_syscall(int pid, int *status, int flags);
static uint64_t exit_syscall(int status) __attribute__((__noreturn__));
static uint64_t sys_listprog(const char *path, uint8_t *buf, uint64_t len, uint64_t *out);
static int copy_user_str(uint64_t cr3, const char *src, char *dst, size_t max, size_t *len_out);
static int64_t ps_syscall(ps_proc_t *buf, uint64_t max);
static uint64_t sleep_syscall(uint64_t ms);
static uint64_t fdflags_syscall(uint64_t fd, uint64_t cmd, uint64_t val, uint64_t *out);
static uint64_t fb_info_syscall(fb_info_t *out);
static uint64_t fb_blit_syscall(const uint32_t *buf, uint64_t x, uint64_t y, uint64_t w, uint64_t h);

uint64_t syscall_handler(uint64_t index, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
	switch (index) {
	case 0:
		return read_syscall(arg0, (uint8_t *)arg1, arg2, (uint64_t *)arg3);
	case 1:
		return write_syscall(arg0, (uint8_t *)arg1, arg2, (uint64_t *)arg3);
	case 2:
		return anon_allocate_syscall(arg0, (uint64_t *)arg1);
	case 3:
		exit_syscall(arg0);
		__builtin_unreachable();
	case 4:
		return anon_free_syscall(arg0, arg1);
	case 5:
		return fork_syscall();
	case 6:
		return execve_syscall((const char *)arg0, (char **)arg1, (char **)arg2);
	case 7:
		return waitpid_syscall((int)arg0, (int *)arg1, (int)arg2);
	case 8:
		return sys_listprog((const char *)arg0, (uint8_t *)arg1, arg2, (uint64_t *)arg3);
	case 9:
		return ps_syscall((ps_proc_t *)arg0, arg1);
	case 10:
		return sleep_syscall((uint64_t)arg0);
	case 11:
		return fdflags_syscall(arg0, arg1, arg2, (uint64_t *)arg3);
	case 12:
		return fb_info_syscall((fb_info_t *)arg0);
	case 13:
		return fb_blit_syscall((uint32_t *)arg0, arg1, arg2, arg3, arg4);
	default:
		printk(QEMU_SERIAL, "Unimplemented syscall called: %x\n", index);
		return ENOSYS;
	}
}

static uint64_t fb_info_syscall(fb_info_t *out) {
	process_t *proc = get_current_process();
	if (!proc) { return EFAULT; }
	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;
	if (!user_range_ok(cr3, (uint64_t)out, sizeof(*out)) ||
	        !is_user_ptr((uint64_t)out, sizeof(*out))) { return EFAULT; }

	fb_info_t tmp;
	fb_get_info(&tmp);
	memcpy(out, &tmp, sizeof(tmp));
	return 0;
}

static uint64_t fb_blit_syscall(const uint32_t *buf, uint64_t x, uint64_t y, uint64_t w, uint64_t h) {
	process_t *proc = get_current_process();
	if (!proc) { return EFAULT; }
	if (w == 0 || h == 0) { return 0; }

	if (w > 0x4000 || h > 0x4000) { return EINVAL; }
	uint64_t bytes = w * h * 4;

	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;
	if (!is_user_ptr((uint64_t)buf, bytes) ||
	        !user_range_ok(cr3, (uint64_t)buf, bytes)) { return EFAULT; }

	return fb_blit(buf, x, y, w, h) ? EINVAL : 0;
}

/* cmd == 0 -> get, 1 -> set */
static uint64_t fdflags_syscall(uint64_t fd, uint64_t cmd, uint64_t val, uint64_t *out) {
	process_t *proc = get_current_process();
	if (!proc) { return EFAULT; }
	if (fd >= MAX_FDS || !proc->fds[fd]) { return EBADF; }

	file_t *f = proc->fds[fd];
	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;

	if (!user_range_ok(cr3, (uint64_t)out, sizeof(*out))) { return EFAULT; }

	if (cmd == 1) { f->flags = (int)val; }
	*out = (uint64_t)f->flags;
	return 0;
}

static uint64_t sleep_syscall(uint64_t ms) {
	uint64_t end = get_ticks_ms() + ms;
	while (get_ticks_ms() < end) {
		__asm__ volatile("sti\n\t" "hlt" : : : "memory");
	}
	return 0;
}

static int64_t ps_syscall(ps_proc_t *buf, uint64_t max) {
	process_t *proc = get_current_process();

	if (!proc) { return -EFAULT; }

	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;
	if ((max > 0) && !(user_range_ok(cr3, (uint64_t)buf, max * sizeof(ps_proc_t)))) {
		return -EFAULT;
	}

	ps_proc_t *kbuf = NULL;
	if (max > 0) { kbuf = (ps_proc_t *)kmalloc(max * sizeof(ps_proc_t)); }
	if ((max > 0) && !kbuf) { return -ENOMEM; }

	uint64_t n = 0, total = 0;

	uint64_t flags = save_irqdisable();
	for (process_t *p = process_list; p; p = p->next, total++) {
		if (n < max) {
			memset(&kbuf[n], 0, sizeof(kbuf[n]));
			kbuf[n].s = p->status;
			kbuf[n].pid = p->pid;
			kbuf[n].tty = 1;
			kbuf[n].time = 0;
			strcpy(kbuf[n].cmd, p->cmd);
			n++;
		}
	}
	irqrestore(flags);

	if (n > 0) {
		memcpy(buf, kbuf, n * sizeof(ps_proc_t));
	}

	if (kbuf) { kfree(kbuf); }

	return (int64_t)total;
}

static uint64_t sys_listprog(const char *path, uint8_t *buf, uint64_t len, uint64_t *out) {
	process_t *proc = get_current_process();
	if (!proc) { return EFAULT; }
	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;

	if (!user_range_ok(cr3, (uint64_t)out, sizeof(*out))) { return EFAULT; }
	if (len > LISTPROG_MAX) { len = LISTPROG_MAX; }
	if (len > 0) {
		if (!is_user_ptr((uint64_t)buf, len) || !user_range_ok(cr3, (uint64_t)buf, len)) {
			return EFAULT;
		}
	}

	char kpath[0x100];
	uint64_t n = 0;
	if (copy_user_str(cr3, path, kpath, sizeof(kpath), &n) != 0) { return EFAULT; }
	--n;

	if (n == 0 || kpath[0] != '/') { return EINVAL; }
	if (kpath[n - 1] != '/') {
		if ((n + 2) > (int)(sizeof(kpath))) { return EINVAL; }
		kpath[n] = '/';
		kpath[n + 1] = '\0';
	}

	uint64_t maxlen = (len > 1) ? len : 1;
	char *kbuf = (char *)kmalloc(maxlen);
	if (kbuf == NULL) { return ENOMEM; }

	int64_t needed = initramfs_list_dir(kpath, kbuf, len);
	if (needed < 0) {
		kfree(kbuf);
		return EINVAL;
	}

	*out = needed;
	if ((uint64_t)needed > len) {
		kfree(kbuf);
		return ERANGE;
	}

	memcpy(buf, kbuf, needed);
	kfree(kbuf);
	return 0;
}

static uint64_t exit_syscall(int status) {
	process_t *proc = get_current_process();
	printk(QEMU_SERIAL, "pid %d exited with %d\n", proc->pid, status);

	anon_list_free(proc->anon_head);

	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	vmm_free_page_table(ba, proc->cr3);
	MUTEX_UNLOCK(g_ba);

	proc->anon_head = NULL;
	proc->ret_value = status;

	kfree(proc->kernel_stack);

	mark_current_proc_as_dead();
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

	int64_t n = 0;

	if (file->flags & O_NONBLOCK) {
		n = file->ops->read_nb(file, kbuf, chunk);

		if (n < 0) { return -n; }
	} else {
		n = file->ops->read(file, kbuf, chunk);
	}


	// echo the input
	// normally this wouldnt belong here, put in a proper tty read function. since we dont have tty support for now and just use pipes i added it here to micic proper behaviour
	if (fd == 0 && n > 0) {
		printk_raw(QEMU_SERIAL | FB, kbuf, n);
	}

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

static uint64_t fork_syscall(void) {
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

	if (get_current_process() != proc) {
		return 0;
	}

	// copy the address space
	uint64_t new_cr3;
	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	vmm_status_t st = vmm_copy_page_table(ba, proc->cr3, &new_cr3);
	MUTEX_UNLOCK(g_ba);

	if (st != VMM_OK) {
		return -ENOMEM;
	}

	process_t *new_proc = kmalloc(sizeof(process_t));
	if (!new_proc) {
		goto fail_pt;
	}

	memcpy(new_proc, proc, sizeof(process_t));
	new_proc->cr3 = new_cr3;
	new_proc->pid = get_unique_pid();

	new_proc->kernel_stack = kmalloc(0x1000);
	if (!new_proc->kernel_stack) {
		kfree(new_proc);
		goto fail_pt;
	}

	// copy the anon alloc linked list
	anon_block_t *copy_head = NULL;
	anon_block_t **tail = &copy_head;

	for (anon_block_t *cur = proc->anon_head; cur; cur = cur->next) {
		anon_block_t *node = kmalloc(sizeof(*node));
		if (!node) {
			anon_list_free(copy_head);
			kfree(new_proc->kernel_stack);
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

	fpu_state_init(new_proc->fpu_state);

	insert_process(new_proc);
	yield();

	return (uint64_t)new_proc->pid;

fail_pt:
	ba = MUTEX_LOCK(g_ba);
	vmm_free_page_table(ba, new_cr3);
	MUTEX_UNLOCK(g_ba);
	return -ENOMEM;
}

#define EXEC_PATH_MAX 0x1000
#define EXEC_MAX_ARGS 128
#define EXEC_MAX_ENVS 128
#define EXEC_MAX_TOTAL (32 * 1024)

typedef struct {
	size_t argc, envc;
	size_t nstr;
	size_t used;
	uint32_t off[EXEC_MAX_ARGS + EXEC_MAX_ENVS];
	char data[EXEC_MAX_TOTAL];
} exec_args_t;

static int copy_user_str(uint64_t cr3, const char *src, char *dst, size_t max, size_t *len_out) {
	for (size_t i = 0; i < max; i++) {
		uint64_t a = (uint64_t)src + i;
		if (i == 0 || (a & 0xfff) == 0) {
			if (!is_user_ptr(a, 1) || !user_range_ok(cr3, a, 1)) {
				return -EFAULT;
			}
		}
		char c = src[i];
		dst[i] = c;
		if (c == 0) { *len_out = i + 1; return 0; }
	}
	return -E2BIG;
}

static int exec_collect(uint64_t cr3, exec_args_t *a, char **vec, size_t max, size_t *count) {
	size_t n = 0;
	if (vec == NULL) { *count = 0; return 0; }   // treat NULL like an empty array

	for (;; n++) {
		uint64_t slot = (uint64_t)&vec[n];
		if (!is_user_ptr(slot, sizeof(char *)) || !user_range_ok(cr3, slot, sizeof(char *))) {
			return -EFAULT;
		}

		const char *s = vec[n];
		if (s == NULL) { break; }
		if (n >= max) { return -E2BIG; }

		size_t len;
		int r = copy_user_str(cr3, s, a->data + a->used, EXEC_MAX_TOTAL - a->used, &len);
		if (r) { return r; }

		a->off[a->nstr++] = a->used;
		a->used += len;
	}
	*count = n;
	return 0;
}

void get_program_name(const char path_buf[EXEC_PATH_MAX], char out[PROC_CMD_MAX]) {
	size_t start = 0;
	size_t end = 0;

	while (end < EXEC_PATH_MAX && path_buf[end] != '\0') {
		if (path_buf[end] == '/') {
			start = end + 1;
		}
		end++;
	}

	size_t i = 0;
	while (start < end && i < PROC_CMD_MAX - 1) {
		out[i++] = path_buf[start++];
	}

	out[i] = '\0';
}

static volatile uint32_t exec_busy = 0;
static uint8_t exec_img[EXEC_MAX_TOTAL + 4096] __attribute__((aligned(16)));

static uint64_t execve_syscall(const char *path, char **argv, char **envp) {
	process_t *proc = get_current_process();
	if (proc == NULL) { return -EFAULT; }
	uint64_t cr3 = proc->cr3 & VMM_ADDR_MASK_4K;

	fpu_state_init(proc->fpu_state);

	char path_buf[EXEC_PATH_MAX];
	size_t path_len;
	int r = copy_user_str(cr3, path, path_buf, sizeof path_buf, &path_len);
	if (r) { return r; }

	uint8_t *elf = NULL;
	uint64_t elf_size = initramfs_lookup(path_buf, &elf);
	if (!elf) { return -ENOENT; }
	if (!is_elf(elf)) {
		return -ENOENT;
	}

	exec_args_t *a = kmalloc(sizeof *a);
	if (!a) { return -ENOMEM; }
	a->nstr = 0;
	a->used = 0;

	r = exec_collect(cr3, a, argv, EXEC_MAX_ARGS, &a->argc);
	if (!r) { r = exec_collect(cr3, a, envp, EXEC_MAX_ENVS, &a->envc); }
	if (r) { kfree(a); return r; }

	size_t argc = a->argc, envc = a->envc;
	size_t words = 1 + argc + 1 + envc + 1 + 2;

	uint64_t str_size = (a->used + 7) & ~7ULL;
	uint64_t str_base = USER_PROC_STACK_ADDRESS_UPPER_BOUND - str_size;
	uint64_t sp       = (str_base - words * 8) & ~0xFULL;
	uint64_t img_size = USER_PROC_STACK_ADDRESS_UPPER_BOUND - sp;

	if (img_size > sizeof exec_img || img_size > USER_PROC_STACK_SIZE / 2) {
		kfree(a);
		return -E2BIG;
	}

	while (__atomic_exchange_n(&exec_busy, 1, __ATOMIC_ACQUIRE) != 0) {
		// wait till the other process finished
		yield();
	}

	memset(exec_img, 0, img_size);
	uint64_t *p = (uint64_t *)exec_img;
	*p++ = argc;
	for (size_t i = 0; i < argc; i++) { *p++ = str_base + a->off[i]; }
	*p++ = 0;
	for (size_t i = 0; i < envc; i++) { *p++ = str_base + a->off[argc + i]; }
	*p++ = 0;
	*p++ = 0;
	*p++ = 0;
	memcpy(exec_img + (str_base - sp), a->data, a->used);

	kfree(a);

	uint64_t *pml4 = phys_to_virt(cr3 & VMM_ADDR_MASK_4K);

	bitmap_allocator_t *ba = MUTEX_LOCK(g_ba);
	for (unsigned i = 0; i < 250; i++) {
		uint64_t e = pml4[i];
		if (!(e & VMM_PRESENT)) {
			continue;
		}
		vmm_free_level(ba, e & VMM_ADDR_MASK_4K, 3);
		pml4[i] = 0;
	}
	MUTEX_UNLOCK(g_ba);

	__asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");

	anon_list_free(proc->anon_head);
	proc->mmap_next = USER_MMAP_BASE;
	proc->anon_head = NULL;
	get_program_name(path_buf, proc->cmd);

	// this is somehow needed for rtdl in mlibc
	ba = MUTEX_LOCK(g_ba);
	vmm_status_t st = vmm_map_range(ba, cr3, 0x1000, 0, 0x2000, VMM_RW | VMM_XD | VMM_US);
	MUTEX_UNLOCK(g_ba);
	if (st != VMM_OK) {
		printk(QEMU_SERIAL, "[FAIL] Cant return from execve syscall\n");
		asm volatile("cli; hlt");
	}
	*(uint64_t *)0x2000 = 0x2000;

	uint64_t entry;
	ba = MUTEX_LOCK(g_ba);
	elf_status_t e_st = elf_load(ba, cr3, elf, elf_size, &entry);
	MUTEX_UNLOCK(g_ba);
	if (e_st != ELF_OK) {
		printk(QEMU_SERIAL, "[FAIL] Cant return from execve syscall\n");
		asm volatile("cli; hlt");
	}

	register uint64_t r_sp    asm("r8") = sp;
	register uint64_t r_words asm("r9") = img_size / 8;

	uint64_t d_lo  = USER_PROC_STACK_ADDRESS_LOWER_BOUND;
	uint64_t d_cnt = USER_PROC_STACK_SIZE / 8;
	uint64_t d_src = (uint64_t)exec_img;

	asm volatile(
	    "cli; cld\n\t"
	    "xorl %%eax, %%eax\n\t"
	    "rep stosq\n\t"
	    "movq %%r8, %%rdi\n\t"
	    "movq %%r9, %%rcx\n\t"
	    "rep movsq\n\t"
	    "movl $0, %[busy]\n\t"
	    "movq %%rdx, %%rcx\n\t"
	    "movq %%r8, %%rsp\n\t"
	    "movl $0x202, %%r11d\n\t"
	    "sysretq\n\t"
	    : "+D"(d_lo), "+c"(d_cnt), "+S"(d_src), [busy] "=m"(exec_busy)
	    : "d"(entry), "r"(r_sp), "r"(r_words)
	    : "rax", "r11", "memory"
	);
	__builtin_unreachable();
}

static uint64_t waitpid_syscall(int pid, int *status, int flags) {
	if (flags != 0)
		// we can only handle this case for now
	{
		return -EFAULT;
	}

	if (status == NULL) {
		return -EFAULT;
	}

	process_t *proc = get_process_from_pid(pid);
	if (!proc) {
		return -EFAULT;
	}

	while (proc->status != DEAD) {
		yield();
	}

	// everything must be cleaned up before this
	remove_process_from_list(proc);

	*status = proc->ret_value;

	kfree(proc);

	return pid;
}
