/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_HASH_H
#define BASIL_HASH_H

#include "defs.h"
#include "str.h"
#include "ustr.h"
#include "slice.h"
#include "io.h"
#include "panic.h"

template<typename T>
bool equals(const T& a, const T& b) {
    return a == b;
}

template<typename T>
bool key_equals(const T& a, const T& b) {
    return a.first == b.first;
}

u64 rotl(u64 u, u64 n);
u64 rotr(u64 u, u64 n);

u64 raw_hash(const void* t, uint64_t size);

template<typename T>
u64 raw_hash(const T& t) {
    return raw_hash(&t, sizeof(T));
}

template<typename T>
u64 hash(const T& t);

template<>
u64 hash(const char* const& s);

template<>
u64 hash(const string& s);

template<>
u64 hash(const ustring& s);

template<>
u64 hash(const u32& u);

template<>
u64 hash(const u64& u);

template<typename T>
u64 key_hash(const T& a) {
    return ::hash(a.first);
}

// Open-addressed general-purpose hash table based on robin-hood hashing, with a 
// fixed stack-allocated buffer of N elements.
template<typename T, int N>
class set {
    enum bucket_status : u8 {
        EMPTY, GHOST, FILLED
    };

    struct bucket {
        u8 data[sizeof(T)];
        bucket_status status;

        bucket(): status(EMPTY) {
            //
        }

        ~bucket() {
            if (status == FILLED) (*(T*)data).~T();
        }

        bucket(const bucket& other): status(other.status) {
            if (status == FILLED) new(data) T(*(T*)other.data);
        }

        bucket& operator=(const bucket& other) {
            if (this != &other) {
                evict();
                status = other.status;
                if (status == FILLED) new(data) T(*(T*)other.data);
            }
            return *this;
        }

        inline const T& value() const {
            return *(const T*)data;
        }

        inline T& value() {
            return *(T*)data;
        }

        inline void fill(const T& value) {
            if (status != FILLED) {
                status = FILLED, new(data) T(value);
            }
            else this->value() = value;
        }

        inline void evict() {
            if (status == FILLED) {
                (*(T*)data).~T();
                status = GHOST;
            }
        }

        inline void clear() {
            if (status == FILLED) (*(T*)data).~T();
            status = EMPTY;
        }
    };

    bucket* data;
    u32 _size, _capacity, _mask;
    bool (*equals)(const T&, const T&);
    u64 (*hash)(const T&);
    bucket fixed[N];

    void init(u32 size) {
        _size = 0, _capacity = size;
        data = _capacity <= N ? fixed : new bucket[size];
    }

    void swap(T& a, T& b) {
        T t = a;
        a = b;
        b = t;
    }

    void free() {
        if (_capacity > N) delete[] data;
    }

    void copy(const bucket* bs) {
        for (u32 i = 0; i < _capacity; ++ i) {
            data[i] = bs[i];
        }
    }

    void grow() {
        bucket* old = data;
        u32 oldsize = _capacity;
        init(_capacity * 2);
        _mask = (_mask << 1) | 1;
        for (u32 i = 0; i < oldsize; ++ i) {
            if (old[i].status == FILLED) insert(old[i].value());
        }
        if (oldsize > N) delete[] old;
    }

public:
    set(bool (*equals_in)(const T&, const T&) = ::equals,
        u64 (*hash_in)(const T&) = ::hash<T>): 
        equals(equals_in), hash(hash_in) {
        init(8);
        _mask = 7;
    }

    ~set() {
        free();
    }

    set(const set& other): set(other.equals, other.hash) {
        init(other._capacity);
        _size = other._size, _mask = other._mask;
        copy(other.data);
    }

    set& operator=(const set& other) {
        if (this != &other) {
            free();
            init(other._capacity);
            hash = other.hash, equals = other.equals;
            _size = other._size, _mask = other._mask;
            copy(other.data);
        }
        return *this;
    }

    class const_iterator {
        const bucket *ptr, *end;
        friend class set;
    public:
        const_iterator(const bucket* ptr_in, const bucket* end_in): 
            ptr(ptr_in), end(end_in) {
            //
        }

        const T& operator*() const {
            return ptr->value();
        }

        const T* operator->() const {
            return &(ptr->value());
        }

        const_iterator& operator++() {
            if (ptr != end) ++ ptr;
            while (ptr != end && ptr->status != FILLED) ++ ptr;
            return *this;
        }

        const_iterator operator++(int) {
            iterator it = *this;
            operator++();
            return it;
        }

        bool operator==(const const_iterator& other) const {
            return ptr == other.ptr;
        }

        bool operator!=(const const_iterator& other) const {
            return ptr != other.ptr;
        }
    };

    class iterator {
        bucket *ptr, *end;
        friend class set;
    public:
        iterator(bucket* ptr_in, bucket* end_in): ptr(ptr_in), end(end_in) {
            //
        }

        T& operator*() {
            return ptr->value();
        }

        T* operator->() {
            return &(ptr->value());
        }

        iterator& operator++() {
            if (ptr != end) ++ ptr;
            while (ptr != end && ptr->status != FILLED) ++ ptr;
            return *this;
        }

        iterator operator++(int) {
            iterator it = *this;
            operator++();
            return it;
        }

        bool operator==(const iterator& other) const {
            return ptr == other.ptr;
        }

        bool operator!=(const iterator& other) const {
            return ptr != other.ptr;
        }

        operator const_iterator() const {
            return const_iterator(ptr, end);
        }
    };

    iterator begin() {
        bucket *start = data, *end = data + _capacity;
        while (start != end && start->status != FILLED) ++ start;
        return iterator(start, end);
    }

