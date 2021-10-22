/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_UTILS_H
#define BASIL_UTILS_H

#include "defs.h"

void* operator new(size_t n);
void* operator new[](size_t n);
void* operator new(size_t n, void* p);
void operator delete(void* ptr);
void operator delete(void* ptr, size_t n);
void operator delete[](void* ptr);
void operator delete[](void* ptr, size_t n);

#define move(x) static_cast<decltype(x)&&>(x)

// We need this so we can avoid depending on libc++.
extern "C" void __cxa_pure_virtual();

#endif