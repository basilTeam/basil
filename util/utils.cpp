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
#include "panic.h"
// #ifdef __GNUC__
// #include "execinfo.h"
// #endif

void* operator new(size_t n) {
    return malloc(n);
}

void* operator new[](size_t n) {
    return malloc(n);
}

void* operator new(size_t n, void* p) {
    return p;
}

void operator delete(void* ptr) {
    free(ptr);
}

void operator delete(void* ptr, size_t n) {
    free(ptr);
}

void operator delete[](void* ptr) {
    free(ptr);
}

void operator delete[](void* ptr, size_t n) {
    free(ptr);
}

void __cxa_pure_virtual() {
    panic("Tried to invoke pure virtual function!");
} 