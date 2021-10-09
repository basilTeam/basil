# **The Jasmine Project**

## **What is Jasmine?**

Jasmine is a low-level, platform-independent instruction set intended to provide an efficient,
portable native backend to compiler projects.

***Warning: Jasmine is currently experimental!*** Most of the claims here are backed up by real benchmarks, but they may not apply in all cases, and Jasmine is still lacking a lot of features. Use at your own risk!

```rs
msg:    lit [u8 * 20] "Hello from Jasmine!\0"
len:    lit u32       20

greet:  frame
        syscall %0, write(i32 1, ptr msg, u32 [len])
        ret
```

---

## **Installation**

Currently, Jasmine is only available as a source library or command-line utility.

To use Jasmine in your compiler, your best option is to clone this repository and include the sources directly:

```sh
# jasmine is enclosed within the larger basil repository, so
# it's currently best to clone the whole thing
$ git clone https://github.com/basilTeam/basil
$ cp -r jasmine/ util/ <your-project>
$ ... # add jasmine/*.cpp and util/*.cpp to your build sources
```

To build Jasmine's command-line driver from source, make sure you have a C++17-conforming C++ compiler.

```sh
$ git clone https://github.com/basilTeam/basil
$ make CXX=<C++ compiler, default=clang++> jasmine-release
$ build/jasmine --help
```

We'll be adding a more convenient single header, along with static and dynamic Jasmine libraries, at some point in the future.

---

## Supported Platforms

Operating Systems:
 - [x] Linux
 - [ ] Windows
 - [ ] MacOS

Planned Architectures:
 - [x] x86_64
 - [ ] AArch64
 - [ ] RISC-V
 - [ ] LLVM
 - [ ] WASM

---

## **Why Jasmine?**

### **Uncompromisingly Fast**

Like similar libraries in this domain, Jasmine abstracts out certain machine-level
requirements, allowing unlimited virtual registers and multiple constants/memory parameters
per instruction. Jasmine instructions are also fully serializable, allowing the distribution
of portable Jasmine binaries. 

Unlike projects such as LLVM or even WASM, however, Jasmine
is much closer to assembly language. This means Jasmine compiles to native extremely 
quickly (~300,000 instructions/s on my machine), while still applying competitive low-level optimizations (register allocation, instruction selection, etc).

It does also mean that Jasmine isn't really a good fit for high-level optimizations. It's recommended you use your own mid-level IR that lowers to Jasmine, instead of compiling a high-level language to Jasmine directly.

```sh
# example jasmine compilation speed, as of October 9th, 2021
$ wc -l test.jasm   # 100,000 fibonacci functions
1000000
$ time jasmine -a text.jasm -o asm.jo

real    0m1.717s    # assembling textual IR at ~600,000 insns/s
user    0m1.457s
sys     0m0.250s
$ time jasmine -c asm.jo -o x86.jo

real    0m3.557s    # compiling IR to x86 at ~300,000 insns/s
user    0m2.777s
sys     0m0.749s
$ time jasmine -R x86.jo -o elf.o

real    0m1.357s    # exporting to ELF at ~700,000 insns/s
user    0m1.227s
sys     0m0.131s
$ file elf.o
elf.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

### **Lightweight**

Jasmine is really small! The current x86_64 release binary is only about 100kB, and depends only on libc and libm. As we add additional backends for different platforms, we do expect this size to increase. But we'll also be looking into making Jasmine more modular, so you can cut out anything you don't want or need.

```sh
# jasmine build, using clang++ 10.0.1, as of October 9th, 2021
$ make clean jasmine-release 
...
$ du -h build/jasmine
104k    build/jasmine
$ ldd build/jasmine
    linux-vdso.so.1 (0x00007fff1dfd1000)
    libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6(0x00007f2f06951000)
    libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6(0x00007f2f06802000)
    /lib64/ld-linux-x86-64.so.2 (0x00007f2f06b51000)
```

### **Portable**

Jasmine instructions are fully serializable and deserializable, and can be distributed in a portable Jasmine object file format. Jasmine objects can also be exported to native object files, and Jasmine bytecode can be retargeted to any native architecture or object file Jasmine supports!

```sh
$ jasmine -a -o example.job
foo:    frame       # assembling instructions from stdin
        local i64 %0
        mov i64 %0, 1
        ret i64 %0
$ file example.job  # jasmine object, contains jasmine bytecode
example.job: data

$ jasmine -R example.job -o example.o
$ file example.o    # ELF object, contains native code
example.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

### **Flexible CLI**

Jasmine's command-line driver allows for easy incremental and partial compilation of Jasmine sources, and works seamlessly with Unix pipes and streams.

```sh
# jasmine -a: assemble jasmine instructions from stdin
# jasmine -c: compile bytecode to native
# jasmine -R: export native object from jasmine object
$ jasmine -a | jasmine -c | jasmine -R > example.o
foo:    frame
        local i64 %0
        mov i64 %0, 1
        ret i64 %0
$ file example.o
example.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

### **Simple API**

All of Jasmine's assemblers, including for its bytecode format, have easy-to-use procedural interfaces in C++! 

```cpp
#include "jasmine/x64.h"
// compiling a simple x86_64 function and running it
void example() {
    using namespace jasmine;
    using namespace x64;

    Object obj({X86_64, DEFAULT_OS});    // x86_64 target
    writeto(obj);                        // use obj as output

    label(global("foo"));                // foo:
    mov(r32(RAX), imm(1));               //     mov eax, 1
    add(r32(RAX), imm(2));               //     add eax, 2 
    ret();                               //     ret

    obj.load();                          // load executable
    void* foo = obj.find(global("foo")); // find symbol "foo"
    auto fn = (int(*)())foo;             // cast to function type
    printf("%d\n", fn());                // prints 3
}
```

### **Customizable**

Compilers using Jasmine can easily define their own meta-instructions for different platforms, with full access to Jasmine's internal assemblers!

(this is currently WIP)

---

## **License**

Jasmine is distributed as part of the larger Basil project, under the [3-Clause BSD License](https://opensource.org/licenses/BSD-3-Clause). See [LICENSE](https://github.com/basilTeam/basil/LICENSE) for details.