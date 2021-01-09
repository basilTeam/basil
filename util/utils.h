#ifndef BASIL_UTILS_H
#define BASIL_UTILS_H

#include "defs.h"

void* operator new[](unsigned long n);
void* operator new(unsigned long n, void* p);
void operator delete[](void* ptr);

#endif