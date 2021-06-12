# Contributing to the Basil compiler.

## Table of Contents

### 1 - Source Directory

#### 1.1 - `compiler/`

#### 1.2 - `jasmine/`

#### 1.3 - `runtime/`

#### 1.4 - `test/`

#### 1.5 - `util/`

---

## 1 - Source Directory

The Basil compiler is organized in a fairly flat structure. Top-level directories are used to categorize **major components** of the compiler. For instance, `jasmine/` - the assembler/loader Basil uses - is kept separate from `compiler/` - the compiler frontend and IR. In general, we avoid adding more directories to the top level to keep the project's organization simplistic.

The current top-level directories and their contents are as follows:

| Directory | Contents |
|---|---|
| `compiler/` | Contains the bulk of the compiler. Includes the lexer and parser, type and value representations, compile-time evaluation, the typed Basil AST, and code generation. |
| `jasmine/` | Contains the x86 assembler and loader used by Basil for lightweight JIT execution. Also handles export to different object file formats. |
| `runtime/` | Contains the Basil runtime, including the garbage collector, built-in function implementations, and syscall bindings. |
| `test/` | Contains unit tests for all of Basil's components, as well as the unit testing framework the tests use. |
| `util/` | Contains utility functions and data structures used to implement the Basil compiler. |

### 1.1 - `compiler/`

The `compiler/` directory includes the implementations of all core Basil compiler features, both frontend (parsing, typechecking) and backend (code generation). In general, most changes to the compiler fall within this directory's contents. It is organized into the following files:

| File | Contents |
|---|---|
| `builtin.h`/`builtin.cpp` | The definitions for all built-in functions and macros included in the Basil language, as well as some associated types. |
| `driver.h`/`driver.cpp` | A collection of functions that direct compilation of Basil source, executing one or more compiler phases in correct sequence. If you want to take some text and run it through the compiler, this is the interface you should look at. |
| `env.h`/`env.cpp` | Defines the `Env` type - a Basil environment, which handles variable bindings - as well as associated functions. |
| `errors.h`/`errors.cpp` | Handles compiler errors of all kinds, including functions for reporting and printing errors. If a program contains ill-formed code and you'd like to let the programmer know about it, report it here. |
| `eval.h`/`eval.cpp` | Implements core compile-time evaluation logic. This includes (in order): resolving forms, expanding aliases and splices, expanding macros, and applying functions. |
| `forms.h`/`forms.cpp` | Defines the `Form` type, which represents how a Basil value is meant to be applied to nearby arguments during context-sensitive parsing. The state machines different forms use to match input are implemented in these files, as well as functions for constructing different kinds of forms. |
| `parse.h`/`parse.cpp` | Handles context-free parsing, transforming a list of tokens into a structured Basil value. Most Basil "syntax" is context-sensitive, but things like indentation and parentheses are handled here. |
| `source.h`/`source.cpp` | Defines an abstraction for source files, as well as a representation of source positions. Basil code must first be loaded into a `Source` before it can be tokenized, and all Basil tokens and values have associated source locations. |
| `token.h`/`token.cpp` | The Basil tokenizer, splitting a source file into smaller pieces. New tokens and their parsing should be added in these files. |
| `type.h`/`type.cpp` | Defines and implements the Basil type system. Provides the `Type` type, several primitive types, a selection of functions for constructing types, and several other functions for manipulating types (such as reading their members). In the source file, all of Basil's type equality and coercion rules are implemented. Additionally, the `Symbol` type and its associated functions are defined here. |
| `value.h`/`value.cpp` | Implements the `Value` type, a representation of any Basil compile-time value. These files provide functions for constructing and manipulating values. |

### 1.2 - `jasmine/`

The `jasmine/` directory implements the lowest level of the Basil compiler, implementing assemblers for all targetable instruction sets, an abstract object file format, and export to OS-dependent object file formats. It is organized into the following files: 

| File | Contents |
|---|---|
| `obj.h`/`obj.cpp` | Implements the `Object` type, a generic object file representation that contains machine code. These objects are produced at the end of Basil compilation, and individual functions within them may be retrieved and called just-in-time. Additionally, the `Object` type defines proper translation behavior to target other native object file formats such as ELF. |
| `sym.h`/`sym.cpp` | Defines the `Symbol` type, which represents a unique label within an object with a local or global linkage. |
| `target.h` | Enumerates different target architectures and operating systems. Also defines a default target architecture - the architecture the compiler itself was compiled for. |
| `utils.h`/`utils.cpp` | Contains utilities used in other components of Jasmine, mostly to do with endianness and allocating executable memory. |
| `x64.h`/`x64.cpp` | Assembler for the x86_64 architecture. Defines types to represent x86_64 instructions and arguments, and writes machine code to an output buffer. |

