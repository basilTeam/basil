/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "jobj.h"
#include "target.h"
#include "util/io.h"
#include "bc.h"

using namespace jasmine;

// Runs the "help" mode of the compiler.
void help(int argc, const char** argv) {
    buffer b;
    writeln(b," • Run a file:                             ", argv[0], " [args...] [file] [method]");
    u32 len = ustring(b).size();
    buffer ver;
    writeln(ver, " Jasmine ", JASMINE_MAJOR_VERSION, ".", JASMINE_MINOR_VERSION, ".", JASMINE_PATCH_VERSION, " ");
    u32 edge = len - ustring(ver).size() - 16;
    if (edge < 16) edge = 16;
    
    for (u32 i = 0; i < 16; i ++) print("━");
    print(" ", BOLDYELLOW, "Jasmine ", RESET, 
        JASMINE_MAJOR_VERSION, ".", JASMINE_MINOR_VERSION, ".", JASMINE_PATCH_VERSION, RESET, " ");
    for (u32 i = 0; i < edge; i ++) print("━");
    println("");

    println("");
    println("Usage: ");
    println(" • Run a file:                             ", BOLD, argv[0], " [", ITALIC, "args...", RESET, BOLD, "] [", ITALIC, "file", RESET, BOLD, "] [", ITALIC, "method", RESET, BOLD, "]", RESET);
    println("");
    println("Subcommands:");
    println(" • Show this help message:                 ", BOLD, " -h, --help", RESET);
    println(" • Run a file:                             ", BOLD, " -r, --run [", ITALIC, "filename", RESET, BOLD, "] [", ITALIC, "method", RESET, BOLD, "]", RESET);
    println(" • Assemble a Jasmine bytecode source:     ", BOLD, " -a, --assemble [", ITALIC, "filename", RESET, BOLD, "]", RESET);
    println(" • Disassemble a Jasmine bytecode object:  ", BOLD, " -d, --disassemble [", ITALIC, "filename", RESET, BOLD, "]", RESET);
    println(" • Compile a Jasmine object to native:     ", BOLD, " -c, --compile [", ITALIC, "filename", RESET, BOLD, "]", RESET);
    println(" • Generate a system object from Jasmine:  ", BOLD, " -R, --relocate [", ITALIC, "filename", RESET, BOLD, "]", RESET);
    println(" • Specify output file:                    ", BOLD, " -o, --output [", ITALIC, "filename", RESET, BOLD, "]", RESET);
    println("");
}

template<typename... Args>
int usage_error(int argc, const char** argv, Args... args) {
    println("[ERROR] ", args...);
    println("");
    help(argc, argv);
    return argc;
}

enum CMDType {
    CMD_HELP,
    CMD_RUN,
    CMD_AS,
    CMD_DISAS,
    CMD_COMPILE,
    CMD_RELOC
};

enum InputType {
    IN_STDIN,
    IN_FILE
};

enum OutputType {
    OUT_STDOUT,
    OUT_FILE
};

CMDType cmd = CMD_RUN;
InputType in = IN_STDIN;
OutputType out = OUT_STDOUT;
const char* in_file = "";
const char* out_file = "";
const char* method = "main";

