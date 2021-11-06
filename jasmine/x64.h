/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef JASMINE_X64_H
#define JASMINE_X64_H

#include "utils.h"
#include "jobj.h"

namespace jasmine::x64 {
    enum Register : u8 {
        RAX = 0,
        RCX = 1,
        RDX = 2,
        RBX = 3,
        RSP = 4,
        RBP = 5,
        RSI = 6,
        RDI = 7,
        R8 = 8,
        R9 = 9,
        R10 = 10,
        R11 = 11,
        R12 = 12,
        R13 = 13,
        R14 = 14,
        R15 = 15,
        XMM0 = 16,
        XMM1 = 17,
        XMM2 = 18,
        XMM3 = 19,
        XMM4 = 20,
        XMM5 = 21,
        XMM6 = 22,
        XMM7 = 23,
        XMM8 = 24,
        XMM9 = 25,
        XMM10 = 26,
        XMM11 = 27,
        XMM12 = 28,
        XMM13 = 29,
        XMM14 = 30,
        XMM15 = 31,
        INVALID = 255
    };

    extern const char* REGISTER_NAMES[16];

    enum Scale : u8 {
        SCALE1 = 0,
        SCALE2 = 1,
        SCALE4 = 2,
        SCALE8 = 3
    };

    enum Size : u8 {
        BYTE = 0,
        WORD = 1,
        DWORD = 2,
        QWORD = 3,
        AUTO = 4
    };

    enum Condition : u8 {
        OVERFLOW = 0,
        NOT_OVERFLOW = 1,
        BELOW = 2,
        NOT_ABOVE_OR_EQUAL = 2,
        CARRY = 2,
        NOT_BELOW = 3,
        ABOVE_OR_EQUAL = 3,
        NOT_CARRY = 3,
        ZERO = 4,
        EQUAL = 4,
        NOT_ZERO = 5,
        NOT_EQUAL = 5,
        BELOW_OR_EQUAL = 6,
        NOT_ABOVE = 6,
        NOT_BELOW_OR_EQUAL = 7,
        ABOVE = 7,
        SIGN = 8,
        NOT_SIGN = 9,
        PARITY = 10,
        PARITY_EVEN = 10,
        NOT_PARITY = 11,
        PARITY_ODD = 11,
        LESS = 12,
        NOT_GREATER_OR_EQUAL = 12,
        NOT_LESS = 13,
        GREATER_OR_EQUAL = 13,
        LESS_OR_EQUAL = 14,
        NOT_GREATER = 14,
        NOT_LESS_OR_EQUAL = 15,
        GREATER = 15
    };

    struct ScaledRegister {
        Register reg;
        Scale scale;

        ScaledRegister(Register reg_in, Scale scale_in = SCALE1);
    };

    ScaledRegister operator*(Register reg, int scale);

    enum ArgType : u8 {
        REGISTER8 = 0,
        REGISTER16 = 1,
        REGISTER32 = 2,
        REGISTER64 = 3,
        IMM8 = 4,
        IMM16 = 5,
        IMM32 = 6,
        IMM64 = 7,
        REGISTER_LABEL8 = 8,
        REGISTER_LABEL16 = 9,
        REGISTER_LABEL32 = 10,
        REGISTER_LABEL64 = 11,
        REGISTER_OFFSET8 = 12,
        REGISTER_OFFSET16 = 13,
        REGISTER_OFFSET32 = 14,
        REGISTER_OFFSET64 = 15,
        LABEL8 = 16,
        LABEL16 = 17,
        LABEL32 = 18,
        LABEL64 = 19,
        ABSOLUTE8 = 20,
        ABSOLUTE16 = 21,
        ABSOLUTE32 = 22,
        ABSOLUTE64 = 23,
        SCALED_INDEX8 = 24,
        SCALED_INDEX16 = 25,
        SCALED_INDEX32 = 26,
        SCALED_INDEX64 = 27,
				RIPRELATIVE8 = 28,
				RIPRELATIVE16 = 29,
				RIPRELATIVE32 = 30,
				RIPRELATIVE64 = 31,
        IMM_AUTO = 240,
        REGISTER_LABEL_AUTO = 241,
        REGISTER_OFFSET_AUTO = 242,
        LABEL_AUTO = 243,
        ABSOLUTE_AUTO = 244,
        SCALED_INDEX_AUTO = 245,
				RIPRELATIVE_AUTO = 246
    };

    struct Arg {
        union {
            i8 imm8;
            i16 imm16;
            i32 imm32;
            i64 imm64;
            Register reg;
            struct {
                Register base;
                jasmine::Symbol label;
            } register_label;
            struct {
                Register base;
                i64 offset;
            } register_offset;
            struct {
                Register base, index;
                Scale scale;
                i64 offset;
            } scaled_index;
            jasmine::Symbol label;
            i64 absolute;
						i64 rip_relative;
        } data;

        ArgType type;
    };    

    bool operator==(const Arg& lhs, const Arg& rhs);
    bool operator!=(const Arg& lhs, const Arg& rhs);

	bool is_register(ArgType type);
    bool is_immediate(ArgType type);
    bool is_memory(ArgType type);
    bool is_label(ArgType type);
    i64 immediate_value(const Arg& arg);
		
