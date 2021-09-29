#ifndef BASIL_SSA_H
#define BASIL_SSA_H

#include "util/vec.h"
#include "util/rc.h"
#include "util/option.h"
#include "util/ustr.h"
#include "util/hash.h"
#include "type.h"

namespace basil {
    struct IRInsn;
    struct IRBlock;
    struct IRFunction;

    enum IRKind {
        IK_NONE,
        IK_VAR,
        IK_INT,
        IK_FLOAT,
        IK_DOUBLE,
        IK_BOOL,
        IK_STRING,
        IK_SYMBOL,
        IK_TYPE,
        IK_CHAR,
        IK_BLOCK
    };

    struct IRParam {
        union Data {
            struct { Symbol name; u32 id; } var;
            i64 i;
            float f32;
            double f64;
            bool b;
            rune ch;
            ustring string;
            Symbol sym;
            Type type;
            u32 block;
            Data();
            ~Data();
        } data;
        IRKind kind;

        IRParam(IRKind kind);
        ~IRParam();
        IRParam(const IRParam& other);
        IRParam& operator=(const IRParam& other);

        // Writes a textual representation of this parameter to the provided
        // output stream.
        void format(stream& io) const;
    };
    
    struct IRBlock {
        u32 id;
        vector<rc<IRInsn>> insns;
        vector<rc<IRBlock>> in, out;

        void add_exit(rc<IRBlock> dest);
        void add_entry(rc<IRBlock> dest);

        // Writes a textual representation of this basic block to the provided
        // output stream.
        void format(stream& io) const;
    };

    // Represents a single Basil procedure. Functions contain graphs of basic blocks,
    // which themselves contain instruction sequences. 
    struct IRFunction {
        Symbol label;
        Type type;
        u32 temp_idx = 0;
        u32 block_idx = 0;
        map<Symbol, u32> var_indices;
        vector<rc<IRBlock>> blocks;
        rc<IRBlock> entry, exit, active_block;

        // Creates an empty function with the provided label and type.
        IRFunction(Symbol label, Type type);

        // Creates a new disconnected basic block within this function.
        rc<IRBlock> new_block();

        // Returns a reference to the basic block with the provided ID.
        rc<IRBlock> get_block(u32 id) const;

        // Returns the current basic block where instructions are being inserted.
        rc<IRBlock> active() const;

        // Adds a new instruction to the active basic block.
        IRParam add_insn(rc<IRInsn> insn);

        // Adds a new basic block, with an exit edge from the current active basic
        // block to the new block.
        void add_block(rc<IRBlock> block);

        // Sets the current active basic block.
        void set_active(rc<IRBlock> block);

        // Completes this function, after all initial blocks and instructions have
        // been added. This is done by adding an edge to the exit block, and adding
        // a return instruction that returns the provided result.
        void finish(Type return_type, const IRParam& result);

        // Writes a textual representation of this function to the provided
        // output stream.
        void format(stream& io) const;
    };

    struct IRInsn {
        Type type;
        optional<IRParam> dest;
        vector<IRParam> src; 
        IRInsn(Type type_in, const optional<IRParam>& dest_in);

        virtual ~IRInsn();

        // Writes a textual representation of this instruction to the provided
        // output stream.
        virtual void format(stream& io) const = 0;
    };

    IRParam ir_var(Symbol name);
    IRParam ir_int(i64 i);
    IRParam ir_float(float f);
    IRParam ir_double(double d);
    IRParam ir_bool(bool b);
    IRParam ir_string(const ustring& s);
    IRParam ir_sym(Symbol s);
    IRParam ir_type(Type t);
    IRParam ir_char(rune ch);
    IRParam ir_none();
    // Creates a fresh temporary variable, unique within the provided function.
    IRParam ir_temp(rc<IRFunction> func);

    rc<IRInsn> ir_add(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_sub(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_mul(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_div(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_rem(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_and(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_or(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_xor(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_not(rc<IRFunction> func, Type type, const IRParam& operand);
    rc<IRInsn> ir_less(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_less_eq(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_greater(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_greater_eq(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_eq(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_not_eq(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs);
    rc<IRInsn> ir_goto(rc<IRFunction> func, rc<IRBlock> block);
    rc<IRInsn> ir_if(rc<IRFunction> func, const IRParam& cond, rc<IRBlock> ifTrue, rc<IRBlock> ifFalse);
    rc<IRInsn> ir_head(rc<IRFunction> func, Type list_type, const IRParam& list);
    rc<IRInsn> ir_tail(rc<IRFunction> func, Type list_type, const IRParam& list);
    rc<IRInsn> ir_cons(rc<IRFunction> func, Type list_type, const IRParam& head, const IRParam& tail);
    rc<IRInsn> ir_call(rc<IRFunction> func, Type func_type, const IRParam& proc, const vector<IRParam>& args);
    rc<IRInsn> ir_arg(rc<IRFunction> func, Type type, const IRParam& dest);
    rc<IRInsn> ir_assign(rc<IRFunction> func, Type type, const IRParam& dest, const IRParam& src);
    rc<IRInsn> ir_phi(rc<IRFunction> func, Type type, const vector<IRParam>& inputs);
    rc<IRInsn> ir_return(Type return_type, const IRParam& value);

    template<typename... Args>
    rc<IRInsn> ir_phi(rc<IRFunction> func, Type type, const Args&... args) {
        return ir_phi(func, type, vector_of<IRParam>(args...));
    }
}

void write(stream& io, const basil::IRParam& param);
void write(stream& io, const rc<basil::IRInsn>& insn);
void write(stream& io, const rc<basil::IRBlock>& block);
void write(stream& io, const rc<basil::IRFunction>& func);

#endif