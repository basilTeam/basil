#include "value.h"
#include "test.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

TEST(foo) {
    Value v = v_list({}, t_list(T_INT), v_double({}, 1.0), v_void({}));
}