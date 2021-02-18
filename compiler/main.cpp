#include "source.h"
#include "eval.h"
#include "values.h"
#include "driver.h"
#include "ast.h"
#include "builtin.h"
#include "unistd.h"

using namespace basil;

void print_banner() {
	println("");
	println("┌────────────────────────────────────────┐");
	println("│                                        │");
	println("│          ", BOLDGREEN, R"(𝐵𝑎𝑠𝑖𝑙)",
		RESET, " — version 0.1      │");
	println("│                                        │");
	println("└────────────────────────────────────────┘");
	println(RESET);
}

static bool repl_done = false;

Value repl_quit(ref<Env> env, const Value& args) {
	repl_done = true;
	return Value(VOID);
}

Value print_tokens(ref<Env> env, const Value& args) {
	basil::print_tokens(args.get_product()[0].get_bool());
	return Value(VOID);
}

Value print_parse(ref<Env> env, const Value& args) {
	basil::print_parsed(args.get_product()[0].get_bool());
	return Value(VOID);
}

Value print_ast(ref<Env> env, const Value& args) {
	basil::print_ast(args.get_product()[0].get_bool());
	return Value(VOID);
}

Value print_ir(ref<Env> env, const Value& args) {
	basil::print_ir(args.get_product()[0].get_bool());
	return Value(VOID);
}

Value print_asm(ref<Env> env, const Value& args) {
	basil::print_asm(args.get_product()[0].get_bool());
	return Value(VOID);
}

Value repl_help(ref<Env> env, const Value& args) {
	println("");
	println("Commands: ");
	println(" - $help                 => prints this command list.");
	println(" - $quit                 => exits REPL.");
	println(" - $print-tokens <bool>  => toggles token printing.");
	println(" - $print-parse <bool>   => toggles parse tree printing.");
	println(" - $print-ast <bool>     => toggles AST printing.");
	println(" - $print-ir <bool>      => toggles IR printing.");
	println(" - $print-asm <bool>     => toggles assembly printing.");
	println("");
	return Value(VOID);
}

int intro();

int main(int argc, char** argv) {
	Builtin REPL_HELP(find<FunctionType>(find<ProductType>(), VOID), repl_help, nullptr),
		REPL_QUIT(find<FunctionType>(find<ProductType>(), VOID), repl_quit, nullptr),
		PRINT_TOKENS(find<FunctionType>(find<ProductType>(BOOL), VOID), print_tokens, nullptr),
		PRINT_PARSE(find<FunctionType>(find<ProductType>(BOOL), VOID), print_parse, nullptr),
		PRINT_AST(find<FunctionType>(find<ProductType>(BOOL), VOID), print_ast, nullptr),
		PRINT_IR(find<FunctionType>(find<ProductType>(BOOL), VOID), print_ir, nullptr),
		PRINT_ASM(find<FunctionType>(find<ProductType>(BOOL), VOID), print_asm, nullptr);

	if (argc == 1) { // repl mode
		print_banner();
		println(BOLDGREEN, "Enter any Basil expression at the prompt, or '", 
			RESET, "$help", BOLDGREEN, "' for a list of commands!", RESET);
		println("");
		Source src;

		ref<Env> root = create_root_env();
		root->def("$help", Value(root, REPL_HELP), 0);
		root->def("$quit", Value(root, REPL_QUIT), 0);
		root->def("$print-tokens", Value(root, PRINT_TOKENS), 1);
		root->def("$print-parse", Value(root, PRINT_PARSE), 1);
		root->def("$print-ast", Value(root, PRINT_AST), 1);
		root->def("$print-ir", Value(root, PRINT_IR), 1);
		root->def("$print-asm", Value(root, PRINT_ASM), 1);
		ref<Env> global = newref<Env>(root);
		Function main_fn("main");

		while (!repl_done) repl(global, src, main_fn), clear_errors();
		return 0;
	}
	else if (string(argv[1]) == "intro") {
		return intro();
	}
	else if (argc == 3 && string(argv[1]) == "run") {
		Source src(argv[2]);
		return run(src);
	}
	else if (argc == 3 && string(argv[1]) == "build") {
		Source src(argv[2]);
		return build(src, argv[2]);
	}
	else if (argc > 2 && string(argv[1]) == "exec") {
		Source src;
		string code;
		for (int i = 2; i < argc; i ++) {
			code += argv[i];
			code += ' ';
		}
		src.add_line(code);
		return run(src);
	}
	else if (string(argv[1]) != "help") {
		Source src(argv[1]);
		return run(src);
	}

	print_banner();

	println(BOLDGREEN, "Welcome to the Basil programming language!", RESET);
	println("");
	println("Commands: ");
	println(" - basil                 => starts REPL.");
	println(" - basil <file>          => executes <file>.");
	println(" - basil help            => prints usage information.");
	println(" - basil intro           => runs interactive introduction.");
	println(" - basil exec <code...>  => executes <code...>.");
	println(" - basil build <file>    => compiles <file> to object.");
	println("");
}

