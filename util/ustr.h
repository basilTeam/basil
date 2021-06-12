#ifndef BASIL_USTRING_H
#define BASIL_USTRING_H

#include "defs.h"
#include "slice.h"
#include "vec.h"
#include "utf8.h"

class ustring {
    u8* data;
    u32 _size, _count, _capacity;
    u8 buf[12];

    void free();
    void init(u32 size);
    void copy(const u8* s, u32 n);
    void grow();
    i32 cmp(const u8* s) const;
public:
    ustring();
    ~ustring();
    ustring(const char* cstr);
    ustring(const const_slice<u8>& range);
    ustring(const string& str);
    ustring(buffer& buf);
    ustring(const ustring& other);
    ustring(ustring&& other);
    ustring& operator=(const ustring& other);
    ustring& operator=(ustring&& other);

    struct iterator {
        const char* _ptr;

        iterator(const char* ptr);
        rune operator*() const;
        iterator& operator++();
        iterator operator++(int);
        iterator& operator--();
        iterator operator--(int);
        bool operator==(const iterator& other) const;
        bool operator!=(const iterator& other) const;
    };

    iterator begin() const;
    iterator end() const;

    ustring& operator+=(char ch);
    ustring& operator+=(rune ch);
    ustring& operator+=(const char* cstr);
    ustring& operator+=(const ustring& other);
    u32 size() const;
    u32 bytes() const;
    u32 capacity() const;
    rune front() const;
    rune back() const;
    void clear();
    const char* raw() const;
    bool operator==(const char* s) const;
    bool operator==(const ustring& s) const;
    bool operator<(const char* s) const;
    bool operator<(const ustring& s) const;
    bool operator>(const char* s) const;
    bool operator>(const ustring& s) const;
    bool operator!=(const char* s) const;
    bool operator!=(const ustring& s) const;
    bool operator<=(const char* s) const;
    bool operator<=(const ustring& s) const;
    bool operator>=(const char* s) const;
    bool operator>=(const ustring& s) const;
};

ustring operator+(ustring s, char c);
ustring operator+(ustring s, const char* cs);
ustring operator+(ustring s, const ustring& cs);
ustring escape(const ustring& s);

void write(stream& io, rune r);

void write(stream& io, const ustring& s);
void read(stream& io, ustring& s);

#endif