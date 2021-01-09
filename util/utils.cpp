#include "utils.h"
#include "stdlib.h"

void* operator new[](unsigned long n) {
    return malloc(n);
}

void* operator new(unsigned long n, void* p) {
    return p;
}

void operator delete[](void* ptr) {
    free(ptr);
}