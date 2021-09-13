#include "test.h"
#include "compiler/errors.h"

test_node* test_list;

void (*setup_fn)() = nullptr;

int main(int argc, char** argv) {
    if (setup_fn) setup_fn();
    
    println("---------------- ", (const char*)argv[0], " ----------------");
    int n = 0, c = 0;
    while (test_list) {
        test_node node = *test_list;
        test_list = node.next;
        print("Running test '", node.name, "'... ");
        n ++;
        try {
            node.callback();
            if (basil::error_count()) throw exc_message{"Encountered compiler errors!"};
            println("...passed!");
            c ++;
        } catch (const exc_message& msg) {
            println("...failed!");
            println(">\t", msg.msg);
            println("");
            if (basil::error_count()) basil::print_errors(_stdout, nullptr);
        }
    }
    println("");
    println("Passed ", c, " / ", n, " tests.");
    println("---------------- ", (const char*)argv[0], " ----------------");
    println("");
}