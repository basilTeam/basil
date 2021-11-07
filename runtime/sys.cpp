/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "sys.h"

extern "C" void* _sys_mmap(void*, u64, u64, u64);
extern "C" void _sys_munmap(void*, u64);
extern "C" void _sys_exit(i64);
extern "C" i64 _sys_read(u64, char*, u64);
extern "C" i64 _sys_write(u64, const char*, u64);

/* * * * * * * * * * * * * * * *
 *                             *
 *        System Calls         *
 *                             *
 * * * * * * * * * * * * * * * */

#ifdef BASIL_WINDOWS
#include "windows.h"
#endif

extern "C" void* _sys_mmap(void* addr, u64 len, u64 prot, u64 flags) {
    #if defined(BASIL_UNIX)
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
        void* ret;

        asm volatile (
            "mov $" MMAP_CODE ", %%rax\n\t"
            "mov %%rcx, %%r10\n\t"
            "mov $-1, %%r8\n\t"
            "mov $0, %%r9\n\t"
            "syscall\n\t"
            : "=a" (ret)
            : "D" (addr), "S" (len), "d" (prot), "c" (flags)
            : "r8", "r9", "r10", "r11", "memory"
        );

        return ret;
    #elif defined(BASIL_WINDOWS)
        #define PROT_READ 0x1		
        #define PROT_WRITE 0x2		
        #define PROT_EXEC 0x4		
        static const DWORD PROT[8] = {
            0, PAGE_READONLY, PAGE_READWRITE, PAGE_READWRITE,
            PAGE_EXECUTE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE, PAGE_EXECUTE_READWRITE
        };

        return VirtualAlloc(nullptr, (SIZE_T)len, MEM_COMMIT | MEM_RESERVE, PROT[prot]);
    #endif
}

extern "C" void _sys_munmap(void* addr, u64 len) {
    #if defined(BASIL_UNIX)
        #ifdef __APPLE__
            #define MUNMAP_CODE "0x2000049"
        #elif defined(__linux__)
            #define MUNMAP_CODE "11"
        #endif

        asm volatile (
            "mov $" MUNMAP_CODE ", %%rax\n\t"
            "syscall\n\t"
            : 
            : "D" (addr), "S" (len)
            : "rax", "rcx", "r11", "memory"
        );

    #elif defined(BASIL_WINDOWS)
        VirtualFree((LPVOID)addr, 0, (DWORD)MEM_RELEASE);
    #endif
}

extern "C" void _sys_exit(i64 code) {
    #if defined(BASIL_UNIX)
        #ifdef __APPLE__
            #define EXIT_CODE "0x2000001"
        #elif defined(__linux__)
            #define EXIT_CODE "60"
        #endif
        asm volatile (
            "mov $" EXIT_CODE ", %%rax\n\t"
            "syscall\n\t"
            :
            : "D" (code)
            : "rax", "rcx", "r11", "memory"
        );
    #elif defined(BASIL_WINDOWS)
        ExitProcess((UINT)code);
    #endif
}

extern "C" i64 _sys_read(u64 fd, char* buf, u64 len) {
    #if defined(BASIL_UNIX)
        #ifdef __APPLE__
            #define READ_CODE "0x2000003"
        #elif defined(__linux__)
            #define READ_CODE "0"
        #endif
        u64 ret;
        asm volatile (
            "mov $" READ_CODE ", %%rax\n\t"
            "syscall\n\t"
            : "=a" (ret)
            : "D" (fd), "S" (buf), "d" (len)
            : "rcx", "r11", "memory"
        );   
        return ret;
    #elif defined(BASIL_WINDOWS)
        HANDLE hnd = (HANDLE)fd;
        DWORD ret;
        ReadFile(hnd, (LPVOID)buf, (DWORD)len, &ret, nullptr);
        return ret;
    #endif
}

extern "C" i64 _sys_write(u64 fd, const char* text, u64 len) {
    #if defined(BASIL_UNIX)
        #if defined(BASIL_MACOS)
            #define WRITE_CODE "0x2000004"
        #elif defined(BASIL_LINUX)
            #define WRITE_CODE "1"
        #endif
        i64 ret = 0;
        asm volatile (
            "mov $" WRITE_CODE ", %%rax\n\t"
            "syscall\n\t"
            : "=a" (ret)
            : "D" (fd), "S" (text), "d" (len)
            : "rcx", "r11", "memory"
        );   
        return ret;
    #elif defined(BASIL_WINDOWS)
        HANDLE hnd = (HANDLE)fd;
        DWORD ret;
        WriteFile(hnd, (LPCVOID)text, (DWORD)len, &ret, nullptr);
        return (i64)ret;
    #endif
}

