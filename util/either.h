#ifndef BASIL_EITHER_H
#define BASIL_EITHER_H

#include "defs.h"
#include "io.h"

template<typename T, typename U>
struct either {
    union {
        u8 left[sizeof(T)];
        u8 right[sizeof(U)];
    } data;
    bool _right;

    either(const T& l): _right(false) {
        new (data.left) T(l); // copy in
    }

    either(const U& r): _right(true) {
        new (data.right) U(r);
    }

    ~either() {
        if (_right) (*(U*)data.right).~U();
        else (*(T*)data.left).~T();
    }

    either(const either& other): _right(other._right) {
        if (_right) new(data.right) U(*(U*)other.data.right);
        else new(data.left) T(*(T*)other.data.left);
    }

    either& operator=(const either& other) {
        if (this != &other) {
            if (_right) (*(U*)data.right).~U();
            else (*(T*)data.left).~T();
            _right = other._right;
            if (_right) new(data.right) U(*(U*)other.data.right);
            else new(data.left) T(*(T*)other.data.left);
        }
        return *this;
    }

    bool is_left() const {
        return !_right;
    }

    bool is_right() const {
        return _right;
    }

    template<typename V>
    bool is() const;

    template<>
    bool is<T>() const {
        return is_left();
    }

    template<>
    bool is<U>() const {
        return is_right();
    }

    const T& left() const {
        if (_right) panic("Attempted to read left value of right-containing either!");
        return *(const T*)data.left;
    }

    T& left() {
        if (_right) panic("Attempted to read left value of right-containing either!");
        return *(T*)data.left;
    }

    const U& right() const {
        if (!_right) panic("Attempted to read right value of left-containing either!");
        return *(const U*)data.right;
    }

    U& right() {
        if (!_right) panic("Attempted to read right value of left-containing either!");
        return *(U*)data.right;
    }
};

template<typename T, typename U>
void write(stream& io, const either<T, U>& e) {
    if (e.is_left()) write(io, e.left());
    else write(io, e.right());
}

#endif