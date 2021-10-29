/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "driver.h"
#include "value.h"
#include "parse.h"
#include "token.h"
#include "source.h"
#include "obj.h"
#include "eval.h"
#include "stdlib.h"
#include "jasmine/obj.h"

namespace basil {
    void init() {
        init_types_and_symbols();
    }

    void deinit() {
        free_root_env();
        free_types();
    }
    
    optional<rc<Object>> load_artifact(const char* path) {
        auto fpath = locate_source(path);
        if (!fpath) return none<rc<Object>>();

        bool is_object = true;
        { // used to close `f` via RAII
            file f(fpath->raw(), "r");
            const char* magic = "#!basil\n\v\v";
            for (u32 i = 0; i < 10; i ++) {
                if (!f || f.peek() != magic[i]) {
                    is_object = false;
                    break;
                }
                f.read();
            }
        }

        rc<Object> obj = ref<Object>();
        if (is_object) { // deserialize object from file
            file f(fpath->raw(), "r");
            obj->read(f);
            if (error_count()) {
                print_errors(_stdout, nullptr);
                return none<rc<Object>>();
            }
            return some<rc<Object>>(obj);
        }
        else { // create new object with single source section
            rc<Source> src = ref<Source>(*fpath);
            obj->sections.push(source_section(*fpath, src));
            obj->main_section = some<u32>(0);
            return some<rc<Object>>(obj);
        }
    }

    optional<rc<Section>> lex_and_parse(rc<Section> section) {
        if (section->type != ST_SOURCE) panic("Tried to lex and parse non-source section!");
        rc<Source> source = source_from_section(section);
        vector<Token> token_cache = lex_step(source);
        TokenView tview(source, token_cache);

        vector<Value> program_terms;
        program_terms.push(v_symbol({}, S_DO)); // wrap the whole program in a giant "do"

        while (tview) {
            if (auto v = parse(tview)) {
                program_terms.push(*v);
            }
        }
        if (error_count()) {
            print_errors(_stdout, source);
            discard_errors();
            return none<rc<Section>>();
        }

        Value program = v_list(span(program_terms.front().pos, program_terms.back().pos), 
            t_list(T_ANY), move(program_terms));
        return some<rc<Section>>(parsed_section(section->name, program));
    }

    optional<rc<Section>> eval_section(rc<Section> section) {
        if (section->type != ST_PARSED) panic("Tried to evaluate non-parsed section!");
        rc<Env> env = extend(root_env());
        Value program = parsed_from_section(section);
        resolve_form(env, program);

        Value result = eval(env, program);
        if (error_count()) {
            print_errors(_stdout, nullptr);
            discard_errors();
            return none<rc<Section>>();
        }
        return some<rc<Section>>(module_section(section->name, result, env));
    }

    optional<rc<Section>> to_ast(rc<Section> section) {
        if (section->type != ST_EVAL) panic("Tried to lower non-module section to AST!");
        rc<Env> env = module_from_section(section);
        Value main = module_main(section);

        if (!main.type.of(K_RUNTIME)) main = lower(env, main);

        map<Symbol, rc<AST>> functions;
        for (auto& [k, v] : env->values) {
            if (v.type.of(K_FUNCTION)) for (auto& [_, v] : v.data.fn->resolutions) {
                for (auto& [t, v] : v->insts) {
                    functions[mangle(k, v->func->type(env))] = v->func;
                }
            }
        }

        if (error_count()) {
            print_errors(_stdout, nullptr);
            discard_errors();
            return none<rc<Section>>();
        }

        // for (const auto& [k, v] : functions) println(k, ": ", v);

        return some<rc<Section>>(ast_section(section->name, main.data.rt->ast, functions, env));
    }

    optional<rc<Section>> to_ir(rc<Section> section) {
        if (section->type != ST_AST) panic("Tried to lower non-AST section to IR!");
        map<Symbol, rc<AST>> functions = ast_from_section(section);
        rc<AST> main = ast_main(section);
        rc<Env> env = ast_env(section);

        rc<IRFunction> main_ir = ref<IRFunction>(symbol_from(".basil_main"), t_func(T_VOID, T_VOID));
        main->gen_ssa(env, main_ir);
        main_ir->finish(T_VOID, ir_none());

        if (error_count()) {
            print_errors(_stdout, nullptr);
            discard_errors();
            return none<rc<Section>>();
        }

        map<Symbol, rc<IRFunction>> ir_functions;
        for (auto& [k, v] : functions) v->gen_ssa(root_env(), main_ir), ir_functions[k] = get_ssa_function(v);

        // for (auto& [k, v] : ir_functions) println(v);

        if (error_count()) {
            print_errors(_stdout, nullptr);
            discard_errors();
            return none<rc<Section>>();
        }

        return some<rc<Section>>(ir_section(section->name, main_ir, ir_functions));
    }

