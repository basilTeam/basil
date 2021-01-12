#ifndef BASIL_VECTOR_H
#define BASIL_VECTOR_H

#include "defs.h"
#include "slice.h"

template<typename T>
class vector {
    u8* data;
    u32 _size, _capacity;

    void free(u8* array) {
        T* tptr = (T*)array;
        for (u32 i = 0; i < _size; i ++) tptr[i].~T();
        delete[] array;
    }

    void init(u32 size) {
        _size = 0, _capacity = size;
        data = new u8[_capacity * sizeof(T)];
    }

    void copy(const T* ts, u32 n) {
        _size = 0;
        T* tptr = (T*)data;
        for (u32 i = 0; i < n; i ++) {
            new(tptr + i) T(ts[i]);
            ++ _size;
        }
    }

    void destruct(u32 i) {
        T* tptr = (T*)data;
        tptr[i].~T();
    }

    void grow() {
        u8* old = data;
        u32 oldsize = _size;
        init(_capacity * 2);
        copy((const T*)old, oldsize);
        free(old);
    }

public:
    vector() {
        init(16);
    }

    vector(const const_slice<T>& init): vector() {
        for (const T& t : init) push(t);
    }

    ~vector() {
        free(data);
    }

    vector(const vector& other) {
        init(other._capacity);
        copy((const T*)other.data, other._size);
    }

    vector& operator=(const vector& other) {
        if (this != &other) {
            free(data);
            init(other._capacity);
            copy((const T*)other.data, other._size);
        }
        return *this;
    }

    void push(const T& t) {
        while (_size + 1 >= _capacity) grow();
        T* tptr = (T*)data;
        new(tptr + _size) T(t);
        ++ _size;
    }
    
    void pop() {
        -- _size;
        destruct(_size);
    }

    void clear() {
        for (u32 i = 0; i < _size; ++ i) destruct(i);
        _size = 0;
    }

    const T& operator[](u32 i) const {
        return ((T*)data)[i];
    }

    T& operator[](u32 i) {
        return ((T*)data)[i];
    }

    const_slice<T> operator[](pair<u32, u32> range) const {
        return { range.second - range.first, (T*)data + range.first };
    }

    slice<T> operator[](pair<u32, u32> range) {
        return { range.second - range.first, (T*)data + range.first };
    }

    const T* begin() const {
        return (const T*)data;
    }

    T* begin() {
        return (T*)data;
    }

    const T* end() const {
        return (const T*)data + _size;
    }

    T* end() {
        return (T*)data + _size;
    }

    u32 size() const {
        return _size;
    }

    u32 capacity() const {
        return _capacity;
    }

    const T& front() const { 
        return *begin();
    }

    T& front() { 
        return *begin();
    }

    const T& back() const { 
        return *(end() - 1);
    }

    T& back() { 
        return *(end() - 1);
    }
};

template<typename T>
void fill_vector(vector<T>& v) {
    //
}

template<typename T>
void fill_vector(vector<T>& v, const T& t) {
    v.push(t);
}

template<typename T, typename ...Args>
void fill_vector(vector<T>& v, const T& t, const Args&... args) {
    v.push(t);
    fill_vector(v, args...);
}

template<typename T, typename ...Args>
vector<T> vector_of(const Args&... args) {
    vector<T> v;
    fill_vector(v, args...);
    return v;
}

#endif