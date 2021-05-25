#include "sys.h"

extern void* _mmap(void*, u64, u64, u64) asm("_mmap");
extern void _munmap(void*, u64) asm("_munmap");
extern void _exit(u64) asm("_exit");
extern i64 _read(u64, char*, u64) asm("_read");
extern void _write(u64, const char*, u64) asm("_write");

/* * * * * * * * * * * * * * * *
 *                             *
 *        System Calls         *
 *                             *
 * * * * * * * * * * * * * * * */

inline void* _mmap(void* addr, u64 len, u64 prot, u64 flags) {
    void* ret;
    
    #ifdef __APPLE__
        #define MMAP_CODE "0x20000C5"
    #elif defined(__linux__)
        #define MMAP_CODE "9"
    #endif

    #define PROT_READ 0x1		
    #define PROT_WRITE 0x2		
    #define PROT_EXEC 0x4		
    #define PROT_NONE 0x0		
    #define PROT_GROWSDOWN 0x01000000	
    #define PROT_GROWSUP 0x02000000	

    #define MAP_SHARED	0x01
    #define MAP_PRIVATE	0x02
    #define MAP_ANONYMOUS 0x20

    asm volatile ("mov $" MMAP_CODE ", %%rax\n\t"
        "mov %1, %%rdi\n\t"
        "mov %2, %%rsi\n\t"
        "mov %3, %%rdx\n\t"
        "mov %4, %%r10\n\t"
        "mov $-1, %%r8\n\t"
        "mov $0, %%r9\n\t"
        "syscall\n\t"
        "mov %%rax, %0"
        : "=r" (ret)
        : "r" (addr), "r" (len), "r" (prot), "r" (flags)
        : "rdi", "rsi", "rdx", "r10", "r9", "r8", "rax"
    );

    return ret;
}

inline void _munmap(void* addr, u64 len) {
    #ifdef __APPLE__
        #define MUNMAP_CODE "0x2000049"
    #elif defined(__linux__)
        #define MUNMAP_CODE "11"
    #endif

    asm volatile ("mov $" MMAP_CODE ", %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "mov %1, %%rsi\n\t"
        "syscall\n\t"
        : 
        : "r" (addr), "r" (len)
        : "rdi", "rsi", "rax"
    );
}

inline void _exit(u64 ret) {
    #ifdef __APPLE__
        #define EXIT_CODE "0x2000001"
    #elif defined(__linux__)
        #define EXIT_CODE "60"
    #endif
    asm volatile ("mov $" EXIT_CODE ", %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "syscall\n\t"
        :
        : "r" (ret)
        : "rax", "rdi"
    );
}

inline i64 _read(u64 fd, char* buf, u64 len) {
    u64 ret;
    #ifdef __APPLE__
        #define READ_CODE "0x2000003"
    #elif defined(__linux__)
        #define READ_CODE "0"
    #endif
    asm volatile ("mov $" READ_CODE ", %%rax\n\t"
        "mov %1, %%rdi\n\t"
        "mov %2, %%rsi\n\t"
        "mov %3, %%rdx\n\t"
        "syscall\n\t"
        "mov %%rax, %0"
        : "=r" (ret)
        : "r" (fd), "r" (buf), "r" (len)
        : "rax", "rdi", "rsi", "rdx"
    );   
    return ret;
}

inline void _write(u64 fd, const char* text, u64 len) {
    #ifdef __APPLE__
        #define WRITE_CODE "0x2000004"
    #elif defined(__linux__)
        #define WRITE_CODE "1"
    #endif
    asm volatile ("mov $" WRITE_CODE ", %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "mov %1, %%rsi\n\t"
        "mov %2, %%rdx\n\t"
        "syscall\n\t"
        :
        : "r" (fd), "r" (text), "r" (len)
        : "rax", "rdi", "rsi", "rdx"
    );   
}

void memcpy(void* dst, const void* src, int size) {
    int i = 0;
    while (i < (size & ~7)) *(u64*)dst = *(u64*)src, i += 8;
    while (i < size) *(u8*)dst = *(u8*)src, i ++;
}

void exit(u64 code) {
    _exit(code);
}

struct stream {
    i32 fd;
    u32 start, end;
    char buf[4096];
};

static stream internal_stdout = { 1, 0, 0 }, 
    internal_stdin = { 0, 0, 0 };

stream &_stdout = internal_stdout, 
    &_stdin = internal_stdin;