    optional<rc<Section>> to_jasmine(rc<Section> section) {
        if (section->type != ST_IR) panic("Tried to lower non-IR section to Jasmine bytecode!");
        map<Symbol, rc<IRFunction>> functions = ir_from_section(section);
        rc<IRFunction> main = ir_main(section);

        rc<jasmine::Object> object = ref<jasmine::Object>(jasmine::Target{ jasmine::JASMINE, jasmine::DEFAULT_OS });
        jasmine::bc::writeto(*object);

        for (auto& [k, v] : functions) optimize(v, OPT_FAST), v->emit(object->get_context());
        optimize(main, OPT_FAST);
        main->emit(object->get_context());

        if (error_count()) {
            print_errors(_stdout, nullptr);
            discard_errors();
            return none<rc<Section>>();
        }

        vector<jasmine::Insn> insns = jasmine::disassemble_all_insns(object->get_context(), *object);
        for (auto& insn : insns) jasmine::print_insn(object->get_context(), _stdout, insn);

        return some<rc<Section>>(jasmine_section(section->name, object));
    }

    optional<rc<Section>> to_native(rc<Section> section) {
        if (section->type != ST_JASMINE) panic("Tried to compile non-Jasmine section to native binary!");
        rc<jasmine::Object> object = jasmine_from_section(section);
        
        jasmine::Object native = object->retarget(jasmine::DEFAULT_TARGET);
        rc<jasmine::Object> boxed_native = ref<jasmine::Object>(native);

        if (error_count()) {
            print_errors(_stdout, nullptr);
            discard_errors();
            return none<rc<Section>>();
        }

        return some<rc<Section>>(native_section(section->name, boxed_native));
    }

    optional<rc<Section>> advance_section(rc<Section> section, SectionType target) {
        optional<rc<Section>> product = some<rc<Section>>(section); 
        rc<Source> src = section->type == ST_SOURCE ? source_from_section(section) : nullptr;
        while (product && (*product)->type < target) switch ((*product)->type) {
            case ST_SOURCE: product = apply(product, lex_and_parse); break;
            case ST_PARSED: product = apply(product, eval_section); break;
            case ST_EVAL: product = apply(product, to_ast); break;
            case ST_AST: product = apply(product, to_ir); break;
            case ST_IR: product = apply(product, to_jasmine); break;
            case ST_JASMINE: product = apply(product, to_native); break;
            default: product = none<rc<Section>>();
        }
        if (error_count()) {
            print_errors(_stdout, src);
            return none<rc<Section>>();
        }
        return product;
    }

    rc<Source> load_step(const ustring& str) {
        buffer b;
        write(b, str);
        return ref<Source>(b);
    }

    vector<Token> lex_step(rc<Source> source) {
        Source::View view(*source);
        return lex_all(view);
    }

    Value parse_step(const vector<Token>& tokens) {
        vector<Token> tokens_copy = tokens;
        TokenView view(nullptr, tokens_copy);
        vector<Value> values;
        while (view)
            if (auto maybe_expr = parse(view))
                values.push(*maybe_expr);
        if (values.size() == 0) return v_void({});
        else if (values.size() == 1) return values[0];
        else return v_list(span(values.front().pos, values.back().pos), t_list(T_ANY), move(values));
    }

    Value resolve_step(const Value& term) {
        Value term_copy = term;
        resolve_form(root_env(), term_copy);
        return term_copy;
    }

    Value eval_step(const Value& term) {
        Value term_copy = term;
        Value result = eval(root_env(), term_copy);
        if (error_count()) print_errors(_stdout, nullptr), discard_errors();
        return result;
    }

    rc<AST> ast_step(const Value& value) {
        if (value.type == T_ERROR) return nullptr;
        Value result = lower(root_env(), value);
        if (error_count()) print_errors(_stdout, nullptr), discard_errors();
        if (!result.type.of(K_RUNTIME)) return nullptr;
        return result.data.rt->ast;
    }

    map<Symbol, rc<IRFunction>> ssa_step(const rc<AST>& ast_in) {
        rc<AST> ast = ast_in;
        if (!ast) return {};
        rc<IRFunction> main_ir = ref<IRFunction>(symbol_from(".basil_main"), t_func(T_VOID, T_VOID));
        ast->gen_ssa(root_env(), main_ir);
        main_ir->finish(T_VOID, ir_none());

        if (error_count()) print_errors(_stdout, nullptr), discard_errors();

        map<Symbol, rc<IRFunction>> functions;
        functions[symbol_from(".basil_main")] = main_ir;
        for (const auto& [k, v] : root_env()->values) if (v.type.of(K_FUNCTION)) {
            for (const auto& [_, v] : v.data.fn->resolutions) for (const auto& [_, v] : v->insts) {
                if (v->func && get_ssa_function(v->func)) {
                    functions[k] = get_ssa_function(v->func);
                    if (!functions[k]) err(v->func->pos, "Couldn't lower function '", v->func, "'.");
                }
            }
        }
        return functions;
    }

