#include "util/vec.h"
#include "test.h"

TEST(push) {
    vector<int> v;
    v.push(1);
    v.push(2);
    v.push(3);
    ASSERT_EQUAL(v.size(), 3);
    ASSERT_EQUAL(v[0], 1);
    ASSERT_EQUAL(v[1], 2);
    ASSERT_EQUAL(v[2], 3);
}

TEST(pop_swap) {
    vector<int> v, w;
    v.push(1);
    v.push(2);
    v.push(3);
    ASSERT_EQUAL(v.size(), 3);
    ASSERT_EQUAL(w.size(), 0);
    for (int i = 0; i < 3; i ++) w.push(v.back()), v.pop();
    ASSERT_EQUAL(v.size(), 0);
    ASSERT_EQUAL(w.size(), 3);
    ASSERT_EQUAL(w[0], 3);
    ASSERT_EQUAL(w[1], 2);
    ASSERT_EQUAL(w[2], 1);
    for (int i = 0; i < 3; i ++) v.push(w.back()), w.pop();
    ASSERT_EQUAL(v.size(), 3);
    ASSERT_EQUAL(w.size(), 0);
    ASSERT_EQUAL(v[0], 1);
    ASSERT_EQUAL(v[1], 2);
    ASSERT_EQUAL(v[2], 3);
}

TEST(clear) {
    vector<double> v;
    v.push(1);
    v.push(2);
    v.push(3);
    ASSERT_EQUAL(v.size(), 3);
    v.clear();
    ASSERT_EQUAL(v.size(), 0);
    v.push(4);
    ASSERT_EQUAL(v.size(), 1);
    v.clear();
    ASSERT_EQUAL(v.size(), 0);
}

TEST(deep_copy) {
    vector<int> v;
    v.push(1);
    v.push(2);
    vector<int> w = v;
    w.push(3);
    ASSERT_EQUAL(v.size(), 2);
    ASSERT_EQUAL(w.size(), 3);
    w[0] = 5, w[1] = 4;
    ASSERT_EQUAL(v[0], 1);
    ASSERT_EQUAL(v[1], 2);
}

TEST(deep_assign) {
    vector<int> v;
    v.push(1);
    v.push(2);
    vector<int> w;
    w.push(3);
    w.push(4);
    w.push(5);
    ASSERT_EQUAL(w[0], 3);
    ASSERT_EQUAL(w[1], 4);
    ASSERT_EQUAL(w[2], 5);
    ASSERT_EQUAL(w.size(), 3);
    w = v;
    ASSERT_EQUAL(w.size(), 2);
    ASSERT_EQUAL(w[0], 1);
    ASSERT_EQUAL(w[1], 2);
    w[0] = 6;
    w[1] = 7;
    w.push(8);
    ASSERT_EQUAL(v.size(), 2);
    ASSERT_EQUAL(v[0], 1);
    ASSERT_EQUAL(v[1], 2);
    v[0] = 9;
    ASSERT_EQUAL(w[0], 6);
}

TEST(front_back) {
    vector<int> v;
    v.push(1);
    ASSERT_EQUAL(v.front(), 1);
    ASSERT_EQUAL(v.back(), 1);
    v.push(2);
    ASSERT_EQUAL(v.front(), 1);
    ASSERT_EQUAL(v.back(), 2);
    v.push(3);
    ASSERT_EQUAL(v.back(), 3);
    v.pop();
    v.pop();
    ASSERT_EQUAL(v.back(), 1);
    v[0] = 4;
    ASSERT_EQUAL(v.front(), 4);
    ASSERT_EQUAL(v.back(), 4);
}

struct Destructible {
    int* ptr;
    Destructible(int* ptr) {
        this->ptr = ptr;
    }

    ~Destructible() {
        *ptr += 1;
    }
};

struct Copyable {
    int* ptr;
    Copyable(int* ptr) {
        this->ptr = ptr;
    }

    Copyable(const Copyable& other) {
        ptr = other.ptr;
        *ptr += 1;
    }
};

void dtor_helper(int* p) {
    vector<Destructible> v;
    v.push({p});
    v.push({p});
    v.push({p});
    v[0] = {p};
    v[1] = {p};
}

TEST(item_dtors) {
    int counter = 0;
    dtor_helper(&counter);
    ASSERT_EQUAL(counter, 8); // 3 from vec + 5 from temporaries
}

TEST(item_copy_ctors) {
    int counter = 0;
    vector<Copyable> v;
    v.push({&counter});
    v.push({&counter});
    v.push({&counter});
    ASSERT_EQUAL(counter, 3);
    vector<Copyable> w = v;
    ASSERT_EQUAL(counter, 6);
}

TEST(large_vector) {
    vector<int> v;
    for (int i = 0; i < 1000; i ++) v.push(i);
    vector<int> w;
    for (int i = 0; i < 1000; i ++) w.push(v[1000 - i - 1]);
    for (int i = 0, j = 999; i < j; i ++, j --) ASSERT_EQUAL(v[i], w[j]);
}