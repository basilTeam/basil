#ifndef BASIL_SLICE_H
#define BASIL_SLICE_H

#include "defs.h"

template<typename T, typename U>
struct pair {
    T first;
    U second;

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
        return _data[i];
    }

    const_slice operator[](pair<u32, u32> range) const {
        return { range.second - range.first, _data + range.first };
    }

    u32 size() const {
        return _size;
    }

    const T& front() const {
        return _data[0];
    }

    const T& back() const {
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
        return _data[i];
    }

    T& operator[](u32 i) {
        return _data[i];
    }

    const_slice<T> operator[](pair<u32, u32> range) const {
        return { range.second - range.first, _data + range.first };
    }

    slice operator[](pair<u32, u32> range) {
        return { range.second - range.first, _data + range.first };
    }

    u32 size() const {
        return _size;
    }

    const T& front() const {
        return _data[0];
    }

    const T& back() const {
        return _data[_size - 1];
    }

    T& front() {
        return _data[0];
    }

    T& back() {
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