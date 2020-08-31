#ifndef JASMINE_UTIL_H
#define JASMINE_UTIL_H

#include <cstdint>

#include "../util/defs.h"
#include "../util/vec.h"
#include "../util/hash.h"

enum class EndianOrder : u32 {
    UTIL_LITTLE_ENDIAN = 0x03020100ul,
    UTIL_BIG_ENDIAN = 0x00010203ul,
    UTIL_PDP_ENDIAN = 0x01000302ul,     
    UTIL_HONEYWELL_ENDIAN = 0x02030001ul
};

static const union { 
    u8 bytes[4]; 
    u32 value; 
} host_order = { { 0, 1, 2, 3 } };

template<typename T>
T flip_endian(T value) {
    T result = 0;
    constexpr const i64 bits = sizeof(T) * 8;
    for (i64 i = bits - 8; i >= 0; i -= 8) {
        result |= ((value >> i) & 0xff) << ((bits - 8) - i); 
    }
    return result;
}

template<typename T>
T little_endian(T value) {
    if ((EndianOrder)host_order.value == EndianOrder::UTIL_LITTLE_ENDIAN)
        return value;
    return flip_endian(value);
}

template<typename T>
T big_endian(T value) {
    if ((EndianOrder)host_order.value == EndianOrder::UTIL_BIG_ENDIAN)
        return value;
    return flip_endian(value);
}

template<typename T>
T from_big_endian(T value) {
    if ((EndianOrder)host_order.value == EndianOrder::UTIL_BIG_ENDIAN)
        return value;
    return flip_endian(value);
}

template<typename T>
T from_little_endian(T value) {
    if ((EndianOrder)host_order.value == EndianOrder::UTIL_LITTLE_ENDIAN)
        return value;
    return flip_endian(value);
}

class byte_buffer {
    u64 _start;
    u64 _end;
    u64 _capacity;
    u8* _data;
public:
    byte_buffer();
    ~byte_buffer();
    byte_buffer(const byte_buffer& other);
    byte_buffer& operator=(const byte_buffer& other);

    u8 peek() const;
    u8 read();
    void read(char* buffer, u64 length);
    void write(u8 byte);
    void write(const char* string, u64 length);
    u64 size() const;
    void clear();

    template<typename T>
    T read() {
        // serializes object from lowest to highest address
        static u8 buffer[sizeof(T)];
        for (u32 i = 0; i < sizeof(T); i ++) 
          buffer[i] = byte_buffer::read();
        return *(T*)buffer;
    }
    
    template<typename T>
    void write(const T& value) {
        // deserializes object, assuming first byte is lowest address
        const u8* data = (const u8*)&value;
        for (u32 i = 0; i < sizeof(T); i ++) write(data[i]);
    }

    template<typename... Args>
    void write(u8 b, Args... args) {
        write(b);
        write(args...);
    }
};

// allocates writable, executable memory
void* alloc_exec(u64 size);

// protects executable memory from being written
void protect_exec(void* exec, u64 size);

// deallocates executable memory
void free_exec(void* exec, u64 size);

#endif