#ifndef JASMINE_SYMBOL_H
#define JASMINE_SYMBOL_H

#include "utils.h"

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

#endif