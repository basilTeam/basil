#include "type.h"
#include "test.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

TEST(primitive_equality) {
    ASSERT_EQUAL(T_INT, T_INT);
    ASSERT_NOT_EQUAL(T_SYMBOL, T_BOOL);
}

TEST(list_equality) {
    Type a = t_list(T_INT), b = t_list(T_INT), c = t_list(T_SYMBOL);
    ASSERT_EQUAL(a, b);
    ASSERT_NOT_EQUAL(a, c);
    ASSERT_NOT_EQUAL(b, c);
}

TEST(numeric_coercion) {
    ASSERT_TRUE(T_INT.coerces_to(T_FLOAT));
    ASSERT_TRUE(T_FLOAT.coerces_to(T_DOUBLE));
    ASSERT_TRUE(T_INT.coerces_to(T_DOUBLE));
    ASSERT_FALSE(T_FLOAT.coerces_to(T_INT));
    ASSERT_FALSE(T_DOUBLE.coerces_to(T_INT));
    ASSERT_FALSE(T_DOUBLE.coerces_to(T_FLOAT));
}

TEST(list_and_void_coercion) {
    Type ilist = t_list(T_INT), flist = t_list(T_FLOAT);
    ASSERT_TRUE(T_VOID.coerces_to(ilist));  // void can coerce to any list type, but not in reverse
    ASSERT_TRUE(T_VOID.coerces_to(flist));
    ASSERT_FALSE(ilist.coerces_to(T_VOID));
    
    Type tlist = t_list(T_TYPE);
    ASSERT_TRUE(tlist.coerces_to(T_TYPE)); // [type] can convert to type
    ASSERT_FALSE(T_TYPE.coerces_to(tlist));

    Type alist = t_list(T_ANY);
    ASSERT_TRUE(ilist.coerces_to(alist)); // all lists can convert to [any]
    ASSERT_TRUE(flist.coerces_to(alist));
    ASSERT_TRUE(tlist.coerces_to(alist));

    ASSERT_FALSE(alist.coerces_to(ilist)); // ...but not in reverse
}

TEST(tuple_equality) {
    Type a = t_tuple(T_INT, T_INT), b = t_tuple(T_INT, T_INT), c = t_incomplete_tuple(T_INT, T_INT),
        d = t_incomplete_tuple(T_INT, T_INT), e = t_tuple(T_INT, T_FLOAT), f = t_tuple(T_FLOAT, T_FLOAT);
    
    ASSERT_EQUAL(a, a);
    ASSERT_EQUAL(a, b);
    ASSERT_NOT_EQUAL(a, c);
    ASSERT_EQUAL(c, d);
    ASSERT_NOT_EQUAL(a, e);
    ASSERT_NOT_EQUAL(c, e);
    ASSERT_NOT_EQUAL(e, f);
}

TEST(complete_tuple_coercion) {
    Type a = t_tuple(T_INT, T_INT), b = t_tuple(T_FLOAT, T_INT), c = t_tuple(T_INT, T_FLOAT),
        d = t_tuple(T_FLOAT, T_FLOAT);
    
    ASSERT_TRUE(a.coerces_to(b)); // ints in any position should coerce to corresponding floats
    ASSERT_TRUE(a.coerces_to(c));
    ASSERT_TRUE(a.coerces_to(d));
    ASSERT_TRUE(b.coerces_to(d));
    ASSERT_TRUE(c.coerces_to(d));

    ASSERT_FALSE(d.coerces_to(a)); // floats in any position should not coerce to corresponding ints
    ASSERT_FALSE(d.coerces_to(b));
    ASSERT_FALSE(d.coerces_to(c));
    ASSERT_FALSE(b.coerces_to(a));
    ASSERT_FALSE(c.coerces_to(a));

    Type e = t_tuple(T_INT, T_INT, T_INT); // can't coerce to larger tuples
    ASSERT_FALSE(a.coerces_to(e));
    ASSERT_FALSE(e.coerces_to(a));
}

