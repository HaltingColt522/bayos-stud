#include "mlibc/tcb.hpp"
#include <abi-bits/errno.h>
#include <bits/ensure.h>
#include <bits/syscall.h>
#include <mlibc/all-sysdeps.hpp>
#include <string.h>
#include <stdint.h>

#define STUB()                                                                                     \
    ({                                                                                             \
        __ensure(!"STUB function was called");                                                     \
        for(;;);                                                                   \
        __builtin_unreachable(); \
    })

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_ANON_ALLOCATE 2
#define SYS_EXIT 3
#define SYS_ANON_FREE 4
#define SYS_FORK 5
#define SYS_EXECVE 6
#define SYS_WAITPID 7
#define SYS_LIST_PROG 8

namespace mlibc {
	int sys_listprog(const char *path, uint8_t *buf, uint64_t len, uint64_t *out) {
		auto result = syscall(SYS_LIST_PROG, path, buf, len, out);

		if (result != 0) {
			return result;
		}

		return 0;
	}


    int sys_write(int fd, const void *buffer, size_t count, ssize_t *written) {
        auto result = syscall(SYS_WRITE, fd, buffer, count, (uint64_t)written);

        if (result != 0) {
            return result;
        }
    
        return 0;
    }

    int sys_isatty(int) { return 0; }

    int sys_read(int fd, void *buf, size_t count, ssize_t *bytes_read) {
        auto result = syscall(SYS_READ, fd, buf, count, (uint64_t)bytes_read);
    
        if (result < 0) {
            *bytes_read = 0;
            return -result;
        }
    
        return 0;
    }

    [[noreturn]] void sys_exit(int status) {
        syscall(SYS_EXIT, status);
    }

    int sys_close(int) {
        STUB();
    }

    int sys_open(char const*, int, unsigned int, int*) {
        STUB();
    }

    int sys_clock_get(int, long*, long*) {
        STUB();
    }

    int sys_seek(int, long, int, long*) { return ESPIPE; }

    int sys_futex_wait(int*, int, timespec const*) { STUB(); }

    int sys_futex_wake(int*) { STUB(); }

    void sys_libc_log(char const* msg) {
        ssize_t unused;
        sys_write(1, msg, strlen(msg), &unused);
    }

    int sys_vm_map(void*, unsigned long, int, int, int, long, void**) { STUB(); }

    int sys_tcb_set(void*) { return 0; }

    void sys_libc_panic(void) {
        sys_libc_log("!!! mlibc panic !!!");
        sys_exit(-1);
        __builtin_trap();
    }

    int sys_anon_free(void* ptr, unsigned long size) {
		/* could be return syscall(...) but im too lazy to change rn */
		auto result = syscall(SYS_ANON_FREE, (uint64_t)ptr, size);
		if (result != 0) {
			return result;
		}

		return 0;
	}

    int sys_anon_allocate(unsigned long size, void** ptr) { 
        auto result = syscall(SYS_ANON_ALLOCATE, size, (uint64_t)ptr);
        if (result != 0) {
			return result;
		}

		return 0;
    }

    int sys_fork(pid_t *child) {
        auto res = syscall(SYS_FORK);
    
        if (res < 0)
            return -res;

        *child = res;
        return 0;
    }

    int sys_execve(const char *path, char *const argv[], char *const envp[]) {
        auto res = syscall(SYS_EXECVE, path, argv, envp);

        if (res < 0)
            return -res;

        return 0;
    }


    int sys_waitpid(pid_t pid, int *status, int flags, struct rusage *siginfo, pid_t *ret_pid) {
        (void)siginfo;

        int res = syscall(SYS_WAITPID, pid, status, flags);

        if (res < 0)
            return -res;

        *ret_pid = static_cast<pid_t>(res);
        return 0;
    }
}

extern "C" int listprog(const char *path, uint8_t *buf, uint64_t len, uint64_t *out) {
	return mlibc::sys_listprog(path, buf, len, out);
}
