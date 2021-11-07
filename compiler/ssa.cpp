/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "ssa.h"

template<>
u64 hash(const basil::VarInfo& i) {
    return raw_hash(&i, sizeof(basil::VarInfo));   
}

namespace basil {
    IRParam::Data::Data(): i(0) {}
    IRParam::Data::~Data() {}
    
    IRParam::IRParam(IRKind kind_in): kind(kind_in) {
        if (kind == IK_STRING) new(&data.string) ustring();
    }

    IRParam::~IRParam() {
        if (kind == IK_STRING) data.string.~ustring();
    }

    IRParam::IRParam(const IRParam& other): kind(other.kind) {
        switch (kind) {
            case IK_NONE: break;
            case IK_VAR: data.var = other.data.var; break;
            case IK_INT: data.i = other.data.i; break;
            case IK_FLOAT: data.f32 = other.data.f32; break;
            case IK_DOUBLE: data.f64 = other.data.f64; break;
            case IK_BOOL: data.b = other.data.b; break;
            case IK_STRING: new (&data.string) ustring(other.data.string); break;
            case IK_SYMBOL: data.sym = other.data.sym; break;
            case IK_TYPE: data.type = other.data.type; break;
            case IK_CHAR: data.ch = other.data.ch; break;
            case IK_LABEL: data.label = other.data.label; break;
            case IK_BLOCK: data.block = other.data.block; break;
        }
    }

    IRParam& IRParam::operator=(const IRParam& other) {
        if (&other != this) {
            kind = other.kind;
            switch (kind) {
                case IK_NONE: break;
                case IK_VAR: data.var = other.data.var; break;
                case IK_INT: data.i = other.data.i; break;
                case IK_FLOAT: data.f32 = other.data.f32; break;
                case IK_DOUBLE: data.f64 = other.data.f64; break;
                case IK_BOOL: data.b = other.data.b; break;
                case IK_STRING: data.string = other.data.string; break;
                case IK_SYMBOL: data.sym = other.data.sym; break;
                case IK_TYPE: data.type = other.data.type; break;
                case IK_CHAR: data.ch = other.data.ch; break;
                case IK_LABEL: data.label = other.data.label; break;
                case IK_BLOCK: data.block = other.data.block; break;
            }
        }
        return *this;
    }

    const IRFunction* IRParam::CURRENT_FN = nullptr;

    void IRParam::format(stream& io) const {
        switch (kind) {
            case IK_NONE: return write(io, "()");
            case IK_VAR: {
                const auto& i = CURRENT_FN->vars[data.var];
                write(io, i.name);
                if (*string_from(i.name).begin() != '#') write(io, BOLDMAGENTA, '#', i.id, RESET);
                break;
            }
            case IK_INT: return write(io, data.i);
            case IK_FLOAT: return write(io, data.f32);
            case IK_DOUBLE: return write(io, data.f64);
            case IK_BOOL: return write(io, data.b);
            case IK_STRING: return write(io, '"', escape(data.string), '"');
            case IK_SYMBOL: return write(io, ':', data.sym);
            case IK_TYPE: return write(io, data.type);
            case IK_CHAR: return write(io, "'", escape(ustring() + data.ch), "'");
            case IK_LABEL: return write(io, BOLDYELLOW, data.label, RESET);
            case IK_BLOCK: return write(io, BOLDYELLOW, "BB", data.block, RESET);
        }
    }

    static u32 const_idx = 0;

    void emit_data(IRFunction& func, u64 val, void (*callback)(u64)) {
        func.callbacks.push({
            none<jasmine::Symbol>(),
            val,
            callback
        });
    }

    void emit_data(IRFunction& func, jasmine::Symbol sym, u64 val, void (*callback)(u64)) {
        func.callbacks.push({
            some<jasmine::Symbol>(sym),
            val,
            callback
        });
    }

    jasmine::Symbol emit_data(IRFunction& func, const ustring& s) {
        ustring label_name = format<ustring>(".CC", const_idx ++);
        u32 string_length = s.bytes() + 1; // +1 to include the null terminator

        emit_data(func, string_length, [](u64 val) { jasmine::bc::lit32(val); });

        jasmine::Symbol label = jasmine::local((const char*)label_name.raw());
        for (u32 i = 0; i < string_length; i ++) {
            if (i == 0) emit_data(func, label, s.raw()[i], [](u64 val) { jasmine::bc::lit8(val); });
            else emit_data(func, s.raw()[i], [](u64 val) { jasmine::bc::lit8(val); });
        }
        return label;
    }

