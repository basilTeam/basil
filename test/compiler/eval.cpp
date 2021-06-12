#include "eval.h"
#include "test.h"
#include "driver.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

TEST(constants) {
    Value iconst = v_int({}, 1),
        fconst = v_float({}, 1.0f),
        dconst = v_double({}, 2.0),
        cconst = v_char({}, 'V'),
        sconst = v_string({}, "hello"),
        vconst = v_void({});
    rc<Env> env = ref<Env>(); // we'll just use an empty env

    ASSERT_EQUAL(iconst, eval(env, iconst).value); // all of these values should evaluate to themselves
    ASSERT_EQUAL(fconst, eval(env, fconst).value);
    ASSERT_EQUAL(dconst, eval(env, dconst).value);
    ASSERT_EQUAL(cconst, eval(env, cconst).value);
    ASSERT_EQUAL(sconst, eval(env, sconst).value);
    ASSERT_EQUAL(vconst, eval(env, vconst).value);
}

TEST(variables) {
    Value x = v_symbol({}, symbol_from("x")), y = v_symbol({}, symbol_from("y"));

    rc<Env> env = ref<Env>();
    env->def(symbol_from("x"), v_int({}, 1));

    ASSERT_EQUAL(eval(env, x).value, v_int({}, 1)); // we should look up x in env and get the right value (1)

    ASSERT_EQUAL(eval(env, y).value, v_error({})); // we should get an error for an undefined variable
    ASSERT_EQUAL(error_count(), 1);

    env->def(symbol_from("y"), v_int({}, 2));
    ASSERT_EQUAL(eval(env, y).value, v_int({}, 2)); // after defining y, we should get the right value and no additional error
    ASSERT_EQUAL(error_count(), 1);
}

TEST(simple_prefix_group) {
    Value code = compile("foo 1 \"hello\"", load_step, lex_step, parse_step);

    rc<Env> env = ref<Env>();
    env->def(symbol_from("foo"), v_void({}).with(f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR, P_VAR)));
    env = group(env, code);

    ASSERT_EQUAL(code, compile("(foo 1 \"hello\")", load_step, lex_step, parse_step));
}

TEST(simple_infix_group) {
    Value code = compile("1 foo \"hello\"", load_step, lex_step, parse_step);

    rc<Env> env = ref<Env>();
    env->def(symbol_from("foo"), v_void({}).with(f_callable(0, ASSOC_LEFT, P_VAR, P_SELF, P_VAR)));
    env = group(env, code);

    ASSERT_EQUAL(code, compile("(foo 1 \"hello\")", load_step, lex_step, parse_step));
}

TEST(left_associative) {
    Value code = compile("1 + 2 + 3", load_step, lex_step, parse_step);

    rc<Env> env = ref<Env>();
    env->def(symbol_from("+"), v_void({}).with(f_callable(20, ASSOC_LEFT, P_VAR, P_SELF, P_VAR)));
    group(env, code);
    
    ASSERT_EQUAL(code, compile("(+ (+ 1 2) 3)", load_step, lex_step, parse_step));
}

TEST(right_associative) {
    Value code = compile("1 + 2 + 3", load_step, lex_step, parse_step);

    rc<Env> env = ref<Env>();
    env->def(symbol_from("+"), v_void({}).with(f_callable(0, ASSOC_RIGHT, P_VAR, P_SELF, P_VAR)));
    env = group(env, code);

    ASSERT_EQUAL(code, compile("(+ 1 (+ 2 3))", load_step, lex_step, parse_step));
}

TEST(infix_precedence) {
    Value code = compile("1 + 2 * 3 + 4", load_step, lex_step, parse_step);

    rc<Env> env = ref<Env>();
    env->def(symbol_from("+"), v_void({}).with(f_callable(20, ASSOC_LEFT, P_VAR, P_SELF, P_VAR)));
    env->def(symbol_from("*"), v_void({}).with(f_callable(40, ASSOC_LEFT, P_VAR, P_SELF, P_VAR)));
    env = group(env, code);

    ASSERT_EQUAL(code, compile("(+ (+ 1 (* 2 3)) 4)", load_step, lex_step, parse_step));
}

