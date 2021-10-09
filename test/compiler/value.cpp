/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "value.h"
#include "test.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

TEST(foo) {
    Value v = v_list({}, t_list(T_ANY), v_double({}, 1.0), v_void({}));
}