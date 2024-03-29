/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "bytebuf.h"

template<>
float big_endian(float f) {
    u32 u = big_endian<u32>(*(u32*)&f);
    return *(float*)&u;
}

template<>
double big_endian(double f) {
    u64 u = big_endian<u64>(*(u64*)&f);
    return *(double*)&u;
}

template<>
float little_endian(float f) {
    u32 u = little_endian<u32>(*(u32*)&f);
    return *(float*)&u;
}

template<>
double little_endian(double f) {
    u64 u = little_endian<u64>(*(u64*)&f);
    return *(double*)&u;
}

template<>
float from_big_endian(float f) {
    u32 u = from_big_endian<u32>(*(u32*)&f);
    return *(float*)&u;
}

template<>
double from_big_endian(double f) {
    u64 u = from_big_endian<u64>(*(u64*)&f);
    return *(double*)&u;
}

template<>
float from_little_endian(float f) {
    u32 u = from_little_endian<u32>(*(u32*)&f);
    return *(float*)&u;
}

template<>
double from_little_endian(double f) {
    u64 u = from_little_endian<u64>(*(u64*)&f);
    return *(double*)&u;
}

bytebuf::bytebuf():
    _start(0), _end(0), _capacity(32), _data(new u8[_capacity]) {
    // starts with empty (uninitialized) buffer of 32 bytes
}

bytebuf::~bytebuf() {
    delete[] _data;
}

bytebuf::bytebuf(const bytebuf& other):
    _start(other._start), _end(other._end), 
    _capacity(other._capacity), _data(new u8[_capacity]) {

    // copies memory from start to end
    for (u64 i = _start; i != _end; i = (i + 1) & (_capacity - 1))
        _data[i] = other._data[i];
}

bytebuf& bytebuf::operator=(const bytebuf& other) {
    if (this != &other) {
        // free existing allocation
        delete[] _data;
        
        // copy from other
        _start = other._start;
        _end = other._end;
        _capacity = other._capacity;
        _data = new u8[_capacity];
        for (u64 i = _start; i != _end; i = (i + 1) & (_capacity - 1))
            _data[i] = other._data[i];
    }
    return *this;
}

u8 bytebuf::peek() const {
    // empty buffer returns null char
    if (_start == _end) return '\0';

    // return next byte
    return _data[_start];
}

u8 bytebuf::read() {
    // empty buffer returns null char
    if (_start == _end) return '\0';

    // read next byte
    u8 byte = _data[_start];
    _start = (_start + 1) & (_capacity - 1);
    return byte;
}

void bytebuf::read(char* buffer, u64 length) {
    for (u64 i = 0; i < length; i ++)
        buffer[i] = read();
}

void bytebuf::write(u8 byte) {
    u64 new_end = (_end + 1) & (_capacity - 1);
    if (new_end == _start) {
        // hold onto old buffer
        u64 old_start = _start, old_end = _end, old_capacity = _capacity;
        u8* old_data = _data;

        // grow and clear buffer
        _start = 0;
        _end = 0;
        _capacity *= 2;
        _data = new u8[_capacity];

        // copy from old buffer
        while (old_start != old_end) {
            write(old_data[old_start]);
            old_start = (old_start + 1) & (old_capacity - 1);
        }

        // free old buffer
        delete[] old_data;
    }

    _data[_end] = byte;
    _end = (_end + 1) & (_capacity - 1);
}

void bytebuf::write(const char* string, u64 length) {
    for (u64 i = 0; i < length; i ++)
        write((u8)string[i]);
}

u64 bytebuf::size() const {
    u64 end = _end;
    if (end < _start) end += _capacity; // account for wraparound
    return end - _start;
}

void bytebuf::clear() {
    _start = _end;
}