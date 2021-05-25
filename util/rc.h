#ifndef BASIL_RC_H
#define BASIL_RC_H

#include "defs.h"
#include "io.h"

template<typename T>
class rc {
public:
  struct data {
    u64 count;
    T* obj;
    ~data() {
      delete obj;
    }
  };
  data* _data;

  rc(data* ptr): _data(ptr) { if (_data) _data->count ++; }
  rc(): _data(nullptr) {}
  rc(T* t): _data(new data{1, t}) {}
  rc(decltype(nullptr) null): rc() {}
  virtual ~rc() {
    if (_data && !--_data->count) {
      delete _data;
    }
  }
  
  rc(const rc& other): _data(other._data) {
    if (_data) ++ _data->count;
  }

  rc& operator=(const rc& other) {
    if (other._data) ++ other._data->count;
    if (_data && !--_data->count) {
      delete _data;
    }
    _data = other._data;
		return *this;
  }

  const T& operator*() const {
    if (!_data) panic("Attempted to dereference null refcell!");
    return *_data->obj;
  }

  T& operator*() {
    if (!_data) panic("Attempted to dereference null refcell!");
    return *_data->obj;
  }

  const T* operator->() const {
    if (!_data) panic("Attempted to dereference null refcell!");
    return _data->obj;
  }

  T* operator->() {
    if (!_data) panic("Attempted to dereference null refcell!");
    return _data->obj;
  }

  operator bool() const {
    return _data;
  }

  bool is(const rc& other) const {
    return _data == other._data;
  }

  bool operator!=(const rc& other) const {
    return _data != other._data;
  }

  T* raw() {
    if (!_data) return nullptr;
    return _data->obj;
  }

  const T* raw() const {
    if (!_data) return nullptr;
    return _data->obj;
  }

  template<typename U>
  operator rc<U>() {
    return rc<U>((typename rc<U>::data*)_data);
  }
};

template<typename T>
rc<T> ref(const T& t) {
  return rc<T>(new T(t));
}

template<typename T, typename... Args>
rc<T> ref(Args&&... args) {
  return rc<T>(new T(args...));
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