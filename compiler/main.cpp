#include "driver.h"
#include "eval.h"

// Runs the "help" mode of the compiler.
void help(int argc, const char** argv) {
    println(" ◅ ", BOLDGREEN, "Basil ", RESET, 
        BASIL_MAJOR_VERSION, ".", BASIL_MINOR_VERSION, ".", BASIL_PATCH_VERSION, RESET, " ▻ ");   
    println("");
    println("Usage: ");
    println(" • Start a REPL:                           ", BOLD, argv[0], RESET);
    println(" • Run a file:                             ", BOLD, argv[0], " <", ITALIC, "filename", RESET, ">");
    println("");
    println("Subcommands:");
    println(" • Start the interactive tutorial:         ", BOLD, argv[0], " intro [", ITALIC, "chapter", RESET, "]");
    println(" • Show this help message:                 ", BOLD, argv[0], " help", RESET);
    println(" • Run a file:                             ", BOLD, argv[0], " run <", ITALIC, "filename", RESET, ">");
    println(" • Compile a file to a Basil object:       ", BOLD, argv[0], " compile <", ITALIC, "filename", RESET, ">");
    println("    ○ ...as a raw source file:             ", GRAY, argv[0], " compile ", BOLDWHITE, "--source ", GRAY, "<", ITALIC, "filename", RESET, GRAY, ">", RESET);
    println("    ○ ...as a parsed source file:          ", GRAY, argv[0], " compile ", BOLDWHITE, "--parse ", GRAY, "<", ITALIC, "filename", RESET, GRAY, ">", RESET);
    println("    ○ ...as a compile-time module:         ", GRAY, argv[0], " compile ", BOLDWHITE, "--eval ", GRAY, "<", ITALIC, "filename", RESET, GRAY, ">", RESET);
    println("    ○ ...as a typed AST:                   ", GRAY, argv[0], " compile ", BOLDWHITE, "--ast ", GRAY, "<", ITALIC, "filename", RESET, GRAY, ">", RESET);
    println("    ○ ...as a portable bytecode object:    ", GRAY, argv[0], " compile ", BOLDWHITE, "--jasmine ", GRAY, "<", ITALIC, "filename", RESET, GRAY, ">", RESET);
    println("    ○ ...as a native binary:               ", GRAY, argv[0], " compile ", BOLDWHITE, "--native ", GRAY, "<", ITALIC, "filename", RESET, GRAY, ">", RESET);
    println(" • Link several Basil objects together:    ", BOLD, 
        argv[0], " link <", ITALIC, "inputs...", RESET,    "> <", ITALIC, "output", RESET, ">");
    println(" • Build a native executable:              ", BOLD, argv[0], " build <", ITALIC, "filename", RESET, ">");
    println(" • Display a Basil object's contents:      ", BOLD, argv[0], " show <", ITALIC, "filename", RESET, ">");
    println("");
}

// Runs a file.
void run(int argc, const char** argv) {
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

int main(int argc, const char** argv) {
    basil::init();

    map<ustring, void(*)(int, const char**)> drivers;
    drivers["help"] = help;
    drivers["run"] = run;

    if (argc == 1) {
        basil::repl();
    }
    else if (argc > 1) {
        auto it = drivers.find(argv[1]);
        if (it == drivers.end()) run(argc, argv);
        else drivers[argv[1]](argc, argv);
    }
    else {
        help(argc, argv);
    }
    basil::deinit();
    return 0;
}