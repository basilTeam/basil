#include "utils.h"
#include "stdlib.h"
#include "io.h"
// #ifdef __GNUC__
// #include "execinfo.h"
// #endif

void* operator new(unsigned long n) {
    return malloc(n);
}

void* operator new[](unsigned long n) {
    return malloc(n);
}

void* operator new(unsigned long n, void* p) {
    return p;
}

void operator delete(void* ptr) {
    free(ptr);
}

void operator delete[](void* ptr) {
    free(ptr);
}

void __internal_panic(const char* file, int line, const char* msg) {
    println("");
    println("[", file, ":", line, " - ", BOLDRED, "PANIC!", RESET, "] ", BOLDRED, msg, RESET);
    println("");
    println("A panic indicates some kind of internal compiler error occurred. ");
    println("If you came across this and aren't implementing the compiler, please"); 
    println("consider reporting it!");
    // #ifdef __GNUC__
    // println("");
    // println("Backtrace:");
    // void *trace[10];
    // size_t size = backtrace(trace, 10);
    // backtrace_symbols_fd(trace, size, 1); // stdout
    // #endif
    exit(1);
}

void __cxa_pure_virtual() {
    panic("Tried to invoke pure virtual function!");
}