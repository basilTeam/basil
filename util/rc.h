#ifndef BASIL_RC_H
#define BASIL_RC_H

#include "defs.h"
#include "stddef.h"

class RC {
  u64 _count;
public:
  RC();
  virtual ~RC();
  void inc();
  void dec();
};

template<typename T>
class ref {
public:
  u64* _count;
  T* _data;

  ref(): _count(nullptr), _data(nullptr) {}
  ref(T* t): _count(new u64(1)), _data(t) {}
  ref(decltype(nullptr) null): ref() {}
  virtual ~ref() {
    if (_data && !--*_count) {
      delete _data;
      delete _count;
    }
  }
  
  ref(const ref& other): _count(other._count), _data(other._data) {
    if (_data) ++ *_count;
  }

  ref& operator=(const ref& other) {
    if (other._data) ++ *(u64*)other._count;
    if (_data && !--*_count) {
      delete _data;
      delete _count;
    }
    _data = other._data;
    _count = other._count;
		return *this;
  }

  const T& operator*() const {
    return *_data;
  }

  T& operator*() {
    return *_data;
  }

  const T* operator->() const {
    return _data;
  }

  T* operator->() {
    return _data;
  }

  operator bool() const {
    return _data;
  }

  template<typename U>
  operator ref<U>() {
    ref<U> copy;
    if (_data) {
      copy._data = (U*)_data;
      copy._count = _count;
      ++ *copy._count;
    }
    return copy;
  }
};

template<typename T, typename... Args>
ref<T> newref(Args... args) {
  return ref<T>(new T(args...));
}

#endif