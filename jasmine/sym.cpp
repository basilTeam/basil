/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#include "sym.h"
#include "string.h"
#include "util/str.h"

namespace jasmine {
    struct SymbolTable {
        vector<string> symbol_names;
        map<string, Symbol> symbols;
    
        SymbolTable() {}
        ~SymbolTable() {
					//
        }

        Symbol enter(const string& name, SymbolLinkage linkage) {
            auto it = symbols.find(name);
            if (it == symbols.end()) {
                symbol_names.push(name);
                return symbols[name] = { symbol_names.size() - 1, linkage };
            }
            else return it->second;
        }
    };

    static SymbolTable table;

    bool operator==(Symbol a, Symbol b) {
        return a.id == b.id;
    }
    
    Symbol global(const char* name) {
        return table.enter(name, GLOBAL_SYMBOL);
    }

    Symbol local(const char* name) {
        return table.enter(name, LOCAL_SYMBOL);
    }

    const char* name(Symbol symbol) {
        return (const char*)table.symbol_names[symbol.id].raw();
    }
}

template<>
u64 hash(const jasmine::Symbol& symbol) {
    return hash<u64>((u64)symbol.id | (u64)symbol.type << 32);
}