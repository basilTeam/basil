# Utilities

All of the following was implemented before the jam. None of Basil's
primary functionality is contained within these files, they simply
provide an independent implementation of classes in the C++ standard
library.

Most of these files are taken from [https://github.com/elucent/collections](https://github.com/elucent/collections).

| File | Contents |
|---|---|
| `defs.h` | A few shared typedefs and forward declarations. | 
| `hash.h/cpp` | A standard polymorphic hash function, hash set, and hash map based on robin hood probing. |
| `io.h/cpp` | A suite of variadic io functions and a stream abstraction, which is implemented both by a file wrapper class and an in-memory buffer. |
| `slice.h` | A slice type, for representing subranges of strings and containers. |
| `str.h/cpp` | A resizable byte string type. implements small string optimization and only allocates for strings greater than 15 characters. |
| `rc.h/cpp` | A reference-counted smart pointer type and an embeddable  reference-counting interface for other classes. |
| `vec.h` | A resizable vector type. |