#ifndef BASIL_BUILTIN_H
#define BASIL_BUILTIN_H

#include "util/defs.h"
#include "util/rc.h"

namespace basil {
    class Type;
    class Value;
    class Env;

    class Builtin {
        using Function = Value(*)(ref<Env>, const Value&);
        const Type* _type;
        Function _eval, _compile;
    public:
        Builtin();
        Builtin(const Type* type, Function eval, Function compile);

        const Type* type() const;
        Value eval(ref<Env> env, const Value& args) const;
        Value compile(ref<Env> env, const Value& args) const;
    };

    extern void define_builtins(ref<Env> env);

    extern Builtin
        ADD, SUB, MUL, DIV, REM,
        AND, OR, XOR, NOT,
        EQUALS, NOT_EQUALS, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL,
        IS_EMPTY, HEAD, TAIL, CONS,
        DISPLAY, READ_LINE, READ_WORD, READ_INT,
        LENGTH, AT, STRCAT, SUBSTR,
        ANNOTATE, TYPEOF, LIST_TYPE,
        ASSIGN, IF;
}

#endif