static bool running_intro = false;
static i64 intro_section = 0;
static i64 intro_section_index = 0;

Value intro_start(ref<Env> env, const Value& args) {
	running_intro = true;
	return Value(VOID);
}

Value intro_help(ref<Env> env, const Value& args) {
	println("");
	println("Commands: ");
	println(" - $help                 => prints this command list.");
	println(" - $quit                 => exits tour.");
	println(" - $start                => starts tour.");
	println(" - $contents             => prints table of contents.");
	println(" - $section <number>     => skips to section with number <number>.");
	println("");
	return Value(VOID);
}

Value intro_contents(ref<Env> env, const Value& args) {
	println("");
	println("Sections: ");
	println(BOLDWHITE, "1. Basics", RESET);
	println("   - Hello, World!");
	println("   - Constants");
	println("   - Lists");
	println("   - Expressions");
	println("   - Indentation");
	println("   - Quotation");
	println(BOLDWHITE, "2. Definitions", RESET);
	println("   - Variables");
	println("   - Procedures");
	println("   - Lambdas");
	println(BOLDWHITE, "3. Arithmetic and Logic", RESET);
	println("   - Arithmetic Operators");
	println("   - Booleans");
	println("   - Logical Operators");
	println("   - Comparisons");
	println(BOLDWHITE, "4. Infix Operators", RESET);
	println("   - Binary Operators");
	println("   - Unary Operators");
	println("   - Higher-Arity Operators");
	println("   - Mixfix Procedures");
	println(BOLDWHITE, "5. Control Flow", RESET);
	println("   - If Expressions");
	println("   - While Loops");
	println(BOLDWHITE, "6. Metaprogramming", RESET);
	println("   - Splices");
	println("   - Aliases");
	println("   - Macro Procedures");
	println("   - Macro Operators");
	println(BOLDWHITE, "7. Imports", RESET);
	println("   - Use Statement");
	println("");
	return Value(VOID);
}

Value intro_set_section(ref<Env> env, const Value& args) {
	if (!args.get_product()[0].is_int()) {
		err(args.get_product()[0].loc(), "Expected integer for section number, given ",
			args.get_product()[0]);
		return Value(ERROR);
	}
	if (args.get_product()[0].get_int() < 1 
		|| args.get_product()[0].get_int() > 7) {
		err(args.get_product()[0].loc(), "Invalid section number. Must be between 1 ",
			"and 7.");
		return Value(ERROR);
	}
	intro_section = args.get_product()[0].get_int() - 1;
	intro_section_index = 0;
	return intro_start(env, args);
}

struct IntroStep {
	const char* text;
	const char* instruction;
	Value expected;
	const char* title = nullptr;
};

struct IntroSection {
	vector<IntroStep> steps;
};

static vector<IntroSection> intro_sections;

void add_step(IntroStep step) {
	intro_sections.back().steps.push(step);
}

Value runtime(const Type* t) {
	return new ASTSingleton(t);
}