extern "C" i64 _sys_open(const char* path, i64 flags) {
    #if defined(BASIL_UNIX)
        #if defined(BASIL_MACOS)
            #define OPEN_CODE "0x2000005"
        #elif defined(BASIL_LINUX)
            #define OPEN_CODE "2"
        #endif
        
        i64 nflags = 0, mode = 0;

        if (flags & BASIL_WRITE) nflags |= 0x40; // O_CREAT  
        if (flags & BASIL_APPEND) nflags |= 0x400; // O_APPEND
        if (flags & BASIL_READ && flags & BASIL_WRITE) nflags |= 0x2, mode |= 0666; // RDWR
        else if (flags & BASIL_WRITE) nflags |= 0x1, mode |= 0222; // WRONLY
        else mode |= 0444;
        // default to RDONLY

        i64 ret = 0;
        asm volatile (
            "mov $" OPEN_CODE ", %%rax\n\t"
            "syscall\n\t"
            : "=a" (ret)
            : "D" (path), "S" (nflags), "d" (mode)
            : "rcx", "r11", "memory"
        );   
        return ret; // should return -1 on failure
    #elif defined(BASIL_WINDOWS)
        DWORD access = 0, creation = 0;
        if (flags & BASIL_READ) access |= GENERIC_READ, creation = OPEN_EXISTING;
        if (flags & BASIL_WRITE) access |= GENERIC_WRITE, creation = OPEN_ALWAYS;
        if (flags & BASIL_APPEND) creation = OPEN_ALWAYS;
        HANDLE ret = CreateFileA((LPCSTR)path, access, 0, nullptr, creation, , (DWORD)len, &ret, FILE_ATTRIBUTE_NORMAL);
        if (ret == INVALID_HANDLE_VALUE) return -1; // -1 for invalid file
        return (i64)ret;
    #endif
}

extern "C" i64 _sys_close(i64 fd) {
    #if defined(BASIL_UNIX)
        #if defined(BASIL_MACOS)
            #define CLOSE_CODE "0x2000006"
        #elif defined(BASIL_LINUX)
            #define CLOSE_CODE "3"
        #endif

        i64 ret = 0;
        asm volatile (
            "mov $" CLOSE_CODE ", %%rax\n\t"
            "syscall\n\t"
            : 
            : "D" (fd)
            : "rax", "rcx", "r11", "memory"
        );   
        return ret; // should return -1 on failure
    #elif defined(BASIL_WINDOWS)
        CloseHandle(fd);
    #endif
}

extern "C" i64 _sys_memcpy(void* dst, const void* src, size_t size) {
    i64 i = 0;
    while (i < size) ((u8*)dst)[i] = ((u8*)src)[i], i ++;
    return i;
}

namespace sys {
    #define UTF8_MINIMAL
    #include "util/utf8.cpp" // embed subset of utf8 features in sys
    #undef UTF8_MINIMAL

    #define STREAMBUF_SIZE 16368
    #define N_STREAMS 65536

    struct stream {
        i32 fd;
        u32 start, end, unused;
        char buf[STREAMBUF_SIZE];
    };

    static stream* _sys_streams[N_STREAMS];

    static_assert(sizeof(stream) == 16384);

