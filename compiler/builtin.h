#ifndef BASIL_BUILTIN_H
#define BASIL_BUILTIN_H

#include "util/defs.h"
#include "util/rc.h"
#include "util/vec.h"

namespace basil {
    class Type;
    class Value;
    class Env;

    enum BuiltinFlags { AUTO_LOWER = 0, NO_AUTO_LOWER = 1 };

    class Builtin {
        using Function = Value (*)(ref<Env>, const Value&);
        const Type* _type;
        Function _eval, _compile;
        u32 _flags;
        vector<u64> _args;

      public:
        Builtin();
        Builtin(const Type* type, Function eval, Function compile, BuiltinFlags flags = AUTO_LOWER);
        Builtin(const Type* type, Function eval, Function compile, const vector<u64>& args,
                BuiltinFlags flags = AUTO_LOWER);

        const Type* type() const;
        const vector<u64>& args() const;
        Value eval(ref<Env> env, const Value& args) const;
        Value compile(ref<Env> env, const Value& args) const;
        bool should_lower() const;
        bool runtime_only() const;
    };

    extern void define_builtins(ref<Env> env);

    extern Builtin ADD_INT, ADD_SYMBOL, SUB, MUL, DIV, REM, AND, OR, XOR, NOT, EQUALS, NOT_EQUALS, LESS, GREATER,
        LESS_EQUAL, GREATER_EQUAL, IS_EMPTY, HEAD, TAIL, CONS, DICT_PUT, DICT_IN, DICT_NOT_IN, DICT_REMOVE, DISPLAY,
        READ_LINE, READ_WORD, READ_INT, LENGTH, AT_INT, AT_LIST, AT_ARRAY_TYPE, AT_DYNARRAY_TYPE, AT_DICT_TYPE,
        AT_MODULE, AT_DICT, AT_DICT_LIST, STRCAT, SUBSTR, ANNOTATE, TYPEOF, TYPEDEF, LIST_TYPE, OF_TYPE_MACRO, OF_TYPE,
        ASSIGN, IF;
} // namespace basil

#endif