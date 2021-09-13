#ifndef JASMINE_UTIL_H
#define JASMINE_UTIL_H

#include "stdint.h"

#include "../util/defs.h"
#include "../util/vec.h"
#include "../util/hash.h"
#include "../util/bytebuf.h"

// allocates writable, executable memory
void* alloc_exec(u64 size);

// protects executable memory from being written
void protect_exec(void* exec, u64 size);

// deallocates executable memory
void free_exec(void* exec, u64 size);

#endif