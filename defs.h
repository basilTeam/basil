#ifndef BASIL_DEFS_H
#define BASIL_DEFS_H

#include <cstdint>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// hash.h

template<typename T>
class set;

template<typename K, typename V>
class map;

// io.h

class stream;
class file;
class buffer;

// slice.h

template<typename T, typename U>
struct pair;

template<typename T>
class const_slice;

template<typename T>
class slice;

// str.h

class string;

// utf8.h

struct uchar;
class ustring;

// vec.h

template<typename T>
class vector;

namespace basil {
  struct Definition;
  class Env;

  class Value;
  class ListValue;
  class SumValue;
  class ProductValue;
  class FunctionValue;

  class Type;
  class SingletonType;
  class ListType;
  class SumType;
  class ProductType;
  class FunctionType;
}

#endif