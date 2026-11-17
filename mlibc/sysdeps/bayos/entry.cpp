#include <stdint.h>
#include <stdlib.h>
#include <mlibc/elf/startup.h>

extern "C" void __dlapi_enter(uintptr_t *);

extern char **environ;

extern "C" void __mlibc_entry(uintptr_t *entry_stack, int (*main_fn)(int argc, char *argv[], char *env[])) {
    __dlapi_enter(entry_stack);

    int result;
    if (entry_stack != ((void*)0x7fffffffe000)) {
        int argc = (int)entry_stack[0];
        char **argv = (char **)(entry_stack + 1);
        char **envp = argv + argc + 1;

        environ = envp;

        result = main_fn(argc, argv, envp);
    } else {
        result = main_fn(0, nullptr, nullptr);
    }

    exit(result);
}
