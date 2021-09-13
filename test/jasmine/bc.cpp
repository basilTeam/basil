#include "jasmine/bc.h"
#include "test.h"

using namespace jasmine;

TEST(simple_parse) {
    buffer b;
    write(b, R"(
type Tree { 
    left : 8, 
    right : 8, 
    val : i64 * 4 
}

mov i64 %0, [%1 + Tree.right]

fib:    frame
        param i64 %0
        local i64 %1
        jl i64 %0, 2 .L0
        push i64 %0
        sub i64 %1, %0, 1
        call i64 %1, fib(i64 %1)
        xchg i64 %0, %1
        sub i64 %1, %1, 2
        call i64 %1, fib(i64 %1)
        add i64 %0, %0, %1
        pop i64 %1
.L0:    ret i64 %0
)");
    Context ctx;
    Insn insn = parse_insn(ctx, b);
    Insn insn2 = parse_insn(ctx, b);

    // ASSERT_TRUE(insn.opcode == OP_ADD);
    // ASSERT_TRUE(insn.type.kind == K_I64);
    // ASSERT_EQUAL(insn.params.size(), 3);
    // ASSERT_TRUE(insn2.params[0].kind == PK_MEM);

    println("");
    print_insn(ctx, _stdout, insn);
    print_insn(ctx, _stdout, insn2);
}