    static bool repl_mode = false;

    bool is_repl() {
        return repl_mode;
    }

    bool endswith(const ustring& s, const ustring& suffix) {
        auto back = s.end();
        for (u32 i = 0; i < suffix.size(); i ++) back --;
        auto second = suffix.begin();
        for (u32 i = 0; i < suffix.size(); i ++) 
            if (*back != *second) return false;
            else ++ back, ++ second;
        return true;
    }

    bool exists(const ustring& s) {
        return ::exists(s.raw());
    }

    optional<ustring> locate_source(const char* path) {
        if (exists(path)) return some<ustring>(path);
        if (!endswith(path, ".bl") && exists(ustring(path) + ".bl"))
            return some<ustring>(ustring(path) + ".bl");
        if (!endswith(path, ".bob") && exists(ustring(path) + ".bob"))
            return some<ustring>(ustring(path) + ".bob");
        return none<ustring>();
    }

    ustring compute_object_name(const char* path, const char* suffix) {
        if (suffix && *suffix && endswith(path, suffix)) return path;
        else {
            ustring upath = path;
            auto it = upath.begin();
            auto dot = upath.begin();
            while (it != upath.end()) {
                if (*it == '.') dot = it;
                ++ it;
            }
            ustring newpath;
            it = upath.begin();
            while (it != dot) {
                newpath += *it;
                it ++;
            }
            newpath += suffix;
            return newpath;
        }
    }
    
    bool out_of_input(TokenView& view) {
        if (!view) {
            if (repl_mode) { // try and read additional input
                print(". ");
                if (view.expand_line(_stdin)) return false; // successfully read additional input
                else {
                    print_errors(_stdout, view.source);
                    return true;
                }
            }
            else return true; // don't try and pull input if not REPL
        }
        else return false;
    }

    i64 square(i64 n) {
        return n * n;
    }

    void repl() {
        repl_mode = true;
        rc<Source> source = ref<Source>();
        vector<Token> token_cache;
        TokenView view(source, token_cache);

        rc<Env> global = extend(root_env());

        bool done = false;
        while (!done) {
            print("? ");
            view.expand_line(_stdin);
            vector<Value> code;
            code.push(v_symbol({}, S_DO));
            bool parse_errored = false;
            while (view) {
                if (view.peek().kind == TK_NEWLINE) {
                    view.read();
                    break;
                }

                if (auto v = parse(view)) code.push(*v);
            }
            if (code.size() == 1) continue;
            if (error_count()) {
                print_errors(_stdout, source);
                discard_errors();
                continue;
            }

            Value list = v_list(span(code.front().pos, code.back().pos), t_list(T_ANY), move(code));
            Value result = eval(global, list);
            if (error_count()) {
                print_errors(_stdout, source);
                discard_errors();
                continue;
            }

            if (result.type == T_VOID) continue;

            if (!result.type.of(K_RUNTIME)) {
                println("= ", ITALICBLUE, result, RESET);
                println("");
                continue;
            }

            rc<AST> ast = result.data.rt->ast;
            ast->type(global); // run typechecking
            rc<IRFunction> main_ir = ref<IRFunction>(symbol_from(".basil_main"), t_func(T_VOID, T_VOID));
            
            main_ir->finish(T_INT, ast->gen_ssa(global, main_ir));

            for (const auto& [k, v] : global->values) if (v.type.of(K_FUNCTION)) {
                for (const auto& [_, v] : v.data.fn->resolutions) for (const auto& [_, v] : v->insts) {
                    if (v->func && get_ssa_function(v->func)) {
                        optimize(get_ssa_function(v->func), OPT_FAST);
                        // println(get_ssa_function(v->func));
                    }
                }
            }
            
            optimize(main_ir, OPT_FAST);
            // println(main_ir);

            jasmine::Object jobj({ jasmine::JASMINE, jasmine::DEFAULT_OS });
            jasmine::bc::writeto(jobj);

            for (const auto& [k, v] : global->values) if (v.type.of(K_FUNCTION)) {
                for (const auto& [_, v] : v.data.fn->resolutions) for (const auto& [_, v] : v->insts) {
                    if (v->func && get_ssa_function(v->func)) {
                        get_ssa_function(v->func)->emit(jobj.get_context());
                    }
                }
            }
            main_ir->emit(jobj.get_context());

            auto insns = jasmine::disassemble_all_insns(jobj.get_context(), jobj);
            for (const auto& insn : insns) jasmine::print_insn(jobj.get_context(), _stdout, insn);

            jasmine::Object native = jobj.retarget(jasmine::DEFAULT_TARGET); // retarget to native
            // native.writeELF("out.o");
            native.define_native(jasmine::global("square"), (void*)square);
            native.load();
            auto main = (i64(*)())native.find(jasmine::global(".basil_main"));
            println(main());
        }
    }

