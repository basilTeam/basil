#ifndef BASIL_STRING_H
#define BASIL_STRING_H

#include "defs.h"
#include "stdio.h"

class string {
    u8* data;
    u32 _size, _capacity;
    u8 buf[16];

    void free();
    void init(u32 size);
    void copy(const u8* s);
    void grow();
    i32 cmp(const u8* s) const;
    i32 cmp(const char* s) const;

public:
    string();
    ~string();
    string(const string& other);
    string(const char* s);
    string(const const_slice<u8>& range);
    string& operator=(const string& other);

    string& operator+=(u8 c);
    string& operator+=(char c);
    string& operator+=(const u8* s);
    string& operator+=(const char* s);
    string& operator+=(const string& s);
    u32 size() const;
    u32 capacity() const;
    const u8& operator[](u32 i) const;
    u8& operator[](u32 i);
    const_slice<u8> operator[](pair<u32, u32> range) const;
    slice<u8> operator[](pair<u32, u32> range);
    const u8* raw() const;
    bool operator==(const u8* s) const;
    bool operator==(const char* s) const;
    bool operator==(const string& s) const;
    bool operator<(const u8* s) const;
    bool operator<(const char* s) const;
    bool operator<(const string& s) const;
    bool operator>(const u8* s) const;
    bool operator>(const char* s) const;
    bool operator>(const string& s) const;
    bool operator!=(const u8* s) const;
    bool operator!=(const char* s) const;
    bool operator!=(const string& s) const;
    bool operator<=(const u8* s) const;
    bool operator<=(const char* s) const;
    bool operator<=(const string& s) const;
    bool operator>=(const u8* s) const;
    bool operator>=(const char* s) const;
    bool operator>=(const string& s) const;
};

string operator+(string s, char c);
string operator+(string s, const char* cs);
string operator+(string s, const string& cs);
string escape(const string& s);

void write(stream& io, const string& s);
void read(stream& io, string& s);

void write(stream& io, const const_slice<u8>& str);
void write(stream& io, const slice<u8>& str);

#endif