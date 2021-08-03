#ifndef JASMINE_VERSION_H
#define JASMINE_VERSION_H

#include "utils.h"

const u8 JASMINE_VERSION = 1;

enum Architecture : u8 {
    UNSUPPORTED = 0,
    X86_64 = 1,
    AMD64 = 1,
    X86 = 2,
    AARCH64 = 3,
    JASMINE = 4, // architecture for jasmine bytecode
};

#if defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__)
    const Architecture DEFAULT_ARCH = X86_64;
#elif defined(__i386) || defined(__i386__) || defined(__x86) || defined(__x86__)
    const Architecture DEFAULT_ARCH = X86;
#elif defined(__aarch64) || defined(__aarch64__)
    const Architecture DEFAULT_ARCH = AARCH64;
#endif

#endif