/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef JASMINE_UTIL_H
#define JASMINE_UTIL_H

#include "stdint.h"

#include "../util/defs.h"
#include "../util/vec.h"
#include "../util/hash.h"
#include "../util/bytebuf.h"

// allocates writable, executable memory
void* alloc_vmem(u64 size);

// protects executable memory from being written
void protect_exec(void* exec, u64 size);

// protects data memory from being written or executed
void protect_data(void* data, u64 size);

// protects static memory from being executed
void protect_static(void* stat, u64 size);

// deallocates executable memory
void free_vmem(void* mem, u64 size);

#endif