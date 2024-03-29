/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_IO_H
#define BASIL_IO_H

#include "defs.h"
#include "stdio.h"

class stream {
public:
    virtual void write(u8 c) = 0;
    virtual u8 read() = 0; 
    virtual u8 peek() const = 0;
    virtual void unget(u8 c) = 0;
    virtual operator bool() = 0;
};

bool exists(const char* path);

class file : public stream {
    FILE* f;
    bool done;
public:
    file(const char* fname, const char* flags);
    file(FILE* f_in);
    ~file();
    file(const file& other) = delete;
    file& operator=(const file& other) = delete;

    void write(u8 c) override;
    u8 read() override;
    u8 peek() const override;
    void unget(u8 c) override;
    operator bool() override;
};

class buffer : public stream {
    u8* data;
    u32 _start, _end, _capacity;
    u8 buf[8];

    void init(u32 size);
    void free();
    void copy(u8* other, u32 size, u32 start, u32 end);
    void grow();
public:
    buffer();
    ~buffer();
    buffer(const buffer& other);
    buffer& operator=(const buffer& other);

    void write(u8 c) override;
    u8 read() override;
    u8 peek() const override;
    void unget(u8 c) override;
    u32 size() const;
    u32 capacity() const;
    operator bool() override;
    u8* begin();
    const u8* begin() const;
    u8* end();
    const u8* end() const;
};

extern stream &_stdin, &_stdout;
void setprecision(u32 p);

void write(stream& io);
void write(stream& io, u8 c);
void write(stream& io, u16 n);
void write(stream& io, u32 n);
void write(stream& io, u64 n);
void write(stream& io, i8 c);
void write(stream& io, i16 n);
void write(stream& io, i32 n);
void write(stream& io, i64 n);
void write(stream& io, float f);
void write(stream& io, double d);
void write(stream& io, char c);
void write(stream& io, bool b);
void write(stream& io, const u8* s);
void write(stream& io, const char* s);
void write(stream& io, const buffer& b);

template<typename... Args>
void print(const Args&... args) {
    write(_stdout, args...);
}

template<typename T, typename... Args>
void write(stream& s, const T& t, const Args&... args) {
    write(s, t);
    write(s, args...);
}

template<typename... Args>
void println(const Args&... args) {
    writeln(_stdout, args...);
}

template<typename... Args>
void writeln(stream& s, const Args&... args) {
    write(s, args...);
    write(s, '\n');
}

class string;
class ustring;

bool isspace(u8 c);

void read(stream& io, u8& c);
void read(stream& io, u16& n);
void read(stream& io, u32& n);
void read(stream& io, u64& n);
void read(stream& io, i8& c);
void read(stream& io, i16& n);
void read(stream& io, i32& n);
void read(stream& io, i64& n);
void read(stream& io, float& f);
void read(stream& io, double& d);
void read(stream& io, char& c);
void read(stream& io, bool& b);
void read(stream& io, u8* s);
void read(stream& io, char* s);

template<typename T, typename... Args>
void read(stream& io, T& t, Args&... args) {
    read(io, t);
    read(io, args...);
}

template<typename String, typename... Args>
String format(const Args&... args) {
    buffer b;
    write(b, args...);
    return b;
}

// Writes a series of elements to the provided stream. Writes 'begin' before the first element,
// 'end' after the last element, and 'separator' in-between elements.
template<typename Iterable>
void write_seq(stream& io, const Iterable& items, const ustring& begin, const ustring& separator, const ustring& end) {
    bool first = true;
    write(io, begin);
    for (const auto& item : items) {
        if (!first) write(io, separator);
        first = false;
        write(io, item);
    }
    write(io, end);
}

// Writes a series of pairs to the provided stream. Writes 'begin' before the first pair,
// 'end' after the last pair, 'pair' between the elements within each pair, and 'separator'
// in-between pairs.
template<typename Iterable>
void write_pairs(stream& io, const Iterable& items, const ustring& begin, 
    const ustring& pair, const ustring& separator, const ustring& end) {
    bool first = true;
    write(io, begin);
    for (const auto& [a, b] : items) {
        if (!first) write(io, separator);
        first = false;
        write(io, a, pair, b);
    }
    write(io, end);
}

// Writes the first elements of a series of pairs to the provided stream. Writes 'begin' before 
// the first key, 'end' after the last key, and 'separator' in-between keys.
template<typename Iterable>
void write_keys(stream& io, const Iterable& items, const ustring& begin, const ustring& separator, const ustring& end) {
    bool first = true;
    write(io, begin);
    for (const auto& [a, _] : items) {
        if (!first) write(io, separator);
        first = false;
        write(io, a);
    }
    write(io, end);
}

// Writes the second elements of a series of pairs to the provided stream. Writes 'begin' before 
// the first value, 'end' after the last value, and 'separator' in-between values.
template<typename Iterable>
void write_values(stream& io, const Iterable& items, const ustring& begin, const ustring& separator, const ustring& end) {
    bool first = true;
    write(io, begin);
    for (const auto& [_, b] : items) {
        if (!first) write(io, separator);
        first = false;
        write(io, b);
    }
    write(io, end);
}

#endif