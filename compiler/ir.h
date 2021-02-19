#ifndef BASIL_IR_H
#define BASIL_IR_H

#include "type.h"
#include "util/defs.h"
#include "util/io.h"
#include "util/str.h"
#include "util/vec.h"

#include "jasmine/x64.h"

namespace basil {
    using namespace jasmine;

    enum LocationType { LOC_NONE, LOC_LOCAL, LOC_IMMEDIATE, LOC_CONSTANT, LOC_LABEL, LOC_REGISTER };

    struct LocalInfo {
        string name;
        u32 index;
        const Type* type;
        i64 reg;
        i64 offset;
    };

    struct ConstantInfo {
        string name;
        vector<u8> data;
        const Type* type;
    };

    struct Location {
        union {
            u32 local_index;
            i64 immediate;
            u32 constant_index;
            u32 label_index;
            u32 reg;
        };
        LocationType type;

        const Type* value_type();
    };

    extern const u8 BINARY_INSN;

    enum InsnType {
        LOAD_INSN = 0,
        STORE_INSN = 1,
        LOAD_ARG_INSN = 2,
        GOTO_INSN = 3,
        IFZERO_INSN = 4,
        CALL_INSN = 5,
        ADDRESS_INSN = 6,
        NOT_INSN = 7,
        LOAD_PTR_INSN = 8,
        STORE_PTR_INSN = 9,
        RET_INSN = 10,
        LABEL = 11,
        ADD_INSN = 128,
        SUB_INSN = 129,
        MUL_INSN = 130,
        DIV_INSN = 131,
        REM_INSN = 132,
        AND_INSN = 133,
        OR_INSN = 134,
        XOR_INSN = 135,
        EQ_INSN = 136,
        NOT_EQ_INSN = 137,
        LESS_INSN = 138,
        LESS_EQ_INSN = 139,
        GREATER_INSN = 140,
        GREATER_EQ_INSN = 141,
    };

    Location loc_none();
    Location loc_immediate(i64 i);
    Location loc_label(const string& label);
    const Type* type_of(const Location& loc);

    i64 immediate_of(const Location& loc);
    const string& label_of(const Location& loc);
    LocalInfo& local_of(const Location& loc);
    ConstantInfo& constant_of(const Location& loc);

    class Function;

    class Insn {
      protected:
        InsnType _kind;
        Location _loc;
        Function* _func;
        vector<Insn*> _succ;
        set<u32> _in, _out;
        virtual Location lazy_loc() = 0;

        friend class Function;
        void setfunc(Function* func);

      public:
        Insn(InsnType kind);
        virtual ~Insn();

        InsnType kind() const;
        const Location& loc() const;
        Location& loc();
        virtual void emit() = 0;
        virtual void format(stream& io) const = 0;
        vector<Insn*>& succ();
        const vector<Insn*>& succ() const;
        set<u32>& in();
        set<u32>& out();
        const set<u32>& in() const;
        const set<u32>& out() const;
        virtual void liveout() = 0;
    };

    u32 find_label(const string& label);
    u32 add_label(const string& label);
    u32 next_label();
    Location next_local(const Type* t);
    Location const_loc(u32 label, const string& constant);
    void emit_constants(Object& object);

    class Function {
        vector<Function*> _fns;
        vector<Insn*> _insns;
        i64 _stack;
        vector<Location> _locals;
        map<u32, u32> _labels;
        u32 _label;
        u32 _end;
        Location _ret;
        void liveness();
        void to_registers();
        Function(u32 label);

      public:
        Function(const string& label);
        ~Function();

        void place_label(u32 label);
        Function& create_function();
        Function& create_function(const string& name);
        Location create_local(const Type* t);
        Location create_local(const string& name, const Type* t);
        Location add(Insn* insn);
        u32 label() const;
        void allocate();
        void emit(Object& obj, bool exit = false);
        void format(stream& io) const;
        u32 end_label() const;
        const Location& ret_loc() const;
        Insn* last() const;
    };

    class LoadInsn : public Insn {
        Location _src;

      protected:
        Location lazy_loc() override;

