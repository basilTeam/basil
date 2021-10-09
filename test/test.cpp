/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "test.h"
#include "compiler/errors.h"

test_node* test_list;

void (*setup_fn)() = nullptr;
bool setup_failed = false;

bool __internal_require(jasmine::Architecture arch) {
    if (jasmine::DEFAULT_ARCH != arch)
        return setup_failed = true;
    return false;
}

int main(int argc, char** argv) {
    if (setup_fn) {
        setup_fn();
        if (setup_failed) return 0;
    }
    
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