int main(int argc, const char** argv) {
    map<ustring, int(*)(int, int, const char**)> drivers;
    drivers["-h"] = drivers["--help"] = [](int i, int argc, const char** argv) -> int {
        cmd = CMD_HELP;
        return argc;
    };
    drivers["-r"] = drivers["--run"] = [](int i, int argc, const char** argv) -> int {
        i ++;
        cmd = CMD_RUN;
        if (i >= argc || argv[i][0] == '-') in = IN_STDIN;
        else in = IN_FILE,  in_file = argv[i ++];
        return i;
    };
    drivers["-a"] = drivers["--assemble"] = [](int i, int argc, const char** argv) -> int {
        i ++;
        cmd = CMD_AS;
        if (i >= argc || argv[i][0] == '-') in = IN_STDIN;
        else in = IN_FILE,  in_file = argv[i ++];
        return i;
    };
    drivers["-d"] = drivers["--disassemble"] = [](int i, int argc, const char** argv) -> int {
        i ++;
        cmd = CMD_DISAS;
        if (i >= argc || argv[i][0] == '-') in = IN_STDIN;
        else in = IN_FILE,  in_file = argv[i ++];
        return i;
    };
    drivers["-c"] = drivers["--compile"] = [](int i, int argc, const char** argv) -> int {
        i ++;
        cmd = CMD_COMPILE;
        if (i >= argc || argv[i][0] == '-') in = IN_STDIN;
        else in = IN_FILE,  in_file = argv[i ++];
        return i;
    };
    drivers["-o"] = drivers["--output"] = [](int i, int argc, const char** argv) -> int {
        i ++;
        if (i >= argc || argv[i][0] == '-') 
            return usage_error(argc, argv, "Expected output file after '", argv[i - 1], 
                "' parameter, but found '", argv[i], "'.");
        else out = OUT_FILE,  out_file = argv[i ++];
        return i;
    };
    drivers["-R"] = drivers["--relocate"] = [](int i, int argc, const char** argv) -> int {
        i ++;
        cmd = CMD_RELOC;
        if (i >= argc || argv[i][0] == '-') in = IN_STDIN;
        else in = IN_FILE,  in_file = argv[i ++];
        return i;
    };

    if (argc == 1) {
        cmd = CMD_RUN;
        in = IN_STDIN;
        out = OUT_STDOUT;
    }
    else if (argc > 1) {
        for (int i = 1; i < argc;) {
            if (argv[i][0] == '-') { // config option
                if (drivers.contains(argv[i])) {
                    i = drivers[argv[i]](i, argc, argv);
                }
                else {
                    usage_error(argc, argv, "Found unknown configuration flag '", argv[i], "'.");
                    return 0;
                }
            }
            else {
                if (in == IN_STDIN) in = IN_FILE,  in_file = argv[i ++];
                else if (string(method) == string("main")) method = argv[i ++]; 
                else {
                    usage_error(argc, argv, "Found unexpected parameter '", argv[i], "'.");
                    return 0;
                }
            }
        }
    }
    
    FILE* fin = in == IN_STDIN ? stdin : fopen(in_file, "r");
    FILE* fout = out == OUT_STDOUT ? stdout : fopen(out_file, "w");

    switch (cmd) {
        case CMD_HELP: help(argc, argv); break;
        case CMD_RUN: {
            Object obj;
            obj.read(fin);
            if (obj.get_target().arch != DEFAULT_ARCH 
                || obj.get_target().os != DEFAULT_OS) 
                obj.retarget(DEFAULT_TARGET);
            obj.load();
            auto func = (int(*)())obj.find(global(method));
            if (!func) usage_error(argc, argv, "Could not find entry-point symbol '", method, "'.");
            return func();
        }
        case CMD_AS: {
            file inf(fin);
            Context ctx;
            auto insns = parse_all_insns(ctx, inf);
            Object obj({ JASMINE, DEFAULT_OS });
            for (const auto& insn : insns) assemble_insn(ctx, obj, insn);
            obj.write(fout);
            return 0;
        }
        case CMD_DISAS: {
            file outf(fout);
            Object obj;
            obj.read(fin);
            if (obj.get_target().arch != JASMINE) {
                usage_error(argc, argv, "Jasmine can only disassemble objects containing Jasmine bytecode.");
                return 0;
            }
            Context ctx;
            auto insns = disassemble_all_insns(ctx, obj);
            for (const auto& insn : insns) print_insn(ctx, outf, insn);
            return 0;
        }
        case CMD_COMPILE: {
            Object obj({JASMINE, DEFAULT_OS});
            obj.read(fin);
            Object obj2 = obj.retarget(DEFAULT_TARGET);
            obj2.write(fout);
            return 0;
        }
        case CMD_RELOC: {
            Object obj;
            obj.read(fin);
            if (obj.get_target().arch == JASMINE) {
                Object obj2 = move(obj.retarget(DEFAULT_TARGET));
                obj = move(obj2);
            }
            if (obj.get_target().arch != DEFAULT_ARCH) {
                usage_error(argc, argv, "Object file does not contain native machine code.");
                return 0;
            }
            obj.writeObj(fout);
            return 0;
        }
    }

    if (in == IN_FILE) delete fin;
    if (out == OUT_FILE) delete fout;
    return 0;
}