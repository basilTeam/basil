#include "ssa.h"

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
            case IK_STRING: data.string = other.data.string; break;
            case IK_SYMBOL: data.sym = other.data.sym; break;
            case IK_TYPE: data.type = other.data.type; break;
            case IK_CHAR: data.ch = other.data.ch; break;
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
                case IK_BLOCK: data.block = other.data.block; break;
            }
        }
        return *this;
    }

    void IRParam::format(stream& io) const {
        switch (kind) {
            case IK_NONE: return write(io, "()");
            case IK_VAR: 
                write(io, data.var.name);
                if (data.var.id) write(io, data.var.id);
                break;
            case IK_INT: return write(io, data.i);
            case IK_FLOAT: return write(io, data.f32);
            case IK_DOUBLE: return write(io, data.f64);
            case IK_BOOL: return write(io, data.b);
            case IK_STRING: return write(io, '"', data.string, '"');
            case IK_SYMBOL: return write(io, ':', data.sym);
            case IK_TYPE: return write(io, data.type);
            case IK_CHAR: return write(io, "'", data.ch, "'");
            case IK_BLOCK: return write(io, "BB", data.block);
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
        writeln(io, "BB", id, ":");
        for (const auto& insn : insns) {
            write(io, '\t');
            insn->format(io);
            writeln(io, "");
        }
    }

    IRFunction::IRFunction(Symbol label_in, Type type_in): 
        label(label_in), type(type_in), entry(new_block()), exit(nullptr), active_block(entry) {}

    rc<IRBlock> IRFunction::new_block() {
        rc<IRBlock> block = ref<IRBlock>();
        block->id = blocks.size();
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
        writeln(io, "---- ", label, " : ", type, "\t(", blocks.size(), " blocks)");
        for (rc<IRBlock> block : blocks) block->format(io);
    }

    IRParam ir_temp(rc<IRFunction> func) {
        IRParam p(IK_VAR);
        p.data.var = { symbol_from(format<ustring>("#", func->temp_idx ++)), 0 };
        return p;
    }

    IRParam ir_var(Symbol name) {
        IRParam p(IK_VAR);
        p.data.var = { name, 0 };
        return p;
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

    IRParam ir_none() {
        return IRParam(IK_NONE);
    }

    IRParam ir_block(u32 block) {
        IRParam p(IK_BLOCK);
        p.data.block = block;
        return p;
    }

    IRInsn::IRInsn(Type type_in, const optional<IRParam>& dest_in):
        type(type_in), dest(dest_in) {}
    
    IRInsn::~IRInsn() {}

    struct IRUnary : public IRInsn {
        IRUnary(rc<IRFunction> func, Type type, const IRParam& operand):
            IRInsn(type, some<IRParam>(ir_temp(func))) {
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
        IRBinary(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRInsn(type, some<IRParam>(ir_temp(func))) {
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
            IRBinary(func, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " + ", right());
        }
    };

    rc<IRInsn> ir_add(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRAdd>(func, type, lhs, rhs);
    }

    struct IRSub : public IRBinary {
        IRSub(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " - ", right());
        }
    };

    rc<IRInsn> ir_sub(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRSub>(func, type, lhs, rhs);
    }

    struct IRMul : public IRBinary {
        IRMul(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " * ", right());
        }
    };

    rc<IRInsn> ir_mul(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRMul>(func, type, lhs, rhs);
    }

    struct IRDiv : public IRBinary {
        IRDiv(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " / ", right());
        }
    };

    rc<IRInsn> ir_div(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRDiv>(func, type, lhs, rhs);
    }

    struct IRRem : public IRBinary {
        IRRem(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " % ", right());
        }
    };
    
    rc<IRInsn> ir_rem(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRRem>(func, type, lhs, rhs);
    }
    
    struct IRAnd : public IRBinary {
        IRAnd(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " and ", right());
        }
    };
    
    rc<IRInsn> ir_and(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRAnd>(func, type, lhs, rhs);
    }
    
    struct IROr : public IRBinary {
        IROr(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " or ", right());
        }
    };

    rc<IRInsn> ir_or(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IROr>(func, type, lhs, rhs);
    }
    
    struct IRXor : public IRBinary {
        IRXor(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs):
            IRBinary(func, type, lhs, rhs) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " xor ", right());
        }
    };

    rc<IRInsn> ir_xor(rc<IRFunction> func, Type type, const IRParam& lhs, const IRParam& rhs) {
        return ref<IRXor>(func, type, lhs, rhs);
    }
    
    struct IRNot : public IRUnary {
        IRNot(rc<IRFunction> func, Type type, const IRParam& operand):
            IRUnary(func, type, operand) {}

        void format(stream& io) const override {
            write(io, *dest, " = not ", operand());
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
            IRBinary(func, type, lhs, rhs), kind(kind_in) {}

        void format(stream& io) const override {
            write(io, *dest, " = ", left(), " ", compare_ops[kind], " ", right());
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
            IRInsn(T_VOID, none<IRParam>()) {
            src.push(ir_block(block->id));
        }

        void format(stream& io) const override {
            write(io, "goto ", src[0]);
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

    struct IRIf : public IRInsn {
        IRIf(rc<IRFunction> func, const IRParam& cond, rc<IRBlock> ifTrue, rc<IRBlock> ifFalse):
            IRInsn(T_VOID, none<IRParam>()) {
            src.push(cond);
            src.push(ir_block(ifTrue->id));
            src.push(ir_block(ifFalse->id));
        }

        void format(stream& io) const override {
            write(io, "if ", src[0], " goto ", src[1], " else ", src[2]);
        }
    };

    rc<IRInsn> ir_if(rc<IRFunction> func, const IRParam& cond, rc<IRBlock> ifTrue, rc<IRBlock> ifFalse) {
        return ref<IRIf>(func, cond, ifTrue, ifFalse);
    }

    // rc<IRInsn> ir_head(rc<IRFunction> func, Type list_type, const IRParam& list);
    // rc<IRInsn> ir_tail(rc<IRFunction> func, Type list_type, const IRParam& list);
    // rc<IRInsn> ir_cons(rc<IRFunction> func, Type list_type, const IRParam& head, const IRParam& tail);

    struct IRCall : public IRInsn {
        IRCall(rc<IRFunction> func, Type func_type, const IRParam& proc, const vector<IRParam>& args):
            IRInsn(func_type, some<IRParam>(ir_temp(func))) {
            src.push(proc);
            for (const auto& p : args) src.push(p);
        }

        void format(stream& io) const override {
            write(io, *dest, " = ", src[0]);
            write_seq(io, src[{1, src.size()}], "(", ", ", ")");
        }
    };

    rc<IRInsn> ir_call(rc<IRFunction> func, Type func_type, const IRParam& proc, const vector<IRParam>& args) {
        return ref<IRCall>(func, func_type, proc, args);
    }
    // rc<IRInsn> ir_arg(rc<IRFunction> func, Type type, const IRParam& dest);

    struct IRAssign : public IRInsn {
        IRAssign(rc<IRFunction> func, Type type, const IRParam& dest, const IRParam& src_in):
            IRInsn(type, some<IRParam>(dest)) {
            src.push(src_in);
        }

        void format(stream& io) const override {
            write(io, *dest, " = ", src[0]);
        }
    }; 

    rc<IRInsn> ir_assign(rc<IRFunction> func, Type type, const IRParam& dest, const IRParam& src) {
        return ref<IRAssign>(func, type, dest, src);
    }

    struct IRPhi : public IRInsn {
        rc<IRBlock> block;
        IRPhi(rc<IRFunction> func, Type type, const vector<IRParam>& inputs):
            IRInsn(type, some<IRParam>(ir_temp(func))), block(func->active()) {
            src = inputs;
        }

        void format(stream& io) const override {
            write(io, *dest, " = Φ ");
            write_seq(io, src, "", ", ", "");
        }
    };

    rc<IRInsn> ir_phi(rc<IRFunction> func, Type type, const vector<IRParam>& inputs) {
        return ref<IRPhi>(func, type, inputs);
    }

    struct IRReturn : public IRInsn {
        IRReturn(Type type, const IRParam& value):
            IRInsn(type, none<IRParam>()) {
            src.push(value);
        }

        void format(stream& io) const override {
            write(io, "return ", src[0]);
        }
    };

    rc<IRInsn> ir_return(Type return_type, const IRParam& value) {
        return ref<IRReturn>(return_type, value);
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