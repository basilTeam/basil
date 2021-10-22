/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "sets.h"
#include "io.h"
#include "panic.h"

void bitset::grow(u32 n) {
    u64 new_capacity = n;
    new_capacity += 32; // round up to nearest 32
    new_capacity /= 32;
    new_capacity *= 32;
    u32* old_data = data;
    data = new u32[new_capacity / 32];
    for (u32 i = 0; i < new_capacity / 32; i ++) data[i] = 0;
    for (u32 i = 0; i < size / 32; i ++) data[i] = old_data[i];
    if (old_data != &local) delete[] old_data;
    size = new_capacity;
}

bitset::bitset(): data(&local), size(32), local(0) {}

bitset::~bitset() {
    if (data != &local) delete[] data;
}

bitset::bitset(const bitset& other): size(other.size) {
    if (size > 32) {
        data = new u32[other.size / 32];
        for (u32 i = 0; i < size; i += 32) data[i / 32] = other.data[i / 32];
    }
    else local = other.local, data = &local;
}

bitset& bitset::operator=(const bitset& other) {
    if (this != &other) {
        if (data != &local) delete[] data;
        size = other.size;
        if (size > 32) {
            data = new u32[other.size / 32];
            for (u32 i = 0; i < size; i += 32) data[i / 32] = other.data[i / 32];
        }
        else local = other.local, data = &local;
    }
    return *this;
}

bool bitset::contains(u32 n) const {
    if (n >= size) return false;
    u32 block = data[n / 32];
    return block & (1 << n % 32);
}

bool bitset::insert(u32 n) {
    if (n >= size) grow(n);
    u32& block = data[n / 32];
    bool set = block & (1 << n % 32);
    block |= (1 << n % 32);
    return !set;
}

bool bitset::erase(u32 n) {
    if (n >= size) grow(n);
    u32& block = data[n / 32];
    bool set = block & (1 << n % 32);
    block &= ~(1 << n % 32);
    return set;
}

void bitset::clear() {
    for (u32 i = 0; i < size / 32; i ++) data[i] = 0;
}

bool bitset::operator|=(const bitset& other) {
    bool changed = false;
    if (size < other.size) grow(other.size), changed = true;
    for (u32 i = 0; i < other.size / 32; i ++) 
        changed = (data[i] |= other.data[i]) != data[i] || changed;
    return changed;
}

bool bitset::operator&=(const bitset& other) {
    bool changed = false;
    for (u32 i = 0; i < size / 32; i ++) 
        if (i * 32 >= other.size) data[i] = 0, changed = true;
        else changed = (data[i] &= other.data[i]) != data[i] || changed;
    return changed;
}

bool bitset::operator==(const bitset& other) {
    u32 i = 0, j = 0;
    for (; i < size / 32; i ++) 
        if (i * 32 >= size || j * 32 >= other.size) break;
        else if (data[i] != other.data[i]) return false;

    u32* larger = size > other.size ? data : other.data;
    u32 larger_size = size > other.size ? size : other.size;
    for (; i < larger_size / 32; i ++) if (larger[i]) return false;

    return true;
}

bool bitset::operator!=(const bitset& other) {
    u32 i = 0, j = 0;
    for (; i < size / 32; i ++) 
        if (i * 32 >= size || j * 32 >= other.size) break;
        else if (data[i] != other.data[i]) return true;

    u32* larger = size > other.size ? data : other.data;
    u32 larger_size = size > other.size ? size : other.size;
    for (; i < larger_size / 32; i ++) if (larger[i]) return true;
    
    return false;
}

bitset::const_iterator::const_iterator(const bitset& b_in, u32 i_in): b(b_in), i(i_in) {}

u32 bitset::const_iterator::operator*() const {
    return i;
}

bool bitset::const_iterator::operator==(const bitset::const_iterator& other) const {
    return &b == &other.b && i == other.i;
}

bool bitset::const_iterator::operator!=(const bitset::const_iterator& other) const {
    return &b != &other.b || i != other.i;
}

bitset::const_iterator& bitset::const_iterator::operator--() {
    i --;
    while (i < -1u && !b.contains(i)) i --;
    return *this;
}

bitset::const_iterator& bitset::const_iterator::operator++() {
#ifdef __builtin_ffsll
    u32 old_i = i;
    u32 d = b.data[i / 32] & ~((2 << (i % 32)) - 1); // clear bits below and including starting pos
    if (d) {
        i = (i & -32) + __builtin_ffsll(d) - 1;
        return *this;
    }
    for (u32 j = i / 32 + 1; j < b.size / 32; ++ j) {
        u32 d = b.data[j]; // block at pos
        if (!d) continue;  // all zeroes means we continue
        i = j * 32 + __builtin_ffsll(d) - 1;
        return *this;      // we found the next 1-bit
    }
    i = b.size; // if we got to the end without a 1-bit, go to the end
    return *this;
#else
    i ++;
    while (i < b.size && !b.contains(i)) i ++;
    return *this;
#endif 
}

bitset::const_iterator bitset::begin() const {
    return contains(0) ? const_iterator(*this, 0) : ++const_iterator(*this, 0);
}

bitset::const_iterator bitset::end() const {
    return const_iterator(*this, size);
}