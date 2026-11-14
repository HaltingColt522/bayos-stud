#include "mlibc/tcb.hpp"
#include <abi-bits/errno.h>
#include <bits/ensure.h>
#include <bits/syscall.h>
#include <mlibc/all-sysdeps.hpp>
#include <string.h>

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

namespace mlibc {

    int sys_write(int fd, const void *buffer, size_t count, ssize_t *written) {
        auto result = syscall(SYS_WRITE, fd, buffer, count);

        if (result < 0) {
            return -result;
        }
    
        *written = count;
        return 0;
    }

    int sys_isatty(int) { return 0; }

    int sys_read(int fd, void *buf, size_t count, ssize_t *bytes_read) {
        auto result = syscall(SYS_READ, fd, buf, count);
    
        if (result < 0) {
            *bytes_read = 0;
            return -result;
        }
    
        *bytes_read = result;
        return 0;
    }

    [[noreturn]] void sys_exit(int) {
    // we dont have this yet
        for (;;);
        syscall(SYS_EXIT);
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

    int sys_anon_free(void*, unsigned long) { return 0; }

    int sys_anon_allocate(unsigned long size, void** ptr) { 
        auto result = syscall(SYS_ANON_ALLOCATE, size, ptr); 
        return 0;
    }
}
