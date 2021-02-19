#ifndef BASIL_SSA_H
#define BASIL_SSA_H

#include "ir.h"
#include "util/defs.h"
#include "util/rc.h"
#include "util/vec.h"

namespace basil {
    class Def;
    class Env;

    class BasicBlock;

    struct SSAIdent {
        u64 base, count;
    };

    // maj  min kind
    // 000  00  000
    enum SSAKind : u8 {
        SSA_BINARY = 0,
        SSA_BINARY_MATH = SSA_BINARY,
        SSA_ADD = SSA_BINARY_MATH,
        SSA_SUB = SSA_BINARY_MATH + 1,
        SSA_MUL = SSA_BINARY_MATH + 2,
        SSA_DIV = SSA_BINARY_MATH + 3,
        SSA_REM = SSA_BINARY_MATH + 4,
        SSA_BINARY_LOGIC = SSA_BINARY + 8,
        SSA_AND = SSA_BINARY_LOGIC,
        SSA_OR = SSA_BINARY_LOGIC + 1,
        SSA_XOR = SSA_BINARY_LOGIC + 2,
        SSA_BINARY_COMPARE = SSA_BINARY + 16,
        SSA_EQ = SSA_BINARY_COMPARE,
        SSA_NOT_EQ = SSA_BINARY_COMPARE + 1,
        SSA_LESS = SSA_BINARY_COMPARE + 2,
        SSA_LESS_EQ = SSA_BINARY_COMPARE + 3,
        SSA_GREATER = SSA_BINARY_COMPARE + 4,
        SSA_GREATER_EQ = SSA_BINARY_COMPARE + 5,
        SSA_UNARY = 32,
        SSA_UNARY_MATH = SSA_UNARY,
        SSA_UNARY_LOGIC = SSA_UNARY + 8,
        SSA_NOT = SSA_UNARY_LOGIC,
        SSA_CONSTANT = 64,
        SSA_INT = SSA_CONSTANT,
        SSA_BOOL = SSA_CONSTANT + 1,
        SSA_STRING = SSA_CONSTANT + 2,
        SSA_VOID = SSA_CONSTANT + 3,
        SSA_SYMBOL = SSA_CONSTANT + 4,
        SSA_FUNCTION = SSA_CONSTANT + 5,
        SSA_OTHER = 96,
        SSA_LOAD = SSA_OTHER,
        SSA_STORE = SSA_OTHER + 1,
        SSA_CALL = SSA_OTHER + 2,
        SSA_RET = SSA_OTHER + 3,
        SSA_IF = SSA_OTHER + 4,
        SSA_GOTO = SSA_OTHER + 5,
        SSA_PHI = SSA_OTHER + 6,
        SSA_BB = SSA_OTHER + 7,
        SSA_LOAD_LABEL = SSA_OTHER + 8,
    };

    extern const u8 MAJOR_MASK;
    extern const u8 MINOR_MASK;

    class SSANode : public RC {
        ref<BasicBlock> _parent;
        const Type* _type;
        SSAKind _kind;
        SSAIdent _id;

      public:
        SSANode(ref<BasicBlock> parent, const Type* type, SSAKind kind, const string& name = ".");

        bool is(SSAKind kind) const;
        ref<BasicBlock> parent() const;
        ref<BasicBlock>& parent();
        SSAIdent id() const;
        SSAKind kind() const;
        const Type* type() const;
        virtual Location emit(Function& fn) = 0;
        virtual void format(stream& io) const = 0;
    };

    class BasicBlock {
        string _label;
        vector<ref<SSANode>> _members;
        vector<ref<BasicBlock>> _pred, _succ;

      public:
        BasicBlock(const string& label = "");

        const string& label() const;
        u32 size() const;
        const ref<SSANode>* begin() const;
        ref<SSANode>* begin();
        const ref<SSANode>* end() const;
        ref<SSANode>* end();
        void push(ref<SSANode> SSANode);
        const vector<ref<SSANode>>& members() const;
        vector<ref<SSANode>>& members();
        Location emit(Function& fn);
        void format(stream& io) const;
    };

    class SSAInt : public SSANode {
        i64 _value;