    stream* new_stream(i32 fd) {
        stream* s = (stream*)_sys_mmap(nullptr, sizeof(stream), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
        s->fd = fd;
        s->start = 0;
        s->end = 0;
        return s;
    }

    void init_io() {
        for (u32 i = 0; i < N_STREAMS; i ++) 
            _sys_streams[i] = nullptr; // ensure zero init
        _sys_streams[0] = new_stream(0); // stdin
        _sys_streams[1] = new_stream(1); // stdout
        _sys_streams[2] = new_stream(2); // stderr
    }

    stream& io_for_fd(i64 i) {
        return *_sys_streams[i];
    }

    static void flush_input(stream& io) {
        _sys_memcpy(io.buf, io.buf + io.start, io.end - io.start);
        io.end -= io.start, io.start = 0;
        i64 amt = _sys_read(io.fd, io.buf + io.end, 4096 - (io.end - io.start));
        io.end += amt;
    }

    static void flush_output(stream& io) {
        _sys_write(io.fd, io.buf + io.start, io.end - io.start);
        io.end = io.start;
    }

    void exit(i64 code) {
        flush_output(io_for_fd(BASIL_STDOUT_FD));
        flush_output(io_for_fd(BASIL_STDERR_FD));
        _sys_exit(code);
    }
    
    i64 open(const char* path, i64 flags) {
        i64 fd = _sys_open(path, flags);
        if (fd < 0) return -1;
        else {
            i64 hnd = -1;
            for (u32 i = 3; i < N_STREAMS; i ++) { // skip fds 0-2 since they're standard
                if (!_sys_streams[i]) {
                    hnd = i;
                    _sys_streams[i] = new_stream(fd);
                    break;
                }
            }
            return hnd; // returns -1 if we couldn't find an open stream
        }
    }

    void close(i64 i) {
        if (i < 0 || i >= N_STREAMS || !_sys_streams[i]) return;
        if (_sys_streams[i]->end != _sys_streams[i]->start) flush_output(*_sys_streams[i]);
        _sys_close(_sys_streams[i]->fd);
        _sys_munmap(_sys_streams[i], sizeof(stream));
        _sys_streams[i] = nullptr;
    }

    static inline void push_if_necessary(stream& io, u32 n = 64) {
        if (STREAMBUF_SIZE - io.end < n) flush_output(io);
    }

    static inline void pull_if_necessary(stream& io, u32 n = 64) {
        if (io.end - io.start < n) flush_input(io);
    }

    static inline void put(stream& io, u8 c) {
        io.buf[io.end ++] = c;
        if (&io == &io_for_fd(BASIL_STDOUT_FD) && c == '\n') flush_output(io);
    }

    const char* digits[] = {
        "00", "01", "02", "03", "04", "05", "06", "07", "08", "09",
        "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
        "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
        "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
        "40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
        "50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
        "60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
        "70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
        "80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
        "90", "91", "92", "93", "94", "95", "96", "97", "98", "99"
    };

    void write_uint(stream& io, u64 u) {
        push_if_necessary(io, 24);
        if (!u) return put(io, '0');
        int c = 0, d = 0;
        u64 p = 1;
        while (p <= u) p *= 10, ++ c;
        d = c;
        while (c >= 2) {
            *(u16*)(io.buf + io.end + c - 2) = *(u16*)(digits[u % 100]); 
            u /= 100;
            c -= 2;
        }
        if (c) io.buf[io.end + c - 1] = "0123456789"[u % 10];
        io.end += d;
    }

    void write_int(stream& io, i64 i) {
        push_if_necessary(io, 24);
        if (i < 0) put(io, '-'), i = -i;
        write_uint(io, i); 
    }

    void write_string(stream& io, const char* str, u32 n) {
        u32 i = 0;
        if (&io == &io_for_fd(BASIL_STDOUT_FD)) for (i = 0; i < n; i ++) {
            push_if_necessary(io, 4);
            io.buf[io.end ++] = str[i];
            if (str[i] == '\n') flush_output(io);
        }
        else while (n) {
            u32 chunk = n > STREAMBUF_SIZE ? STREAMBUF_SIZE : n;
            push_if_necessary(io, chunk);
            u32 written = _sys_memcpy(io.buf + io.end, str + i, chunk);
            io.end += written;
            i += written;
            n -= written;
        }
    }

    void write_char(stream& io, rune c) {
        push_if_necessary(io, 4);
        io.end += utf8_encode(&c, 1, io.buf + io.end, 4);
    }

    void write_byte(stream& io, u8 c) {
        push_if_necessary(io, 1);
        put(io, c);
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
        while (n >= 8192) {
            pull_if_necessary(io, (n + 63) / 64 * 64);
            _sys_memcpy(str, io.buf + io.start, n);
            n -= 8192;
        }
        pull_if_necessary(io, (n + 63) / 64 * 64);
        _sys_memcpy(str, io.buf + io.start, n);
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
}