static void flush_input(stream& io) {
    memcpy(io.buf, io.buf + io.start, io.end - io.start);
    io.end -= io.start, io.start = 0;
    i64 amt = _read(io.fd, io.buf + io.end, 4096 - (io.end - io.start));
    io.end += amt;
}

static void flush_output(stream& io) {
    _write(io.fd, io.buf + io.start, io.end - io.start);
    io.end = io.start;
}

static void push_if_necessary(stream& io, u32 n = 64) {
    if (4096 - io.end < n) flush_output(io);
}

static void pull_if_necessary(stream& io, u32 n = 64) {
    if (io.end - io.start < n) flush_input(io);
}

static inline void put(stream& io, u8 c) {
    io.buf[io.end ++] = c;
    if (&io == &_stdout && c == '\n') flush_output(io);
}

static inline void write_uint(stream& io, u64 u) {
    if (!u) return put(io, '0');
    int c = 0, d = 0;
    u64 p = 1;
    while (p < u) p *= 10, ++ c;
    d = c;
    while (u) {
        io.buf[io.end + c] = '0' + u % 10; 
        u /= 10;
        -- c;
    }
    io.end += d + 1;
}

static inline void write_int(stream& io, i64 i) {
    if (i < 0) put(io, '-'), i = -i;
    write_uint(io, i); 
}

void write(stream& io, const char* str, u32 n) {
    push_if_necessary(io, (n + 63) / 64 * 64);
    if (&io == &_stdout) for (u32 i = 0; i < n; i ++) {
        io.buf[io.end ++] = str[i];
        if (str[i] == '\n') flush_output(io);
    }
    else memcpy(io.buf + io.end, str, n);
}

void write(stream& io, const char& c) {
    push_if_necessary(io);
    put(io, c);
}

void write(stream& io, const u8& c) {
    push_if_necessary(io);
    put(io, c);
}

void write(stream& io, const u16& u) {
    push_if_necessary(io);
    write_uint(io, u);
}

void write(stream& io, const u32& u) {
    push_if_necessary(io);
    write_uint(io, u);
}

void write(stream& io, const u64& u) {
    push_if_necessary(io);
    write_uint(io, u);
}

void write(stream& io, const i8& c) {
    push_if_necessary(io);
    put(io, c);
}

void write(stream& io, const i16& i) {
    push_if_necessary(io);
    write_int(io, i);
}

void write(stream& io, const i32& i) {
    push_if_necessary(io);
    write_int(io, i);
}

void write(stream& io, const i64& i) {
    push_if_necessary(io);
    write_int(io, i);
}

bool isdigit(char c) {
    return c >= '0' && c <= '9';
}

u64 read_uint(stream& io) {
    u64 acc = 0;
    u8 i = 0;
    while (isdigit(io.buf[io.start]) && i < 18) {
        acc *= 10;
        acc += io.buf[io.start] - '0';
        i ++;
        io.start ++;
    }
    return acc;
}

i64 read_int(stream& io) {
    i64 i = 1;
    if (io.buf[io.start] == '-') i = -1;
    return i * read_uint(io);
}

void read(stream& io, char* str, u32 n) {
    while (n >= 4096) {
        pull_if_necessary(io, (n + 63) / 64 * 64);
        memcpy(str, io.buf + io.start, n);
        n -= 4096;
    }
    pull_if_necessary(io, (n + 63) / 64 * 64);
    memcpy(str, io.buf + io.start, n);
}

void read(stream& io, u8& c) {
    pull_if_necessary(io);
    c = io.buf[io.start ++];
}

void read(stream& io, u16& u) {
    pull_if_necessary(io);
    u = read_uint(io);
}

void read(stream& io, u32& u) {
    pull_if_necessary(io);
    u = read_uint(io);
}

void read(stream& io, u64& u) {
    pull_if_necessary(io);
    u = read_uint(io);
}

void read(stream& io, i8& c) {
    pull_if_necessary(io);
    c = io.buf[io.start ++];
}

void read(stream& io, i16& i) {
    pull_if_necessary(io);
    i = read_int(io);
}

void read(stream& io, i32& i) {
    pull_if_necessary(io);
    i = read_int(io);
}

void read(stream& io, i64& i) {
    pull_if_necessary(io);
    i = read_int(io);
}

void flush(stream& io) {
    flush_output(io);
}

void write(stream& io) {}
void read(stream& io) {}