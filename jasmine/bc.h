#ifndef JASMINE_BYTECODE_H
#define JASMINE_BYTECODE_H

#include "utils.h"
#include "util/io.h"
#include "obj.h"
#include "sym.h"

namespace jasmine {
    enum Kind {
        K_S8, K_S16, K_S32, K_S64,
        K_U8, K_U16, K_U32, K_U64,
        K_F32, K_F64,
        K_PTR, K_TUP, K_ARR
    };
    
    struct Type {
        Kind kind;
        union {
            struct { Type element; u64 size; } arr;
            struct { Type[] elements; } tup;
        } data;
    };

    using Opcode = u8;

    using Reg = u64;
    using Imm = i64;

    extern const Reg FP, SP, RET, LABEL;

    enum OpType : u8 {
        S8, S16, S32, S64, U8, U16, U32, U64, F32, F64, PTR,
    };

    enum ParamKind : u8 {
        PK_NONE, PK_REG, PK_MEM, PK_IMM, PK_LABEL
    };

    struct Param {
        ParamKind kind;
        union {
            Reg reg;
            Imm imm;
            struct { Reg reg; i64 off; } mem;
            Symbol label;
        } data;
    };

    Param r(Reg reg);
    Param imm(Imm i);
    Param m(Reg reg);
    Param m(Reg reg, i64 off);
    Param l(Symbol symbol);
    Param l(const char* name);

    void writeto(Object& buf);

    void add(OpType type, const Param& lhs, const Param& rhs);
    void sub(OpType type, const Param& lhs, const Param& rhs);
    void mul(OpType type, const Param& lhs, const Param& rhs);
    void div(OpType type, const Param& lhs, const Param& rhs);
    void rem(OpType type, const Param& lhs, const Param& rhs);
    void and_(OpType type, const Param& lhs, const Param& rhs);
    void or_(OpType type, const Param& lhs, const Param& rhs);
    void xor_(OpType type, const Param& lhs, const Param& rhs);
    void not_(OpType type, const Param& operand);
    void mov(OpType type, const Param& lhs, const Param& rhs);
    void cmp(OpType type, const Param& lhs, const Param& rhs);
    void sxt(OpType type, const Param& lhs, const Param& rhs);
    void zxt(OpType type, const Param& lhs, const Param& rhs);
    void icast(OpType type, const Param& lhs, const Param& rhs);
    void f32cast(OpType type, const Param& lhs, const Param& rhs);
    void f64cast(OpType type, const Param& lhs, const Param& rhs);
    void shl(OpType type, const Param& lhs, const Param& rhs);
    void shr(OpType type, const Param& lhs, const Param& rhs);
    void frame(u64 size, u64 num_regs);
    void ret();
    void push(OpType type, const Param& operand);
    void pop(OpType type, const Param& operand);
    void call(OpType type, const Param& dest, Symbol symbol);
    void rcall(OpType type, const Param& dest, const Param& func);
    void jl(Symbol symbol);
    void jle(Symbol symbol);
    void jg(Symbol symbol);
    void jge(Symbol symbol);
    void jeq(Symbol symbol);
    void jne(Symbol symbol);
    void jump(Symbol symbol);
    void arg(OpType type, const Param& operand);
    void label(Symbol symbol);
    void label(const char* symbol);

    void disassemble(Object& obj, stream& io);

    Object jasmine_to_x86(Object& obj);
}

#endif
