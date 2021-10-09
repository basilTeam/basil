/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "ustr.h"
#include "utf8.h"
#include "str.h"
#include "io.h"

void ustring::free() {
    if (_capacity > 12) delete[] data;
}

void ustring::init(u32 size) {
    if (size > 12) {
        _size = 0, _count = 0, _capacity = size ? size : 12;
        data = new u8[_capacity];
    }
    else data = buf, _size = 0, _count = 0, _capacity = 12;
    *data = '\0';
}

void ustring::copy_raw(const u8* s, u32 n) {
    u8* dptr = data;
    while (*s && n) {
        const u8* next = (const u8*)utf8_forward((const char*)s);
        if (unicode_error()) panic("UTF-8 encoding error!");
        while (s != next) *(dptr ++) = *(s ++), ++ _size, -- n;
        _count ++;
    }
    *dptr = '\0';
}

void ustring::copy(const u8* s, u32 count, u32 n) {
    _count = count;
    u8* dptr = data;
    while (*s && n) *(dptr ++) = *(s ++), ++ _size, -- n;
    *dptr = '\0';
}

void ustring::grow() {
    u8* old = data;
    auto size = _size;
    auto count = _count;
    init(_capacity * 2);
    copy(old, count, size);
    if (_capacity / 2 > 12) delete[] old;
}

i32 ustring::cmp(const u8* s) const {
    u8* dptr = data;
    while (*s && *s == *dptr) {
        ++ s, ++ dptr;
    }
    return i32(*dptr) - i32(*s);
}

ustring::ustring() {
    init(12);
}

ustring::~ustring() {
    free();
}

ustring::ustring(const char* cstr): ustring() {
    operator+=(cstr);
}

ustring::ustring(const const_slice<u8>& range) {
    init(range.size() + 1);
    copy_raw(&range.front(), range.size());
}

ustring::ustring(const string& str) {
    init(str.capacity());
    copy_raw(str.raw(), str.size());
}

ustring::ustring(buffer& buf): ustring() {
    operator+=(string(buf));
}

ustring::ustring(const ustring& other) {
    init(other.capacity());
    copy(other.data, other._count, other._size);
}

ustring::ustring(ustring&& other):
    data(other.data), _size(other._size), _count(other._count), _capacity(other._capacity) {
    other._capacity = 0; // prevent destructor
}

ustring& ustring::operator=(const ustring& other) {
    if (this != &other) {
        free();
        init(other._capacity);
        copy(other.data, other._count, other._size);
    }
    return *this;
}

ustring& ustring::operator=(ustring&& other) {
    if (this != &other) {
        free();
        _size = other._size;
        _count = other._count;
        _capacity = other._capacity;
        data = other.data;
        other._capacity = 0; // prevent destructor
    }
    return *this;
}

ustring& ustring::operator+=(char ch) {
    if (_size + 1 >= _capacity) grow();
    data[_size ++] = ch;
    data[_size] = '\0';
    _count ++;
    return *this;
}

ustring& ustring::operator+=(rune ch) {
    char buf[4];
    auto len = utf8_encode(&ch, 1, buf, 4);
    if (unicode_error()) panic("UTF-8 encoding error!");
    if (_size + len >= _capacity) grow();
    for (u32 i = 0; i < len; i ++) data[_size ++] = buf[i];
    data[_size] = '\0';
    _count ++;
    return *this;
}

ustring& ustring::operator+=(const char* cstr) {
    u32 size = 0;
    const char* sptr = cstr;
    while (*sptr) ++ size, ++ sptr;
    while (_size + size + 1 >= _capacity) grow();
    u8* dptr = data + _size;
    sptr = cstr;
    while (*sptr) {
        const char* forward = utf8_forward(sptr);
        if (unicode_error()) panic("UTF-8 encoding error!");
        while (sptr != forward) *(dptr ++) = *(sptr ++);
        _count ++;
    }
    *dptr = '\0';
    _size += size;
    return *this;
}

ustring& ustring::operator+=(const ustring& other) {
    u32 size = 0;
    const u8* sptr = other.data;
    while (*sptr) ++ size, ++ sptr;
    while (_size + size + 1 >= _capacity) grow();
    u8* dptr = data + _size;
    sptr = other.data;
    while (*sptr) *(dptr ++) = *(sptr ++);
    *dptr = '\0';
    _count += other.size();
    _size += size;
    return *this;
}

