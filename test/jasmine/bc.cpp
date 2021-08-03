#include "jasmine/bc.h"
#include "test.h"

using namespace jasmine;

TEST(assemble) {
    Object obj(JASMINE);
    writeto(obj);

    label("func");
    frame(0, 1000);
    arg(S64, r(0));
    mov(S64, r(1), r(0));
    add(S64, r(1), imm(2));
    mov(S64, r(2), imm(3));
    label(".L0");
    add(S64, r(0), imm(1));
    sub(S64, r(1), imm(1));
    jl(local(".L0"));
    add(S64, r(1), r(2));
    ret();

    println("");
    println("");
    disassemble(obj, _stdout);

    println("");
    obj.retarget(X86_64);
    println("");
}