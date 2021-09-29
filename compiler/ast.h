#ifndef BASIL_AST_H
#define BASIL_AST_H

#include "util/either.h"
#include "type.h"
#include "source.h"
#include "env.h"
#include "ssa.h"

namespace basil {
    struct Function;

    // Unique kinds for each distinct type of AST node.
    enum ASTKind {
        AST_DEF, AST_FUNCTION, AST_FUNCTION_STUB, AST_CALL, AST_OVERLOAD,
        AST_IF, AST_IF_ELSE, AST_WHILE, AST_DO,
        AST_INT, AST_FLOAT, AST_DOUBLE, AST_SYMBOL, AST_STRING, AST_CHAR, AST_VOID, AST_TYPE, AST_BOOL,
        AST_VAR, AST_UNKNOWN,
        AST_ADD, AST_SUB, AST_MUL, AST_DIV, AST_REM,
        AST_AND, AST_OR, AST_XOR, AST_NOT,
        AST_HEAD, AST_TAIL, AST_CONS,
        AST_LESS, AST_LESS_EQUAL, AST_GREATER, AST_GREATER_EQUAL, AST_EQUAL, AST_NOT_EQUAL,
        AST_ASSIGN, AST_COERCE
    };

    // Base class of all AST nodes. AST nodes are statically typed,
    // and represent code intended to be compiled and executed at runtime,
    // instead of evaluated wholly by the compiler.
    struct AST {
        Source::Pos pos;
        Type t, cached_type;
        ASTKind k;
        
        // Whether or not this AST node's type has been resolved already.
        bool resolved = false;

        // Constructs an AST node from the provided env, kind, and type.
        // The type an AST is constructed with should represent the type of
        // this particular AST node, not the value it ultimately produces -
        // that is to say, its type without any children. For leaf nodes that
        // wouldn't have any children regardless, this is simply the type of
        // the value the node represents. For branch nodes, this is usually
        // a function type expressing the type of the operation it performs.
        AST(Source::Pos pos, ASTKind kind, Type type);
        virtual ~AST();

        virtual void format(stream& io) const = 0;
        virtual rc<AST> clone() const = 0;

        // The pointers begin() and end() return behave like iterators. Typical
        // implementations point these to an internal array of child pointers,
        // at the beginning and just past the end respectively.
        virtual const rc<AST>* begin() const = 0;
        virtual const rc<AST>* end() const = 0;

        // Returns the number of children this AST node has.
        u32 children() const;

        // Returns the type of the sub-AST rooted at this node. 
        // This function will recursively type (and typecheck) any children of this 
        // node, then check their types against this node's desired parameters. If 
        // this check passes, this function will return the return type of this node's 
        // operation. If an error occurs, this function will report an error and 
        // return the error type.
        virtual Type type(rc<Env> env);

        // Writes SSA instructions to the active basic block in the provided function.
        // Returns the resulting IR value - either a terminal value like a variable or
        // constant, or the result variable of the generated instruction.
        virtual IRParam gen_ssa(rc<Env> env, rc<IRFunction> func) = 0;

        // Returns the kind of AST node this is.
        ASTKind kind() const;
    };

    // AST node constructors.

    rc<AST> ast_def(Source::Pos pos, Symbol name, rc<AST> init);
    rc<AST> ast_func_stub(Source::Pos pos, Type type, Symbol name);
    rc<AST> ast_func(Source::Pos pos, Type type, rc<Env> fn_env, optional<Symbol> name, rc<AST> body);
    rc<AST> ast_call(Source::Pos pos, rc<AST> func, const vector<rc<AST>>& args);
    rc<AST> ast_overload(Source::Pos pos, Type type, const map<Type, either<Builtin, rc<InstTable>>>& cases);

    // Constants

    rc<AST> ast_int(Source::Pos pos, Type type, i64 value);
    rc<AST> ast_float(Source::Pos pos, Type type, float value);
    rc<AST> ast_double(Source::Pos pos, Type type, double value);
    rc<AST> ast_symbol(Source::Pos pos, Type type, Symbol value);
    rc<AST> ast_string(Source::Pos pos, Type type, const ustring& value);
    rc<AST> ast_char(Source::Pos pos, Type type, rune value);
    rc<AST> ast_void(Source::Pos pos);
    rc<AST> ast_type(Source::Pos pos, Type type, Type value);
    rc<AST> ast_bool(Source::Pos pos, Type type, bool value);
    rc<AST> ast_var(Source::Pos pos, rc<Env> env, Symbol name);
    rc<AST> ast_unknown(Source::Pos pos, Type type);

    // Arithmetic

    rc<AST> ast_add(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_sub(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_mul(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_div(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_rem(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);

    // Logic

    rc<AST> ast_and(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_or(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_xor(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_not(Source::Pos pos, Type type, rc<AST> operand);

    // Comparisons 

    rc<AST> ast_less(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_less_equal(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_greater(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_greater_equal(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_equal(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);
    rc<AST> ast_not_equal(Source::Pos pos, Type type, rc<AST> left, rc<AST> right);

    // Control

    rc<AST> ast_if(Source::Pos pos, rc<AST> cond, rc<AST> ifTrue);
    rc<AST> ast_if_else(Source::Pos pos, rc<AST> cond, rc<AST> ifTrue, rc<AST> ifFalse);
    rc<AST> ast_while(Source::Pos pos, rc<AST> cond, rc<AST> body);
    rc<AST> ast_do(Source::Pos pos, const vector<rc<AST>>& exprs);

    template<typename... Args>
    rc<AST> ast_do(Source::Pos pos, const Args&... args) {
        return ast_do(pos, vector_of<rc<AST>>(args...));
    }

    // Lists

    rc<AST> ast_head(Source::Pos pos, Type type, rc<AST> operand);
    rc<AST> ast_tail(Source::Pos pos, Type type, rc<AST> operand);
    rc<AST> ast_cons(Source::Pos pos, Type type, rc<AST> head, rc<AST> tail);
    rc<AST> ast_coerce(Source::Pos pos, rc<AST> value, Type dest);

    // Utility Functions

    rc<AST> resolve_overloads(rc<Env> env, rc<AST> root);
    rc<IRFunction> get_ssa_function(rc<AST> function);
}

void write(stream& io, const rc<basil::AST>& param);

#endif