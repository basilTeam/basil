#ifndef BASIL_RC_H
#define BASIL_RC_H

#include "defs.h"

class RC {
  u64 _count;
public:
  RC();
  virtual ~RC();
  void inc();
  void dec();
};

enum NullType { NULL_VALUE };

template<typename T>
class ref {
  struct BoxedValue {
    u64 count;
    T data;
  }* _value;

  ref(NullType): _value(nullptr) {}
public:
  static constexpr ref null();
  template<typename... Args>
  ref(Args... args): _value(new BoxedValue{1, T(args...)}) {}
  
  ~ref() {
    if (_value && !--_value->count) delete _value;
  }
  
  ref(const ref& other): _value(other._value) {
    if (other._value) other._value->count ++;
  }

  ref& operator=(const ref& other) {
    if (other._value) other._value->count ++;
    if (_value && !--_value->count) delete _value;
    _value = other._value;
		return *this;
  }

  const T& operator*() const {
    return _value->data;
  }

  T& operator*() {
    return _value->data;
  }

  const T* operator->() const {
    return &_value->data;
  }

  T* operator->() {
    return &_value->data;
  }

  operator bool() const {
    return _value;
  }
};

template<typename T>
constexpr ref<T> ref<T>::null() {
  return ref(NULL_VALUE);
}

#endif