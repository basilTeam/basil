/*
 * Copyright (c) 2021, the Basil authors
 * All rights reserved.
 *
 * This source code is licensed under the 3-Clause BSD License, the full text
 * of which can be found in the LICENSE file in the root directory
 * of this project.
 */

#ifndef JASMINE_SYMBOL_H
#define JASMINE_SYMBOL_H

#include "utils.h"
#include "hash.h"

namespace jasmine {
    enum SymbolLinkage : u8 {
        GLOBAL_SYMBOL, LOCAL_SYMBOL
    };

    struct Symbol {
        u32 id;
        SymbolLinkage type;
    };

    bool operator==(Symbol a, Symbol b);

    Symbol global(const char* name);
    Symbol local(const char* name);
    const char* name(Symbol symbol);
}

template<>
u64 hash(const jasmine::Symbol& symbol);

#endif