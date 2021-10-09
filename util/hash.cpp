/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "hash.h"

u64 rotl(u64 u, u64 n) {
	return (u << n) | ((u >> (64 - n)) & ~(-1 << n));
}

u64 rotr(u64 u, u64 n) {
	return (u >> n) | ((u << (64 - n)) & (-1 & ~(-1 << n)));
}

// MurmurHash, 64-bit version, unaligned by Austin Appleby (https://sites.google.com/site/murmurhash/)
// The source has been slightly modified, using basil typedefs, and a fixed seed (a 64-bit prime).

u64 raw_hash(const void* input, u64 size){
	const u64 m = 0xc6a4a7935bd1e995;
	const u32 r = 47;

	u64 h = 7576351903513440497ul ^ (size * m);

	const u64* data = (const u64*)input;
	const u64* end = data + (size / 8);

	while(data != end) {
		u64 k = *data ++;
		k *= m; 
		k ^= k >> r; 
		k *= m; 
		h ^= k;
		h *= m; 
	}

	const u8* data2 = (const u8*)data;
	switch(size & 7) {
	case 7: h ^= u64(data2[6]) << 48;
	case 6: h ^= u64(data2[5]) << 40;
	case 5: h ^= u64(data2[4]) << 32;
	case 4: h ^= u64(data2[3]) << 24;
	case 3: h ^= u64(data2[2]) << 16;
	case 2: h ^= u64(data2[1]) << 8;
	case 1: h ^= u64(data2[0]);
	        h *= m;
	};
 
	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
} 

template<>
u64 hash(const char* const& s) {
    u32 size = 0;
    const char* sptr = s;
    while (*sptr) ++ sptr, ++ size;
    return raw_hash((const u8*)s, size);
}

template<>
u64 hash(const string& s) {
    return raw_hash((const u8*)s.raw(), s.size());
}

template<>
u64 hash(const ustring& s) {
    return raw_hash((const u8*)s.raw(), s.bytes());
}