    jasmine::Param IRParam::emit(IRFunction& func, Context& ctx) const {
        switch (kind) {
            case IK_NONE: return jasmine::bc::imm(0);
            case IK_VAR: return jasmine::bc::r(data.var);
            case IK_INT: return jasmine::bc::imm(data.i);
            case IK_BOOL: return jasmine::bc::imm(data.b ? 1 : 0);
            case IK_SYMBOL: return jasmine::bc::imm(data.sym.id);
            case IK_TYPE: return jasmine::bc::imm(data.type.id);
            case IK_CHAR: return jasmine::bc::imm(data.ch.u);
            case IK_FLOAT: return jasmine::bc::immfp(data.f32);
            case IK_DOUBLE: return jasmine::bc::immfp(data.f64);
            case IK_STRING: return jasmine::bc::l(emit_data(func, data.string));
            case IK_LABEL: return jasmine::bc::l(jasmine::global(string_from(data.label).raw()));
            case IK_BLOCK: // return jasmine::bc::label();
                panic("Unimplemented IR parameter conversion!");
                return jasmine::bc::imm(0);
        }
    }

    void IRBlock::add_exit(rc<IRBlock> dest) {
        for (const auto& edge : out) if (edge.is(dest)) return; // no duplicates
        out.push(dest);
    }

    void IRBlock::add_entry(rc<IRBlock> dest) {
        for (const auto& edge : in) if (edge.is(dest)) return; // no duplicates
        in.push(dest);
    }

    void IRBlock::format(stream& io) const {
        write(io, BOLDYELLOW, "BB", id, RESET, ":\t", GRAY);
        write(io, "(in =");
        for (rc<IRBlock> bb : in) write(io, " ", bb->id);
        write(io, ")\t");
        write(io, "(out =");
        for (rc<IRBlock> bb : out) write(io, " ", bb->id);
        write(io, ")\t");
        if (dom.begin() != dom.end()) {
            write_seq(io, dom, "(DOM = ", ", ", ")\t");
            write(io, "idom = ", idom ? ::format<ustring>(idom->id) : "Ø", "\t");
            write_seq(io, dom_frontier, "(DF = ", ", ", ")\t");
        }
        write(io, RESET, "\n");
        
        for (const auto& insn : insns) {
            writeln(io, '\t', insn);
        }
    }

    void IRBlock::emit(IRFunction& func, Context& ctx) {
        jasmine::bc::label(label(), jasmine::OS_CODE);
        for (const auto& insn : insns) {
            insn->emit(func, ctx);
        }
    }

    jasmine::Symbol IRBlock::label() {
        if (!lbl) {
            ustring name = ::format<ustring>(".BB", uid);
            lbl = some<jasmine::Symbol>(jasmine::local(name.raw()));
        }
        return *lbl;
    }

    bool operator==(const VarInfo& a, const VarInfo& b) {
        return a.name == b.name && a.id == b.id;
    }

    IRFunction::IRFunction(Symbol label_in, Type type_in): 
        label(label_in), type(type_in), entry(new_block()), exit(nullptr), active_block(entry) {}

    rc<IRBlock> IRFunction::new_block() {
        static u32 bb_uid = 0;
        rc<IRBlock> block = ref<IRBlock>();
        block->id = blocks.size();
        block->uid = bb_uid ++;
        blocks.push(block);
        return block;
    }

    rc<IRBlock> IRFunction::get_block(u32 id) const {
        return blocks[id];
    }

    rc<IRBlock> IRFunction::active() const {
        return active_block;
    }

    IRParam IRFunction::add_insn(rc<IRInsn> insn) {
        active_block->insns.push(insn);
        return insn->dest ? *insn->dest : ir_none();
    }

    void IRFunction::add_block(rc<IRBlock> block) {
        block->add_entry(active_block);
        active_block->add_exit(block);
    }

    void IRFunction::set_active(rc<IRBlock> block) {
        active_block = block;
    }

    rc<IRInsn> ir_goto(IRFunction& func, rc<IRBlock> block);

    void IRFunction::finish(Type return_type, const IRParam& result) {
        exit = new_block();
        add_block(exit);
        add_insn(ir_goto(*this, exit));
        set_active(exit);
        add_insn(ir_return(return_type, result));
    }

    void IRFunction::format(stream& io) const {
        IRParam::CURRENT_FN = this;
        writeln(io, "---- ", BOLDCYAN, label, RESET, " : ", type, "\t(", blocks.size(), " blocks)");
        for (rc<IRBlock> block : blocks) write(io, block);
    }

    void IRFunction::emit(Context& ctx) {
        jasmine::bc::label(jasmine::global(string_from(label).raw()), jasmine::OS_CODE);
        jasmine::bc::frame();
        for (rc<IRBlock> block : block_layout) block->emit(*this, ctx);

        for (const auto& cb : callbacks) {
            if (cb.label) jasmine::bc::label(*cb.label, jasmine::OS_CODE);
            cb.callback(cb.val);
        }
    }

    IRParam find_var(rc<IRFunction> func, VarInfo info) {
        IRParam p(IK_VAR);
        auto existing = func->var_indices.find(info);
        if (existing != func->var_indices.end()) {
            p.data.var = existing->second;
        }
        else {
            func->var_indices[info] = func->vars.size();
            p.data.var = func->vars.size();
            func->vars.push(info);
        }
        return p;
    }

