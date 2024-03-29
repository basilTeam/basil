/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "str.h"
#include "slice.h"
#include "io.h"
#include "panic.h"

void string::free() {
    if (_capacity > 16) delete[] data;
}

void string::init(u32 size) {
    if (size > 16) {
        _size = 0, _capacity = size ? size : 16;
        data = new u8[_capacity];
    }
    else data = buf, _size = 0, _capacity = 16;
    *data = '\0';
}

void string::copy(const u8* s) {
    u8* dptr = data;
    while (*s) *(dptr ++) = *(s ++), ++ _size;
    *dptr = '\0';
}

void string::grow() {
    u8* old = data;
    init(_capacity * 2);
    copy(old);
    if (_capacity / 2 > 16) delete[] old;
}

i32 string::cmp(const u8* s) const {
    u8* dptr = data;
    while (*s && *s == *dptr) ++ s, ++ dptr;
    return i32(*dptr) - i32(*s);
}

i32 string::cmp(const char* s) const {
    u8* dptr = data;
    while (*s && *s == *dptr) ++ s, ++ dptr;
    return i32(*dptr) - i32(*s);
}

string::string() { 
    init(16); 
}

string::~string() {
    free();
}

string::string(const string& other) {
    init(other._capacity);
    copy(other.data);
}

string::string(string&& other): 
    data(other.data), _size(other._size), _capacity(other._capacity) {
    other._capacity = 0; // prevent destructor
}

string::string(const char* s): string() {
    operator+=(s);
}

string::string(buffer& buf): string() {
    while (buf.peek()) operator+=(buf.read());
}

string::string(const const_slice<u8>& range): string() {
    for (u8 c : range) operator+=(c);
}

string& string::operator=(const string& other) {
    if (this != &other) {
        free();
        init(other._capacity);
        copy(other.data);
    }
    return *this;
}

string& string::operator=(string&& other) {
    if (this != &other) {
        free();
        _size = other._size;
        _capacity = other._capacity;
        data = other.data;
        other._capacity = 0; // prevent destructor
    }
    return *this;
}

string& string::operator+=(u8 c) {
    if (_size + 1 >= _capacity) grow();
    data[_size ++] = c;
    data[_size] = '\0';
    return *this;
}

string& string::operator+=(char c) {
    return operator+=(u8(c));
}

string& string::operator+=(const u8* s) {
    u32 size = 0;
    const u8* sptr = s;
    while (*sptr) ++ size, ++ sptr;
    while (_size + size + 1 >= _capacity) grow();
    u8* dptr = data + _size;
    sptr = s;
    while (*sptr) *(dptr ++) = *(sptr ++);
    *dptr = '\0';
    _size += size;
    return *this;
}

string& string::operator+=(const char* s) {
    return operator+=((const u8*)s);
}

string& string::operator+=(const string& s) {
    return operator+=(s.data);
}

u32 string::size() const {
    return _size;
}

u32 string::capacity() const {
    return _capacity;
}

const u8& string::operator[](u32 i) const {
    if (i >= _size) panic("Attempted out-of-bounds access of string char!");
    return data[i];
}

u8& string::operator[](u32 i) {
    if (i >= _size) panic("Attempted out-of-bounds access of string char!");
    return data[i];
}

const_slice<u8> string::operator[](pair<u32, u32> range) const {
    if (range.first >= _size || range.second > _size) panic("Attempted out-of-bounds slice of string!");
    if (range.second < range.first) panic("Attempted slice of string with reversed bounds!");
    return { range.second - range.first, data + range.first };
}

slice<u8> string::operator[](pair<u32, u32> range) {
    if (range.first >= _size || range.second > _size) panic("Attempted out-of-bounds slice of string!");
    if (range.second < range.first) panic("Attempted slice of string with reversed bounds!");
    return { range.second - range.first, data + range.first };
}

bool string::endswith(u8 c) const {
    if (_size == 0) panic("Attempted to get ending of empty string!");
    return data[_size - 1] == c;
}

const u8* string::raw() const {
    return data;
}

bool string::operator==(const u8* s) const {
    return cmp(s) == 0;
}

bool string::operator==(const char* s) const {
    return cmp(s) == 0;
}

bool string::operator==(const string& s) const {
    return cmp(s.data) == 0;
}

bool string::operator<(const u8* s) const {
    return cmp(s) < 0;
}

bool string::operator<(const char* s) const {
    return cmp(s) < 0;
}

bool string::operator<(const string& s) const {
    return cmp(s.data) < 0;
}

bool string::operator>(const u8* s) const {
    return cmp(s) > 0;
}

bool string::operator>(const char* s) const {
    return cmp(s) > 0;
}

bool string::operator>(const string& s) const {
    return cmp(s.data) > 0;
}

bool string::operator!=(const u8* s) const {
    return cmp(s) != 0;
}

bool string::operator!=(const char* s) const {
    return cmp(s) != 0;
}

bool string::operator!=(const string& s) const {
    return cmp(s.data) != 0;
}

bool string::operator<=(const u8* s) const {
    return cmp(s) <= 0;
}

bool string::operator<=(const char* s) const {
    return cmp(s) <= 0;
}

bool string::operator<=(const string& s) const {
    return cmp(s.data) <= 0;
}

bool string::operator>=(const u8* s) const {
    return cmp(s) >= 0;
}

bool string::operator>=(const char* s) const {
    return cmp(s) >= 0;
}

bool string::operator>=(const string& s) const {
    return cmp(s.data) >= 0;
}

string operator+(string s, char c) {
    s += c;
    return s;
}

string operator+(string s, const char* cs) {
    s += cs;
    return s;
}

string operator+(string s, const string& cs) {
    s += cs;
    return s;
}

string escape(const string& s) {
  string escaped = "";
  for (u32 i = 0; i < s.size(); i ++) {
    switch (s[i]) {
      case '\n': 
        escaped += "\\n";
        break;
      case '\t':
        escaped += "\\t";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\v':
        escaped += "\\v";
        break;
      case '\0':
        escaped += "\\0";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\"':
        escaped += "\\\"";
        break;
      default:
        escaped += s[i];
    }
  }
  return escaped;
}

void write(stream& io, const string& s) {
    write(io, s.raw());
}

void read(stream& io, string& s) {
    while (isspace(io.peek())) io.read(); // consume leading spaces
    while (io.peek() && !isspace(io.peek())) s += io.read();
}

void write(stream& io, const const_slice<u8>& str) {
    for (u8 c : str) io.write(c);
}

void write(stream& io, const slice<u8>& str) {
    for (u8 c : str) io.write(c);
}
