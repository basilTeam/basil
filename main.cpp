#include "values.h"
#include "lex.h"
#include "vec.h"
#include "parse.h"
#include "eval.h"

using namespace basil;

#define PRINT_TOKENS false
#define PRINT_AST false
#define PRINT_EVAL true

int main() {
  Source repl;

  ref<Env> root = create_root_env();
  ref<Env> global(root);

  while (true) {
    print("? ");
    Source::View view = repl.expand(_stdin);

    vector<Token> tokens;
    while (view.peek())
      tokens.push(scan(view));
    if (error_count()) {
      print_errors(_stdout, repl), clear_errors();
      clear_errors();
      continue;
    }
    else if (PRINT_TOKENS) {
      print(BOLDYELLOW, "⬤ ");
      for (const Token& t : tokens) print(t, " ");
      println(RESET);
    }
    
    vector<Value> lines;
    TokenView tview(tokens, repl, true);
    while (tview.peek()) {
      Value line = parse_line(tview, tview.peek().column);
      if (!line.is_void()) lines.push(line);
    }
    if (error_count()) {
      print_errors(_stdout, repl), clear_errors();
      clear_errors();
      continue;
    }
    else if (PRINT_AST) for (Value v : lines) 
      println(BOLDGREEN, "∧ ", v, RESET);

    vector<Value> results;
    for (Value v : lines) results.push(eval(global, v));

    if (error_count()) {
      print_errors(_stdout, repl), clear_errors();
      clear_errors();
      continue;
    }
    else if (PRINT_EVAL)
      println(BOLDBLUE, "= ", results.back(), RESET, "\n");
  }
}