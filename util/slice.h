/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_SLICE_H
#define BASIL_SLICE_H

#include "defs.h"
#include "panic.h"

template<typename T, typename U>
struct pair {
    T first;
    U second;

    pair() {}

    pair(const T& _first, const U& _second):
        first(_first), second(_second) {}
    
    bool operator==(const pair& other) const {
        return first == other.first && second == other.second;
    }
};

template<typename T>
class const_slice {
    u32 _size;
    const T* _data;
public:
    const_slice(u32 size, const T* data):
        _size(size), _data(data) {
        //
    }

    const T& operator[](u32 i) const {
        if (i >= _size) panic("Attempted out-of-bounds access of slice element!");
        return _data[i];
    }

    const_slice operator[](pair<u32, u32> range) const {
        if (range.first >= _size || range.second > _size) panic("Attempted out-of-bounds subslice of slice!");
        if (range.second < range.first) panic("Attempted subslice with reversed bounds!");
        return { range.second - range.first, _data + range.first };
    }

    u32 size() const {
        return _size;
    }

    const T& front() const {
        if (_size == 0) panic("Attempted to get first element of empty slice!");
        return _data[0];
    }

    const T& back() const {
        if (_size == 0) panic("Attempted to get last element of empty slice!");
        return _data[_size - 1];
    }

    const T* begin() const {
        return _data;
    }

    const T* end() const {
        return _data + _size;
    }
};

template<typename T>
class slice {
    u32 _size;
    T* _data;
public:
    slice(u32 size, T* data):
        _size(size), _data(data) {
        //
    }

    const T& operator[](u32 i) const {
        if (i >= _size) panic("Attempted out-of-bounds access of slice element!");
        return _data[i];
    }

    T& operator[](u32 i) {
        if (i >= _size) panic("Attempted out-of-bounds access of slice element!");
        return _data[i];
    }

    const_slice<T> operator[](pair<u32, u32> range) const {
        if (range.first >= _size || range.second > _size) panic("Attempted out-of-bounds subslice of slice!");
        if (range.second < range.first) panic("Attempted subslice with reversed bounds!");
        return { range.second - range.first, _data + range.first };
    }

    slice operator[](pair<u32, u32> range) {
        if (range.first >= _size || range.second > _size) panic("Attempted out-of-bounds subslice of slice!");
        if (range.second < range.first) panic("Attempted subslice with reversed bounds!");
        return { range.second - range.first, _data + range.first };
    }

    u32 size() const {
        return _size;
    }

    const T& front() const {
        if (_size == 0) panic("Attempted to get first element of empty slice!");
        return _data[0];
    }

    const T& back() const {
        if (_size == 0) panic("Attempted to get last element of empty slice!");
        return _data[_size - 1];
    }

    T& front() {
        if (_size == 0) panic("Attempted to get first element of empty slice!");
        return _data[0];
    }

    T& back() {
        if (_size == 0) panic("Attempted to get last element of empty slice!");
        return _data[_size - 1];
    }

    const T* begin() const {
        return _data;
    }

    const T* end() const {
        return _data + _size;
    }

    T* begin() {
        return _data;
    }

    T* end() {
        return _data + _size;
    }

    operator const_slice<T>() const {
        return { _size, _data };
    }
};

template<typename T>
const_slice<T> span(const const_slice<T>& a, const const_slice<T>& b) {
    const const_slice<T>& from = a._data < b._data ? a : b;
    const const_slice<T>& to = a._data > b._data ? a : b;
    return { to._size + to._data - from._data, from._data };
}

template<typename T>
slice<T> span(const slice<T>& a, const slice<T>& b) {
    const slice<T>& from = a._data < b._data ? a : b;
    const slice<T>& to = a._data > b._data ? a : b;
    return { to._size + to._data - from._data, from._data };
}

#endif