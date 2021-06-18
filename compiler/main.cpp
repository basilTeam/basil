#include "driver.h"
#include "eval.h"

using namespace basil;

int main(int argc, char** argv) {
    init();
    if (argc == 2) {
        if (ustring(argv[1]) == "help") help(argv[0]);
        else run(argv[1]);
    }
    else if (argc == 1) {
        repl();
    }
    else {
        help(argv[0]);
    }
    deinit();
    return 0;
}