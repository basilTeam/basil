#ifndef FAST_IO_H
#define FAST_IO_H

#include "util/defs.h"

struct stream;

extern stream &_stdout, &_stdin;

void exit(u64 code);
extern "C" void* memcpy(void* dst, const void* src, unsigned long size);

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
    ::write(io, t);
    write(io, args...);
}

template<typename T, typename ...Args>
void read(stream& io, T& t, Args&... args) {
    ::read(io, t);
    read(io, args...);
}

template<typename T>
T read(stream& io) {
    T t;
    ::read(io, t);
    return t;
}

template<typename ...Args>
void print(const Args&... args) {
    write(_stdout, args...);
}

template<typename ...Args>
void input(Args&... args) {
    read(_stdin, args...);
}

template<typename T>
T input() {
    return read<T>(_stdin);
}

#endif