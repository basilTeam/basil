/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "driver.h"
#include "eval.h"
#include "obj.h"

// Runs the "help" mode of the compiler.
void help(int argc, const char** argv) {
    buffer b;
    writeln(b, "    ○ ...as a portable bytecode object:    ", argv[0], 
        " compile -j, --jasmine <filename> ");
    u32 len = ustring(b).size();
    buffer ver;
    writeln(ver, " Basil ", BASIL_MAJOR_VERSION, ".", BASIL_MINOR_VERSION, ".", BASIL_PATCH_VERSION, " ");
    u32 edge = len - ustring(ver).size() - 16;
    if (edge < 16) edge = 16;
    
    for (u32 i = 0; i < 16; i ++) print("━");
    print(" ", BOLDGREEN, "Basil ", RESET, 
        BASIL_MAJOR_VERSION, ".", BASIL_MINOR_VERSION, ".", BASIL_PATCH_VERSION, RESET, " ");
    for (u32 i = 0; i < edge; i ++) print("━");
    println("");
       
    println("");
    println("Usage: ");
    println(" • Start a REPL:                           ", BOLD, argv[0], RESET);
    println(" • Run a file:                             ", BOLD, argv[0], " <", ITALIC, "filename", BOLD, ">", RESET);
    println("");
    println("Subcommands:");
    println(" • Start the interactive tutorial:         ", BOLD, argv[0], " intro [", ITALIC, "chapter", BOLD, "]", RESET);
    println(" • Show this help message:                 ", BOLD, argv[0], " help", RESET);
    println(" • Run a file:                             ", BOLD, argv[0], " run <", ITALIC, "filename", BOLD, ">", RESET);
    println(" • Compile a file to a Basil object:       ", BOLD, argv[0], " compile <", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a raw source file:             ", GRAY, argv[0], " compile ", BOLDWHITE, "-s, --source ", GRAY, "<", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a parsed source file:          ", GRAY, argv[0], " compile ", BOLDWHITE, "-p, --parse ", GRAY, "<", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a compile-time module:         ", GRAY, argv[0], " compile ", BOLDWHITE, "-e, --eval ", GRAY, "<", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a typed AST:                   ", GRAY, argv[0], " compile ", BOLDWHITE, "-a, --ast ", GRAY, "<", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a low-level SSA form:          ", GRAY, argv[0], " compile ", BOLDWHITE, "-i, --ir ", GRAY, "<", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a portable bytecode object:    ", GRAY, argv[0], " compile ", BOLDWHITE, "-j, --jasmine ", GRAY, "<", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a native binary:               ", GRAY, argv[0], " compile ", BOLDWHITE, "-n, --native ", GRAY, "<", ITALIC, "filename", BOLD, ">", RESET);
    println(" • Link several Basil objects together:    ", BOLD, 
        argv[0], " link <", ITALIC, "inputs...", RESET,    "> <", ITALIC, "output", BOLD, ">", RESET);
    println(" • Build a native executable:              ", BOLD, argv[0], " build <", ITALIC, "filename", BOLD, ">", RESET);
    println(" • Display a Basil object's contents:      ", BOLD, argv[0], " show <", ITALIC, "filename", BOLD, ">", RESET);
    println("");
}

// Runs a file.
void run_implicit(int argc, const char** argv) {
    if (argc != 2) {
        println("Usage: ", BOLD, argv[0], " <", ITALIC, "filename", BOLD, ">", RESET);
        println("");
        help(argc, argv);
        return;
    }
    {
        file f(argv[1], "r");
        if (!f) {
            println("Couldn't find source file '", (const char*)argv[1], "'.");
            println("");
            help(argc, argv);
            return;
        }
    }
    return basil::run(argv[1]);
}

// Runs a file.
void run(int argc, const char** argv) {
    if (argc != 3) {
        println("Usage: ", BOLD, argv[0], " run <", ITALIC, "filename", BOLD, ">", RESET);
        println("");
        help(argc, argv);
        return;
    }
    {
        file f(argv[2], "r");
        if (!f) {
            println("Couldn't find source file '", (const char*)argv[2], "'.");
            println("");
            help(argc, argv);
            return;
        }
    }
    return basil::run(argv[2]);
}

// Compiles a file.
void compile(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        println("Usage: ", BOLD, argv[0], " compile <", ITALIC, "filename", BOLD, ">", RESET);
        println("");
        help(argc, argv);
        return;
    }
    {
        file f(argv[argc - 1], "r");
        if (!f) {
            println("Couldn't find source file '", (const char*)argv[argc - 1], "'.");
            println("");
            help(argc, argv);
            return;
        }
    }

    map<ustring, basil::SectionType> typemap;
    typemap["-s"] = typemap["--source"] = basil::ST_SOURCE;
    typemap["-p"] = typemap["--parse"] = basil::ST_PARSED;
    typemap["-e"] = typemap["--eval"] = basil::ST_EVAL;
    typemap["-a"] = typemap["--ast"] = basil::ST_AST;
    typemap["-i"] = typemap["--ir"] = basil::ST_IR;
    typemap["-j"] = typemap["--jasmine"] = basil::ST_JASMINE;
    typemap["-n"] = typemap["--native"] = basil::ST_NATIVE;
    basil::SectionType target;
    if (argc == 3) target = basil::ST_NATIVE; // take it as far as it can go
    else if (typemap.contains(argv[2]))
        target = typemap[argv[2]];
    else {
        println("Unknown compilation phase '", argv[2], "' - valid options are 'source', ",
            "'parse', 'eval', 'ast', 'ir', 'jasmine', and 'native'.");
        return;
    }

    basil::compile(argv[argc - 1], target);
}

int main(int argc, const char** argv) {
    basil::init();

    map<ustring, void(*)(int, const char**)> drivers;
    drivers["help"] = help;
    drivers["run"] = run;
    drivers["compile"] = compile;

    if (argc == 1) {
        basil::repl();
    }
    else if (argc > 1) {
        auto it = drivers.find(argv[1]);
        if (it == drivers.end()) run_implicit(argc, argv);
        else drivers[argv[1]](argc, argv);
    }
    else {
        help(argc, argv);
    }
    basil::deinit();
    return 0;
}