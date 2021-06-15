#include "driver.h"
#include "eval.h"
#include "test.h"

using namespace basil;

SETUP {
    init();
}

TEST(arithmetic) {
    ASSERT_EQUAL(compile("1 + 2 * 3", load_step, lex_step, parse_step, eval_step), v_int({}, 7));
    ASSERT_EQUAL(compile("(1 + 2) * 3", load_step, lex_step, parse_step, eval_step), v_int({}, 9));
}

TEST(define) {
    compile("def x 1", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("x", load_step, lex_step, parse_step, eval_step), v_int({}, 1));

    compile("def (id x?) x", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("id 1", load_step, lex_step, parse_step, eval_step), v_int({}, 1));

    compile("def (x? add y?) (x + y)", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("1 add 2", load_step, lex_step, parse_step, eval_step), v_int({}, 3));

    compile("def (apply f? x? y?) (x f y)", load_step, lex_step, parse_step, eval_step);
    ASSERT_EQUAL(compile("apply add 1 2", load_step, lex_step, parse_step, eval_step), v_int({}, 3));
}