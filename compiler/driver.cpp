#include "driver.h"
#include "value.h"
#include "parse.h"
#include "token.h"
#include "source.h"
#include "eval.h"

namespace basil {
    void init() {
        init_types_and_symbols();
    }

    void deinit() {
        free_root_env();
        free_types();
    }

    Source load_step(const char* const& str) {
        buffer b;
        write(b, str);
        return Source(b);
    }

    vector<Token> lex_step(const Source& source) {
        Source::View view(source);
        return lex_all(view);
    }

    Value parse_step(const vector<Token>& tokens) {
        TokenView view(tokens);
        vector<Value> values;
        while (view)
            if (auto maybe_expr = parse(view))
                values.push(*maybe_expr);
        if (values.size() == 0) return v_void({});
        else if (values.size() == 1) return values[0];
        else return v_list(span(values.front().pos, values.back().pos), t_list(T_ANY), values);
    }

    Value resolve_step(const Value& term) {
        Value term_copy = term;
        auto new_env = resolve_form(root_env(), term_copy);
        return term_copy;
    }

    Value eval_step(const Value& term) {
        Value term_copy = term;
        return eval(root_env(), term_copy).value;
    }
}