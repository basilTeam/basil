/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_UTF8_H
#define BASIL_UTF8_H

#include "defs.h"
#include "panic.h"

// representation of a single unicode character
struct rune {
    uint32_t u;

    rune();
    rune(uint32_t u);
    operator uint32_t() const;
    operator uint32_t&();
};

typedef enum UnicodeCategory_t {
    UNICODE_INVALID = 0,
    UNICODE_CONTROL = 1,
    UNICODE_FORMAT = 2,
    UNICODE_NOT_ASSIGNED = 3,
    UNICODE_PRIVATE_USE = 4,
    UNICODE_SURROGATE = 5,
    UNICODE_CASED_LETTER = 6,
    UNICODE_LOWERCASE_LETTER = 7,
    UNICODE_LETTER_MODIFIER = 8,
    UNICODE_OTHER_LETTER = 9,
    UNICODE_TITLECASE_LETTER = 10,
    UNICODE_UPPERCASE_LETTER = 11,
    UNICODE_SPACING_COMBINING_MARK = 12,
    UNICODE_ENCLOSING_MARK = 13,
    UNICODE_NONSPACING_MARK = 14,
    UNICODE_DECIMAL_NUMBER = 15,
    UNICODE_LETTER_NUMBER = 16,
    UNICODE_OTHER_NUMBER = 17,
    UNICODE_PUNCTUATION_CONNECTOR = 18,
    UNICODE_PUNCTUATION_DASH = 19,
    UNICODE_PUNCTUATION_CLOSE = 20,
    UNICODE_PUNCTUATION_FINAL_QUOTE = 21,
    UNICODE_PUNCTUATION_INITIAL_QUOTE = 22,
    UNICODE_PUNCTUATION_OTHER = 23,
    UNICODE_PUNCTUATION_OPEN = 24,
    UNICODE_CURRENCY_SYMBOL = 25,
    UNICODE_MODIFIER_SYMBOL = 26,
    UNICODE_MATH_SYMBOL = 27,
    UNICODE_OTHER_SYMBOL = 28,
    UNICODE_LINE_SEPARATOR = 29,
    UNICODE_PARAGRAPH_SEPARATOR = 30,
    UNICODE_SPACE_SEPARATOR = 31
} UnicodeCategory;

typedef enum UnicodeError_t {
    NO_ERROR = 0,
    INCORRECT_FORMAT,
    RAN_OUT_OF_BOUNDS,
    BUFFER_TOO_SMALL,
    INVALID_RUNE
} UnicodeError;

// returns the most recent internal error, or NO_ERROR if there was none.
UnicodeError unicode_error();

// moves a pointer forward by one rune
const char* utf8_forward(const char* str);

// moves a pointer backward by one rune
const char* utf8_backward(const char* str);

// moves a pointer forward by one rune, writing the rune to out
const char* utf8_decode_forward(const char* str, rune* out);

// moves a pointer backward by one rune, writing the rune to out
const char* utf8_decode_backward(const char* str, rune* out);

// returns the length of str in runes
unsigned long int utf8_length(const char* str, unsigned long int str_length);

// decodes str_length bytes from str into runes, and writes up to out_length runes to out.
// returns the number of runes written to out.
unsigned long int utf8_decode(const char* str, unsigned long int str_length, 
                              rune* out, unsigned long int out_length);

// encodes str_length runes from str, writing up to out_length bytes to out.
// returns the number of bytes written to out.
unsigned long int utf8_encode(const rune* str, unsigned long int str_length,
                              char* out, unsigned long int out_length);

// lexicographically compares strings a and b
// returns comparison result
long int utf8_compare(const char* a, unsigned long int a_length,
                      const char* b, unsigned long int b_length);

// these are todo for now. probably not too hard, right? :-)
unsigned long int utf16_length(const char* str, unsigned long int str_length);
unsigned long int utf16_decode(const char* str, unsigned long int str_length, 
                               rune* out, unsigned long int out_length);
unsigned long int utf16_encode(const rune* str, unsigned long int str_length,
                               char* out, unsigned long int out_length);

// returns the decimal digit value of the provided rune. sets error if rune is not a digit
int utf8_digit_value(rune r);

int utf8_is_other(rune r);
int utf8_is_control(rune r);
int utf8_is_format(rune r);
int utf8_is_not_assigned(rune r);
int utf8_is_private_use(rune r);
int utf8_is_surrogate(rune r);

int utf8_is_letter(rune r);
int utf8_is_cased_letter(rune r);
int utf8_is_lowercase(rune r);
int utf8_is_letter_modifier(rune r);
int utf8_is_other_letter(rune r);
int utf8_is_titlecase(rune r);
int utf8_is_uppercase(rune r);

int utf8_is_mark(rune r);
int utf8_is_spacing_combining_mark(rune r);
int utf8_is_enclosing_mark(rune r);
int utf8_is_nonspacing_mark(rune r);

int utf8_is_number(rune r);
int utf8_is_digit(rune r);
int utf8_is_letter_number(rune r);
int utf8_is_other_number(rune r);

int utf8_is_punctuation(rune r);
int utf8_is_connector(rune r);
int utf8_is_dash(rune r);
int utf8_is_punctuation_close(rune r);
int utf8_is_final_quote(rune r);
int utf8_is_initial_quote(rune r);
int utf8_is_other_punctuation(rune r);
int utf8_is_punctuation_open(rune r);

int utf8_is_symbol(rune r);
int utf8_is_currency_symbol(rune r);
int utf8_is_modifier_symbol(rune r);
int utf8_is_math_symbol(rune r);
int utf8_is_other_symbol(rune r);

int utf8_is_separator(rune r);
int utf8_is_line_separator(rune r);
int utf8_is_paragraph_separator(rune r);
int utf8_is_space_separator(rune r);

#endif