    IRParam ir_temp(rc<IRFunction> func) {
        return find_var(func, { symbol_from(format<ustring>("#", func->temp_idx ++)), 0 });
    }

    IRParam ir_var(rc<IRFunction> func, Symbol name) {
        return find_var(func, { name, 0 });
    }

    IRParam ir_int(i64 i) {
        IRParam p(IK_INT);
        p.data.i = i;
        return p;
    }

    IRParam ir_float(float f) {
        IRParam p(IK_FLOAT);
        p.data.f32 = f;
        return p;
    }

    IRParam ir_double(double d) {
        IRParam p(IK_DOUBLE);
        p.data.f64 = d;
        return p;
    }

    IRParam ir_bool(bool b) {
        IRParam p(IK_BOOL);
        p.data.b = b;
        return p;
    }

    IRParam ir_string(const ustring& s) {
        IRParam p(IK_STRING);
        p.data.string = s;
        return p;
    }

    IRParam ir_sym(Symbol s) {
        IRParam p(IK_SYMBOL);
        p.data.sym = s;
        return p;
    }

    IRParam ir_type(Type t) {
        IRParam p(IK_TYPE);
        p.data.type = t;
        return p;
    }

    IRParam ir_char(rune ch) {
        IRParam p(IK_CHAR);
        p.data.ch = ch;
        return p;
    }
    
    IRParam ir_label(Symbol l) {
        IRParam p(IK_LABEL);
        p.data.label = l;
        return p;
    }

    IRParam ir_none() {
        return IRParam(IK_NONE);
    }

    IRParam ir_block(u32 block) {
        IRParam p(IK_BLOCK);
        p.data.block = block;
        return p;
    }

    IRInsn::IRInsn(IROp op_in, Type type_in, const optional<IRParam>& dest_in):
        op(op_in), type(type_in), dest(dest_in) {}
    
    IRInsn::~IRInsn() {}

    bool IRInsn::liveout() {
        bool result = false;
        bitset new_in = out;
        for (const IRParam& p : src) if (p.kind == IK_VAR) new_in.insert(p.data.var);
        if (dest && dest->kind == IK_VAR) new_in.erase(dest->data.var);
        for (u32 i : new_in) result = in.insert(i) || result;
        // print("\tinstruction ");
        // format(_stdout);
        // show_liveness(_stdout);
        // println(" is ", result ? "not " : "", "done working");
        return result;
    }
    
    void IRInsn::show_liveness(stream& io) const {
        vector<IRParam> in_vars, out_vars;
        for (u32 i : in) {
            IRParam p(IK_VAR);
            p.data.var = i;
            in_vars.push(p);
        }
        for (u32 i : out) {
            IRParam p(IK_VAR);
            p.data.var = i;
            out_vars.push(p);
        }
        write_seq(io, in_vars, "{", ", ", "}");
        write(io, " => ");
        write_seq(io, out_vars, "{", ", ", "}");
    }

    struct IRUnary : public IRInsn {
        IRUnary(rc<IRFunction> func, IROp op, Type type, const IRParam& operand):
            IRInsn(op, type, some<IRParam>(ir_temp(func))) {
            src.push(operand);
        }

        const IRParam& operand() const {
            return src[0];
        }

        IRParam& operand() {
            return src[0];
        }
    };

    struct IRBinary : public IRInsn {
        IRBinary(rc<IRFunction> func, IROp op, Type type, const IRParam& lhs, const IRParam& rhs):
            IRInsn(op, type, some<IRParam>(ir_temp(func))) {
            src.push(lhs);
            src.push(rhs);
        }

        const IRParam& left() const {
            return src[0];
        }

        IRParam& left() {
            return src[0];
        }

        const IRParam& right() const {
            return src[1];
        }

        IRParam& right() {
            return src[1];
        }
    };

