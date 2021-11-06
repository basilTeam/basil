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
#include "string.h"

void bitset::grow(u32 n) {
    u64 new_capacity = n;
    new_capacity += 64; // round up to nearest 64
    new_capacity /= 64;
    new_capacity *= 64;
    u64* old_data = data;
    data = new u64[new_capacity / 64];
    for (u32 i = 0; i < new_capacity / 64; i ++) data[i] = 0;
    for (u32 i = 0; i < size / 64; i ++) data[i] = old_data[i];
    if (old_data != &local) delete[] old_data;
    size = new_capacity;
}

bitset::bitset(): data(&local), size(64), local(0) {}

bitset::~bitset() {
    if (data != &local) delete[] data;
}

bitset::bitset(const bitset& other): size(other.size) {
    if (size > 64) {
        data = new u64[other.size / 64];
        for (u32 i = 0; i < size; i += 64) data[i / 64] = other.data[i / 64];
    }
    else local = other.local, data = &local;
}

bitset& bitset::operator=(const bitset& other) {
    if (this != &other) {
        if (data != &local) delete[] data;
        size = other.size;
        if (size > 64) {
            data = new u64[other.size / 64];
            for (u32 i = 0; i < size; i += 64) data[i / 64] = other.data[i / 64];
        }
        else local = other.local, data = &local;
    }
    return *this;
}

bool bitset::contains(u32 n) const {
    if (n >= size) return false;
    u64 block = data[n / 64];
    return block & (1ul << n % 64);
}

bool bitset::insert(u32 n) {
    if (n >= size) grow(n);
    u64& block = data[n / 64];
    bool set = block & (1ul << n % 64);
    block |= (1ul << n % 64);
    return !set;
}

bool bitset::erase(u32 n) {
    if (n >= size) grow(n);
    u64& block = data[n / 64];
    bool set = block & (1ul << n % 64);
    block &= ~(1ul << n % 64);
    return set;
}

void bitset::clear() {
    for (u32 i = 0; i < size / 64; i ++) data[i] = 0;
}

bool bitset::operator|=(const bitset& other) {
    bool changed = false;
    if (size < other.size) grow(other.size), changed = true;
    for (u32 i = 0; i < other.size / 64; i ++) 
        changed = (data[i] |= other.data[i]) != data[i] || changed;
    return changed;
}

bool bitset::operator&=(const bitset& other) {
    bool changed = false;
    for (u32 i = 0; i < size / 64; i ++) 
        if (i * 64 >= other.size) data[i] = 0, changed = true;
        else changed = (data[i] &= other.data[i]) != data[i] || changed;
    return changed;
}

bool bitset::operator==(const bitset& other) {
    u32 i = 0, j = 0;
    for (; i < size / 64; i ++) 
        if (i * 64 >= size || j * 64 >= other.size) break;
        else if (data[i] != other.data[i]) return false;

    u64* larger = size > other.size ? data : other.data;
    u32 larger_size = size > other.size ? size : other.size;
    for (; i < larger_size / 64; i ++) if (larger[i]) return false;

    return true;
}

bool bitset::operator!=(const bitset& other) {
    u32 i = 0, j = 0;
    for (; i < size / 64; i ++) 
        if (i * 64 >= size || j * 64 >= other.size) break;
        else if (data[i] != other.data[i]) return true;

    u64* larger = size > other.size ? data : other.data;
    u32 larger_size = size > other.size ? size : other.size;
    for (; i < larger_size / 64; i ++) if (larger[i]) return true;
    
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
    u64 mask = -2ul << i;
    for (u64 j = (i + 1) / 64; j < b.size / 64; j ++) {
        u64 block = b.data[j];
        if (j == i / 64) block &= mask; // clear bits at or below i
        if (block) {
            int r = ffsll(block);
            i = j * 64 + r - 1;
            return *this;
        }
    }
    i = b.size;
    return *this;
}

bitset::const_iterator bitset::begin() const {
    return contains(0) ? const_iterator(*this, 0) : ++const_iterator(*this, 0);
}

bitset::const_iterator bitset::end() const {
    return const_iterator(*this, size);
}