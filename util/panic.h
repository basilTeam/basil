/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_PANIC_H
#define BASIL_PANIC_H

#include "defs.h"
#include "io.h"

#ifdef BASIL_RELEASE
#define panic(...)
#else
#define panic(...) internal_panic(__FILE__, __LINE__, __VA_ARGS__)
#endif 

void exit_in_a_panic();

template<typename... Args>
void internal_panic(const char* file, int line, Args... args) {
    println((const char*)"");
    println((const char*)"[", file, (const char*)":", line, (const char*)" - ", BOLDRED, (const char*)"PANIC!", RESET, (const char*)"] ", BOLDRED, args..., RESET);
    println((const char*)"");
    println((const char*)"A panic indicates some kind of internal compiler error occurred. ");
    println((const char*)"If you came across this and aren't implementing the compiler, please"); 
    println((const char*)"consider reporting it!");

    exit_in_a_panic();
}

#endif