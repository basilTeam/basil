#include "driver.h"
#include "eval.h"

using namespace basil;

void do_thing() {
    Value code = compile("1 + 2", load_step, lex_step, parse_step);
    auto er = eval(root_env(), code);
    println(er.value);
}

int main(int argc, char** argv) {
    init();
    do_thing();
    deinit();
    return 0;
}