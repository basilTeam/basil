#include "driver.h"
#include "value.h"
#include "parse.h"
#include "token.h"
#include "source.h"
#include "eval.h"

namespace basil {
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

    auto resolve_step(rc<Env> env) {
        return [=](const Value& term) -> EvalResult {
            Value term_copy = term;
            auto new_env = resolve_form(env, term_copy);
            return { new_env, term_copy };
        };
    }

    auto eval_step(rc<Env> env) {
        return [=](const Value& term) -> EvalResult {
            Value term_copy = term;
            return eval(env, term_copy);
        };
    }
}