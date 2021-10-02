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

TEST(round_trip) {
    buffer in;
    write(in,
"foo:\tlocal i64 %0\n"
"\tmov i64 %0, 1\n"
"\tcall i64 %1, foo(i64 %0, i64 0, i64 1, i64 2, i64 3)\n");
    buffer copy(in);
    Context ctx;
    Insn insns[3];
    for (u8 i = 0; i < 3; i ++) insns[i] = parse_insn(ctx, in);

    Object object(JASMINE);
    for (u8 i = 0; i < 3; i ++) assemble_insn(ctx, object, insns[i]);

    bytebuf buf = object.code();
    for (u8 i = 0; i < 3; i ++) insns[i] = disassemble_insn(ctx, buf, object);

    buffer out;
    for (u8 i = 0; i < 3; i ++) print_insn(ctx, out, insns[i]);

    string a(copy), b(out);
    ASSERT_EQUAL(a, b);
}

TEST(labeled_branches) {
    buffer in;
    write(in,
"foo:\tframe\n"
"\tparam i64 %0\n"
"\tlocal i64 %1\n"
"_L0:\tjeq i64 _L1 %0, %1\n"
"\tsub i64 %0, %0, 1\n"
"\tjump _L0\n"
"_L1:\tret i64 %0\n");
    buffer copy(in);
    Context ctx;
    Insn insns[7];
    for (u8 i = 0; i < 7; i ++) insns[i] = parse_insn(ctx, in);

    Object object(JASMINE);
    for (u8 i = 0; i < 7; i ++) assemble_insn(ctx, object, insns[i]);

    bytebuf buf = object.code();
    for (u8 i = 0; i < 7; i ++) insns[i] = disassemble_insn(ctx, buf, object);

    buffer out;
    for (u8 i = 0; i < 7; i ++) print_insn(ctx, out, insns[i]);

    string a(copy), b(out);
    ASSERT_EQUAL(a, b);
}

TEST(x86_regalloc) {
    buffer in;
    write(in,R"(
foo:    frame
        param i64 %0
        local i64 %1
_L0:    jeq i64 _L1 %0, %1
        sub i64 %0, %0, 1
        jump _L0
_L1:    ret i64 %0)");
    Context ctx;
    vector<Insn> insns;
    for (u8 i = 0; i < 7; i ++) insns.push(parse_insn(ctx, in));
    
    jasmine_to_x86(insns);
}