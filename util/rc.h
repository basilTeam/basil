#ifndef BASIL_RC_H
#define BASIL_RC_H

#include "defs.h"
#include "io.h"

template<typename T>
class rc final {
  inline void inc() const {
    if (_data) (*(u64*)_data) ++;
  }

  inline void dec() {
    if (_data) if (!(-- *(u64*)_data)) {
      value()->~T();
      delete[] _data;
    }
  }

  inline T* value() {
    return (T*)(_data + sizeof(u64));
  }

  inline const T* value() const {
    return (const T*)(_data + sizeof(u64));
  }

public:
  struct Copy { uint8_t* ptr; };
  uint8_t* _data;

  // Signifies that we're copying from an existing refcell.
  rc(Copy copy): _data(copy.ptr) { inc(); }
  rc(): _data(nullptr) {}

  rc(const T& t): _data(new uint8_t[sizeof(u64) + sizeof(T)]) {
    *(u64*)_data = 1;
    new(value()) T(t); // copy value into place
  }

  rc(decltype(nullptr) null): rc() {}
  
  ~rc() {
    dec();
  }
  
  rc(const rc& other): _data(other._data) {
    inc();
  }

  rc(rc&& other): _data(other._data) { other._data = nullptr; }

  rc& operator=(const rc& other) {
    if (!is(other)) {
      other.inc();
      dec();
      _data = other._data;
    }
		return *this;
  }

  rc& operator=(rc&& other) {
    if (!is(other)) {
      dec();
      _data = other._data;
      other._data = nullptr;
    }
    return *this;
  }

  const T& operator*() const {
    if (!_data) panic("Attempted to dereference null refcell!");
    return *value();
  }

  T& operator*() {
    if (!_data) panic("Attempted to dereference null refcell!");
    return *value();
  }

  const T* operator->() const {
    if (!_data) panic("Attempted to dereference null refcell!");
    return value();
  }

  T* operator->() {
    if (!_data) panic("Attempted to dereference null refcell!");
    return value();
  }

  operator bool() const {
    return _data;
  }

  // Reference equality between two refcells.
  bool is(const rc& other) const {
    return _data == other._data;
  }

  T* raw() {
    if (!_data) return nullptr;
    return value();
  }

  u64 count() const {
    return _data ? *(u64*)_data : 0;
  }

  void manual_dec() {
    if (_data) -- *(u64*)_data;
  }

  const T* raw() const {
    if (!_data) return nullptr;
    return value();
  }

  template<typename U>
  operator rc<U>() const {
    return rc<U>(typename rc<U>::Copy{_data});
  }
};

template<typename T>
rc<T> ref(const T& t) {
  return rc<T>(t);
}

template<typename T, typename... Args>
rc<T> ref(Args&&... args) {
  return rc<T>(T(args...));
}

template<typename T>
void write(stream& io, const rc<T>& r) {
  if (r) write(io, "RC(", *r, ")");
  else write(io, "null");
}

// funkytown
// 
// #define constructor(name) name##_data
// #define destructor(name) ~name##_data

// #define boxed1(name) name##_data; \
// struct name : public rc<name##_data> { \
// public: \
//   name(typename rc<name##_data>::data* ptr): rc<name##_data>(ptr) {} \
//   name(): rc<name##_data>() {} \
//   name(name##_data* t): rc<name##_data>(t) {} \
//   name(decltype(nullptr) null): rc<name##_data>() {} \
//   template<typename ...Args> \
//   explicit name(Args... args): rc<name##_data>(new name##_data(args...)) {} \
// }; \
// struct name##_data 

// #define boxed2(name, base) name##_data; \
// struct name : public base { \
// public: \
//   name(typename rc<name##_data>::data* ptr): base((typename rc<base##_data>::data*)ptr) {} \
//   name(): base() {} \
//   name(name##_data* t): base((base##_data*)t) {} \
//   name(decltype(nullptr) null): base() {} \
//   template<typename ...Args> \
//   name(Args... args): base((base##_data*)new name##_data(args...)) {} \
//   operator rc<base##_data>::data*() { \
//     return (typename rc<base##_data>::data*)_data; \
//   } \
// }; \
// struct name##_data : public base##_data 

// #define __get_macro(_1, _2, NAME, ...) NAME
// #define boxed(...) __get_macro(__VA_ARGS__, boxed2, boxed1)(__VA_ARGS__)

#endif