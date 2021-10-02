#include "utils.h"
#include "stdlib.h"
#include "cxxabi.h"
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

void exit_in_a_panic() {
    exit(1);
}

void __cxa_pure_virtual() {
    panic("Tried to invoke pure virtual function!");
} 