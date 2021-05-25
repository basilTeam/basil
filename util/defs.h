#ifndef BASIL_DEFS_H
#define BASIL_DEFS_H

#include "stdint.h"
#include "utils.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// ubuntu and mac color codes, from 
// https://stackoverflow.com/questions/9158150/colored-output-in-c/
#define RESET   "\033[0m"
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

// hash.h

template<typename T>
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

template<typename T>
class vector;

#endif