    void run(const char* filename) {
        auto maybe_obj = load_artifact(filename);
        if (!maybe_obj) return println("Couldn't locate valid Basil file at path '", filename, "'.");
        auto obj = *maybe_obj;
        if (!obj->main_section) return println("Loaded Basil object has no 'main' section!");

        for (rc<Section>& section : obj->sections) {
            auto sect = advance_section(section, ST_NATIVE); // fully compile everything
            if (!sect) {
                print_errors(_stdout, nullptr);
                discard_errors();
                return;
            }
            section = *sect;
        }

        rc<jasmine::Object> native = native_from_section(obj->sections[*obj->main_section]);
        native->load();
        auto main = (void(*)())native->find(jasmine::global(".basil_main"));
        main();
    }

    void build(const char* filename, SectionType target) {
        ustring dest = compute_object_name(filename, ".bob");
        if (exists(dest) && dest != filename) {
            return println("Tried to compile '", filename, "' to object file, but '", dest, "' already exists!");
        }

        auto maybe_obj = load_artifact(filename);
        if (!maybe_obj) return println("Couldn't locate valid Basil file at path '", filename, "'.");
        auto obj = *maybe_obj;

        for (rc<Section>& section : obj->sections) {
            if (auto new_section = advance_section(section, target))
                section = *new_section;
            else {
                print_errors(_stdout, nullptr);
                return;
            }
        }
        
        file output(dest.raw(), "w");
        obj->write(output);
    }

    void compile(const char* filename, NativeType target, const_slice<const char*> args) {
        auto maybe_obj = load_artifact(filename);
        if (!maybe_obj) return println("Couldn't locate valid Basil file at path '", filename, "'.");
        auto obj = *maybe_obj;
        if (!obj->main_section) return println("Loaded Basil object has no 'main' section!");

        for (rc<Section>& section : obj->sections) {
            auto sect = advance_section(section, ST_NATIVE); // fully compile everything
            if (!sect) {
                print_errors(_stdout, nullptr);
                discard_errors();
                return;
            }
            section = *sect;
        }

        rc<jasmine::Object> native = native_from_section(obj->sections[*obj->main_section]);

        switch (target) {
            case NT_OBJECT: native->writeELF(compute_object_name(filename, ".o").raw()); break;
            case NT_EXECUTABLE: {
                ustring obj = compute_object_name(filename, ".o");
                native->writeELF(obj.raw());
                ustring cmd = format<ustring>("gcc -nostdlib ", obj, " -Wl,-e.basil_main -o ", compute_object_name(obj.raw(), ""));
                for (const char* arg : args) cmd += " ", cmd += arg; // add linker args
                // println(cmd);
                system(cmd.raw());
                cmd = format<ustring>("rm ", obj);
                system(cmd.raw());
                break;
            }
            default: panic("Unimplemented!");
        }
    }

    static map<ustring, rc<Env>> modules;

    optional<rc<Env>> load(const char* filename) {
        auto resolved = locate_source(filename);
        if (!resolved) {
            err({}, "Couldn't resolve file path '", filename, "'.");
            return none<rc<Env>>();
        }
        if (modules.find(*resolved) == modules.end()) {
            rc<Source> source = ref<Source>(*resolved);
            Source::View view(*source);
            vector<Token> token_cache = lex_all(view);
            TokenView tview(source, token_cache);
            
            rc<Env> global = extend(root_env());
            vector<Value> program_terms;
            program_terms.push(v_symbol({}, S_DO)); // wrap the whole program in a giant "do"

            bool old_repl = is_repl(); // store whether we are in a repl
            repl_mode = false; // prevents asking for user input in the module
            while (tview) {
                if (auto v = parse(tview)) {
                    program_terms.push(*v);
                }
                if (error_count()) {
                    print_errors(_stdout, source);
                    discard_errors();
                    repl_mode = old_repl; // restore previous repl state
                    return none<rc<Env>>();
                }
            }
            repl_mode = old_repl; // restore previous repl state
            if (error_count()) {
                print_errors(_stdout, source);
                discard_errors();
                return none<rc<Env>>();
            }

            Value program = v_list(span(program_terms.front().pos, program_terms.back().pos), 
                t_list(T_ANY), move(program_terms));
            Value result = eval(global, program);
            if (error_count()) {
                print_errors(_stdout, source);
                discard_errors();
                return none<rc<Env>>();
            }

            modules.put(*resolved, global); // store top-level environment

            return some<rc<Env>>(global);
        }
        return modules[*resolved];
    }
}