<br>
<p align=center><img src="logo.png" width=100vw></p>

# The Basil Programming Language

```rb
println "Welcome to Basil!"

def greetings = ["Hello" "Nice to meet you" "Salutations"]

def greet name? greeting? =
    println greeting + ", " + name + "!"

println "What's your name?"
greet (read String) greetings[1]
```

<p align=center><sub>For more examples, see the <code>example/</code> directory located in this project's root!</sub></p>

### Basil is a _fast_ and _flexible_ language for expressing _complex problems_ in _natural terms_ without compromising _readability_, _simplicity_, or _performance_.

### ***Featuring...****
 * A novel ***context-sensitive parser*** that allows seamless manipulation of language syntax.
 * Homoiconicity, supporting ***Lisp-style metaprogramming*** via quotations and `eval`.
 * A "***first-class everything***" approach - Basil has no keywords, and almost no rigid syntax, so even primitive types and operations can be extended and manipulated.
 * A ***static, structural type system*** that permits expressive type-level programming. 
 * Evaluation is ***compile-time by default***, with the compiler capable of evaluating arbitrary Basil code.
 * ***Partial evaluation*** allows the compiler to "lower" expensive or effectful code, compiling it to efficient native code instead of evaluating it ahead-of-time.
 * Our ***home-grown compiler backend*** compiles Basil code quickly, and applies competitive optimizations.
 * Finally, the whole compiler and runtime fits in ***under a megabyte*** and depends only on libc. 

<p align=center><sub>*Note: Basil is <i><b>highly WIP</b></i>! The language is unstable, and these claims may or may not apply on all platforms or for all applications.</sub></p>

---

## **Installation**

Currently, we only support building the Basil compiler from source. You'll need a **C++17-conformant** C++ compiler, a **Python 2.7** or **Python 3** interpreter, and maybe a bit of
resourcefulness...

```sh
$ git clone https://github.com/basilTeam/basil
$ ./build.py --help             # lists all build options (compiler to use, additional flags, etc)
$ ./build.py basil-release
$ build/basil help
```

Basil's language runtime can be compiled separately, as either a statically or dynamically linked library.

```sh
$ ./build.py librt-static       # to build a statically-linked library
$ ./build.py librt-dynamic      # to build a dynamically-linked library
```

---

## **Supported Platforms**

Operating Systems:
 - [x] Linux
 - [x] Windows
 - [x] MacOS

Architectures:
 - [x] x86_64
 - [ ] AArch64
 - [ ] RISC-V
 - [ ] LLVM
 - [ ] WASM

---

## **License**

Basil is distributed under the [3-Clause BSD License](https://opensource.org/licenses/BSD-3-Clause). See [LICENSE](https://github.com/basilTeam/basil/blob/master/LICENSE) for details.