#include "eval.h"
#include "test.h"

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