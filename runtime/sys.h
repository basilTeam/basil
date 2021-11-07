/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_SYS_H
#define BASIL_SYS_H

#include "util/defs.h"

namespace sys {
    struct stream;

    #define UTF8_MINIMAL
    #include "util/utf8.h" // embed subset of utf8 features in sys
    #undef UTF8_MINIMAL

    void exit(i64 code);
    extern "C" void* memcpy(void* dst, const void* src, size_t size);

    #define BASIL_STDIN_FD 0
    #define BASIL_STDOUT_FD 1
    #define BASIL_STDERR_FD 2

    #define BASIL_READ 1
    #define BASIL_WRITE 2
    #define BASIL_APPEND 4

    i64 open(const char* path, i64 flags);
    void close(i64 i);
    void init_io();
    stream& io_for_fd(i64 i);

    void write_string(stream& io, const char* str, u32 n);
    void write_char(stream& io, rune c);
    void write_byte(stream& io, u8 c);
    void write_uint(stream& io, u64 u);
    void write_int(stream& io, i64 i);
    void read_string(stream& io, char* str, u32 n);
    rune read_char(stream& io);
    u8 read_byte(stream& io);
    u64 read_uint(stream& io);
    i64 read_int(stream& io);

    void flush(stream& io);
}

#endif