# Contributing to the Basil compiler.

## Table of Contents

### 1 - Source Directory

#### 1.1 - `compiler/`

#### 1.2 - `example/`

#### 1.3 - `jasmine/`

#### 1.4 - `runtime/`

#### 1.5 - `test/`

#### 1.6 - `util/`

### 2 - Programming Practices

#### 2.1 - Naming Conventions

#### 2.2 - Paradigms and Code Organization

#### 2.3 - File Structure

#### 2.4 - Comments and Documentation

#### 2.5 - Safety and Errors

#### 2.6 - Programming for Performance

#### 2.7 - Dependencies

### 3 - Contribution Process

#### 3.1 - Content Requirements

#### 3.2 - Testing Requirements

#### 3.3 - Contribution Workflow

#### 3.4 - Approval and Release Process

---

## 1 - Source Directory

The Basil compiler is organized in a fairly flat structure. Top-level directories are used to categorize **major components** of the compiler. For instance, `jasmine/` - the assembler/loader Basil uses - is kept separate from `compiler/` - the compiler frontend and IR. In general, we avoid adding more directories to the top level to keep the project's organization simplistic.

The current top-level directories and their contents are as follows:

| Directory | Contents |
|---|---|
| `compiler/` | Contains the bulk of the compiler. Includes the lexer and parser, type and value representations, compile-time evaluation, the typed Basil AST, and code generation. |
| `example/` | Contains example programs written in Basil. |
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

### 1.2 - `example/`

The `example/` directory contains a bunch of small examples of different Basil language features, and that's about all there is to it. Just about any working example is fair game for this folder, but we'd like to show some restraint so we only add things that show off new features or large useful programs.

### 1.3 - `jasmine/`

The `jasmine/` directory implements the lowest level of the Basil compiler, implementing assemblers for all targetable instruction sets, an abstract object file format, and export to OS-dependent object file formats. It is organized into the following files: 

| File | Contents |
|---|---|
| `obj.h`/`obj.cpp` | Implements the `Object` type, a generic object file representation that contains machine code. These objects are produced at the end of Basil compilation, and individual functions within them may be retrieved and called just-in-time. Additionally, the `Object` type defines proper translation behavior to target other native object file formats such as ELF. |
| `sym.h`/`sym.cpp` | Defines the `Symbol` type, which represents a unique label within an object with a local or global linkage. |
| `target.h` | Enumerates different target architectures and operating systems. Also defines a default target architecture - the architecture the compiler itself was compiled for. |
| `utils.h`/`utils.cpp` | Contains utilities used in other components of Jasmine, mostly to do with endianness and allocating executable memory. |
| `x64.h`/`x64.cpp` | Assembler for the x86_64 architecture. Defines types to represent x86_64 instructions and arguments, and writes machine code to an output buffer. |

### 1.4 - `runtime/`

The `runtime/` directory implements the Basil core library - all the underlying code necessary to implement Basil's built-in functions. While the Basil compiler is generally meant to have few dependencies, the core library should have _no_ dynamic dependencies whatsoever. Ultimately, the files here are compiled, then linked statically with the object file Basil outputs when compiling a binary ahead-of-time. So, we'd like to keep these small and simple. 

`runtime/` is organized into the following files:

| File | Contents |
|---|---|
| `sys.h`/`sys.cpp` | Defines procedure wrappers for any system calls Basil needs to depend on, as well as some utility functions for IO. |
| `core.cpp` | Implements any Basil built-ins not directly handled in code generation as C++ procedures. All procedures defined here must have `extern "C"` linkage. |

### 1.5 - `test/`

The `test/` directory contains any and all unit tests defined for components of the Basil compiler. The top-level files `test.h` and `test.cpp` define some utility functions and macros for creating unit tests.

Besides the top-level `test.h`/`test.cpp`, every other file in `test/` must be within an appropriate subdirectory. The organization of these files mirror that of the top-level project structure. If we wanted to test `compiler/eval.cpp`, for instance, we would put our test in `test/compiler/eval.cpp`. At the moment, we only create one test file for each ordinary source file - additional tests for `compiler/eval.cpp` should be appended to the existing tests, if they exist. It is permissible to define additional files specifically if a test depends on them - for example, a small Basil source file that a test could load and execute. These should go in the same directory as the test(s) that make use of them. Overall though, to keep the number of files down, loading string constants from within a test's source is preferable to loading reference code from separate files.

### 1.6 - `util/`

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

## 2 - Programming Practices

We aren't super strict about how compiler development should be done. In general, as long as you roughly try and follow the existing style, you should be okay - we won't split hairs about formatting or code logic too aggressively.

