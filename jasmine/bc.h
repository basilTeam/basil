#ifndef JASMINE_BYTECODE_H
#define JASMINE_BYTECODE_H

#include "utils.h"
#include "util/io.h"
#include "util/vec.h"
#include "util/option.h"
#include "obj.h"
#include "sym.h"

namespace jasmine {
    enum Kind {
        K_STRUCT, K_PTR, K_F32, K_F64,
        K_I8, K_I16, K_I32, K_I64,
        K_U8, K_U16, K_U32, K_U64,
        NUM_KINDS
    };

    enum Opcode {
        OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_REM,     // basic arithmetic
        OP_AND, OP_OR, OP_XOR, OP_NOT,              // logic
        OP_ICAST, OP_F32CAST, OP_F64CAST,           // floating-point conversion
        OP_EXT, OP_ZXT,                             // extension
        OP_SL, OP_SLR, OP_SAR,                      // shifts 
        OP_LOCAL, OP_PARAM,                         // variable declaration
        OP_PUSH, OP_POP,                            // stack manipulation
        OP_FRAME, OP_RET, OP_CALL,                  // procedure structure
        OP_JEQ, OP_JNE,                             // equality jumps
        OP_JL, OP_JLE, OP_JG, OP_JGE,               // relational jumps
        OP_JO, OP_JNO,                              // overflow jumps
        OP_JUMP,                                    // unconditional jump
        OP_NOP,                                     // no-op
        OP_CEQ, OP_CNE,                             // equality comparisons
        OP_CL, OP_CLE, OP_CG, OP_CGE,               // relational comparisons
        OP_MOV, OP_XCHG,                            // move and exchange
        OP_TYPE, OP_GLOBAL,                         // top-level definitions
        NUM_OPS
    };

    struct Reg {
        bool global : 1;
        u64 id : 63;
    };

    struct Imm {
        i64 val;
    };

    extern const Reg FP, SP;

    enum ParamKind {
        PK_REG, PK_IMM, PK_LABEL, PK_MEM
    };

    enum MemKind {
        MK_REG_OFF,
        MK_LABEL_OFF,
        MK_TYPE
    };

    struct Type {
        Kind kind : 4;
        u64 id : 60;
    };

    struct Member {
        string name;
        u64 count;
        optional<Type> type;
    };

    // The members of a struct type.
    struct TypeInfo {
        u64 id;
        string name;
        vector<Member> members;
    };

    struct Context {
        vector<TypeInfo> type_info;
        map<string, u64> type_decls;
        vector<string> global_info;
        map<string, Reg> global_decls;
    };

    extern const Type I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, PTR;

    Type struct_type(Symbol name);

    struct Param {
        ParamKind kind;
        optional<Type> annotation = none<Type>();
        union {
            Reg reg;
            Imm imm;

            // Memory is complicated.
            // Register-offset uses 'reg' and 'off' to store [%reg + off]
            // Label uses 'label' to store [label]
            // Label-offset uses 'label' and 'off' to store [label + off]
            // Type uses 'reg' and 'type' to store [%reg + type] (such as [%0 + ptr])
            // Type *also* uses 'reg', 'type', and 'off' to store [%reg + type.off]
            // ...in the last case, 'off' denotes the index of the field within the type,
            // not the byte offset in memory.
            struct { 
                MemKind kind; 
                Reg reg;
                Symbol label;
                Type type;
                i64 off;
            } mem; 
            Symbol label;
        } data;
    };

    // Represents a single instruction, with an opcode, type, and several
    // parameters.
    struct Insn {
        Opcode opcode;
        Type type;
        vector<Param> params;

        void format(stream& io) const;
    };

    Param r(u64 id);
    Param gr(Symbol symbol);
    Param gr(const char* name);
    Param imm(Imm i);
    Param m(Reg reg);
    Param m(Reg reg, i64 off);
    Param m(Reg reg, Symbol type, Symbol field);
    Param l(Symbol symbol);
    Param l(const char* name);

    void writeto(Object& buf);

    void add(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void sub(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void mul(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void div(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void rem(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void and_(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void or_(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void not_(Type type, const Param& dest, const Param& operand);
    void icast(Type type, const Param& dest, const Param& operand);
    void f32cast(Type type, const Param& dest, const Param& operand);
    void f64cast(Type type, const Param& dest, const Param& operand);
    void sxt(Type type, const Param& dest, const Param& operand);
    void zxt(Type type, const Param& dest, const Param& operand);
    void icast(Type type, const Param& lhs, const Param& rhs);
    void f32cast(Type type, const Param& lhs, const Param& rhs);
    void f64cast(Type type, const Param& lhs, const Param& rhs);
    void sl(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void slr(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void sar(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void local(Type type, const Param& dest);
    void param(Type type, const Param& dest);
    void push(Type type, const Param& operand);
    void pop(Type type, const Param& dest);
    void frame();
    void ret();
    void begincall(Type type, const Param& dest, const Param& func);
    void arg(Type type, const Param& param);
    void endcall();
    void call_recur();

    template<typename ...Args>
    void call_recur(Type type, const Param& arg, const Args&... args) {
        jasmine::arg(type, arg);
        call_recur(args...);
    }

    template<typename... Args>
    void call(Type type, const Param& dest, const Param& func, const Args&... args) {
        begincall(type, dest, func);
        call_recur(args...);
    }

    void jeq(Type type, Symbol symbol, const Param& lhs, const Param& rhs);
    void jne(Type type, Symbol symbol, const Param& lhs, const Param& rhs);
    void jl(Type type, Symbol symbol, const Param& lhs, const Param& rhs);
    void jle(Type type, Symbol symbol, const Param& lhs, const Param& rhs);
    void jg(Type type, Symbol symbol, const Param& lhs, const Param& rhs);
    void jge(Type type, Symbol symbol, const Param& lhs, const Param& rhs);
    void jo(Type type, Symbol symbol, const Param& lhs, const Param& rhs);
    void jno(Type type, Symbol symbol, const Param& lhs, const Param& rhs);
    void jump(Symbol symbol);
    void nop();
    void ceq(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void cne(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void cl(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void cle(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void cg(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void cge(Type type, const Param& dest, const Param& lhs, const Param& rhs);
    void mov(Type type, const Param& dest, const Param& operand);
    void begintype(Symbol name);
    void member(Symbol name, u64 size);
    void endtype();
    
    void type_recur();
    
    template<typename... Args>
    void type_recur(Symbol name, u64 size, const Args&... args) {
        member(name, size);
        type_recur(args...);
    }

    template<typename... Args>
    void type(Symbol name, const Args&... args) {
        begintype(name);
        type_recur(args...);
    }

    void label(Symbol symbol);
    void label(const char* symbol);
    void global(Type type, Symbol symbol);
    
    Insn parse_insn(Context& context, stream& io);
    Insn disassemble_insn(stream& io);
    void print_insn(Context& context, stream& io, const Insn& insn);

    Object jasmine_to_x86(Object& obj);
}

#endif