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
)");
    Context ctx;
    Insn insn = parse_insn(ctx, b);

    ASSERT_TRUE(insn.opcode == OP_TYPE);
    ASSERT_TRUE(insn.type.kind == K_STRUCT);

    Insn insn2 = parse_insn(ctx, b);

    ASSERT_TRUE(insn2.opcode == OP_MOV);
    ASSERT_TRUE(insn2.type.kind == K_I64);
    ASSERT_TRUE(insn2.params.size() == 2);
    ASSERT_TRUE(insn2.params[0].kind == PK_REG);
    ASSERT_EQUAL(insn2.params[0].data.reg.id, 0);
    ASSERT_FALSE(insn2.params[0].data.reg.global);
    ASSERT_TRUE(insn2.params[1].kind == PK_MEM);
    ASSERT_TRUE(insn2.params[1].data.mem.kind == MK_REG_TYPE);
    ASSERT_EQUAL(insn2.params[1].data.mem.reg.id, 1);
    ASSERT_TRUE(insn2.params[1].data.mem.type.id == insn.type.id); // should have same type as insn
}

TEST(simple_assemble) {
    buffer in;
    write(in, R"(
        local i64 %0
        mov i64 %0, 1
        call i64 %1, foo(i64 %0, i64 0, i64 1, i64 2, i64 3)
)");
    println("");
    println("Original source:");
    println(R"(        local i64 %0
        mov i64 %0, 1
        call i64 %1, foo(i64 %0, i64 0, i64 1, i64 2, i64 3)
)");
    Context ctx;
    Insn insns[3];
    for (u8 i = 0; i < 3; i ++) insns[i] = parse_insn(ctx, in);

    Object object(JASMINE);
    for (u8 i = 0; i < 3; i ++) assemble_insn(ctx, object, insns[i]);

    // bytebuf tmp = object.code();
    // while (tmp.size()) printf("%02x ", tmp.read());
    // println("");
    bytebuf buf = object.code();
    for (u8 i = 0; i < 3; i ++) insns[i] = disassemble_insn(ctx, buf, object);

    println("After round trip:");
    for (u8 i = 0; i < 3; i ++) print_insn(ctx, _stdout, insns[i]);
    println("");
}

// fib:    frame
//         param i64 %0
//         local i64 %1
//         jl i64 %0, 2 .L0
//         push i64 %0
//         sub i64 %1, %0, 1
//         call i64 %1, fib(i64 %1)
//         xchg i64 %0, %1
//         sub i64 %1, %1, 2
//         call i64 %1, fib(i64 %1)
//         add i64 %0, %0, %1
//         pop i64 %1
// .L0:    ret i64 %0