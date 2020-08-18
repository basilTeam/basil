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
    virtual operator bool() const = 0;
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
    operator bool() const override;
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
    operator bool() const override;
    u8* begin();
    const u8* begin() const;
    u8* end();
    const u8* end() const;
};

extern stream &_stdin, &_stdout;
void setprecision(u32 p);

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

#endif