      public:
        LoadInsn(Location src);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class StoreInsn : public Insn {
        Location _dest, _src;

      protected:
        Location lazy_loc() override;

      public:
        StoreInsn(Location dest, Location src);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class LoadPtrInsn : public Insn {
        Location _src;
        const Type* _type;
        i32 _offset;

      protected:
        Location lazy_loc() override;

      public:
        LoadPtrInsn(Location src, const Type* t, i32 offset);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class StorePtrInsn : public Insn {
        Location _dest, _src;
        i32 _offset;

      protected:
        Location lazy_loc() override;

      public:
        StorePtrInsn(Location dest, Location src, i32 offset);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class AddressInsn : public Insn {
        Location _src;
        const Type* _type;

      protected:
        Location lazy_loc() override;

      public:
        AddressInsn(Location src, const Type* t);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class BinaryInsn : public Insn {
        const char* _name;

      protected:
        Location _left, _right;

      public:
        BinaryInsn(InsnType kind, const char* name, Location left, Location right);

        void format(stream& io) const override;
        void liveout() override;
        Location& left();
        const Location& left() const;
        Location& right();
        const Location& right() const;
    };

    class BinaryMathInsn : public BinaryInsn {
      protected:
        Location lazy_loc() override;

      public:
        BinaryMathInsn(InsnType kind, const char* name, Location left, Location right);
    };

    class AddInsn : public BinaryMathInsn {
      public:
        AddInsn(Location left, Location right);
        void emit() override;
    };

    class SubInsn : public BinaryMathInsn {
      public:
        SubInsn(Location left, Location right);
        void emit() override;
    };

    class MulInsn : public BinaryMathInsn {
      public:
        MulInsn(Location left, Location right);
        void emit() override;
    };

    class DivInsn : public BinaryMathInsn {
      public:
        DivInsn(Location left, Location right);
        void emit() override;
    };

    class RemInsn : public BinaryMathInsn {
      public:
        RemInsn(Location left, Location right);
        void emit() override;
    };

    class BinaryLogicInsn : public BinaryInsn {
      protected:
        Location lazy_loc() override;

      public:
        BinaryLogicInsn(InsnType kind, const char* name, Location left, Location right);
    };

    class AndInsn : public BinaryLogicInsn {
      public:
        AndInsn(Location left, Location right);
        void emit() override;
    };

    class OrInsn : public BinaryLogicInsn {
      public:
        OrInsn(Location left, Location right);
        void emit() override;
    };

    class XorInsn : public BinaryLogicInsn {
      public:
        XorInsn(Location left, Location right);
        void emit() override;
    };

    class NotInsn : public Insn {
        Location _src;

      protected:
        Location lazy_loc() override;

      public:
        NotInsn(Location src);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class BinaryEqualityInsn : public BinaryInsn {
      protected:
        Location lazy_loc() override;

      public:
        BinaryEqualityInsn(InsnType kind, const char* name, Location left, Location right);
    };

    class EqualInsn : public BinaryEqualityInsn {
      public:
        EqualInsn(Location left, Location right);
        void emit() override;
    };

    class InequalInsn : public BinaryEqualityInsn {
      public:
        InequalInsn(Location left, Location right);
        void emit() override;
    };

    class BinaryRelationInsn : public BinaryInsn {
      protected:
        Location lazy_loc() override;

      public:
        BinaryRelationInsn(InsnType kind, const char* name, Location left, Location right);
    };

    class LessInsn : public BinaryRelationInsn {
      public:
        LessInsn(Location left, Location right);
        void emit() override;
    };

    class LessEqualInsn : public BinaryRelationInsn {
      public:
        LessEqualInsn(Location left, Location right);
        void emit() override;
    };

    class GreaterInsn : public BinaryRelationInsn {
      public:
        GreaterInsn(Location left, Location right);
        void emit() override;
    };

    class GreaterEqualInsn : public BinaryRelationInsn {
      public:
        GreaterEqualInsn(Location left, Location right);
        void emit() override;
    };

    class RetInsn : public Insn {
        Location _src;

      protected:
        Location lazy_loc() override;

      public:
        RetInsn(Location src);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class LoadArgumentInsn : public Insn {
        u32 _index;
        const Type* _type;

      protected:
        Location lazy_loc() override;

      public:
        LoadArgumentInsn(u32 index, const Type* type);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class CallInsn : public Insn {
        Location _fn;
        vector<Location> _args;
        const Type* _ret;

      protected:
        Location lazy_loc() override;

      public:
        CallInsn(Location fn, const vector<Location>& args, const Type* ret);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class Label : public Insn {
        u32 _label;

      protected:
        Location lazy_loc() override;

      public:
        Label(u32 label);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class GotoInsn : public Insn {
        u32 _label;

      protected:
        Location lazy_loc() override;

      public:
        GotoInsn(u32 label);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };

    class IfZeroInsn : public Insn {
        u32 _label;
        Location _cond;
        const Type* _result;

      protected:
        Location lazy_loc() override;

      public:
        IfZeroInsn(u32 label, Location cond);

        void emit() override;
        void format(stream& io) const override;
        void liveout() override;
    };
} // namespace basil

void write(stream& io, const basil::Location& loc);
void write(stream& io, basil::Insn* insn);
void write(stream& io, const basil::Insn* insn);
void write(stream& io, const basil::Function& func);

#endif