TEST(term_parameter) {
    Value code1 = compile("foo bar baz", load_step, lex_step, parse_step);
    Value code2 = compile("foo bar baz", load_step, lex_step, parse_step);

    rc<Env> env1 = ref<Env>(), env2 = ref<Env>();
    env1->def(symbol_from("bar"), v_void({}).with(f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR)));
    env1->def(symbol_from("foo"), v_void({}).with(f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR)));
    
    env2->def(symbol_from("bar"), v_void({}).with(f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR)));
    env2->def(symbol_from("foo"), v_void({}).with(f_callable(0, ASSOC_RIGHT, P_SELF, P_TERM)));

    env1 = group(env1, code1);
    env2 = group(env2, code2);

    ASSERT_EQUAL(code1, compile("(foo (bar baz))", load_step, lex_step, parse_step)); // normal right associativity
    ASSERT_EQUAL(code2, compile("(foo bar) baz", load_step, lex_step, parse_step)); // bar is a term, and not grouped with baz

    Value code3 = compile("foo (1 + 2)", load_step, lex_step, parse_step);

    rc<Env> env3 = ref<Env>();
    env3->def(symbol_from("foo"), v_void({}).with(f_callable(0, ASSOC_RIGHT, P_SELF, P_TERM)));
    env3->def(symbol_from("+"), v_void({}).with(f_callable(0, ASSOC_LEFT, P_VAR, P_SELF, P_VAR)));
    env3 = group(env3, code3);

    ASSERT_EQUAL(code3, compile("(foo (1 + 2))", load_step, lex_step, parse_step)); // (1 + 2) is one term, so 1 + 2 should not be grouped
}

TEST(maximal_munch) {
    Value code = compile("foo 1 2 3", load_step, lex_step, parse_step);

    rc<Env> env = ref<Env>();
    env->def(symbol_from("foo"), v_void({}).with(*f_overloaded(0, ASSOC_RIGHT,
        f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR),
        f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR, P_VAR),
        f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR, P_VAR, P_VAR)
    )));
    env = group(env, code);
    
    ASSERT_EQUAL(code, compile("(foo 1 2 3)", load_step, lex_step, parse_step));
}

TEST(keyword_match) {
    Value code = compile("if 1 < 2 1 if 1 < 2 1 else 2", load_step, lex_step, parse_step);

    rc<Env> env = ref<Env>();
    env->def(symbol_from("<"), v_void({}).with(f_callable(5, ASSOC_LEFT, P_VAR, P_SELF, P_VAR)));
    env->def(symbol_from("if"), v_void({}).with(*f_overloaded(0, ASSOC_RIGHT, 
        f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR, P_VAR),
        f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR, P_VAR, p_keyword(symbol_from("else")), P_VAR)
    )));
    env = group(env, code);

     // we should only take the second form if 'else' is present
    ASSERT_EQUAL(code, compile("(if (< 1 2) 1) (if (< 1 2) 1 2)", load_step, lex_step, parse_step));

    Value code2 = compile("foo 1 end 2", load_step, lex_step, parse_step);
    env->def(symbol_from("foo"), v_void({}).with(*f_overloaded(0, ASSOC_RIGHT,
        f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR, P_VAR, P_VAR),
        f_callable(0, ASSOC_RIGHT, P_SELF, P_VAR, p_keyword(symbol_from("end")))
    )));
    env = group(env, code2);

    // we should prefer the shorter form because it has a keyword match
    ASSERT_EQUAL(code2, compile("(foo 1) 2", load_step, lex_step, parse_step));
}

TEST(form_callback) {
    FormCallback callback = [](rc<Env> env, const Value& term) -> rc<Form> {
        const Value& target = v_head(v_tail(term)); // get second parameter
        if (target.type != T_STRING) return F_TERM; // if it's not a string, return term
        Value sym = v_symbol(target.pos, symbol_from(target.data.string->data));
        env = resolve_form(env, sym);
        return sym.form; // return form of the symbol
    };

    Value code = compile("1 + 2 (like \"+\") 3", load_step, lex_step, parse_step);

    rc<Env> env = ref<Env>();
    env->def(symbol_from("+"), v_void({}).with(f_callable(20, ASSOC_LEFT, P_VAR, P_SELF, P_VAR)));
    env->def(symbol_from("like"), v_void({}).with(f_callable(0, ASSOC_RIGHT, callback, P_SELF, P_VAR)));
    env = group(env, code);

    ASSERT_EQUAL(code, compile("((like \"+\") (+ 1 2) 3)", load_step, lex_step, parse_step));
}

TEST(simple_math) {
    Value code = compile("1 + 2 * 3 - 4", load_step, lex_step, parse_step);
    EvalResult result = eval(root_env(), code);

    ASSERT_EQUAL(result.value, v_int({}, 3));
}