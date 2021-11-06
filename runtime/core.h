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

extern const i64 Stdout, Stdin, Stderr;
extern "C" i64 open_si(const char* path, i64 flags);
extern "C" void close_NStreami(i64 io);
extern "C" void writeitem_i(i64 io, i64 value);
extern "C" void writeitem_f(i64 io, float value);
extern "C" void writeitem_d(i64 io, double value);
extern "C" void writeitem_c(i64 io, u32 value);
extern "C" void writeitem_s(i64 io, const char* value);
extern "C" void writeitem_n(i64 io, u32 value);
extern "C" void writeitem_b(i64 io, bool value);
extern "C" void writeitem_t(i64 io, u32 value);
extern "C" void writeitem_v(i64 io, i64 value);

extern "C" void* _cons(i64 value, void* next);
extern "C" i64 _listlen(void* list);
extern "C" i64 _strlen(const char *s);
extern "C" void _display_symbol_list(void* value);
extern "C" void _display_native_string_list(void* value);
extern "C" i64 _strcmp(const char *a, const char *b);
extern "C" const u8* _read_line();
extern "C" i64 _read_int();
extern "C" const u8* _read_word();
extern "C" u8 _char_at(const char *s, i64 idx);
extern "C" const u8* _strcat(const char* a, const char* b);
extern "C" const u8* _substr(const char *s, i64 start, i64 end);

#endif