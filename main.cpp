#include "values.h"
#include "lex.h"
#include "vec.h"
#include "parse.h"
#include "eval.h"

// ubuntu and mac color codes, from 
// https://stackoverflow.com/questions/9158150/colored-output-in-c/
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

using namespace basil;

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
      return 1;
    }
    
    vector<Value> lines;
    TokenView tview(tokens, repl, true);
    while (tview.peek())
      lines.push(parse_line(tview, tview.peek().column));
    if (error_count()) {
      print_errors(_stdout, repl), clear_errors();
      return 1;
    }

    vector<Value> results;
    for (Value v : lines) results.push(eval(global, v));

    if (error_count()) {
      print_errors(_stdout, repl), clear_errors();
      return 1;
    }
    else println(BOLDBLUE, "= ", results.back(), RESET, "\n");
  }
}

// Value append(Value a, Value b) {
//   if (a.is_void()) return b;
//   return cons(head(a), append(tail(a), b));
// }

// Value reverse(Value a, Value acc) {
//   if (a.is_void()) return acc;
//   else return reverse(tail(a), cons(head(a), acc));
// }

// Value reverse(Value a) {
//   return ::reverse(a, empty());
// }

  // Source src("tests/example.bl");

  // Source::View v = src.begin();
  // while (v.peek()) {
  //   Token t = scan(v);
  //   print(t, t.type == T_NEWLINE ? "\n" : " ");
  // }

  // Source repl;

  // while (true) {
  //   Source::View view = repl.expand(_stdin);
  //   while (view.peek())
  //     println(scan(view));
  //   if (error_count())
  //     print_errors(_stdout, repl), clear_errors();
  // }