    void writeto(jasmine::Object& obj);

    Arg imm8(i8 value);
    Arg imm16(i16 value);
    Arg imm32(i32 value);
    Arg imm64(i64 value);
    Arg imm(i64 value);

    Arg r8(Register reg);
    Arg r16(Register reg);
    Arg r32(Register reg);
    Arg r64(Register reg);

    Arg m8(Register reg, i64 offset);
    Arg m16(Register reg, i64 offset);
    Arg m32(Register reg, i64 offset);
    Arg m64(Register reg, i64 offset);
    Arg mem(Register reg, i64 offset);
    Arg m8(Register base, Register index, Scale scale, i64 offset);
    Arg m16(Register base, Register index, Scale scale, i64 offset);
    Arg m32(Register base, Register index, Scale scale, i64 offset);
    Arg m64(Register base, Register index, Scale scale, i64 offset);
    Arg mem(Register base, Register index, Scale scale, i64 offset);
    Arg m8(Register base, ScaledRegister index, i64 offset);
    Arg m16(Register base, ScaledRegister index, i64 offset);
    Arg m32(Register base, ScaledRegister index, i64 offset);
    Arg m64(Register base, ScaledRegister index, i64 offset);
    Arg mem(Register base, ScaledRegister index, i64 offset);

    Arg abs8(i64 offset);
    Arg abs16(i64 offset);
    Arg abs32(i64 offset);
    Arg abs64(i64 offset);
    Arg abs(i64 offset);
    
    Arg riprel8(i64 offset);
    Arg riprel16(i64 offset);
    Arg riprel32(i64 offset);
    Arg riprel64(i64 offset);
    Arg riprel(i64 offset);

    Arg label8(jasmine::Symbol symbol);
    Arg label16(jasmine::Symbol symbol);
    Arg label32(jasmine::Symbol symbol);
    Arg label64(jasmine::Symbol symbol);

    void add(const Arg& dest, const Arg& src, Size size = AUTO);
    void or_(const Arg& dest, const Arg& src, Size size = AUTO);
    void adc(const Arg& dest, const Arg& src, Size size = AUTO);
    void sbb(const Arg& dest, const Arg& src, Size size = AUTO);
    void and_(const Arg& dest, const Arg& src, Size size = AUTO);
    void sub(const Arg& dest, const Arg& src, Size size = AUTO);
    void xor_(const Arg& dest, const Arg& src, Size size = AUTO);
    void cmp(const Arg& dest, const Arg& src, Size size = AUTO);
    void mov(const Arg& dest, const Arg& src, Size size = AUTO);
    void movsx(const Arg& dest, const Arg& src, Size size = AUTO);
    void movzx(const Arg& dest, const Arg& src, Size size = AUTO);
    void imul(const Arg& dest, const Arg& src, Size size = AUTO);
    void imul(const Arg& dest, const Arg& lhs, const Arg& rhs, Size size = AUTO);
    void rol(const Arg& dest, const Arg& src, Size size = AUTO);
    void ror(const Arg& dest, const Arg& src, Size size = AUTO);
    void rcl(const Arg& dest, const Arg& src, Size size = AUTO);
    void rcr(const Arg& dest, const Arg& src, Size size = AUTO);
    void shl(const Arg& dest, const Arg& src, Size size = AUTO);
    void shr(const Arg& dest, const Arg& src, Size size = AUTO);
    void sar(const Arg& dest, const Arg& src, Size size = AUTO);
    void idiv(const Arg& src, Size size = AUTO);
    void not_(const Arg& src, Size size = AUTO);
    void neg(const Arg& src, Size size = AUTO);
    void inc(const Arg& src, Size size = AUTO);
    void dec(const Arg& src, Size size = AUTO);
    void push(const Arg& src, Size size = AUTO);
    void pop(const Arg& src, Size size = AUTO);
    void lea(const Arg& dest, const Arg& src, Size size = AUTO);
    void cdq();
    void ret();
    void syscall();
    void label(jasmine::Symbol symbol, ObjectSection section);
    void label(const char* name, ObjectSection section);
    void jmp(const Arg& dest, Size size = AUTO);
    void jcc(const Arg& dest, Condition condition);
    void call(const Arg& dest, Size size = AUTO);
	void setcc(const Arg& dest, Condition condition, Size size = AUTO);
    void nop(u32 n_bytes);

    // Pseudo-instructions that embed raw data in the text section of an
    // object file.
    void lit8(u8 val, ObjectSection section = OS_DATA);
    void lit16(u16 val, ObjectSection section = OS_DATA);
    void lit32(u32 val, ObjectSection section = OS_DATA);
    void lit64(u64 val, ObjectSection section = OS_DATA);
    void litf32(float f, ObjectSection section = OS_DATA);
    void litf64(double d, ObjectSection section = OS_DATA);
    void litstr(const ustring& s, ObjectSection section = OS_DATA);
    void rel32(jasmine::Symbol symbol, ObjectSection section = OS_DATA);
    void nop32(u32 val); // embeds 32-bit immediate in nop. 4-byte aligned
}

#endif