ustring::iterator::iterator(const char* ptr): _ptr(ptr) {}

rune ustring::iterator::operator*() const {
    rune r;
    utf8_decode_forward(_ptr, &r);
    if (unicode_error()) panic("UTF-8 encoding error!");
    return r;
}

ustring::iterator& ustring::iterator::operator++() {
    _ptr = utf8_forward(_ptr);
    if (unicode_error()) panic("UTF-8 encoding error!");
    return *this;
}

ustring::iterator ustring::iterator::operator++(int) {
    iterator copy = *this;
    operator++();
    return copy;
}

ustring::iterator& ustring::iterator::operator--() {
    _ptr = utf8_backward(_ptr);
    if (unicode_error()) panic("UTF-8 encoding error!");
    return *this;
}

ustring::iterator ustring::iterator::operator--(int) {
    iterator copy = *this;
    operator--();
    return copy;
}

bool ustring::iterator::operator==(const iterator& other) const {
    return _ptr == other._ptr;
}

bool ustring::iterator::operator!=(const iterator& other) const {
    return _ptr != other._ptr;
}

ustring::iterator ustring::begin() const {
    return { (const char*)data };
}

ustring::iterator ustring::end() const {
    return { (const char*)data + _size };
}

u32 ustring::size() const {
    return _count;
}

u32 ustring::bytes() const {
    return _size;
}

u32 ustring::capacity() const {
    return _capacity;
}

rune ustring::front() const {
    rune r;
    utf8_decode_forward((const char*)data, &r);
    if (unicode_error()) panic("UTF-8 encoding error!");
    return r;
}

rune ustring::back() const {
    rune r;
    utf8_decode_backward((const char*)data + _size, &r);
    if (unicode_error()) panic("UTF-8 encoding error!");
    return r;
}

void ustring::clear() {
    *data = '\0';
    _size = 0, _count = 0;
}

const char* ustring::raw() const {
    return (const char*)data;
}

bool ustring::operator==(const char* s) const {
    return cmp((const u8*)s) == 0;
}

bool ustring::operator==(const ustring& s) const {
    return cmp((const u8*)s.raw()) == 0;
}

bool ustring::operator<(const char* s) const {
    return cmp((const u8*)s) < 0;
}

bool ustring::operator<(const ustring& s) const {
    return cmp((const u8*)s.raw()) < 0;
}

bool ustring::operator>(const char* s) const {
    return cmp((const u8*)s) > 0;
}

bool ustring::operator>(const ustring& s) const {
    return cmp((const u8*)s.raw()) > 0;
}

bool ustring::operator!=(const char* s) const {
    return cmp((const u8*)s) != 0;
}

bool ustring::operator!=(const ustring& s) const {
    return cmp((const u8*)s.raw()) != 0;
}

bool ustring::operator<=(const char* s) const {
    return cmp((const u8*)s) <= 0;
}

bool ustring::operator<=(const ustring& s) const {
    return cmp((const u8*)s.raw()) <= 0;
}

bool ustring::operator>=(const char* s) const {
    return cmp((const u8*)s) >= 0;
}

bool ustring::operator>=(const ustring& s) const {
    return cmp((const u8*)s.raw()) >= 0;
}

ustring operator+(ustring s, char c) {
    s += c;
    return s;
}

ustring operator+(ustring s, const char* cs) {
    s += cs;
    return s;
}

ustring operator+(ustring s, const ustring& cs) {
    s += cs;
    return s;
}

ustring escape(const ustring& s) {
    ustring escaped;
    const char* sptr = s.raw();
    rune r;
    while (*sptr) {
        sptr = utf8_decode_forward(sptr, &r);
        if (unicode_error()) panic("UTF-8 encoding error!");
        switch (r) {
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
            escaped += r;
        }
    }
    return escaped;
}


void write(stream& io, rune r) {
    write(io, ustring() + r);
}

void write(stream& io, const ustring& s) {
    write(io, s.raw());
}

void read(stream& io, ustring& s) {
    while (isspace(io.peek())) io.read(); // consume leading spaces
    string tmp;
    while (io.peek() && !isspace(io.peek())) tmp += (char)io.read();
    s += tmp;
}
