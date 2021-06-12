#ifndef BASIL_UTILS_H
#define BASIL_UTILS_H

#include "defs.h"

void* operator new(unsigned long n);
void* operator new[](unsigned long n);
void* operator new(unsigned long n, void* p);
void operator delete(void* ptr);
void operator delete[](void* ptr);

#ifdef BASIL_RELEASE
#define panic(msg)
#else
#define panic(msg) __internal_panic(__FILE__, __LINE__, msg)
#endif 
void __internal_panic(const char* file, int line, const char* msg);

#endif