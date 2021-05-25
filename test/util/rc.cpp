#include "util/rc.h"
#include "test.h"

TEST(dereference) {
    auto a = ref(1), b = ref(2);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    rc<int> c;
    ASSERT_FALSE(c);
    c = ref(*a + *b);
    ASSERT_TRUE(c);
    ASSERT_EQUAL(*c, 3);
}

TEST(null_ref) {
    rc<int> a, b;
    ASSERT_FALSE(a);
    ASSERT_FALSE(b);
    rc<int> c = a;
    ASSERT_FALSE(c);
    a = ref(1);
    b = a;
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    a = c;
    b = c;
    ASSERT_FALSE(a);
    ASSERT_FALSE(b);
}

class A {
public:
    virtual ~A() {}
    virtual int foo() const = 0;
};

class B : public A {
public:
    int foo() const override { return 1; };
};

class C : public A {
public:
    int foo() const override { return 2; };
};

void write(stream& io, const A& a) {
    write(io, "A");
}

TEST(virtual_call) {
    rc<A> b = ref<B>(), c = ref<C>();
    ASSERT_TRUE(b);
    ASSERT_TRUE(c);
    ASSERT_EQUAL(b->foo(), 1);
    ASSERT_EQUAL(c->foo(), 2);
}

TEST(inheritance) {
    rc<B> b = ref<B>();
    rc<C> c = ref<C>();
    rc<A> a;
    ASSERT_FALSE(a);
    a = b;
    ASSERT_TRUE(a);
    ASSERT_EQUAL(a->foo(), 1);
    a = c;
    ASSERT_TRUE(a);
    ASSERT_EQUAL(a->foo(), 2);
}