    const_iterator begin() const {
        const bucket *start = data, *end = data + _capacity;
        while (start != end && start->status != FILLED) ++ start;
        return const_iterator(start, end);
    }

    iterator end() {
        return iterator(data + _capacity, data + _capacity);
    }

    const_iterator end() const {
        return const_iterator(data + _capacity, data + _capacity);
    }

    void clear() {
        for (u64 i = 0; i < _capacity; i ++) {
            if (data[i].status == FILLED) data[i].clear();
        }
        _size = 0;
    }

    void insert(const T& t) {
        if (double(_size + 1) / double(_capacity) > 0.625) grow();
        u64 h = hash(t);
        u64 dist = 0;
        u64 i = h & _mask;
        T item = t;
        while (true) {
            if (data[i].status == EMPTY || data[i].status == GHOST) {
                data[i].fill(item);
                ++ _size;
                return;
            }

            if (data[i].status == FILLED && equals(data[i].value(), item)) {
                data[i].value() = t; // potentially redundant assignment allows for overwriting existing values
                return;
            }

            u64 other_dist = (i - (hash(data[i].value()) & _mask)) & _mask;
            if (other_dist < dist) {
                if (data[i].status == GHOST) {
                    data[i].fill(item);
                    ++ _size;
                    return;
                }
                swap(item, data[i].value());
                dist = other_dist;
            }
            i = (i + 1) & _mask;
            ++ dist;
        }
    }

    void erase(const T& t) {
        u64 h = hash(t);
        u64 i = h & _mask;
        while (true) {
            if (data[i].status == EMPTY) return;
            if (data[i].status == FILLED && equals(data[i].value(), t)) {
                data[i].evict();
                -- _size;
                return;
            }
            i = (i + 1) & _mask;
        }
    }

    const_iterator find(const T& t) const {
        u64 h = hash(t);
        u64 i = h & _mask;
        while (true) {
            if (data[i].status == EMPTY) return end();
            if (data[i].status != GHOST) {
                u64 dist = (i - h) & _mask;
                u64 oh = hash(data[i].value());
                u64 other_dist = (i - (oh & _mask)) & _mask;
                if (other_dist < dist) return end();
            }
            if (data[i].status == FILLED && equals(data[i].value(), t)) {
                return const_iterator(data + i, data + _capacity);
            }
            i = (i + 1) & _mask;
        }
    }

    iterator find(const T& t) {
        u64 h = hash(t);
        u64 i = h & _mask;
        while (true) {
            if (data[i].status == EMPTY) return end();
            if (data[i].status != GHOST) {
                u64 dist = (i - h) & _mask;
                u64 oh = hash(data[i].value());
                u64 other_dist = (i - (oh & _mask)) & _mask;
                if (other_dist < dist) return end();
            }
            if (data[i].status == FILLED && equals(data[i].value(), t)) {
                return iterator(data + i, data + _capacity);
            }
            i = (i + 1) & _mask;
        }
    }

    bool contains(const T& t) const {
        return find(t) != end();
    }

    u32 size() const {
        return _size;
    }

    u32 capacity() const {
        return _capacity;
    }
};

// Key-value map backed by a hashset.
template<typename K, typename V>
class map : public set<pair<K, V>> {
public:
    map(): set<pair<K, V>>(::key_equals<pair<K, V>>, ::key_hash<pair<K, V>>) {
        //
    }

    void put(const K& key, const V& value) {
        set<pair<K, V>>::insert({ key, value });
    }

    void erase(const K& key) {
        set<pair<K, V>>::erase({ key, V() });
    }

    V& operator[](const K& key) {
        pair<K, V> dummy = { key, V() };
        auto it = set<pair<K, V>>::find(dummy);
        if (it == set<pair<K, V>>::end()) {
            set<pair<K, V>>::insert(dummy);
        }
        return set<pair<K, V>>::find(dummy)->second;
    }

    const V& operator[](const K& key) const {
        pair<K, V> dummy = { key, V() };
        auto it = set<pair<K, V>>::find(dummy);
        if (it == set<pair<K, V>>::end()) {
            panic("Tried to access nonexistent map key!");
        }
        return set<pair<K, V>>::find(dummy)->second;
    }

    typename set<pair<K, V>>::const_iterator find(const K& key) const {
        return set<pair<K, V>>::find({ key, V() });
    }

    typename set<pair<K, V>>::iterator find(const K& key) {
        return set<pair<K, V>>::find({ key, V() });
    }

    bool contains(const K& key) const {
        return set<pair<K, V>>::contains({ key, V() });
    }
};

template<typename T>
void fill_set(set<T>& s) {
    //
}

template<typename T>
void fill_set(set<T>& s, const T& t) {
    s.insert(t);
}

template<typename T, typename ...Args>
void fill_set(set<T>& s, const T& t, const Args&... args) {
    s.insert(t);
    fill_set(s, args...);
}

template<typename T, typename ...Args>
set<T> set_of(const Args&... args) {
    set<T> s;
    fill_set(s, args...);
    return s;
}

template<typename K, typename V>
void fill_map(map<K, V>& m) {
    //
}

template<typename K, typename V>
void fill_map(map<K, V>& m, const K& k, const V& v) {
    m.put(k, v);
}

template<typename K, typename V, typename ...Args>
void fill_map(map<K, V>& m, const K& k, const V& v, const Args&... args) {
    m.put(k, v);
    fill_map(m, args...);
}

template<typename K, typename V, typename ...Args>
map<K, V> map_of(const Args&... args) {
    map<K, V> m;
    fill_map(m, args...);
    return m;
}

#endif