### 1.3 - `runtime/`

The `runtime/` directory implements the Basil core library - all the underlying code necessary to implement Basil's built-in functions. While the Basil compiler is generally meant to have few dependencies, the core library should have _no_ dynamic dependencies whatsoever. Ultimately, the files here are compiled, then linked statically with the object file Basil outputs when compiling a binary ahead-of-time. So, we'd like to keep these small and simple. 

`runtime/` is organized into the following files:

| File | Contents |
|---|---|
| `sys.h`/`sys.cpp` | Defines procedure wrappers for any system calls Basil needs to depend on, as well as some utility functions for IO. |
| `core.cpp` | Implements any Basil built-ins not directly handled in code generation as C++ procedures. All procedures defined here must have `extern "C"` linkage. |

### 1.4 - `test/`

The `test/` directory contains any and all unit tests defined for components of the Basil compiler. The top-level files `test.h` and `test.cpp` define some utility functions and macros for creating unit tests.

Besides the top-level `test.h`/`test.cpp`, every other file in `test/` must be within an appropriate subdirectory. The organization of these files mirror that of the top-level project structure. If we wanted to test `compiler/eval.cpp`, for instance, we would put our test in `test/compiler/eval.cpp`. At the moment, we only create one test file for each ordinary source file - additional tests for `compiler/eval.cpp` should be appended to the existing tests, if they exist. It is permissible to define additional files specifically if a test depends on them - for example, a small Basil source file that a test could load and execute. These should go in the same directory as the test(s) that make use of them. Overall though, to keep the number of files down, loading string constants from within a test's source is preferable to loading reference code from separate files.

### 1.5 - `util/`

The `util/` directory contains standalone implementations for many of the data structures and utility functions provided by the C++ standard library. This allows us to avoid the `libstdc++` dependency, add extra safety checks to data structure operations (to avoid opaque compiler crashes), and cut out unnecessary features/properties to better serve the compiler's purposes. In general, stick to these data structures when implementing other Basil components, and try not to add to their interfaces unless truly necessary. If you find a bug or performance issue in any of these utilities, however, we welcome suggestions for how to improve!

The `util/` directory is organized into the following files:

| File | Contents |
|---|---|
| `defs.h` | Miscellaneous typedefs and macro constants shared throughout the Basil compiler. Perhaps most notably, we define types like `u64` here from their `stdint` counterparts. |
| `hash.h`/`hash.cpp` | Implementations for set and map data structures. These are implemented as hash tables, using robin-hood hashing. These files also define a standard hash function used by default by these data structures (although custom hash functions _can_ be provided, try to stick to the default and simply implement it for new types as needed). |
| `io.h`/`io.cpp` | A collection of IO related functions (such as `println`, a variadic output function) and data structures (such as `stream`, which represents an abstract input and/or output stream). Most of the functions here boil down to `write` and `read` operations on `stream`s, so if you'd like to implement custom input/output behavior for a new type, simply overload these functions with said type. |
| `option.h` | Defines a simple optional type, representing either a value or the absence of one. `optional` is specialized for reference and `rc<T>` types to have minimal overhead. |
| `rc.h` | Defines a reference-counted "smart pointer" type, `rc<T>`. |
| `slice.h` | A simple "slice" type, representing a subsection of a larger contiguous data structure like a vector or string. |
| `str.h`/`str.cpp` | Defines `string`, a resizeable, random-access string type representing any number of bytes. `string` is notably not aware of any particular encoding, such as UTF-8. |
| `ustr.h`/`ustr.cpp` | Defines `ustring`, a UTF-8 encoded string type. `ustring` is resizeable, and bidirectionally iterable, but not random-access. Since it's aware of UTF-8 though, its elements are unicode characters, not individual bytes, and properties such as its size are defined in unicode terms. |
| `utf8.h`/`utf8.cpp` | Contains numerous procedures for manipulating and inspecting unicode characters. Notably implements UTF-8 decoding and encoding, as well as lookup tables for determining unicode class information for individual characters. |
| `utils.h`/`utils.cpp` | A few miscellaneous utility functions used throughout the rest of the codebase. Most notably contains `panic()`, used to report internal compiler errors. |
| `vec.h` | A resizeable, random-access homogeneous container. |
