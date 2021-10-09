/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "util/defs.h"
#include "sys.h"
#include "stdlib.h"

void* operator new[](unsigned long n) {
    return malloc(n);
}

void* operator new(unsigned long n, void* p) {
    return p;
}

void operator delete[](void* ptr) {
    free(ptr);
}

extern "C" void* _cons(i64 value, void* next) {
    void* result = malloc(sizeof(i64) + sizeof(void*));
    *(i64*)result = value;
    *((void**)result + 1) = next;
    return result;
}

extern "C" i64 _listlen(void* list) {
    u32 size = 0;
    while (list) {
        list = *((void**)list + 1);
        size ++;
    }
    return size;
}

extern "C" void _display_int(i64 value) {
    print(value, '\n');
}

extern "C" i64 _strlen(const char *s) {
    const char *copy = s;
    while (*s++);
    return s - copy - 1;
}

static const char** symbol_table;

extern "C" void _display_symbol(u64 value) {
    write(_stdout, symbol_table[value], _strlen(symbol_table[value]));
    write(_stdout, '\n');
}

extern "C" void _display_bool(bool value) {
    if (value) write(_stdout, "true", 5);
    else write(_stdout, "false", 6);
    write(_stdout, '\n');
}

extern "C" void _display_string(const char* value) {
    write(_stdout, value, _strlen(value));
    write(_stdout, '\n');
}

extern "C" void _display_int_list(void* value) {
    write(_stdout, '(');
    bool first = true;
    while (value) {
        i64 i = *(i64*)value;
        if (!first) write(_stdout, ' ');
        write(_stdout, i);
        value = *((void**)value + 1);
        first = false;
    }
    write(_stdout, ")\n", 3);
}

extern "C" void _display_bool_list(void* value) {
    write(_stdout, '(');
    bool first = true;
    while (value) {
        u64 i = *(u64*)value;
        if (!first) write(_stdout, ' ');
        if (i) write(_stdout, "true", 5);
        else write(_stdout, "false", 6);
        value = *((void**)value + 1);
        first = false;
    }
    write(_stdout, ")\n", 3);
}

extern "C" void _display_symbol_list(void* value) {
    write(_stdout, '(');
    bool first = true;
    while (value) {
        u64 i = *(u64*)value;
        if (!first) write(_stdout, ' ');
        write(_stdout, symbol_table[i], _strlen(symbol_table[i]));
        value = *((void**)value + 1);
        first = false;
    }
    write(_stdout, ")\n", 3);
}

extern "C" void _display_native_string_list(void* value) {
    write(_stdout, '(');
    bool first = true;
    while (value) {
        const char* i = *(const char**)value;
        if (!first) write(_stdout, ' ');
        write(_stdout, '"');
        write(_stdout, i, _strlen(i));
        write(_stdout, '"');
        value = *((void**)value + 1);
        first = false;
    }
    write(_stdout, ")\n", 3);
}

extern "C" i64 _strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) a ++, b ++;
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

extern "C" const u8* _read_line() {
    static char buffer[1024];
    read(_stdin, buffer, 1024);
    int length = _strlen(buffer);
    u8* buf = new u8[length + 1];
    for (u32 i = 0; i < length + 1; i ++) buf[i] = buffer[i];
    buf[length] = '\0';
    return buf;
}

extern "C" i64 _read_int() {
    return read<i64>(_stdin);
}

extern "C" const u8* _read_word() {
    return nullptr;
}

extern "C" u8 _char_at(const char *s, i64 idx) {
    return s[idx];
}

extern "C" const u8* _strcat(const char* a, const char* b) {
    u8* buf = new u8[_strlen(a) + _strlen(b) + 1];
    u32 i = 0;
    for (; *a; a ++) buf[i ++] = *a;
    for (; *b; b ++) buf[i ++] = *b;
    buf[i] = '\0';
    return buf; 
}

extern "C" const u8* _substr(const char *s, i64 start, i64 end) {
    if (end < start) return new u8[1]{'\0'};
    u8* buf = new u8[end - start + 1];
    i64 i = start;
    for (; i < end; i ++) buf[i - start] = s[i];
    buf[i + 1] = '\0';
    return buf;
}