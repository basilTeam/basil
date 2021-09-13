#ifndef BASIL_UTILS_H
#define BASIL_UTILS_H

#include "defs.h"

void* operator new(unsigned long n);
void* operator new[](unsigned long n);
void* operator new(unsigned long n, void* p);
void operator delete(void* ptr);
void operator delete[](void* ptr);

#define move(x) static_cast<decltype(x)&&>(x)

#ifdef BASIL_RELEASE
#define panic(...)
#else
#define panic(...) internal_panic(__FILE__, __LINE__, __VA_ARGS__)
#endif 

void exit_in_a_panic();

template<typename... Args> 
extern void println(const Args&... args);

template<typename... Args>
void internal_panic(const char* file, int line, Args... args) {
    println("");
    println("[", file, ":", line, " - ", BOLDRED, "PANIC!", RESET, "] ", BOLDRED, args..., RESET);
    println("");
    println("A panic indicates some kind of internal compiler error occurred. ");
    println("If you came across this and aren't implementing the compiler, please"); 
    println("consider reporting it!");

    exit_in_a_panic();
}

// We need this so we can avoid depending on libc++.
extern "C" void __cxa_pure_virtual();

#endif