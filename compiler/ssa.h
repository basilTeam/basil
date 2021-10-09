/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef BASIL_SSA_H
#define BASIL_SSA_H

#include "util/vec.h"
#include "util/rc.h"
#include "util/option.h"
#include "util/ustr.h"
#include "util/hash.h"
#include "util/sets.h"
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
            u32 var;
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

        // We use this to inform printing of IR parameters, since variables
        // are stored as indices within a function's variable table.
        static const IRFunction* CURRENT_FN;

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

        // The set of block ids this basic block dominates.
        bitset dom, dom_frontier;

        // The immediate dominator of this basic block.
        rc<IRBlock> idom;

        // Tracks the SSA register numbers going in and out of this block.
        map<Symbol, u32> vars_in, vars_out;
        map<Symbol, bitset> phis;

        void add_exit(rc<IRBlock> dest);
        void add_entry(rc<IRBlock> dest);

        // Writes a textual representation of this basic block to the provided
        // output stream.
        void format(stream& io) const;
    };

    struct VarInfo {
        Symbol name;
        u32 id;
    };

    bool operator==(const VarInfo& a, const VarInfo& b);

    // Represents a single Basil procedure. Functions contain graphs of basic blocks,
    // which themselves contain instruction sequences. 
    struct IRFunction {
        Symbol label;
        Type type;

        u32 temp_idx = 0;
        vector<VarInfo> vars;
        map<VarInfo, u32> var_indices;
        map<Symbol, vector<rc<IRBlock>>> defining_blocks;
        map<Symbol, u32> var_numbers;

        u32 block_idx = 0;
        vector<rc<IRBlock>> blocks;
        rc<IRBlock> entry, exit, active_block;

        // Tracks which passes have been done over this function.
        bitset passes;

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
        bitset in, out;

        IRInsn(Type type_in, const optional<IRParam>& dest_in);

        virtual ~IRInsn();

        // Computes live-in set based on this instruction's live-out set.
        // Returns true if the live-in set changed during the process, false otherwise.
        virtual bool liveout();

        // Writes a textual representation of this instruction to the provided
        // output stream.
        virtual void format(stream& io) const = 0;

        // Writes liveness information about this instruction to the provided output
        // stream.
        void show_liveness(stream& io) const;
    };

    IRParam ir_var(rc<IRFunction> func, Symbol name);
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
    rc<IRInsn> ir_arg(rc<IRFunction> func, Type type, const IRParam& dest, u32 arg);
    rc<IRInsn> ir_assign(rc<IRFunction> func, Type type, const IRParam& dest, const IRParam& src);
    rc<IRInsn> ir_phi(rc<IRFunction> func, Type type, const vector<IRParam>& inputs);
    rc<IRInsn> ir_return(Type return_type, const IRParam& value);

    template<typename... Args>
    rc<IRInsn> ir_phi(rc<IRFunction> func, Type type, const Args&... args) {
        return ir_phi(func, type, vector_of<IRParam>(args...));
    }

    // Passes

    using Pass = void(*)(rc<IRFunction>);

    enum PassType {
        ENFORCE_SSA,
        DOMINANCE_FRONTIER,
        LIVENESS,
        REACHING_DEFS,
        DEAD_CODE_ELIM,
        COMMON_SUBEXPR_ELIM,
        GLOBAL_VALUE_NUMBERING,
        CONSTANT_FOLDING,
        OPTIMIZE_ARITHMETIC,
        NUM_PASS_TYPES
    };

    extern Pass PASS_TABLE[NUM_PASS_TYPES];

    // Require that the pass denoted by the provided pass type be performed for the
    // provided function before proceeding. This is used to enforce certain computations
    // be done before others in the optimization process.
    void require(rc<IRFunction> func, PassType pass);

    // Mark a pass type as invalid for the provided function. This is used to denote
    // passes that require repetition, since their invariants have been invalidated
    // by recent changes. Invalidating a pass does not eagerly force it to be recomputed,
    // but future calls to require() for that pass will.
    void invalidate(rc<IRFunction> func, PassType pass);

    // Enforces SSA over the instructions of the provided function. This means
    // detecting duplicate assignments of the same variable and numbering accordingly,
    // as well as inserting any necessary phi nodes.
    void enforce_ssa(rc<IRFunction> func);

    // Computes dominance and dominance frontiers for all basic blocks in the 
    // provided function.
    void dominance_frontiers(rc<IRFunction> func);

    // Computes liveness information for each variable in the provided function.
    void liveness_ssa(rc<IRFunction> func);

    // Computes reaching definitions for each variable in the provided function.
    void rdefs_ssa(rc<IRFunction> func);

    // Performs dead code elimination over all expressions in the provided function.
    void dead_code_elim_ssa(rc<IRFunction> func);

    // Performs common subexpression elimination over all expressions in the provided
    // function.
    void cse_elim_ssa(rc<IRFunction> func);

    // Performs global value numbering and eliminates duplicate computations in the
    // provided function.
    void gvn_ssa(rc<IRFunction> func);

    // Folds any available constant expressions in the provided function.
    void constant_folding_ssa(rc<IRFunction> func);

    // Transforms arithmetic instructions to generally faster equivalents from a
    // high-level perspective. Generally costly instructions like division are
    // avoided, but smaller optimizations like code size reduction are left to
    // a lower-level code generation pass.
    void optimize_arithmetic_ssa(rc<IRFunction> func);
}

template<>
u64 hash(const basil::VarInfo& i);

void write(stream& io, const basil::IRParam& param);
void write(stream& io, const rc<basil::IRInsn>& insn);
void write(stream& io, const rc<basil::IRBlock>& block);
void write(stream& io, const rc<basil::IRFunction>& func);

#endif