      public:
        SSAInt(ref<BasicBlock> parent, i64 value);

        i64 value() const;
        i64& value();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSABool : public SSANode {
        bool _value;

      public:
        SSABool(ref<BasicBlock> parent, bool value);

        bool value() const;
        bool& value();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSAString : public SSANode {
        string _value;

      public:
        SSAString(ref<BasicBlock> parent, const string& value);

        const string& value() const;
        string& value();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSAVoid : public SSANode {
      public:
        SSAVoid(ref<BasicBlock> parent);

        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSASymbol : public SSANode {
        u64 _value;

      public:
        SSASymbol(ref<BasicBlock> parent, u64 value);

        u64 value() const;
        u64& value();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSAFunction : public SSANode {
        u64 _name;

      public:
        SSAFunction(ref<BasicBlock> parent, const Type* type, u64 name);

        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSALoadLabel : public SSANode {
        u64 _name;

      public:
        SSALoadLabel(ref<BasicBlock> parent, const Type* type, u64 name);

        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSAStore : public SSANode {
        ref<Env> _env;
        ref<SSANode> _value;

      public:
        SSAStore(ref<BasicBlock> parent, ref<Env> env, u64 name, ref<SSANode> value);

        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSABinary : public SSANode {
        ref<SSANode> _left, _right;

      public:
        SSABinary(ref<BasicBlock> parent, const Type* type, SSAKind kind, ref<SSANode> left, ref<SSANode> right);

        ref<SSANode> left() const;
        ref<SSANode>& left();
        ref<SSANode> right() const;
        ref<SSANode>& right();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSAUnary : public SSANode {
        ref<SSANode> _child;

      public:
        SSAUnary(ref<BasicBlock> parent, const Type* type, SSAKind kind, ref<SSANode> child);

        ref<SSANode> child() const;
        ref<SSANode>& child();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSACall : public SSANode {
        ref<SSANode> _fn;
        vector<ref<SSANode>> _args;

      public:
        SSACall(ref<BasicBlock> parent, const Type* rettype, ref<SSANode> fn, const vector<ref<SSANode>>& args);

        ref<SSANode> fn() const;
        ref<SSANode>& fn();
        const vector<ref<SSANode>>& args() const;
        vector<ref<SSANode>>& args();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSARet : public SSANode {
        ref<SSANode> _value;

      public:
        SSARet(ref<BasicBlock> parent, ref<SSANode> value);

        ref<SSANode> value() const;
        ref<SSANode>& value();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSAGoto : public SSANode {
        ref<BasicBlock> _target;

      public:
        SSAGoto(ref<BasicBlock> parent, ref<BasicBlock> target);

        ref<BasicBlock> target() const;
        ref<BasicBlock>& target();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSAIf : public SSANode {
        ref<SSANode> _cond;
        ref<BasicBlock> _if_true, _if_false;

      public:
        SSAIf(ref<BasicBlock> parent, ref<SSANode> cond, ref<BasicBlock> if_true, ref<BasicBlock> if_false);

        ref<SSANode> cond() const;
        ref<SSANode>& cond();
        ref<BasicBlock> if_true() const;
        ref<BasicBlock>& if_true();
        ref<BasicBlock> if_false() const;
        ref<BasicBlock>& if_false();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };

    class SSAPhi : public SSANode {
        ref<SSANode> _left, _right;
        ref<BasicBlock> _left_block, _right_block;

      public:
        SSAPhi(ref<BasicBlock> parent, const Type* type, ref<SSANode> left, ref<SSANode> right,
               ref<BasicBlock> left_block, ref<BasicBlock> right_block);

        ref<SSANode> left() const;
        ref<SSANode>& left();
        ref<SSANode> right() const;
        ref<SSANode>& right();
        ref<BasicBlock> left_block() const;
        ref<BasicBlock>& left_block();
        ref<BasicBlock> right_block() const;
        ref<BasicBlock>& right_block();
        Location emit(Function& fn) override;
        void format(stream& io) const override;
    };
} // namespace basil

void write(stream& io, const basil::SSAIdent& id);
void write(stream& io, const ref<basil::SSANode> node);

#endif