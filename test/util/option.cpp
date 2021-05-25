#include "util/option.h"
#include "test.h"

TEST(present) {
    auto a = some<int>(1), b = none<int>();
    ASSERT_TRUE(a);
    ASSERT_FALSE(b);
}

TEST(copy) {
    auto a = some<int>(1);
    ASSERT_TRUE(a);
    ASSERT_EQUAL(*a, 1);
    auto b = a;
    ASSERT_TRUE(b);
    ASSERT_EQUAL(*b, 1);
    b = none<int>();
    auto c = b;
    ASSERT_FALSE(b);
    ASSERT_FALSE(c);
}

TEST(assign) {
    auto a = some<int>(1), b = some<int>(2), c = none<int>();
    auto d = a;
    ASSERT_TRUE(d);
    ASSERT_EQUAL(*d, 1);
    d = b;
    ASSERT_TRUE(d);
    ASSERT_EQUAL(*d, 2);
    *b = 3;
    ASSERT_EQUAL(*b, 3);
    ASSERT_EQUAL(*d, 2);
    d = c;
    ASSERT_FALSE(d);
}

TEST(apply) {
    auto a = some<int>(3);
    ASSERT_EQUAL(*a, 3);
    a = apply(a, [](int i) -> int { return i + 1; });
    ASSERT_TRUE(a);
    ASSERT_EQUAL(*a, 4);
    a = none<int>();
    ASSERT_FALSE(a);
    a = apply(a, [](int i) -> int { return i * 2; });
    ASSERT_FALSE(a);
}