TEST(incomplete_tuple_coercion) {
    Type a = t_tuple(T_INT, T_INT), b = t_incomplete_tuple(), 
        c = t_incomplete_tuple(T_INT), d = t_incomplete_tuple(T_INT, T_INT);
    
    ASSERT_TRUE(a.coerces_to(b)); // a should coerce to all the incomplete examples
    ASSERT_TRUE(a.coerces_to(c));
    ASSERT_TRUE(a.coerces_to(d));

    ASSERT_TRUE(d.coerces_to(c)); // more complete tuples should coerce to less complete ones
    ASSERT_TRUE(c.coerces_to(b));
    ASSERT_TRUE(d.coerces_to(b));

    ASSERT_FALSE(b.coerces_to(a)); // less complete tuples should not coerce to more complete ones
    ASSERT_FALSE(c.coerces_to(a));
    ASSERT_FALSE(d.coerces_to(a));

    Type e = t_incomplete_tuple(T_FLOAT), f = t_incomplete_tuple(T_FLOAT, T_FLOAT);
    ASSERT_TRUE(a.coerces_to(e)); // other type coercion can occur along with incompleteness
    ASSERT_TRUE(a.coerces_to(f));
    ASSERT_TRUE(e.coerces_to(b));
    ASSERT_TRUE(f.coerces_to(b));

    ASSERT_TRUE(f.coerces_to(e)); // still can't coerce float to int
    ASSERT_FALSE(f.coerces_to(c));
}

TEST(type_tuple_coercion) {
    Type a = t_tuple(T_TYPE, T_TYPE), b = t_incomplete_tuple(T_TYPE, T_TYPE, T_TYPE), c = t_tuple(T_TYPE, T_INT);
    ASSERT_TRUE(a.coerces_to(T_TYPE));
    ASSERT_TRUE(b.coerces_to(T_TYPE));
    ASSERT_FALSE(c.coerces_to(T_TYPE));
}

TEST(array_equality) {
    Type a = t_array(T_INT), b = t_array(T_INT), c = t_array(T_BOOL);

    ASSERT_EQUAL(a, a);
    ASSERT_EQUAL(a, b);
    ASSERT_NOT_EQUAL(a, c);

    Type d = t_array(T_INT, 2), e = t_array(T_INT, 3), f = t_array(T_INT, 3);

    ASSERT_NOT_EQUAL(d, e);
    ASSERT_NOT_EQUAL(d, f);
    ASSERT_EQUAL(e, f);
    ASSERT_NOT_EQUAL(d, a);
}

TEST(array_coercion) {
    Type a = t_array(T_INT, 2), b = t_array(T_INT, 3), c = t_array(T_INT);

    ASSERT_TRUE(a.coerces_to(c)); // sized coerces to unsized
    ASSERT_TRUE(b.coerces_to(c));

    ASSERT_FALSE(c.coerces_to(a)); // unsized shouldn't coerce to sized

    ASSERT_FALSE(b.coerces_to(a)); // sized arrays can't coerce among each other
    ASSERT_FALSE(a.coerces_to(b));

    Type d = t_array(T_FLOAT, 2), e = t_array(T_FLOAT);

    ASSERT_TRUE(d.coerces_to(e)); // as above
    ASSERT_FALSE(e.coerces_to(d)); 

    ASSERT_FALSE(a.coerces_to(d)); // arrays are invariant in terms of element type
    ASSERT_FALSE(a.coerces_to(e));
    ASSERT_FALSE(b.coerces_to(e));
    ASSERT_FALSE(c.coerces_to(e));

    Type f = t_array(T_ANY, 2), g = t_array(T_ANY);
    ASSERT_TRUE(a.coerces_to(f)); // can coerce to sized any array of the same size
    ASSERT_TRUE(d.coerces_to(f)); 
    ASSERT_FALSE(b.coerces_to(f));

    ASSERT_TRUE(a.coerces_to(g)); // all arrays can coerce to unsized any array
    ASSERT_TRUE(b.coerces_to(g));
    ASSERT_TRUE(d.coerces_to(g));
    ASSERT_TRUE(f.coerces_to(g));
}

