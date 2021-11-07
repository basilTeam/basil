/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "core.h"
#include "sys.h"
#include "stdlib.h"

using namespace sys;

extern "C" i64 open_si(const char* path, i64 flags) {
    return open(path, flags);
}

extern "C" void close_N6Streami(i64 io) {
    close(io);
}

extern "C" void write_N6Streamii(i64 io, i64 value) {
    write_int(io_for_fd(io), value);
}

extern "C" void write_N6Streamif(i64 io, float value) {
    // write(io_for_fd(io), value);
}

extern "C" void write_N6Streamid(i64 io, double value) {
    // write(io_for_fd(io), value);
}

extern "C" void write_N6Streamic(i64 io, u32 value) {
    write_char(io_for_fd(io), value);
}

extern "C" void write_N6Streamis(i64 io, const char* value) {
    write_string(io_for_fd(io), value, *(u32*)(value - 4) - 1);
}

extern "C" void write_N6Streamin(i64 io, u32 value) {
    // write(io_for_fd(io), value);
}

extern "C" void write_N6Streamib(i64 io, bool value) {
    if (value) write_string(io_for_fd(io), "true", 4);
    else write_string(io_for_fd(io), "false", 5);
    // write(io_for_fd(io), value);
}

extern "C" void writeitem_t(i64 io, u32 value) {
    // write(io_for_fd(io), value);
}

extern "C" void write_N6Streamiv(i64 io, i64 value) {
    // write(io_for_fd(io), "()", 2);
}

extern "C" void exit_i(i64 code) {
    sys::exit(code);
}

extern "C" void init_v() {
    sys::init_io();
}