But, we do have a few recommendations for how to structure code, and how to organize your contributions so that they can be integrated cleanly and maintainably into the rest of the codebase. It'd be much appreciated if you at least look them over before committing for the first time!

### 2.1 - Naming Conventions

We try to use consistent casing:

 * Procedures and variables are generally written in `snake_case`, whether they are class members or top-level.

 * Type names should be written in `PascalCase`.

 * Macro constants, global variables, and static member variables should be written in `CONSTANT_CASE`.

In all cases, casing consistency is _especially_ important if you're adding definitions to a header. 

For types, we try and keep the names for commonly-used types quite short: `Type`, `Value`, `Env`. Abbreviating a longer name is acceptable, but should be avoided unless it both:

 * _Significantly_ reduces the length of a long but frequently-used name.

 * Is still just about as clear as using the longer name.

For example, we shorten `Environment` to `Env`, because it is roughly 70% shorter _and_ still pretty unambiguous. We _don't_ shorten `Value` to `Val`, however, since it doesn't give us much of a gain in terms of brevity. And we don't shorten something like `Intersect` to `Inter` or `Int`, since it could lead to ambiguity with other common programming terms.

Another guideline for type names: if a lot of types share a common descriptor (e.g. `TupleClass`, `ArrayClass`, etc. are all `Class`es), make it a suffix instead of a prefix. So use `TupleClass` instead of something like `ClassTuple`.

Finally, in certain places, we use common prefixes to denote the domain to which certain functions belong. For example, most functions that construct or operate over values begin with `v_`, and most type-related functions begin with `t_`. Try and use the appropriate prefix, if one exists, for whatever function you are writing.

### 2.2 - Paradigms and Code Organization

Overarchingly, Basil's code is written in a procedural style - we prefer to use fairly flat hierarchies of types and a lot of top-level functions over an object-oriented structure.

When it comes to functions, try and keep the public interface of a compilation unit fairly stringent. Each function you make available to other sources should have a clear, useful purpose that it is individually responsible for handling. It should not be left up to the caller, in another source, to set up necessary state or know which function to call first. Stateful, complex, mutually-recursive functions should be kept as much as possible to the implementation. Even within the implementation, try and make sure each function you add has a single clear responsibility.

One potential exception to this rule manifests in terms of testing. A complex internal function might be made accessible so that it can be tested more directly (rather than relying on another function to call it, and test it indirectly).

When it comes to types, try and keep things simple. Most of the types used throughout the compiler are simple structs. In most cases, we leave all members public, and for small structs we avoid adding explicit constructors. This is to avoid object-oriented cruft such as getters and setters that would pollute our headers. If you want to prevent mutation, pass the struct around as a const reference, or (preferably) hide the state using some additional functions.

We also avoid inheritance in _most_ cases. But it's a reasonable approach if it fits the job. For example, if we want to represent an AST node or a type, inheritance can be a natural solution since we might have an unbounded number of different kinds of nodes or types that all implement a shared interface. In these cases, to the greatest possible extent, hide the actual inheritance hierarchy within the implementation. For instance, the Basil type system is implemented internally with type inheritance, but the interface is defined entirely in terms of a space-efficient `Type` handle. This avoids polluting the Basil namespace with complex and specialized types that are unlikely to be used elsewhere.

In general, though, stick to simple functions and transparent structs whenever possible.

### 2.3 - File Structure

The compiler's code structure is intentionally quite flat, and we'd like to keep it that way.

In general, a header/source file pair should contain a unique, distinct component of the compiler. We use files to organize _systems_, not individual classes or functions, even if said classes or functions are fairly large and complex.

So, if you're adding a feature, try and keep it in whatever the most relevant file is. New type kinds should be kept within `type.h`/`type.cpp`, new operations on values should be in `value.h`/`value.cpp`, and new builtin functions should be kept in `builtin.h`/`builtin.cpp`. Try to only add new source files if you are truly introducing a new system that does not fall under the purview of any existing file.

This isn't the _strictest_ rule in the world - there are probably some existing files that are quite small or could be reasonably considered part of a different system. Try to be sparing and use your best judgement. Do _not_, however, introduce new subdirectories. This complicates the hierarchy of the codebase, and we'd like to avoid it until it is seriously necessary.

### 2.4 - Comments and Documentation

There aren't formal requirements for comments and documentation, but we do have some guidelines. Let's go through them in order of importance!

The _most_ important place to document is in the header. This is the public-facing interface of whatever component you are modifying, and is the first place programmers are likely to look when investigating what a function/type/variable does. Unless a function or type's name basically describes its entire functionality, a comment describing the precise purpose of it should be written immediately prior. We generally use a series of line comments: 