TEST(union_equality) {
    Type a = t_union(T_INT, T_BOOL), b = t_union(T_INT, T_BOOL), c = t_union(T_INT, T_FLOAT),
        d = t_union(T_INT, T_BOOL, T_FLOAT), e = t_union(T_BOOL, T_INT);
    ASSERT_EQUAL(a, a);
    ASSERT_EQUAL(a, b);
    ASSERT_EQUAL(a, e);
    ASSERT_NOT_EQUAL(a, c);
    ASSERT_NOT_EQUAL(a, d);
    ASSERT_NOT_EQUAL(d, c);
}

TEST(union_coercion) {
    Type a = t_union(T_INT, T_BOOL), b = t_union(T_INT, T_BOOL, T_FLOAT),
        c = t_union(T_FLOAT, T_BOOL), d = t_union(T_FLOAT, T_BOOL, T_DOUBLE);
    
    ASSERT_TRUE(a.coerces_to(b)); // can coerce to any superset union
    ASSERT_TRUE(c.coerces_to(d));
    ASSERT_TRUE(c.coerces_to(b));

    ASSERT_FALSE(a.coerces_to(c)); // members cannot coerce (i.e. no int -> float)
    ASSERT_FALSE(a.coerces_to(d));

    Type e = t_tuple(T_INT, T_FLOAT), f = t_named(symbol_from("Foo"), T_INT);
    Type g = t_union(T_INT, T_STRING, e, f);
    
    ASSERT_TRUE(T_INT.coerces_to(g)); // all types can coerce to unions containing them
    ASSERT_TRUE(T_STRING.coerces_to(g));
    ASSERT_TRUE(e.coerces_to(g));
    ASSERT_TRUE(f.coerces_to(g));
    ASSERT_FALSE(T_FLOAT.coerces_to(g));
    ASSERT_FALSE(t_named(symbol_from("Bar"), T_INT).coerces_to(g));

    ASSERT_FALSE(g.coerces_to(T_INT)); // unions cannot coerce to members, however
    ASSERT_FALSE(g.coerces_to(e));
    Type h = t_union(e, f);
    ASSERT_FALSE(g.coerces_to(h)); // ...nor subsets

    ASSERT_TRUE(h.coerces_to(g));
}

TEST(intersect_equality) {
    Type a = t_intersect(T_INT, T_FLOAT), b = t_intersect(T_INT, T_FLOAT),
        c = t_intersect(T_INT, T_FLOAT, T_BOOL), d = t_intersect(T_INT, T_DOUBLE),
        e = t_intersect(T_FLOAT, T_INT);
    ASSERT_EQUAL(a, a);
    ASSERT_EQUAL(a, b);
    ASSERT_EQUAL(a, e);
    ASSERT_NOT_EQUAL(a, c);
    ASSERT_NOT_EQUAL(a, d);
}

TEST(intersect_coercion) {
    Type a = t_intersect(T_INT, T_FLOAT, T_BOOL), b = t_intersect(T_INT, T_BOOL), c = t_intersect(T_INT, T_DOUBLE);
    ASSERT_TRUE(a.coerces_to(T_INT)); // intersections can coerce to their members
    ASSERT_TRUE(a.coerces_to(T_FLOAT));
    ASSERT_TRUE(a.coerces_to(T_BOOL));

    ASSERT_FALSE(T_INT.coerces_to(a)); // ...but the reverse is not true
    ASSERT_FALSE(T_FLOAT.coerces_to(a));
    ASSERT_FALSE(T_BOOL.coerces_to(a));

    ASSERT_TRUE(a.coerces_to(b)); // intersections can coerce to subset intersections of their members
    ASSERT_FALSE(a.coerces_to(c));
}

TEST(function_equality) {
    Type a = t_function(T_INT, T_BOOL), b = t_function(T_INT, T_BOOL), c = t_function(T_BOOL, T_INT),
        d = t_function(T_FLOAT, T_BOOL);
    
    ASSERT_EQUAL(a, b);
    ASSERT_NOT_EQUAL(a, c);
    ASSERT_NOT_EQUAL(a, d);
}

