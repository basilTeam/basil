#ifndef BASIL_PARSE_H
#define BASIL_PARSE_H

#include "lex.h"
#include "util/defs.h"
#include "util/vec.h"
#include "values.h"

namespace basil {
<<<<<<< HEAD
  Value parse(TokenView& view, u32 indent);
  void parse_line(vector<Value>& terms, TokenView& view, u32 indent, 
    bool consume_line = true);
}
=======
    Value parse(TokenView& view, u32 indent);
    Value parse_line(TokenView& view, u32 indent, bool consume_line = true);
} // namespace basil
>>>>>>> f840479bc314e660171a3eb91c404f37c4be4a33

#endif