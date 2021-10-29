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
    println(" • Build a Basil object from source:       ", BOLD, argv[0], " build <", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a raw source file:             ", GRAY, argv[0], " build ", BOLDWHITE, "source", GRAY, " <", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a parsed source file:          ", GRAY, argv[0], " build ", BOLDWHITE, "parsed", GRAY, " <", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a compile-time module:         ", GRAY, argv[0], " build ", BOLDWHITE, "module", GRAY, " <", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a typed AST:                   ", GRAY, argv[0], " build ", BOLDWHITE, "ast", GRAY, " <", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a low-level SSA form:          ", GRAY, argv[0], " build ", BOLDWHITE, "ir", GRAY, " <", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a portable bytecode object:    ", GRAY, argv[0], " build ", BOLDWHITE, "jasmine", GRAY, " <", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a native binary:               ", GRAY, argv[0], " build ", BOLDWHITE, "native", GRAY, " <", ITALIC, "filename", BOLD, ">", RESET);
    println(" • Link several Basil objects together:    ", BOLD, 
        argv[0], " link <", ITALIC, "inputs...", RESET,    "> <", ITALIC, "output", BOLD, ">", RESET);
    println(" • Compile a native artifact:              ", BOLD, argv[0], " compile <", ITALIC, "filename", BOLD, ">", RESET);
    println("    ○ ...as a shared object:               ", GRAY, argv[0], " compile ", BOLDWHITE, "library", GRAY, " <", ITALIC, "filename", BOLD, "> [", ITALIC, "objects...", BOLD, "]", RESET);
    println("    ○ ...as an executable:                 ", GRAY, argv[0], " compile ", BOLDWHITE, "executable", GRAY, " <", ITALIC, "filename", BOLD, "> [", ITALIC, "objects...", BOLD, "]", RESET);
    println("    ○ ...as a relocatable object:          ", GRAY, argv[0], " compile ", BOLDWHITE, "object", GRAY, " <", ITALIC, "filename", BOLD, ">", RESET);
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

// Builds a Basil object.
void build(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        println("Usage: ", BOLD, argv[0], " build <", ITALIC, "filename", BOLD, ">", RESET);
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
    typemap["source"] = basil::ST_SOURCE;
    typemap["parsed"] = basil::ST_PARSED;
    typemap["module"] = basil::ST_EVAL;
    typemap["ast"] = basil::ST_AST;
    typemap["ir"] = basil::ST_IR;
    typemap["jasmine"] = basil::ST_JASMINE;
    typemap["native"] = basil::ST_NATIVE;
    basil::SectionType target;
    if (argc == 3) target = basil::ST_NATIVE; // take it as far as it can go
    else if (typemap.contains(argv[2]))
        target = typemap[argv[2]];
    else {
        println("Unknown compilation phase '", argv[2], "' - valid options are 'source', ",
            "'parsed', 'module', 'ast', 'ir', 'jasmine', and 'native'.");
        return;
    }

    basil::build(argv[argc - 1], target);
}

// Compiles a file.
void compile(int argc, const char** argv) {
    if (argc < 3) {
        println("Usage: ", BOLD, argv[0], " compile <", ITALIC, "filename", BOLD, ">", RESET);
        println("");
        help(argc, argv);
        return;
    }

    map<ustring, basil::NativeType> typemap;
    typemap["object"] = basil::NT_OBJECT;
    typemap["library"] = basil::NT_LIBRARY;
    typemap["executable"] = basil::NT_EXECUTABLE;
    basil::NativeType target;
    const char** src;
    if (typemap.contains(argv[2])) {
        target = typemap[argv[2]];
        if (argc < 4) {
            println("Usage: ", BOLD, argv[0], " compile <", ITALIC, "filename", BOLD, ">", RESET);
            println("");
            help(argc, argv);
            return;
        }
        src = argv + 3;
    }
    else {
        target = basil::NT_EXECUTABLE;
        src = argv + 2;
    }

    {
        file f(*src, "r");
        if (!f) {
            println("Couldn't find source file '", *src, "'.");
            println("");
            help(argc, argv);
            return;
        }
    }

    basil::compile(*src, target, const_slice<const char*>((argv + argc - 1) - src, src + 1));
}

int main(int argc, const char** argv) {
    basil::init();

    map<ustring, void(*)(int, const char**)> drivers;
    drivers["help"] = help;
    drivers["run"] = run;
    drivers["build"] = build;
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