void init_intro_sections() {
	intro_sections.push({}); // Basics
	add_step({
R"(Welcome to Basil! 
Basil is a lightweight programming language, which aims 
to make it as easy as possible to write expressive and 
performant code. We'll talk about some of the details 
later, but for now, let's write a hello-world program!
)", 
R"(Type 'display "hello world"' into the prompt, then 
press Enter twice.
)", runtime(VOID), "Basics"});
	add_step({
R"(All Basil programs are made up of terms. The simplest 
terms are constants, which evaluate to themselves. For 
example, an integer constant.
)", 
R"(Type '12' into the prompt, then press Enter twice.
)", Value(12)});
	add_step({
R"(In addition to integer constants, which represent 
numbers, Basil supports string constants, which represent 
pieces of text.
)", 
R"(Type '"hello"' into the prompt, then press Enter twice.
)", Value("hello", STRING)});
	add_step({
R"(Basil also supports values that contain multiple terms.
These values are called lists. If you're familiar with
any Lisp dialects, these will probably be pretty familiar
to you. To express a list value, we enclose other
terms in square brackets.
)", 
R"(Type '[1 2 3]' into the prompt, then press Enter twice.
)", list_of(1, 2, 3)});
	add_step({
R"(The empty list is a little special in Basil, but it can
be written about as you'd expect. It generally represents
the absence of a value.
)", 
R"(Type '[]' into the prompt, then press Enter twice.
)", empty()});
	add_step({
R"(Lists can be useful for storing data, but they can also
be used to represent code. Basil programs themselves are
just lists of terms that the Basil compiler evaluates.
To write a list we want to be evaluated, we enclose other
terms in parentheses, and separate them with spaces.
)",
R"(Type '(+ 1 2)' into the prompt, then press Enter twice.
)", Value(3)});
	add_step({
R"(This is an example of evaluation. '+' is a symbol that
represents a built-in function to add two numbers. When
we evaluate a list that starts with a function, the result
of evaluation is the result of that function. So, (+ 1 2)
is 3.

Writing all these parentheses can be kind of a pain! So
Basil treats lines on the top level of the program as 
lists, as if they were surrounded by parentheses. We still
need to separate every term with spaces, though.
)",
R"(Type '* 3 3' into the prompt, then press Enter twice.
)", Value(9)});
	add_step({
R"(Basil programs can contain multiple terms. This is
why you have to press Enter twice at the REPL - to be
able to enter multiple terms at once. The program
evaluates every term, then returns the last result.
)",
R"(Type:
	1
	2
	3
into the prompt, pressing Enter after each one. Press Enter 
again after the last line.
)", Value(3)});
	add_step({
R"(We can write expressions that evaluate others this way
using the 'do' special form. 'do' will evaluate each expression
in its body, then return the last result - in a way that can be
used in other expressions, like '*'.
)",
R"(Type '* 2 (do 1 2 3)' into the prompt, then press Enter twice.
)", Value(6)});
	add_step({
R"(Sometimes, we want to 'do' quite a lot of things. In
these cases, it can be helpful to split a line into
multiple using an indented block. The indentation level
doesn't matter, as long as it's consistently more than 
the starting line.
)",
R"(Type:
	do
		1
		2
		3
into the prompt, then press Enter twice.
)", Value(3)});
	add_step({
R"(So, we evaluate terms to get values, and those values
are the result of our program. But what if we want to get
the value of some bit of code before it's evaluated? For
this, we use the 'quote' special form.
)",
R"(Type 'quote x' into the prompt, then press Enter twice.
)", Value("x")});
	add_step({
R"(We 'quote' x before it's evaluated, and get a symbol
value. Symbols represent unique names, not bits of text 
like strings.
The 'quote' special form can be abbreviated with a ':'.
)",
R"(Type ':x' into the prompt, then press Enter twice.
)", Value("x")});
	add_step({
R"(That's the basics! But there's a lot more to Basil that
we'll cover in the next sections.
)",
R"(Enter ':okay!' into the prompt when you're ready to continue.
)", Value("okay!")});
	intro_sections.push({}); // Definitions
	add_step({
R"(We've discussed how to write simple expressions and
constants in Basil up until this point. But for more 
complex programs, we might want to assign certain values 
names. We can do just that with the 'def' special form.
)", 
R"(Enter:
	def x 1
	x 
into the prompt.
)", Value(1), "Definitions"});
	add_step({
R"('def' defines a variable in an environment. When a 
symbol is evaluated in an environment, it evaluates to 
the value of the associated variable, if it exists. This 
value can be used in arbitrary expressions.
)", 
R"(Enter:
	def x 1
	def y 2
	+ x y
into the prompt.
)", Value(3)});
	add_step({
R"(We can also define procedures. Procedures take arguments,
and produce a resulting value. We've seen a few built-in
procedures so far, like '+' and '*'. Now, we can define our
own procedures!
)", 
R"(Enter:
	def (square x) 
		* x x
	square 12
into the prompt.
)", Value(144)});
	add_step({
R"(We can add more arguments to a procedure by adding them
to the list after the 'def'.
)", 
R"(Enter:
	def (add-squares x y) 
		+ (* x x) (* y y)
	add-squares 3 4
into the prompt.
)", Value(25)});
	add_step({
R"(If a procedure has multiple terms in its body, it will
return the last result, as if they were wrapped in a 'do'.
)", 
R"(Enter:
	def (print x y)
		display x
		display y
	print 1 2
into the prompt.
)", runtime(VOID)});
	add_step({
R"(If a procedure has no arguments, it will evaluate without
arguments.
)", 
R"(Enter:
	def (greet)
		display "hello world"
	greet
into the prompt.
)", runtime(VOID)});
	add_step({
R"(Sometimes, we might want to produce a function value
without giving it a name. In these cases, we can use the
'lambda' special form to define an anonymous function.
)", 
R"(Enter '(lambda (x y) (+ x y)) 1 2' into the prompt.
)", Value(3)});
	add_step({
R"(That's how definitions work! Next, we'll go over the 
built-in operations Basil supports, before getting into 
control flow in the section after.
)",
R"(Enter ':okay!' into the prompt when you're ready to continue.
)", Value("okay!")});
	intro_sections.push({}); // Arithmetic and such
	add_step({
R"(So far we've introduced the '+' and '*' operators, 
which add and multiply numbers. Basil also supports '-', '/', 
and '%', each of which take two numbers and find the 
difference, quotient, or remainder respectively.
)", 
R"(Enter any expression that evaluates to '81' into the prompt.
)", Value(81), "Arithmetic and Logic"});
	add_step({
R"(Basil includes syntactic sugar for prefix '-' and 
'+'. If either of these are written before a term (with 
no space), they act as unary minus and plus operators.
)", 
R"(Enter any expression that evaluates to '-16' into the prompt.
)", Value(-16)});
	add_step({
R"(Basil also includes syntactic sugar for coefficients. 
If a number is written immediately before another term 
(with no space), it will multiply the term by the number.
)", 
R"(Enter:
	def x 4
	3x + 1 
into the prompt.
)", Value(13)});
	add_step({
R"(In addition to numbers, Basil also has support for a 
boolean type, which can be either of the values 'true' or 
'false'.
)", 
R"(Enter 'true' into the prompt.
)", Value(true, BOOL)});
	add_step({
R"(The 'and', 'or', and 'xor' procedures can be used to 
compute the logical and, inclusive-or, or exclusive-or of 
two booleans respectively..
)", 
R"(Enter 'or (and true false) (xor true true)' into the prompt.
)", Value(false, BOOL)});
	add_step({
R"(The 'not' procedure can be used to compute the logical 
complement of a boolean value.
)", 
R"(Enter 'not false' into the prompt.
)", Value(true, BOOL)});
	add_step({
R"(We can get boolean values by comparing integers or
strings. The '==' and '!=' procedures return whether two 
inputs are equal or inequal, respectively.
)", 
R"(Enter '== "cat" "cat"' into the prompt.
)", Value(true, BOOL)});
	add_step({
R"(We can also compare values by some order. The '<'
and '>' procedures return whether the first argument
is less than or greater than the second, respectively.
'<=' and '>=' behave the same as '<' and '>', but also 
return true if the arguments are equal.
)", 
R"(Enter '< 12 7' into the prompt.
)", Value(false, BOOL)});
	add_step({
R"(That's all the boolean and arithmetic operators
Basil supports! Next up, we'll get into infix
operators, a more unique Basil feature to help write
these expressions more naturally.
)",
R"(Enter ':okay!' into the prompt when you're ready to continue.
)", Value("okay!")});
	intro_sections.push({}); // Infix operators
	add_step({
R"(Until this point, all expressions have been written
in 'prefix notation', with the procedure coming first,
followed by some arguments. This is great for parsers,
but it can be kind of unfamiliar, and inconvenient!
Basil supports this notation, but also allows some
procedures to be used 'infix', placed in-between their
arguments.
)", 
R"(Enter '1 + 2' into the prompt.
)", Value(3), "Infix Operators"});
	add_step({
R"(All of the arithmetic and logical operators we've
seen so far can be used infix. To determine which
operator applies to what, each operator has a certain
precedence. Operators of higher precedence, like '*',
bind more tightly than '+'.
)", 
R"(Enter '1 + 2 * 3' into the prompt.
)", Value(7)});
	add_step({
R"(Chains of infix operators are actually translated
by the compiler into equivalent prefix notation before
they are evaluated. This happens in standalone lists,
like in the previous two examples, and also in the
argument lists of procedure calls or special forms.
)", 
R"(Enter:
	def (double x)
		x * 2
	double 10 / 5
into the prompt.
)", Value(4)});
	add_step({
R"(Infix operators can be defined by the user as well.
This is done with the 'infix' special form. 'infix'
operates very similarly to 'def', but with two key
differences:
 - 'infix' accepts an integer constant after the 'infix'
   symbol, to be used as the operator's precedence.
 - The name of the procedure is the second element of
   the argument list, not the first.
)", 
R"(Enter:
	infix (x add y) 
		x + y
	1 add 2 add 3
into the prompt.
)", Value(6)});
	add_step({
R"(Infix operators don't have to be binary operators.
A unary infix operator behaves like a postfix function.
)", 
R"(Enter:
	infix (x squared) 
		x * x
	13 squared
into the prompt.
)", Value(169)});
	add_step({
R"(Infix operators can have more than two arguments too.
The operator is still written after the first argument,
but more than one argument may follow it.
)", 
R"(Enter:
	infix (s scale x y z) 
		[x * s y * s z * s]
	2 scale 4 5 6
into the prompt.
)", list_of(8, 10, 12)});
	add_step({
R"(Basil also supports a lesser-known concept known as
mixfix syntax. Every Basil procedure definition (so, 
both 'def' and 'infix') allows one or more keywords in
the argument list. These keywords aren't arguments, but
must be present in the function's invocation. This
can help make functions with many arguments more readable.
)", 
R"(Enter:
	def (add x :to y) 
		x + y
	add 1 to 2
into the prompt.
)", Value(3)});
	add_step({
R"(Mixfix syntax can allow a single function invocation
to span multiple lines. This only occurs when the
following lines start with keywords of said function.
In order for this to work, the keywords also need to be
explicitly quoted.
)", 
R"(Enter:
	def (first x :then y :finally z) 
		x
		y
		z
	first 
		"hello" 
	:then
		"hi there"
	:finally
		"goodbye"
into the prompt.
)", Value("goodbye", STRING)});
	add_step({
R"(That's the overview of infix and mixfix procedures!
We think these are some of the coolest features Basil
has to offer. A lot of common programming patterns can
be expressed in very natural terms, without sacrificing
homoiconicity or performance. Next, we'll be going over
Basil's control-flow primitives.
)",
R"(Enter ':okay!' into the prompt when you're ready to continue.
)", Value("okay!")});
	intro_sections.push({}); // Control flow
	add_step({
R"(The simplest conditional expression Basil supports
is the '?' operator. '?' is an infix, ternary operator,
that takes a condition and two pieces of code. If the
condition is true, it evaluates and returns the first;
otherwise, it evaluates and returns the second.
)", 
R"(Enter '4 < 5 ? "less" "greater"' into the prompt.
)", Value("less", STRING), "Control Flow"});
	add_step({
R"(For more complex conditionals, we found that it was
nice to have a little more structure. So the 'if'
special form exists as an alternative to '?'. 'if', 
like '?', picks between two pieces of code based on a
condition. For 'if', though, the case if false needs to
be preceded by the keyword 'else'.
)", 
R"(Enter 'if true :yes else :no"' into the prompt.
)", Value("yes")});
	add_step({
R"('if' can have multiple conditions in the same list,
using any number of 'elif' keywords. Each 'elif'
behaves like its own 'if', within the larger expression.
)", 
R"(Enter: 
	def x 2
	if x == 1 "one"
	:elif x == 2 "two"
	:else "many"
into the prompt.
)", Value("two", STRING)});
	add_step({
R"(Unlike '?', 'if' can have multiple terms in the
body of each conditional. If multiple terms are
provided, each is evaluated and the last result is
returned when the corresponding condition is true.
)", 
R"(Enter: 
	if true
		display "it's true!"
		display "hooray!"
	:else
		display "it's false..."
into the prompt.
)", runtime(VOID)});
	add_step({
R"(Basil also has a 'while' special form. This
operates similarly to if, but instead of evaluating
its body once, it will evaluate its body until its
condition is false.
)", 
R"(Enter: 
	def x 0
	while x < 10
		display x
		x = x + 1
into the prompt.
)", runtime(VOID)});
	add_step({
R"(That's all the built-in control flow Basil has!
We intentionally kept it short, since other Basil
features allow more specific control flow to be
user-defined - including the next topic, macros!
)", 
R"(Enter ':okay!' into the prompt when you're ready to continue.
)", Value("okay!")});
	intro_sections.push({}); // Macros
	add_step({
R"(One of the biggest advantages of a simple, homoiconic
syntax is being able to support macros. In Basil, macros
behave very similarly to variables and procedures, only
operating on source code instead of values. The building
block of all of this is the splice special form, which
will evaluate a list of terms and insert the result in
its place in the source code.
)", 
R"(Enter 'quote (1 + (splice 1 + 1))' into the prompt.
)", list_of(1, Value("+"), 2), "Macros"});
	add_step({
R"(As a bit of syntactic sugar, any terms enclosed in '|'
characters become a splice.
)", 
R"(Enter 'quote (|2 * 2|)' into the prompt.
)", list_of(4)});
	add_step({
R"(The simplest kind of macro is an alias, kind of like
a macro variable. It'll replace itself wherever it's used
with a Basil term. For example, let's say you prefer 'let'
instead of 'def' to define variables:
)", 
R"(Enter:
	macro let def
	let x 1
	x
into the prompt.
)", Value(1)});
	add_step({
R"(We can also define macro procedures. These work just
like normal procedures, only they're defined with 'macro'
instead of 'def', and they automatically quote their
arguments and body. Instead of evaluating the body to get
a value, macro procedures splice code into the body to
generate the desired code.
)", 
R"(Enter:
	macro (dotwice expr)
		|expr|
		|expr|
	dotwice (display "hello")
into the prompt.
)", runtime(VOID)});
	add_step({
R"(Like normal procedures, macros can also be
declared infix. The 'infix-macro' special form is used
which, like 'infix', can optionally be given a precedence.
)", 
R"(Enter:
	infix-macro (expr unless cond)
		if |cond| [] else |expr|
	(display "bad thing!") unless 1 == 1
into the prompt.
)", Value(VOID)});
	add_step({
R"(Again like normal procedures, macros can be declared
mixfix, with any number of keywords. This applies to both
'macro' and 'infix-macro'.
)", 
R"(Enter:
	macro (print expr :if cond)
		if |cond| 
			display |expr|
	print "hello" if (3 > 2)
into the prompt.
)", runtime(VOID)});
	add_step({
R"(That's actually all there is to macros! They're not
too special or complex compared to other functions,
despite giving you direct control over the language's
syntax. Like infix functions, this is one of the Basil
features we're really proud of!
)", 
R"(Enter ':okay!' into the prompt when you're ready to continue.
)", Value("okay!")});
	intro_sections.push({}); // Macros
	add_step({
R"(The last main feature of Basil is importing other
source files. This is done very simply, using the 'use'
special form. 'use' takes one argument, a symbol, and
loads the file at that path (with a '.bl' file extension
appended to it).
)", 
R"(Enter:
	use std/list
	sort [4 2 3 1]
into the prompt.
)", runtime(find<ListType>(INT)), "Imports"});
	add_step({
R"('use' will evaluate the file it's provided, but instead
of producing any side-effects or values, it will simply
take the definitions produced from the file and add them
to the current environment.
There's really not too much else to say about it,
but you can take a look around the project directory
and play around with some of the functions we've written!
Or write your own files if you like!.
)",
R"(Enter ':okay!' into the prompt when you're ready to continue.
)", Value("okay!")});
}