TEST(function_coercion) {
    Type a = t_function(T_INT, T_BOOL), b = t_function(T_FLOAT, T_BOOL), c = t_function(T_ANY, T_BOOL),
        d = t_function(T_INT, T_ANY), e = t_function(T_ANY, T_ANY);
    
    ASSERT_FALSE(a.coerces_to(b)); // functions are not variant in argument or return types
     
    ASSERT_TRUE(a.coerces_to(c)); // functions should be coercible to more generic versions of themselves
    ASSERT_TRUE(a.coerces_to(d));
    ASSERT_TRUE(a.coerces_to(e));
    ASSERT_TRUE(b.coerces_to(c));
    ASSERT_FALSE(b.coerces_to(d));
    ASSERT_TRUE(b.coerces_to(e));

    ASSERT_FALSE(c.coerces_to(a)); // but functions cannot be coerced in the reverse direction
    ASSERT_FALSE(d.coerces_to(a));
    ASSERT_FALSE(e.coerces_to(c));
    ASSERT_FALSE(e.coerces_to(d));
    ASSERT_FALSE(e.coerces_to(a));

    Type f = t_function(T_INT, T_TYPE), g = t_function(T_TYPE, T_INT);

    ASSERT_TRUE(f.coerces_to(T_TYPE)); // function types that return types can be coerced to types themselves
    ASSERT_FALSE(g.coerces_to(T_TYPE));
}

TEST(struct_equality) {
    Type a = t_struct(symbol_from("x"), T_INT, symbol_from("y"), T_INT),
        b = t_struct(symbol_from("y"), T_INT, symbol_from("x"), T_INT),
        c = t_struct(symbol_from("x"), T_INT, symbol_from("y"), T_FLOAT),
        d = t_struct(symbol_from("x"), T_INT),
        e = t_struct(symbol_from("x"), T_INT, symbol_from("y"), T_INT, symbol_from("z"), T_INT);
    ASSERT_EQUAL(a, a);
    ASSERT_EQUAL(a, b);
    ASSERT_NOT_EQUAL(a, c);
    ASSERT_NOT_EQUAL(a, d);
    ASSERT_NOT_EQUAL(a, e);

    Type f = t_incomplete_struct(symbol_from("x"), T_INT, symbol_from("y"), T_INT), 
        g = t_incomplete_struct(symbol_from("y"), T_INT, symbol_from("x"), T_INT),
        h = t_incomplete_struct(symbol_from("y"), T_INT);
    ASSERT_EQUAL(f, g);
    ASSERT_NOT_EQUAL(f, h);
    ASSERT_NOT_EQUAL(f, e);
}

TEST(struct_coercion) {
    Type a = t_struct(symbol_from("x"), T_INT, symbol_from("y"), T_INT),
        b = t_struct(symbol_from("x"), T_INT, symbol_from("y"), T_FLOAT),
        c = t_struct(symbol_from("x"), T_INT),
        d = t_incomplete_struct(symbol_from("x"), T_INT, symbol_from("y"), T_INT),
        e = t_incomplete_struct(symbol_from("x"), T_INT),
        f = t_incomplete_struct(symbol_from("x"), T_FLOAT),
        g = t_incomplete_struct(symbol_from("y"), T_INT),
        h = t_incomplete_struct();
    ASSERT_TRUE(a.coerces_to(a));
    ASSERT_FALSE(a.coerces_to(b)); // structs cannot coerce elementwise, or to subset structs
    ASSERT_FALSE(a.coerces_to(c));

    ASSERT_FALSE(a.coerces_to(d)); // structs can coerce to incomplete subset structs, as long as they have strictly fewer elements
    ASSERT_TRUE(a.coerces_to(e));
    ASSERT_TRUE(b.coerces_to(e));
    ASSERT_FALSE(c.coerces_to(e));
    ASSERT_FALSE(a.coerces_to(f));
    ASSERT_TRUE(a.coerces_to(g));
    ASSERT_FALSE(b.coerces_to(g));
    ASSERT_TRUE(a.coerces_to(h));
    ASSERT_TRUE(b.coerces_to(h));
    ASSERT_TRUE(c.coerces_to(h));
}