#ifndef BASIL_PARSE_H
#define BASIL_PARSE_H

#include "lex.h"
#include "util/defs.h"
#include "util/vec.h"
#include "values.h"

namespace basil {
    Value parse(TokenView& view, u32 indent);
    Value parse_line(TokenView& view, u32 indent, bool consume_line = true);
} // namespace basil

#endif