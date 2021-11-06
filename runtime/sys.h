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

#define UTF8_MINIMAL
#include "util/utf8.h"
#undef UTF8_MINIMAL

namespace sys {
    struct stream;

    void exit(u64 code);
    extern "C" void* memcpy(void* dst, const void* src, size_t size);

    #define BASIL_STDIN_FD 0
    #define BASIL_STDOUT_FD 1
    #define BASIL_STDERR_FD 2

    #define BASIL_READ 1
    #define BASIL_WRITE 2
    #define BASIL_APPEND 4

    i64 open(const char* path, i64 flags);
    void _sys_close(i64 i);
    void init_io();
    stream& io_for_fd(i64 i);

    void write(stream& io, const char* str, u32 n);
    void write(stream& io, const char& c);
    void write(stream& io, const u8& c);
    void write(stream& io, const u16& u);
    void write(stream& io, const u32& u);
    void write(stream& io, const u64& u);
    void write(stream& io, const i8& c);
    void write(stream& io, const i16& u);
    void write(stream& io, const i32& u);
    void write(stream& io, const i64& u);
    void read(stream& io, char* str, u32 n);
    void read(stream& io, u8& c);
    void read(stream& io, u16& c);
    void read(stream& io, u32& c);
    void read(stream& io, u64& c);
    void read(stream& io, i8& c);
    void read(stream& io, i16& c);
    void read(stream& io, i32& c);
    void read(stream& io, i64& c);

    void write(stream& io);
    void read(stream& io);
    void flush(stream& io);

    template<typename T, typename ...Args>
    void write(stream& io, const T& t, const Args&... args) {
        sys::write(io, t);
        write(io, args...);
    }

    template<typename T, typename ...Args>
    void read(stream& io, T& t, Args&... args) {
        sys::read(io, t);
        read(io, args...);
    }

    template<typename T>
    T read(stream& io) {
        T t;
        sys::read(io, t);
        return t;
    }

    template<typename ...Args>
    void print(const Args&... args) {
        write(io_for_fd(BASIL_STDOUT_FD), args...);
    }

    template<typename ...Args>
    void input(Args&... args) {
        read(io_for_fd(BASIL_STDIN_FD), args...);
    }

    template<typename T>
    T input() {
        return read<T>(io_for_fd(BASIL_STDIN_FD));
    }
}

#endif