```cpp
// We try and stick to '//' just to keep things a little more simple. Makes 
// it clear even without syntax highlighting that we have a comment on our
// hands.
void example_function(int x, int y);
```

Avoid using a comment to simply restate obvious information from the function's signature - don't just say that a function takes an int, or returns a bool. Instead, describe the returned _value_, and especially how it depends on the provided arguments.

For complex functions, longer comments are encouraged. Try and describe the nuances in using the function, where it should be used, where it shouldn't be used, etc. Explain to the reader how the function fits into the compiler overall, so they can get a better understanding of its overarching purpose.

All of this pertains to declarations in the _header_. In implementation files, commenting is a lot more touch-and-go. We don't duplicate comments between header and source. And in general, we don't add comments before function definitions, unless they're only defined within the implementation and it's hard to figure out the function's purpose from its body.

Instead, we encourage occasional in-line comments within function implementations. If the purpose of some code is fairly clear at a glance, it probably doesn't need a comment. But if you're implementing complex logic, peppering in comments for each major step can be helpful for readers trying to understand the underlying algorithm. If your function's logic depends on magic numbers, or non-obvious value relationships, it is good practice to mention those explicitly in comments as well:

```cpp
int find_max(int array[], int n) {
    int max = -1; // we know the array only has positive numbers, so
                  // -1 should be a safe initial value
    for (int i = 0; i < n; i ++) {
        if (array[n] > max) max = array[n]; // update max if we find a 
                                            // larger element
    }
    return max;
}
```

### 2.5 - Safety and Errors

Basil is implemented in C++, which of course allows some pretty dangerous things. Our goal is to avoid undefined behavior in the compiler, to eliminate opaque crashes or other strange problems.

Within containers (largely within `util/`), clearly erroneous behavior should be checked for whenever possible, and result in a `panic()` with an appropriate message. Examples of clearly erroneous behavior include out-of-bounds reads of a vector, or dereferencing a null `rc`.

On a somewhat related note, prefer using containers over primitive constructs. Raw pointers and C-style arrays should be avoided, unless you are _quite_ confident they will be used safely. For example, `const char*` might be an acceptable parameter type for a function specifically designed to operate on string literals, or a C-style array might be appropriate for an internal, immutable lookup table. If you want to allocate things on the heap, though, containers and `rc` are best practice.

Within the compiler itself, even if undefined behavior is usually covered by the underlying container implementations, it's generally good practice to assert function requirements and `panic()` if they aren't met. Be careful not to `panic()` too liberally - a type error resulting from a malformed Basil source file, for instance, should result in a compiler error and not immediately crash the compiler itself. `panic()` is mostly useful when an internal compiler function _must_ be called with certain arguments from elsewhere in the compiler - for example, `t_tuple_at()` must be called on a tuple type, and panics if this is not the case. It's the caller's responsibility in this case to ensure that `t_tuple_at()` is only invoked on the right type.

Besides panics, the Basil compiler may also report compiler errors, which should not immediately stop the compiler's progress. These errors are usually only reported after a major compilation phase completes - for example, a tokenization error will not stop the tokenization process, and should instead be reported before entering the parsing phase.

It's a little tricky to figure out what the compiler should do after an error occurs, in some cases. Try to use your best judgement! In tokenization or parsing, we might simply consume the erroneous character or token and resume immediately after it. 

When evaluating, we use the special "error" value to represent that an error occurred, and propagate "error" through the code whenever it is used. Make sure you return an error value if your built-in function encounters a problem, for instance. Or, if you're implementing some kind of special evaluation process outside of normal function application, make sure to check each of the parameters explicitly and return an error if any of them are errors.

### 2.6 - Programming for Performance

At the moment, we aren't trying to hyper-optimize the Basil compiler. But it's useful to use a "performance mindset" during implementation. On a broad level, try and be cognizant of where you are invoking the allocator, where you might be copying a large amount of data, or where you might be calling an expensive function repeatedly.

Idiomatic C++ rules apply. If the object you're passing is large, or has an expensive copy constructor, prefer passing by reference. Default to constant references.

Try and align `struct`s reasonably, and keep their size as small as possible. In general, try and keep your type to the smallest number of words you can. The difference between a 30-byte and 32-byte type is not likely to be very significant, but going from a 16-byte to 24-byte type might matter a bit more (especially if the type is used frequently). If you can fit your data within a single 64-bit word, do that.

Implementing move constructors for frequently-used types that contain internal allocations or `rc` pointers can be useful. Many `util/` containers implement move constructors and move assignment already - and `Value` does too.

### 2.7 - Dependencies

Avoid any external dependencies. External libraries are almost never welcome, at least not without considerable discussion first. We might consider, for instance, an external SSL library for an integrated package manager - implementing that from scratch would almost certainly be the wrong move. But this is a highly exceptional case.

