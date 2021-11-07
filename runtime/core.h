/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_CORE_H
#define BASIL_CORE_H

#include "util/defs.h"

extern "C" i64 open_si(const char* path, i64 flags);
extern "C" void close_N6Streami(i64 io);
extern "C" void write_N6Streamii(i64 io, i64 value);
extern "C" void write_N6Streamif(i64 io, float value);
extern "C" void write_N6Streamid(i64 io, double value);
extern "C" void write_N6Streamic(i64 io, u32 value);
extern "C" void write_N6Streamis(i64 io, const char* value);
extern "C" void write_N6Streamin(i64 io, u32 value);
extern "C" void write_N6Streamib(i64 io, bool value);
extern "C" void write_N6Streamit(i64 io, u32 value);
extern "C" void write_N6Streamiv(i64 io, i64 value);
extern "C" void exit_i(i64 code);
extern "C" void init_v();

#endif