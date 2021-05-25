#include "test.h"
#include "parse.h"

using namespace basil;

SETUP {
    init_types_and_symbols();
}

TEST(main) {
    Source src("test/compiler/long-example");
    Source::View view(src);
    vector<Token> tokens = lex_all(view);

    vector<Value> values;
    TokenView tview(tokens);
    while (tview) values.push(parse(tview));
}