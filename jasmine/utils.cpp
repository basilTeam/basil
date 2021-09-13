#include "utils.h"
#include "stdlib.h"

#if defined(__APPLE__) || defined(__linux__)
    #include "sys/mman.h"

    void* alloc_exec(u64 size) {
        return mmap(nullptr, size, PROT_READ | PROT_EXEC | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    void protect_exec(void* exec, u64 size) {
        mprotect(exec, size, PROT_READ | PROT_EXEC);
    }

    void free_exec(void* exec, u64 size) {
        munmap(exec, size);
    }
#endif