/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "env.h"
#include "value.h"
#include "test.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

TEST(vars) {
    Symbol S1 = symbol_from("foo"), S2 = symbol_from("bar"), S3 = symbol_from("baz");
    Value V1 = v_int({}, 1), V2 = v_int({}, 2), V3 = v_int({}, 3);

    rc<Env> env = ref<Env>();
    env->def(S1, V1);
    env->def(S2, V2);
    
    auto v1 = env->find(S1); // variables in the environment should return the right value
    ASSERT_TRUE(v1);
    ASSERT_EQUAL(*v1, V1);
    auto v2 = env->find(S2);
    ASSERT_TRUE(v2);
    ASSERT_EQUAL(*v2, V2);

    ASSERT_FALSE(env->find(S3)); // variables not in the environment should return a none optional

    env->def(S3, V3);

    auto v3 = env->find(S3); // ...but we should get the right value after we define them
    ASSERT_TRUE(v3);
    ASSERT_EQUAL(*v3, V3);
}

TEST(parent) {
    Symbol S1 = symbol_from("foo"), S2 = symbol_from("bar"), S3 = symbol_from("baz");
    Value V1 = v_int({}, 1), V2 = v_int({}, 2), V3 = v_int({}, 3);

    rc<Env> e1 = ref<Env>(), e2 = ref<Env>(e1), e3 = ref<Env>(e1); // e1 <--+-- e2
                                                                   //       |
                                                                   //       +-- e3
                                    
    e1->def(S1, V1);
    e2->def(S2, V2);
    e3->def(S3, V3);

    auto a = e1->find(S1); // re-checking that e1.a has the right value
    ASSERT_TRUE(a);
    ASSERT_EQUAL(*a, V1);

    auto b = e2->find(S1), c = e3->find(S1); // should find S1 in the parent
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    ASSERT_EQUAL(*a, V1);
    ASSERT_EQUAL(*b, V1);
    ASSERT_EQUAL(*a, *b);

    auto d = e2->find(S2), e = e3->find(S3), f = e1->find(S2), g = e1->find(S3);
    ASSERT_TRUE(d); // we should find these values in the child envs
    ASSERT_TRUE(e);
    ASSERT_EQUAL(*d, V2);
    ASSERT_EQUAL(*e, V3);

    ASSERT_FALSE(f); // but we shouldn't find them in the root env somehow
    ASSERT_FALSE(g);

    e1->def(S1, V3);
    auto h = e2->find(S1), i = e3->find(S1);
    ASSERT_TRUE(h);
    ASSERT_TRUE(i);
    ASSERT_EQUAL(*h, V3); // looking up the redefined variable should return the new value, not some cached version
    ASSERT_EQUAL(*i, V3); 
}

TEST(shadowing) {
    Symbol FOO = symbol_from("foo");
    Value V1 = v_int({}, 1), V2 = v_int({}, 2), V3 = v_int({}, 3);

    rc<Env> e1 = ref<Env>(), e2 = ref<Env>(e1), e3 = ref<Env>(e2), // e1 <--+-- e2 <- e3
            e4 = ref<Env>(e1), e5 = ref<Env>(e4);                  //       |
                                                                   //       +-- e4 <- e5

    e1->def(FOO, V1);
    auto a = e2->find(FOO), b = e3->find(FOO), c = e4->find(FOO), d = e5->find(FOO);
    ASSERT_TRUE(a); // all child envs should be able to resolve FOO
    ASSERT_TRUE(b);
    ASSERT_TRUE(c);
    ASSERT_TRUE(d);
    ASSERT_EQUAL(*a, V1);
    ASSERT_EQUAL(*b, V1);
    ASSERT_EQUAL(*c, V1);
    ASSERT_EQUAL(*d, V1);

    e2->def(FOO, V2);
    auto g = e1->find(FOO), h = e2->find(FOO), i = e3->find(FOO);
    ASSERT_EQUAL(*g, V1); // defining FOO in e2 should result in e2 and e3 resolving it to V2.
    ASSERT_EQUAL(*h, V2); // looking it up in e1 should result in the original value.
    ASSERT_EQUAL(*i, V2);

    e3->def(FOO, V3);
    auto j = e1->find(FOO), k = e2->find(FOO), l = e3->find(FOO);
    ASSERT_EQUAL(*j, V1); // each env should resolve FOO to its own local value
    ASSERT_EQUAL(*k, V2);
    ASSERT_EQUAL(*l, V3);

    e5->def(FOO, V3);
    auto m = e1->find(FOO), n = e4->find(FOO), o = e5->find(FOO);
    ASSERT_EQUAL(*m, V1); // if we redefine FOO in e5, a leaf env, e4 should still resolve FOO to 
    ASSERT_EQUAL(*n, V1); // the value in e1.
    ASSERT_EQUAL(*o, V3);
}