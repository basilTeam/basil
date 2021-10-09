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
#include "cxxabi.h"
// #ifdef __GNUC__
// #include "execinfo.h"
// #endif

void* operator new(unsigned long n) {
    return malloc(n);
}

void* operator new[](unsigned long n) {
    return malloc(n);
}

void* operator new(unsigned long n, void* p) {
    return p;
}

void operator delete(void* ptr) {
    free(ptr);
}

void operator delete(void* ptr, unsigned long n) {
    free(ptr);
}

void operator delete[](void* ptr) {
    free(ptr);
}

void operator delete[](void* ptr, unsigned long n) {
    free(ptr);
}

void exit_in_a_panic() {
    exit(1);
}

void __cxa_pure_virtual() {
    panic("Tried to invoke pure virtual function!");
} 