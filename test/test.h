#ifndef BASIL_TEST_H
#define BASIL_TEST_H

#include "util/vec.h"
#include "util/hash.h"
#include "util/io.h"
#include "util/ustr.h"

struct exc_message {
    ustring msg;
};

struct test_node {
    ustring name;
    void(*callback)();
    test_node* next;
};

extern test_node* test_list;
extern void (*setup_fn)();

int main(int argc, char** argv);

#define TEST(name) void __test_##name(); \
    auto __dummy_##name = (test_list = new test_node{#name, __test_##name, test_list}); \
    void __test_##name()

#define ASSERT_EQUAL(a, b) if ((a) != (b)) throw exc_message{format<ustring>("line ", __LINE__, ": ", (a), " != ", (b))};
#define ASSERT_NOT_EQUAL(a, b) if ((a) == (b)) throw exc_message{format<ustring>("line ", __LINE__, ": ", (a), " == ", (b))};
#define ASSERT_LESS(a, b) if ((a) >= (b)) throw exc_message{format<ustring>("line ", __LINE__, ": ", (a), " >= ", (b))};
#define ASSERT_GREATER(a, b) if ((a) <= (b)) throw exc_message{format<ustring>("line ", __LINE__, ": ", (a), " <= ", (b))};
#define ASSERT_LESS_OR_EQUAL(a, b) if ((a) > (b)) throw exc_message{format<ustring>("line ", __LINE__, ": ", (a), " > ", (b))};
#define ASSERT_GREATER_OR_EQUAL(a, b) if ((a) < (b)) throw exc_message{format<ustring>("line ", __LINE__, ": ", (a), " < ", (b))};
#define ASSERT_TRUE(a) if (!(a)) throw exc_message{format<ustring>("line ", __LINE__, ": ", (bool)(a), " was not true")};
#define ASSERT_FALSE(a) if (a) throw exc_message{format<ustring>("line ", __LINE__, ": ", (bool)(a), " was not false")};
#define ASSERT_NO_ERRORS(src) { if (error_count()) { buffer b; print_errors(b, (src)); discard_errors(); throw exc_message{format<ustring>("line ", __LINE__, ": errors were reported:\n", b)}; } }

#define SETUP void __setup_fn(); auto __setup_dummy = (setup_fn = __setup_fn); void __setup_fn() 

#endif