In particular, Basil's compiler is designed to only depend on `libc`. This does _not_ include `libstdc++`. Do not include definitions from the C++ standard library. Use the various types and procedures in `util/` instead. One consequence of this is that we do not support the use of exceptions within the compiler - prefer `panic` for fatal errors, and the use of `optional` or error value propagation for recoverable errors.

This might seem like a bit of a silly rule, but there are a few particular goals in mind here.

First, we want to make sure the Basil compiler is portable and easy to build - external libraries would make it harder to compile the compiler from scratch on different systems. 

Second, we also want to keep the compiler lightweight. Lots of compilers have hefty runtime requirements and huge binaries which make it relatively difficult to, say, embed them in another program. Keeping the compiler small leaves this door open for Basil.

## 3 - Contribution Process

Contributing to the compiler is open to all. But, we have a few requirements and process guidelines we'd like you to follow before we accept your code.

### 3.1 - Content Requirements

We'd like contributions to the compiler itself to be _non-trivial_. Creating a pull request for a "one second fix" is discouraged. We define "one second" as something that is near-trivial to describe and implement, for example: 

 * A quick name-change to a variable.
 
 * Adding a `const` or `&` to a parameter name.
 
 * Adding or expanding a comment.
 
 * Rearranging the contents of a source file.
 
 * Removing unused code.

 * Adding a missing null-check or comparable condition.

This isn't to say we don't want to hear about these, especially if you find one that could lead to compiler crashes or undefined behavior! But a pull request for these types of issues is complete overkill - just reporting it as a GitHub issue, or a personal message somewhere a maintainer will see it should suffice.

An exception to this would be for a large-scale patch that fixes many of the above issues. For example, adding detailed documentation to an entire file that was previously lacking it is a meaningful change. Additionally, small fixes made _along with_ a larger, more meaningful change are welcome.

This also isn't a super hard and fast rule, we are open to discussion. Just think about it before submitting a PR.

### 3.2 - Testing Requirements

Before submitting a pull request, you should make sure that your modified implementation still passes every unit test on your local machine. You can check this by running `make clean test` (`make test` will often suffice, but `clean` ensures a fresh build).

In addition, we have a somewhat unique rule: all pull requests _must_ add at least one new unit test to the suite! This is good practice for all projects, of course, but we're enforcing it. For contributions that intend to fix an error in the compiler, adding a minimal test that previously errored but now passes would be best. For new features, write a new test that covers a reasonably large subset of the features you introduced, and make sure it passes before you submit.

### 3.3 - Contribution Workflow

We use a _forking workflow_ for the Basil compiler. If you'd like to make a change, please:

 * Fork the compiler repository.

 * Implement and commit your changes.

 * Make sure you pass all the unit tests!

 * Submit a pull request to the most recent version's branch.

Regarding that last step, look for the branch with the highest version number. Do _not_ PR directly to master!

### 3.4 - Approval and Release Process

Describe the changes you made as clearly as possible:

 * If you're fixing an issue, explain the previous issue (and link any relevant issue reports!), as well as how you solved it. 
 
 * If you're proposing a change to compiler behavior, explain the weaknesses in the prior approach (if applicable), and explain the strengths of your new approach.

 * If you're proposing a new feature, explain how the new feature is useful, explain any weaknesses in the existing approach that it addresses, and briefly discuss any existing code your change breaks or weaknesses in your new feature.

Approval of a PR is at the discretion of the project maintainers, so we'll be the ones looking it over and making the final call.

Despite all of our recommended best practices, it is _unlikely_ that we will flat out reject a PR due to style issues or it being "too small". For style issues, we'd probably request you make your consistent before approval. But in general, as long as your contribution improves the compiler, we appreciate it!

The most likely thing to hold up a PR would be potentially buggy or expensive behavior. We'll review your changes and if anything sticks out as problematic, let you know what it is and recommend you change it. Probably not going to lead to the PR being rejected either, ultimately, as long as you fix it up.

The most likely thing to cause a PR to be _rejected_ would be significant changes to the language's semantics or the compiler's internal structure. Major changes like these require a lot of discussion, and ultimately we (the maintainers) are in charge of assessing whether or not a feature "fits" Basil. To minimize the chances that you spend time implementing a feature, only to have it rejected for not "fitting in", discuss it with us beforehand. Make an appropriately tagged issue for it, or discuss it with the maintainers on some community channel.

Anyway, if your PR _is_ accepted, wonderful! It'll likely be included in the next released version of the compiler. This is a little bit predicated on how far-reaching the change is - fixing bugs or non-breaking changes to the compiler are easy to work in. But breaking compiler or language changes could be delayed to a major version increment.