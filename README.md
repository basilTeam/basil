# The Basil Programming Language - revision 0.1

---

## Table of Contents

1. [Project Structure](#language-summary)

2. [Language Summary](#language-summary)

3. [Syntax](#syntax)

4. [Evaluation](#evaluation)

5. [Types](#types)

6. [Built-In Procedures](#built-in-procedures)

7. [Compilation](#compilation)

---

## Project Structure

| File | Description |
|---|---|
| `jasmine/` | An external library for lightweight machine code generation. Repository [here](https://github.com/elucent/jasmine). |
| `util/` | C++ utilities. Described [here](https://repl.it/@basilTeam/basil#util/README.md). |
| `source.h/cpp` | A traversible and growable representation of a Basil source. |
| `errors.h/cpp` | An error reporting and formatting utility. |
| `lex.h/cpp` | A token type, and the implementation of the Basil tokenizer. |
| `type.h/cpp` | A few kinds of types, that can describe any Basil value. |
| `env.h/cpp` | A definition type for Basil variables, and a lexically-scoped environment in which they can be declared and found. |
| `values.h/cpp` | A dynamic value type, which can represent any Basil value, as well as a number of associated primitive operations. |
| `eval.h/cpp` | Functions and utilities for interpreting Basil values as code. |
| `ast.h/cpp` | A suite of typed AST nodes, for use in generating runtime code. |
| `ssa.h/cpp` | A suite of instruction types, generated from AST nodes and lowerable to x86_64 machine code. |
| `native.h/cpp` | A few native intrinsics, used to implement built-in behaviors such as allocation or IO. |
| `driver.h/cpp` | A few frontend functions for invoking the Basil compiler in different ways. |
| `main.cpp` | The driver function for the Basil command-line application. |

---

## Language Summary

Basil is a lightweight language with homoiconic syntax, high performance, and
an emphasis on supporting expressive code. Many of its base features are based on
Lisp, and you could probably consider it a Lisp dialect, but it also does a number
of distinctly un-Lispy things.

All Basil programs are made up of terms. Terms can only be three things: a number,
a string, a symbol, or a list of zero or more terms. All of these terms have valid
Basil values as well, so code can be interpreted as data and data can be interpreted
as code. Homoiconicity!

Unlike most Lisp dialects, Basil cuts down on parentheses as much as possible,
replacing them with significant whitespace rules and infix operator support. 
A program that looks like this in Scheme:
```scheme
(define (factorial x)
	(if (= x 0) 1
		(* x (factorial (- x 1)))))
(display (factorial 10))
```
...looks like this in Basil:
```rb
infix (x factorial)
	if x == 0 1 
	:else x - 1 factorial * x
display 10 factorial
```
Even if you don't think the function itself is much clearer or simpler, you can't
deny that being able to type `display 10 factorial` is pretty cool!

The real killer feature of Basil, however, is its efficiency. Basil is a _compiled_,
_statically-typed_ language, and it manages to achieve this along with great
expressivity thanks to a pretty novel concept.

Basically, when the Basil compiler tries to compile a program, it's really
interpreting it. It will automatically compute any expressions it can, and for many
small toy programs the compiler can evaluate them fully - the executable produced
just spits out a constant.

But the compiler is still a compiler, it doesn't touch any code that might have
consequences. So anything with side-effects, such as IO functions, mutation of
variables, or potentially-non-terminating code gets wrapped up and saved for later.
This wrapped-up code propagates through the program, until at the end of compilation,
you have a statically-typed AST of all the stuff the compiler saved for runtime,
which can then be lowered to machine code.

Ultimately, this means you get all the benefits of a high-level language, without any
cost at runtime! And while the compiler currently doesn't emit terribly fast code
(since it was written from scratch in the last week of the langjam), there's no
reason it couldn't eventually employ the same optimizations as any other
statically-typed language.

---

## Syntax

#### Tokens

| Token | Description | Examples |
|---|---|---|
| `Int` | Any number of decimal digits. | `12` `04` `512` |
| `Coeff` | An `Int` immediately preceding a non-digit character. | `3x` `4(2)` |
| `(` `)` | Left `(` and right `)` parentheses. Delimiter. | `(` `)` |
| `[` `]` | Left `[` and right `]` square brackets. Delimiter. | `[` `]` |
| `|` | A pipe `|` character. Delimiter. | `|` `|` |
| `.` | A single dot `.`. Delimiter. | `.` |
| `String` | Any number of characters between two double-quotes `"`. Delimiter. | `"hello"` `"cat"` `"line\nbreak"` |
| `Symbol` | Any number of non-delimiter, non-space characters not otherwise covered by a token. Cannot start with a digit or underscore `_`. Includes tokens of repeated dots `.` or colons `:`. | `x` `Y2` `*name*` `..` `::` |
| `Plus` | A single `+` immediately preceding an alphanumeric character or delimiter. | `+1` `+y` |
| `Minus` | A single `-` immediately preceding an alphanumeric character or delimiter. | `-3` `-x` |
| `Quote` | A single `:` immediately preceding any non-space character. | `:yes` `:(1 2 3)` |
| `Newline` | A line break character. | `\n` |

#### EBNF Grammar

```ebnf
Term     := Int                     (* examples: 12 22 37             *)
         |  String                  (* examples: "hello" "world")     *)
         |  Symbol                  (* examples: abc Fromage12 ke-bab *)
         |  ( Term+ )               (* examples: (+ 1 2) (5)          *)
         |  [ Term* ]               (* examples: [1 2 3] [:yes :no]   *)
         |  | Term* |               (* examples: |+ 1 2| |my-var|     *)
         |  Coeff Term              (* examples: 3x 45(5) 2(x - 1)    *)
         |  Plus Term               (* examples: +4 +56 +x +(1 + 2)   *)
         |  Minus Term              (* examples: -4 -56 -x -(1 + 2)   *)
         |  Quote Term              (* examples: :hello :(my-list)    *)
				 |  Term . Term 						(* examples: list.tail 1.inc  		*)
         ;

Line     := Term* Newline [<indent> Line* <dedent>]¹ [Quote Line]*²

Program  := Line*
```
1. Indented block. Indentation is context-sensitive, the block continues until the indentation level is less than or equal to the starting line's indentation. Each line is appended as a list to the starting line's contents.
2. Continuation line. The `Quote` token is included in each line. The contents of each line are appended to the starting line.

Out of the above grammar, the following terminals produce single terms in the source
code:

 * `Int` generates an integer term.
 * `String` generates a string term.
 * `Symbol` generates a symbol term.

The following productions produce lists of their contents:

 * `( Term+ )` generates a list of each of its element terms.
 * `[ Term* ]` generates the list `(list-of Term0 Term1 ... TermN)`.
 * `| Term* |` generates the list `(splice Term0 Term1 ... TermN)`.
 * `Coeff Term` generates the list `(* Coeff Term)`.
 * `Plus Term` generates the list `(+ 0 Term)`.
 * `Minus Term` generates the list `(- 0 Term)`.
 * `Quote Term` generates the list `(quote Term)`.
 * `Term . Term` generates the list `(Term Term)`.
 * `Line` generates a list of each of its element terms.
 * `Program` generates a list `(do Line0 Line1 ... LineN)` for each of its lines.

---

## Evaluation

Evaluation is the process of converting Basil code to Basil values. Every Basil term
can be evaluated, and there are a number of distinct evaluation processes depending on
a given term's contents. Each kind of evaluation is called a 'form', and forms that
are based on special symbols baked into the compiler are called 'special forms'.

#### Preparation

Before evaluation occurs, a term must first be prepared. Preparation consists of a few
passes the compiler does to manipulate the term so it can be evaluated properly. This
includes, in order:

 * Expanding `splice` special forms.
 * Finding macro definitions.
 * Expanding macro invocations.
 * Finding variable and procedure definitions.
 * Transforming lists based on infix rules.

The associated special forms for each of these phases are described later in this
section.

#### Environments

All evaluation occurs within an environment. Environments are where variables are
defined and stored. Basil is lexically-scoped, which means that environments are based
on the syntax of the language - e.g. all expressions at the top-level of the program
are evaluated in a _global_ environment, while expressions within a top-level function
definition are evaluated in a _local_ function environment that descends from the 
global environment.

#### Primitives

Integers, strings, and symbols all evaluate by themselves, without any particular
form.

Integer and string constants evaluate to themselves, e.g. `10` evaluates to `10`.

Symbols evaluate to a variable value. The symbol name is looked up in the current
environment. If it exists in the current environment or any parent environment, the 
evaluation result is whatever value is bound to that variable. If it doesn't exist,
the compiler errors.

#### Special Forms

1. Quote

```
quote <value>
```

The `quote` special form returns its value without evaluating it. For example, a
quoted symbol will be an actual symbol value, not a variable looked up in some
environment.

2. Splice

```
splice <value0> <value1> ... <valueN>
```

The `splice` special form evaluates a list of the values provided. It takes the
resulting value, and replaces itself with that value in the source code.

3. Do

```
do <body0> <body1> ... <bodyN>
```

The `do` special form evaluates each of its body terms in order, and returns the last
evaluation result.

4. Definition

```
def <name> <value>
def (<name> [argument0] [argument1] ... [argumentN]) <body0> [body1] ... [bodyN]
```

The first `def` form defines a variable in the current environment, initialized with
the provided value.

The second `def` form defines a procedure in the current environment. The procedure's
name is the first element of the argument list. The rest of the values in the
argument list are declared in a new, local environment as argument variables. Each of
the procedure's body terms are visited in the local environment, but aren't evaluated.
The procedure's body terms are wrapped in a `do` special form.

Arguments in the second form may either be symbols (which define variables as
described), _or_ quoted symbols, which define keywords. Keywords represent custom
syntax for the function call, not additional parameters. For example, for a procedure
`for <name> in <list>`, the `in` term would be a keyword - used to write the
procedure more cleanly, but not representing a value.

The evaluation result of either of these forms is the empty list.

5. Macro Definition

```
macro <name> <term>
macro (<name> [argument0] [argument1] ... [argumentN]) <body0> [body1] ... [bodyN]
```

The first `macro` form defines a macro variable, or alias. When expanded, it replaces
itself with the term it was initialized to. The term in this form is not evaluated.

The second `macro` form defines a macro procedure. Like a normal procedure, the
procedure name, arguments, and any keywords are defined in an argument list. Also
like a normal procedure, all of the body terms are wrapped in a `do` special form.
Unlike a normal procedure, however, none of the body terms are prepared. Splices,
other macro expansions, infix parsing - all of this waits until after the macro is
expanded.

The evaluation result of either of these forms is the empty list.

6. Infix Definitions

```
infix [precedence] (<argument0> <name> [argument1] ... [argumentN]) <body0> [body1] ... [bodyN]
infix-macro [precedence] (<argument0> <name> [argument1] ... [argumentN]) <body0> [body1] ... [bodyN]
```

The `infix` form defines an infix procedure. This behaves identically to `def` with
regards to creating a procedure, with the addition of infix capabilities. The infix
precedence of the procedure can optionally be specified, with an integer constant.
Infix procedures must have at least one argument, and the name of the procedure is the
second term in the argument list, not the first.

The `infix-macro` form is the same as `infix`, but defines a macro procedure instead
of a procedure.

The evaluation result of either of these forms is the empty list.

7. If

```
if <condition> <if-body>* [elif <condition> <elif-body>*]* else <else-body>* 
```

The `if` form creates an if expression. If the provided condition is
true, the provided if-body is evaluated and returned. Otherwise, the provided
else-body is evaluated and returned. If one or more `elif` keywords are provided
before the else, their respective conditions are evaluated, and if true their
respective bodies are evaluated and returned.

All bodies (if-body, elif-body, and else-body) above can contain any number of
terms. These terms are wrapped in `do` forms implicitly.

8. While

```
while <condition> <body0> [body1] ... [bodyN]
```

The `while` form creates a while statement. As long as the provided condition is true,
the body terms are repeatedly evaluated. When the loop terminates, the resulting
value is the empty list. Like `if` and procedure bodies, the body terms in a `while`
are implicitly wrapped in a `do` form.

9. List Of

```
list-of [value0] [value1] ... [valueN]
```

The `list-of` form returns a list value containing each of the provided values as
elements.

10. Lambda

```
lambda ([argument0] [argument1] ... [argumentN]) <body0> [body1] ... [bodyN]
```

The `lambda` form creates an anonymous function. It behaves identically to a
procedure definition, with the exception that the argument list does not contain a
name. Unlike a procedure definition, which evaluates to the empty list, a `lambda`
evaluates to a function value - the function it declares.

#### General Forms

1. Procedure Invocation

```
<procedure> [argument0] [argument1] ... [argumentN]
```

This form invokes a procedure on a number of arguments. The arguments are each
evaluated, passed into the procedure, and the evaluation result is whatever the
procedure returns. The compiler errors if the number of arguments provided is
incorrect for the function.

In each position where the procedure has a keyword argument, the keyword may be
provided either quoted (`:<keyword>`) or unquoted (just `<keyword>`). If the
correct keyword is not provided in either form, the compiler errors.

2. Macro Expansion

```
<macro> [argument0] [argument1] ... [argumentN]
```

This form is identical to the procedure invocation form, with the caveat that instead
of invoking a function, it is expanding a macro. The arguments are not evaluated.
When they are passed to the macro, instead of a value being created, any splice
forms in the macro body are expanded, then the resulting body replaces the macro
expansion in the source code.

3. Singleton Evaluation

```
<term>
```

If a list contains a single term that is not a procedure or macro procedure, then the
list evaluates to whatever that term evaluates to. Essentially, wrapping things in
parentheses more times doesn't change the value.

4. Infix Parsing

```
<argument0> <operator> [argument1] ... [argumentN]
```

If no other form applies, the list will attemptedly handle any infix procedures
within itself, rearranging terms until they can be evaluated with other forms.

All operators are left-associative. The higher an operator's precedence, the more
tightly it binds to arguments. For example, `1 + 2 * 3` translates to `(+ 1 (* 2 3))`,
because `*` has higher precedence than `+`. If only one operator contests a value,
such as if a unary operator is between a value and a binary operator, or for the
interior values of a ternary-or-higher operator, precedence is ignored and the
operator is applied to the value.

The result of infix parsing is a list of other general forms - invocations,
expansions, and singletons - equivalent to the provided infix expressions. The
original list is replaced in the source code with this processed list.

---

## Types

Basil has only a small number of built-in types.

#### Primitives

The `Int` type represents a negative or positive integer, with a bit width equal to
the word size of the machine.

The `Symbol` type represents a unique quoted symbol, with a bit width equal to
the word size of the machine.

The `String` type describes a text string, represented by a pointer to a
null-terminated array of bytes. A string's data is not mutable.

The `Bool` type describes a true or false value, represented by a word-size integer.
Zero represents false, any other value represents true.

#### Lists

The `List` type is parameterized by an element type. For example, an `Int List` is a
list where each element has type `Int`.

List instances are cons cells - pointers to pairs, the first element being a list element, the second element being a pointer to the next cell in the list. A list's
data is not mutable.

The empty list has a special type `Void`. Its runtime representation is the null
pointer.

#### Functions

Function types are parameterized by two types: the argument type and return type. For
example, the type `Int -> Int` describes a function that accepts one integer argument
and returns an integer.

Functions are not currently implemented at runtime. They will eventually be
implemented as pointers to closures or raw function pointers (if no captures).

#### Unification

Unification is a relation from two source types to a destination type. The
destination type must be a type both source types are compatible with.

The `Void` type can unify with any `List` type, for example.

#### Type Variables

Untyped variables in Basil are assigned unique type variables. An unbound type variable can unify with any other type, binding to that type in the process. A bound
type variable acts identically to the type it is bound to.

Type variables can parameterize compound types. When this is the case, said
compound types can unify with other compound types of the same kind, unifying the
type variable parameter with the corresponding parameter in the other type. For
example, a `'T0 List` can unify with an `Int List`, binding `'T0` to `Int` in the
process.

---

## Built-In Procedures

This is a table of all the built-in procedures and variables in Basil. They are
specified in terms of the types they accept and produce.

| Name | Type | Description |
|---|---|---|
| `true` | `Bool` | Boolean true value. |
| `false` | `Bool` | Boolean false value. |
| `+` | `Int * Int -> Int` | Adds two integers. |
| `-` | `Int * Int -> Int` | Subtracts an integer from another. |
| `*` | `Int * Int -> Int` | Multiplies two integers. |
| `/` | `Int * Int -> Int` | Divides an integer by another. |
| `%` | `Int * Int -> Int` | Finds the remainder of dividing an integer by another. |
| `and` | `Bool * Bool -> Bool` | Logical AND operation on two booleans. |
| `or` | `Bool * Bool -> Bool` | Logical OR operation on two booleans. |
| `xor` | `Bool * Bool -> Bool` | Logical XOR operation on two booleans. |
| `not` | `Bool -> Bool` | Logical NOT operation on a boolean. |
| `==`¹ | `'T0 * 'T0 -> Bool` | Returns whether two values are equal. |
| `!=`¹ | `'T0 * 'T0 -> Bool` | Returns whether two values are not equal. |
| `<`² | `'T0 * 'T0 -> Bool` | Returns if the first value is less than the second. |
| `<=`² | `'T0 * 'T0 -> Bool` | Returns if the first value is less than or equal to the second. |
| `>`² | `'T0 * 'T0 -> Bool` | Returns if the first value is greater than the second. |
| `>=`² | `'T0 * 'T0 -> Bool` | Returns if the first value is greater than or equal to the second. |
| `head` | `'T0 List -> 'T0` | Returns the first value in a list. |
| `tail` | `'T0 List -> 'T0 List` | Returns the rest of a list. |
| `::` | `'T0 * 'T0 List -> 'T0 List` | Creates a list of a value and existing list. |
| `empty?` | `'T0 List -> Bool` | Returns whether a provided list is empty. |
| `?`³ | `Bool * 'T0 * 'T0 -> 'T0` | Ternary conditional operator. |
| `=` | `Symbol * 'T0 -> Void` | Assigns variable to value. |
| `display` | `'T0 -> Void` | Prints a value to standard output on its own line. |
| `read-line` | `() -> String` | Reads a line from standard input. |
| `read-word` | `() -> String` | Reads a space-delimited string from standard input. |
| `read-int` | `() -> Int` | Reads an integer from standard input. |
| `length` | `String | 'T0 List -> Int` | Returns the length of a string or list. |
| 

1. Value equality for integers, bools, strings, and symbols; reference equality for
lists.

2. Can only compare integers and strings. Integers are ordered numerically, strings
lexicographically by UTF-8 code point.

3. Only evaluates one path. Behaves similarly to the `if` special form with no
`elif`s.

---

## Compilation 

Sometimes there are values or operations that we might not want the compiler
to perform on its own. Stateful operations, such as printing a line to output, are
things we'd expect to happen when our program is executed, not when it's compiled.
Non-terminating code can be intended functionality, but we'd like our compiler to
terminate so it can produce an executable.

For all such values, we define a new concept called a runtime value. Runtime
values are Basil values that wrap impure operations, allowing us to operate over
them without actually performing something impure.

#### Runtime Values

We define a parametric type `Runtime`. `'T0 Runtime` for some `'T0` describes a typed
expression tree that yields `'T0`. The representation of this type is a node in a
typed AST.

#### Lowering

Many compile-time values can be converted to runtime values. This process is called
lowering, and is defined as follows.

 * Integers are lowered to integer constants.
 * Booleans are lowered to boolean constants.
 * Strings are lowered to string constants.
 * Symbols are lowered to symbol constants.
 * Lists are lowered to trees of `cons` operations. Each list element is lowered,
 and `cons`'d into the list as necessary to produce a list at runtime with the same
 elements and structure.
 * Empty lists are lowered to void constants.

It is a compiler error to lower any other kind of value.

#### Runtime Propagation

Runtime values originate from stateful code. This includes:

 * Mutation (the assignment `=` operator) of a variable.
 * `while` statements.
 * IO operations, such as `display`.
 * Any code not guaranteed to terminate, such as recursive functions.

The evaluation result of any of the above operations is a runtime value. Mutating a
variable additionally lowers the variable being assigned to (if it wasn't runtime
already).

Variables can be bound to runtime values, and when evaluated will evaluate to
runtime values. The resulting runtime value might not be identical (it might just
load the variable from a runtime location), but it will have the same runtime type
as the variable's bound value.

A procedure invocation on one or more runtime values must return a runtime value, 
and lower all non-runtime values it is given. The procedure itself must also be
lowered.

An if expression or ternary built-in with a runtime condition must evaluate and lower
all bodies that descend from it. An if expression or ternary with a non-runtime
condition and a runtime body only needs to return a runtime value if that runtime
body is reached.

#### Compilation

After the entire program is evaluated, we are left with a single value. If this value
is not a runtime value, then the entire program must have been evaluable at 
compile-time. In this case, we lower the value before code generation. Otherwise,
the value is a runtime value containing an expression tree of all program code
left to be resolved at runtime. In this case, we can enter code generation without
any additional lowering.

The code generation and optimization process is not specified here. But the output of
this phase must be executable machine code for some target architecture or virtual
machine. This machine code can either be written to an executable, or executed
directly by the Basil compiler environment. 