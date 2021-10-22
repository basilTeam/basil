/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_DEFS_H
#define BASIL_DEFS_H

#include "stdint.h"
#include "stddef.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define I64_MIN -9223372036854775808ul
#define I64_MAX 9223372036854775807ul

// ubuntu and mac color codes, from 
// https://stackoverflow.com/questions/9158150/colored-output-in-c/
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define ITALIC  "\033[3m"
#define BLACK   "\033[30m"      /* Black */
#define GRAY   "\033[1;30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */
#define ITALICBLACK 	"\033[3m\033[30m"      /* Italic Black */
#define ITALICRED   	"\033[3m\033[31m"      /* Italic Red */
#define ITALICGREEN 	"\033[3m\033[32m"      /* Italic Green */
#define ITALICYELLOW	"\033[3m\033[33m"      /* Italic Yellow */
#define ITALICBLUE  	"\033[3m\033[34m"      /* Italic Blue */
#define ITALICMAGENT	"\033[3m\033[35m"      /* Italic Magenta */
#define ITALICCYAN  	"\033[3m\033[36m"      /* Italic Cyan */
#define ITALICWHITE 	"\033[3m\033[37m"      /* Italic White */

// OS flags 

#if defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__)
    #define BASIL_X86_64
#elif defined(__i386) || defined(__i386__) || defined(__x86) || defined(__x86__)
    #define BASIL_X86_32
#elif defined(__aarch64) || defined(__aarch64__)
    #define BASIL_AARCH64
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #define BASIL_WINDOWS
#elif defined(__APPLE__) || defined(__MACH__)
    #define BASIL_UNIX
    #define BASIL_MACOS
#elif defined(__linux__)
    #define BASIL_UNIX
    #define BASIL_LINUX
#endif


// hash.h

template<typename T, int N = 8>
class set;

template<typename K, typename V>
class map;

// io.h

class stream;
class file;
class buffer;

// option.h

template<typename T>
struct option;

// slice.h

template<typename T, typename U>
struct pair;

template<typename T>
class const_slice;

template<typename T>
class slice;

// str.h

class string;

// ustr.h

class ustring;

// utf8.h

struct rune;

// vec.h

template<typename T, u32 N = 8>
class vector;

#include "utils.h"

#endif