    struct IRAdd : public IRBinary {
        IRAdd(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, IR_ADD, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " + ", right());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::add(type.repr(ctx), dest->emit(func, ctx), left().emit(func, ctx), right().emit(func, ctx));
        }
    };

    rc<IRInsn> ir_add(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRAdd>(func, type, lhs, rhs);
    }

    struct IRSub : public IRBinary {
        IRSub(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, IR_SUB, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " - ", right());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::sub(type.repr(ctx), dest->emit(func, ctx), left().emit(func, ctx), right().emit(func, ctx));
        }
    };

    rc<IRInsn> ir_sub(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRSub>(func, type, lhs, rhs);
    }

    struct IRMul : public IRBinary {
        IRMul(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, IR_MUL, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " * ", right());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::mul(type.repr(ctx), dest->emit(func, ctx), left().emit(func, ctx), right().emit(func, ctx));
        }
    };

    rc<IRInsn> ir_mul(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRMul>(func, type, lhs, rhs);
    }

    struct IRDiv : public IRBinary {
        IRDiv(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, IR_DIV, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " / ", right());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::div(type.repr(ctx), dest->emit(func, ctx), left().emit(func, ctx), right().emit(func, ctx));
        }
    };

    rc<IRInsn> ir_div(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRDiv>(func, type, lhs, rhs);
    }

    struct IRRem : public IRBinary {
        IRRem(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, IR_REM, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " % ", right());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::rem(type.repr(ctx), dest->emit(func, ctx), left().emit(func, ctx), right().emit(func, ctx));
        }
    };
    
    rc<IRInsn> ir_rem(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRRem>(func, type, lhs, rhs);
    }
    
    struct IRAnd : public IRBinary {
        IRAnd(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, IR_AND, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " and ", right());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::and_(type.repr(ctx), dest->emit(func, ctx), left().emit(func, ctx), right().emit(func, ctx));
        }
    };
    
    rc<IRInsn> ir_and(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRAnd>(func, type, lhs, rhs);
    }
    
    struct IROr : public IRBinary {
        IROr(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, IR_OR, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " or ", right());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::or_(type.repr(ctx), dest->emit(func, ctx), left().emit(func, ctx), right().emit(func, ctx));
        }
    };

    rc<IRInsn> ir_or(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IROr>(func, type, lhs, rhs);
    }
    
    struct IRXor : public IRBinary {
        IRXor(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, IR_XOR, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " xor ", right());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::xor_(type.repr(ctx), dest->emit(func, ctx), left().emit(func, ctx), right().emit(func, ctx));
        }
    };

    rc<IRInsn> ir_xor(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRXor>(func, type, lhs, rhs);
    }
    
    struct IRNot : public IRUnary {
        IRNot(rc<IRFunction> func, Type type, const IRParam& operand):
            IRUnary(func, IR_NOT, type, operand) {}

        void format(stream& io) const override {
            write(io, *dest, " = not ", operand());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::not_(type.repr(ctx), dest->emit(func, ctx), operand().emit(func, ctx));
        }
    };

    rc<IRInsn> ir_not(rc<IRFunction> func, Type type, const IRParam& operand) {
        return ref<IRNot>(func, type, operand);
    }

    enum CompareKind {
        COMPARE_LESS,
        COMPARE_LESS_EQ,
        COMPARE_GREATER,
        COMPARE_GREATER_EQ,
        COMPARE_EQUAL,
        COMPARE_NOT_EQ
    };

    static const char* compare_ops[6] = {
        "<", "<=", ">", ">=", "==", "!="
    };

    struct IRCompare : public IRBinary {
        CompareKind kind;

        IRCompare(rc<IRFunction> func, CompareKind kind_in, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, IROp(IR_LT + kind_in), type, lhs, rhs), kind(kind_in) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " ", compare_ops[kind], " ", right());
        }

        void emit(IRFunction& func, Context& ctx) const override {
            static void(* const ops[6])(jasmine::Type, const jasmine::Param&, 
                const jasmine::Param&, const jasmine::Param&) = {
                jasmine::bc::cl, jasmine::bc::cle,
                jasmine::bc::cg, jasmine::bc::cge,
                jasmine::bc::ceq, jasmine::bc::cne
            };
            ops[kind](type.repr(ctx), dest->emit(func, ctx), left().emit(func, ctx), right().emit(func, ctx));
        }
    };

    rc<IRInsn> ir_less(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRCompare>(func, COMPARE_LESS, type, lhs, rhs);
    }

    rc<IRInsn> ir_less_eq(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRCompare>(func, COMPARE_LESS_EQ, type, lhs, rhs);
    }
    
    rc<IRInsn> ir_greater(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRCompare>(func, COMPARE_GREATER, type, lhs, rhs);
    }
    
    rc<IRInsn> ir_greater_eq(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRCompare>(func, COMPARE_GREATER_EQ, type, lhs, rhs);
    }
    
    rc<IRInsn> ir_eq(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRCompare>(func, COMPARE_EQUAL, type, lhs, rhs);
    }
    
    rc<IRInsn> ir_not_eq(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRCompare>(func, COMPARE_NOT_EQ, type, lhs, rhs);
    }

    struct IRGoto : public IRInsn {
        IRGoto(rc<IRBlock> block):
            IRInsn(IR_GOTO, T_VOID, none<IRParam>()) {
            src.push(ir_block(block->id));
        }

        void format(stream& io) const override {
            write(io, "goto ", src[0]);
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::jump(func.get_block(src[0].data.block)->label());
        }
    };

    rc<IRInsn> ir_goto(IRFunction& func, rc<IRBlock> block) {
        func.active()->add_exit(block);
        block->add_entry(func.active());
        return ref<IRGoto>(block);
    }
    
    rc<IRInsn> ir_goto(rc<IRFunction> func, rc<IRBlock> block) {
        return ir_goto(*func, block);
    }

    struct IRIfGoto : public IRInsn {
        bool invert;
        IRIfGoto(const IRParam& cond, bool invert_in, rc<IRBlock> block):
            IRInsn(IR_IFGOTO, T_VOID, none<IRParam>()), invert(invert_in) {
            src.push(cond);
            src.push(ir_block(block->id));
        }

        void format(stream& io) const override {
            write(io, "if ", invert ? "not" : "", src[0], " goto ", src[1]);
        }

        void emit(IRFunction& func, Context& ctx) const override {
            (invert ? jasmine::bc::jeq : jasmine::bc::jne)(
                jasmine::I8,
                func.get_block(src[1].data.block)->label(), 
                src[0].emit(func, ctx), jasmine::bc::imm(0)
            );
        }
    };

    rc<IRInsn> ir_if_goto(const IRParam& cond, bool invert, rc<IRBlock> ifTrue) {
        return ref<IRIfGoto>(cond, invert, ifTrue);
    }

    struct IRIf : public IRInsn {
        IRIf(const IRParam& cond, rc<IRBlock> ifTrue, rc<IRBlock> ifFalse):
            IRInsn(IR_IF, T_VOID, none<IRParam>()) {
            src.push(cond);
            src.push(ir_block(ifTrue->id));
            src.push(ir_block(ifFalse->id));
        }

        void format(stream& io) const override {
            write(io, "if ", src[0], " goto ", src[1], " else ", src[2]);
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::jne(jasmine::I8, func.get_block(src[1].data.block)->label(), 
                src[0].emit(func, ctx), jasmine::bc::imm(0));
            jasmine::bc::jump(func.get_block(src[2].data.block)->label());
        }
    };

    rc<IRInsn> ir_if(rc<IRFunction> func, const IRParam& cond, rc<IRBlock> ifTrue, rc<IRBlock> ifFalse) {
        func->active()->add_exit(ifTrue);
        func->active()->add_exit(ifFalse);
        ifTrue->add_entry(func->active());
        ifFalse->add_entry(func->active());
        return ref<IRIf>(cond, ifTrue, ifFalse);
    }

    // rc<IRInsn> ir_head(rc<IRFunction> func, Type list_type, const IRParam& list);
    // rc<IRInsn> ir_tail(rc<IRFunction> func, Type list_type, const IRParam& list);
    // rc<IRInsn> ir_cons(rc<IRFunction> func, Type list_type, const IRParam& head, const IRParam& tail);

    struct IRCall : public IRInsn {
        IRCall(rc<IRFunction> func, Type func_type, const IRParam& proc, const vector<IRParam>& args):
            IRInsn(IR_CALL, func_type, some<IRParam>(ir_temp(func))) {
            src.push(proc);
            for (const auto& p : args) src.push(p);
        }

        void format(stream& io) const override {
            write(io, *dest, " = ", src[0]);
            if (src.size() == 1) write(io, "()");
            else write_seq(io, src[{1, src.size()}], "(", ", ", ")");
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::begincall(t_ret(type).repr(ctx), dest->emit(func, ctx), src[0].emit(func, ctx));
            Type arg = t_arg(type);
            if (arg.of(K_TUPLE)) for (u32 i = 1; i < src.size(); i ++) {
                jasmine::bc::arg(t_tuple_at(arg, i - 1).repr(ctx), src[i].emit(func, ctx));
            }
            else if (arg != T_VOID) jasmine::bc::arg(arg.repr(ctx), src[1].emit(func, ctx));
            jasmine::bc::endcall();
        }
    };

    rc<IRInsn> ir_call(rc<IRFunction> func, Type func_type, const IRParam& proc, const vector<IRParam>& args) {
        return ref<IRCall>(func, func_type, proc, args);
    }

    struct IRArg : public IRInsn {
        u32 arg;
        IRArg(rc<IRFunction> func, Type type, const IRParam& dest, u32 arg_in):
            IRInsn(IR_ARG, type, some<IRParam>(dest)), arg(arg_in) {}

        void format(stream& io) const override {
            write(io, *dest, " = arg ", arg);
        }

        void emit(IRFunction& func, Context& ctx) const override {
            // we assume the args are in order
            jasmine::bc::param(type.repr(ctx), dest->emit(func, ctx));
        }
    };

    rc<IRInsn> ir_arg(rc<IRFunction> func, Type type, const IRParam& dest, u32 arg) {
        return ref<IRArg>(func, type, dest, arg);
    }

    struct IRAssign : public IRInsn {
        IRAssign(rc<IRFunction> func, Type type, const IRParam& dest, const IRParam& src_in):
            IRInsn(IR_ASSIGN, type, some<IRParam>(dest)) {
            src.push(src_in);
        }

        void format(stream& io) const override {
            write(io, *dest, " = ", src[0]);
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::mov(type.repr(ctx), dest->emit(func, ctx), src[0].emit(func, ctx));
        }
    }; 

    rc<IRInsn> ir_assign(rc<IRFunction> func, Type type, const IRParam& dest, const IRParam& src) {
        return ref<IRAssign>(func, type, dest, src);
    }

    struct IRPhi : public IRInsn {
        rc<IRBlock> block;
        IRPhi(rc<IRFunction> func, Type type, const IRParam& dest, const vector<IRParam>& inputs):
            IRInsn(IR_PHI, type, some<IRParam>(dest)), block(func->active()) {
            src = inputs;
        }

        void format(stream& io) const override {
            write(io, *dest, " = Φ");
            write_seq(io, src, "(", ", ", ")");
        }

        void emit(IRFunction& func, Context& ctx) const override {
            panic("Phi nodes should be eliminated before lowering to Jasmine bytecode!");
        }
    };

    rc<IRInsn> ir_phi(rc<IRFunction> func, Type type, const vector<IRParam>& inputs) {
        return ref<IRPhi>(func, type, ir_temp(func), inputs);
    }

    rc<IRInsn> ir_phi(rc<IRFunction> func, Type type, const IRParam& dest, const vector<IRParam>& inputs) {
        return ref<IRPhi>(func, type, dest, inputs);
    }

    struct IRReturn : public IRInsn {
        IRReturn(Type type, const IRParam& value):
            IRInsn(IR_RETURN, type, none<IRParam>()) {
            src.push(value);
        }

        void format(stream& io) const override {
            write(io, "return ", src[0]);
        }

        void emit(IRFunction& func, Context& ctx) const override {
            jasmine::bc::ret(type.repr(ctx), src[0].emit(func, ctx));
        }
    };

    rc<IRInsn> ir_return(Type return_type, const IRParam& value) {
        return ref<IRReturn>(return_type, value);
    }

    // Passes

    Pass PASS_TABLE[NUM_PASS_TYPES] = {
        enforce_ssa,
        dominance_frontiers,
        liveness_ssa,
        rdefs_ssa,
        dead_code_elim_ssa,
        cse_elim_ssa,
        gvn_ssa,
        constant_folding_ssa,
        optimize_arithmetic_ssa,
        linearize_cfg,
        phi_elim,
        cleanup_nops
    };
    
    void require(rc<IRFunction> func, PassType pass) {
        if (!func->passes.contains(pass)) {
            func->passes.insert(pass);
            PASS_TABLE[pass](func);
        }
    }

    void invalidate(rc<IRFunction> func, PassType pass) {
        func->passes.erase(pass);
    }

    void enforce_ssa_block(rc<IRFunction> func, rc<IRBlock> block) {
        // create stub phis
        vector<rc<IRInsn>> phis;
        for (const auto& [k, v] : block->phis) {
            auto it = func->var_numbers.find(k);
            phis.push(ir_phi(func, T_VOID, ir_var(func, k), {}));
        }

        for (rc<IRInsn>& insn : block->insns) phis.push(insn);
        block->insns = move(phis);

        // number instructions in this block
        for (auto& insn : block->insns) {
            // rename src vars
            for (IRParam& p : insn->src) if (p.kind == IK_VAR) {
                Symbol s = func->vars[p.data.var].name;
                auto it = func->var_numbers.find(s);
                if (it != func->var_numbers.end()) // use current register id
                    p = find_var(func, { s, it->second });
                else 
                    panic("Found variable '", s, "' usage before any definition!");
            }

            // rename dest var
            if (insn->dest && insn->dest->kind == IK_VAR) {
                Symbol s = func->vars[insn->dest->data.var].name;
                auto it = func->var_numbers.find(s);
                if (it != func->var_numbers.end()) // increment register id and continue
                    *insn->dest = find_var(func, { s, ++ it->second });
                else func->var_numbers[s] = 0; // must be the first def, so we start at zero
                block->vars_out[s] = it->second;
            }
        }
    }
    
    void enforce_ssa(rc<IRFunction> func) {
        require(func, DOMINANCE_FRONTIER);

        // find defining blocks
        for (auto& block : func->blocks) for (auto& insn : block->insns) {
            if (insn->src.size() && insn->src[0].kind == IK_VAR)
                func->defining_blocks[func->vars[insn->src[0].data.var].name].push(block);
        }

        // determine phis
        for (auto& [k, v] : func->defining_blocks) {
            bool done = false;
            while (!done) {
                done = true;
                vector<rc<IRBlock>> new_defs;
                for (auto& block : v) for (u32 other : block->dom_frontier) {
                    if (func->blocks[other]->phis.contains(k)) continue;
                    func->blocks[other]->phis.put(k, {});
                    new_defs.push(func->blocks[other]);
                    done = false;
                }
                for (auto& block : new_defs) v.push(block);
            }
        }

        // reset any previous attempts at ssa
        func->var_numbers.clear();
        for (auto& block : func->blocks) block->vars_in.clear(), block->vars_out.clear();
        for (auto& block : func->blocks) for (auto& insn : block->insns) {
            // reset all src and dest params to index 0
            for (IRParam& p : insn->src) if (p.kind == IK_VAR) 
                p = ir_var(func, func->vars[p.data.var].name);
            if (insn->dest) if (insn->dest->kind == IK_VAR) 
                insn->dest = some<IRParam>(ir_var(func, func->vars[insn->dest->data.var].name));
        }

        // compute SSA numberings for each block
        for (auto& block : func->blocks) enforce_ssa_block(func, block);

        // fill out empty phi nodes
        for (auto& block : func->blocks) {
            for (u32 i = 0; i < block->phis.size(); i ++) if (block->insns[i]->src.size() == 0) { // empty phis
                Symbol var = func->vars[block->insns[i]->dest->data.var].name;
                for (auto& bin : block->in) if (bin->vars_out.contains(var))
                    block->insns[i]->src.push(find_var(func, { var, bin->vars_out[var] }));
            }
        }

        // remove unnecessary phi nodes
        for (auto& block : func->blocks) {
            rc<IRInsn>* out = &block->insns[0];
            u32 removed = 0, n_phis = block->phis.size();
            for (u32 i = 0; i < block->insns.size(); i ++) {
                if (i < n_phis && block->insns[i]->src.size() < 2) {
                    Symbol var = func->vars[block->insns[i]->dest->data.var].name;
                    block->phis.erase(var);
                    removed ++;
                }
                else *out ++ = block->insns[i];
            }
            for (u32 j = 0; j < removed; j ++) block->insns.pop();
        }
    }

    void dominance_frontiers(rc<IRFunction> func) {
        // compute dominance

        // entry node dominates itself
        func->entry->dom.insert(func->entry->id);

        // all other nodes are dominated by all nodes to start
        for (rc<IRBlock>& block : func->blocks) if (!block.is(func->entry)) {
            for (const rc<IRBlock>& other : func->blocks) block->dom.insert(other->id);
        }

        // iteratively work on computing dominance
        bool working = true;
        while (working) {
            working = false;
            for (u32 i = 1; i < func->blocks.size(); i ++) { // skip entry block (id 0)
                bool first = true;
                bitset tmp;
                for (const rc<IRBlock>& bb : func->blocks[i]->in) {
                    // print(" ", bb->id);
                    if (first) tmp = bb->dom, first = false;
                    else for (u32 i : tmp) 
                        if (!bb->dom.contains(i)) tmp.erase(i); // safe to erase mid-iteration for bitset
                }
                tmp.insert(i);
                // println("");
                // print("tmp(", i, ") = ");
                // write_seq(_stdout, tmp, "", ", ", "\n");
                // print("DOM(", i, ") = ");
                // write_seq(_stdout, func->blocks[i]->dom, "", ", ", "\n");

                bool diff = false;
                for (u32 i : tmp) if (!func->blocks[i]->dom.contains(i)) diff = true;
                for (u32 i : func->blocks[i]->dom) if (!tmp.contains(i)) diff = true;
                // if tmp(i) != DOM(i), replace DOM with tmp
                if (diff) {
                    func->blocks[i]->dom = tmp;
                    working = true;
                }
            }
        }

        // compute immediate dominators for all nodes other than the entry
        for (rc<IRBlock>& block : func->blocks) if (!block.is(func->entry)) {
            vector<rc<IRBlock>> queue;
            for (rc<IRBlock>& pred : block->in) queue.push(pred);
            while (queue.size()) {
                rc<IRBlock> b = queue.back();
                queue.pop();
                if (block->dom.contains(b->id)) {
                    block->idom = b;
                    break;
                }
                else for (rc<IRBlock>& pred : b->in) queue.push(pred);
            }
        }

        // compute dominance frontiers
        for (rc<IRBlock>& block : func->blocks) if (block->in.size() > 1) { // only consider join points
            for (rc<IRBlock>& pred : block->in) { // for each immediate predecessor of block...
                rc<IRBlock> runner = pred;
                // we'll look for all nodes between us and the nearest dominating node
                while (runner && !runner.is(block->idom) && !runner.is(block)) {
                    runner->dom_frontier.insert(block->id);
                    runner = runner->idom;
                }
            }
        }
    }

    bool liveness_block(rc<IRBlock> block) {
        bool working = false;
        for (i64 i = i64(block->insns.size()) - 1; i >= 0; i --) {
            rc<IRInsn>& insn = block->insns[i];
            if (i < i64(block->insns.size()) - 1) {
                for (u32 i : block->insns[i + 1]->in)
                    working = insn->out.insert(i) || working;
            }
            working = insn->liveout() || working;
        }
        // println("block ", block->id, " is ", working ? "not " : "", "done working");
        return working;
    }
    
    void liveness_ssa(rc<IRFunction> func) {
        for (auto block : func->blocks) for (auto insn : block->insns)
            insn->in.clear(), insn->out.clear();
        
        bool working = true;
        while (working) {
            working = false;
            for (i64 i = i64(func->blocks.size()) - 1; i >= 0; i --) {
                working = liveness_block(func->blocks[i]) || working; // compute liveness for this basic block

                for (auto& pred : func->blocks[i]->in) {
                    for (u32 j : func->blocks[i]->insns.front()->in) // unify our new in with each predecessor's out
                        working = pred->insns.back()->out.insert(j) || working;
                }
            }
        }
    }
    
    void rdefs_ssa(rc<IRFunction> func) {
        panic("Unimplemented!");
    }

    void dead_code_elim_ssa(rc<IRFunction> func) {
        panic("Unimplemented!");
    }

    void cse_elim_ssa(rc<IRFunction> func) {
        panic("Unimplemented!");
    }

    void gvn_ssa(rc<IRFunction> func) {
        panic("Unimplemented!");
    }

    void constant_folding_ssa(rc<IRFunction> func) {
        panic("Unimplemented!");
    }

    void optimize_arithmetic_ssa(rc<IRFunction> func) {
        panic("Unimplemented!");
    }

    void linearize_postorder(vector<rc<IRBlock>>& ordering, bitset& visited, const rc<IRBlock>& block) {
        visited.insert(block->id);
        for (i64 i = i64(block->out.size()) - 1; i >= 0; i --) if (!visited.contains(block->out[i]->id))
            linearize_postorder(ordering, visited, block->out[i]);
        ordering.push(block);
    }

    void linearize_cfg(rc<IRFunction> func) {
        vector<rc<IRBlock>> ordering;
        bitset visited;
        linearize_postorder(ordering, visited, func->entry);
        while (ordering.size()) {
            ordering.back()->ord = func->block_layout.size();
            func->block_layout.push(ordering.back());
            ordering.pop();
        }
    }

    void phi_elim(rc<IRFunction> func) {
        for (rc<IRBlock> block : func->blocks) {
            for (rc<IRInsn> insn : block->insns) if (insn->op == IR_PHI) {
                for (u32 i = 0; i < insn->src.size(); i ++) {
                    rc<IRBlock> src = block->in[i];
                    rc<IRInsn> branch = src->insns.back(); // remove branching instruction from the end
                    src->insns.pop();
                    src->insns.push(ir_assign(func, insn->type, *insn->dest, insn->src[i]));
                    src->insns.push(branch); // restore branching instruction
                }
            }
            block->remove_if([](const IRInsn& insn) -> bool { return insn.op == IR_PHI; });
        }
    }

    void cleanup_nops(rc<IRFunction> func) {
        require(func, LINEARIZE_CFG);

        for (rc<IRBlock> block : func->blocks) {
            if (block->insns.size() == 0) continue;

            // the only spot a goto could be is at the end of a block
            rc<IRInsn>& insn = block->insns.back();
            if (insn->op == IR_GOTO && func->get_block(insn->src[0].data.block)->ord == block->ord + 1)
                block->insns.pop();
            else if (insn->op == IR_IF) {
                rc<IRBlock> ifTrue = func->get_block(insn->src[1].data.block);
                rc<IRBlock> ifFalse = func->get_block(insn->src[2].data.block);
                if (ifTrue->ord == block->ord + 1) insn = ir_if_goto(insn->src[0], true, ifFalse);
                if (ifFalse->ord == block->ord + 1) insn = ir_if_goto(insn->src[0], false, ifTrue);
            }
        }

        map<u32, u32> fixup;
        for (u32 i = 0; i < func->block_layout.size(); i ++) if (func->block_layout[i]->insns.size() == 0) {
                // i + 1 is safe because the exit block is never empty
            fixup[func->block_layout[i]->id] = func->block_layout[i + 1]->id;
        }

        // remove all blocks with no instructions
        rc<IRBlock>* write = &func->block_layout[0];
        for (rc<IRBlock>& block : func->block_layout) {
            *write = block;
            if (block->insns.size() != 0) write ++;
        }
        while (&func->block_layout.back() >= write) func->block_layout.pop();

        for (rc<IRBlock>& block : func->block_layout) for (rc<IRInsn>& insn : block->insns) {
            for (IRParam& p : insn->src) if (p.kind == IK_BLOCK) {
                auto it = fixup.find(p.data.block);
                if (it != fixup.end()) p.data.block = it->second;
            }
        }
    }
    
    void optimize(rc<IRFunction> func, OptLevel level) {
        // compute some common properties
        require(func, DOMINANCE_FRONTIER);
        require(func, LIVENESS);

        // necessary prep for bytecode generation
        require(func, LINEARIZE_CFG);
        require(func, PHI_ELIMINATION);
        require(func, CLEANUP_NOPS);
    }
}

void write(stream& io, const basil::IRParam& param) {
    param.format(io);
}

void write(stream& io, const rc<basil::IRInsn>& insn) {
    insn->format(io);
}

void write(stream& io, const rc<basil::IRBlock>& block) {
    block->format(io);
}

void write(stream& io, const rc<basil::IRFunction>& func) {
    func->format(io);
}