#include "stdlib.h"
#include "time.h"

void print_intro_conclusion() {
	println(BOLDGREEN, "Congratulations! \n", RESET);
	println(
R"(Congratulations! You've completed the Basil 
interactive tour!

If you'd like to continue to play around with the 
language, try quitting this tour and running './basil help' 
in command line for a full list of Basil commands!

If you really liked the project, consider:
 - starring it on GitHub! https://github.com/basilTeam/basil
 - following the devs on Twitter! (@elucentdev, @minchyAK)

If you want to get in touch, a Twitter DM (handles above)
or a Discord message (elucent#1839, minchy#2474) would be
great.

Bye for now!
)");
}

int intro() {
	srand(time(0));
	init_intro_sections();
	
	print_banner();
	println(BOLDGREEN, "Welcome to the interactive Basil tour!", RESET);
	println(BOLDGREEN, "Press Enter twice after entering a command to submit it.", RESET);

	Source src;

	Builtin REPL_QUIT(find<FunctionType>(find<ProductType>(), VOID), repl_quit, nullptr),
		INTRO_HELP(find<FunctionType>(find<ProductType>(), VOID), intro_help, nullptr),
		INTRO_CONTENTS(find<FunctionType>(find<ProductType>(), VOID), intro_contents, nullptr),
		INTRO_START(find<FunctionType>(find<ProductType>(), VOID), intro_start, nullptr),
		INTRO_SET_SECTION(find<FunctionType>(find<ProductType>(INT), VOID), intro_set_section, nullptr);

	ref<Env> root = create_root_env();
	root->def("$quit", Value(root, REPL_QUIT), 0);
	root->def("$help", Value(root, INTRO_HELP), 0);
	root->def("$contents", Value(root, INTRO_CONTENTS), 0);
	root->def("$start", Value(root, INTRO_START), 0);
	root->def("$section", Value(root, INTRO_SET_SECTION), 1);

	intro_help(root, Value(VOID));

	const char* congratulations[] = {
		"Awesome!",
		"Great!",
		"Nice!",
		"Cool!",
		"You did it!",
		"You got it!"
	};

	while (!repl_done) {
		ref<Env> global = newref<Env>(root);
		Function main_fn("main");

		bool is_running_intro = running_intro;
		if (is_running_intro) {
			const IntroStep& step = intro_sections[intro_section].steps[intro_section_index];
			if (step.title) 
				println(intro_section + 1, ". ", BOLDGREEN, step.title, RESET, '\n');
			println(BOLDWHITE, step.text, RESET);
			println(ITALICWHITE, step.instruction, RESET);
		}

		if (is_running_intro) {
			bool correct = false;

			while (!correct && !repl_done) {
				Value result = repl(global, src, main_fn);

				const IntroStep& step = intro_sections[intro_section].steps[intro_section_index];
				if (result.is_runtime() && step.expected.is_runtime())
					correct = result.type() == step.expected.type();
				else correct = result == step.expected;

				if (!correct)
					println(ITALICRED, "Expected '", step.expected, "', but given '", 
						result, "'.", RESET, '\n');
				else {
					println(ITALICWHITE, congratulations[rand() % 6], RESET);
					intro_section_index ++;
					if (intro_section_index >= intro_sections[intro_section].steps.size())
						intro_section ++, intro_section_index = 0;
					println("");
					usleep(1000000);
					println("⸻", "\n");
					if (intro_section >= intro_sections.size())
						repl_done = true, print_intro_conclusion();
				}
			}
		}
		else repl(global, src, main_fn);
	}

	return 0;
}
