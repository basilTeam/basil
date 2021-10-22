/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

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
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #include "windows.h"

    void* alloc_exec(u64 size) {
        return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }

    void protect_exec(void* exec, u64 size) {
        DWORD old = PAGE_EXECUTE_READWRITE;
        VirtualProtect(exec, size, PAGE_EXECUTE_READ, &old);
    }

    void free_exec(void* exec, u64 size) {
        VirtualFree(exec, 0, MEM_RELEASE);
    }
#endif