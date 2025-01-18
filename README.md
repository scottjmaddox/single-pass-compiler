# single-pass-compiler

A single-pass compiler, for learning, that targets the Apple M1 chip.

## Building

To build and run the compiler on the examples and run the tests:

```sh
make
```

## Grammar

The currently supported grammar, in extended Backus–Naur form (EBNF):

```ebnf
program = { const_def } ;
const_def = "const" ident "=" fn_def ;
fn_def = "fn" "(" ")" "->" type_expr block ;
type_expr = ident ;
block = "{" { expr } "}" ;
expr = block | if_expr | let_expr | "(" expr ")" | int_literal | fn_call | var_expr | op_expr ;
if_expr = "if" expr block { "else" "if" expr block } [ "else" block ] ;
let_expr = "let" ident "=" expr ;
fn_call = ident "(" ")" ;
var_expr = ident ;
op_expr = prefix_op expr | expr infix_op expr ;

ident = alpha { alphanumeric } ;
int_literal = dec_literal | bin_literal | oct_literal | hex_literal ;
dec_literal = dec_digit { "_" | dec_digit } ;
bin_literal = "0b" { "_" | bin_digit } bin_digit { "_" | bin_digit } ;
oct_literal = "0o" { "_" | oct_digit } oct_digit { "_" | oct_digit } ;
hex_literal = "0x" { "_" | hex_digit } hex_digit { "_" | hex_digit } ;

alphanumeric = alpha | dec_digit ;
alpha = "_" | "A" .. "Z" | "a" .. "z" ;
bin_digit = "0" .. "1" ;
oct_digit = "0" .. "7" ;
dec_digit = "0" .. "9" ;
hex_digit = "0" .. "9" | "a" .. "f" | "A" .. "F" ;

prefix_op = "-" | "~" | "!" ;
infix_op = "*" | "/" | "%" | "+" | "-" | "<<" | ">>"
         | "<" | "<=" | ">" | ">=" | "==" | "!=" | "&&"  | "||" ;

line_comment ::= "//" { ? any character except '\n' ? } "\n"
```

The following lexical forms that ambiguously resemble valid number literals are reserved:

```ebnf
reserved_number =
                | "0" dec_literal
                | bin_literal ( "2" .. "9" )
                | oct_literal ( "8" .. "9" )
                | int_literal ( "." | "a" .. "f" | "A" .. "F" )
                | "0b" { "_" } | "0o" { "_" } | "0x" { "_" }
```

## References

- [Arm A-profile A64 Instruction Set Architecture](https://developer.arm.com/documentation/ddi0602/latest/?lang=en)
- [Simple but Powerful Pratt Parsing](https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
- [Wikipedia: Operators in C and C++: Operator precedence](https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence)
- [Writing ARM64 code for Apple platforms](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms)

## TODO

- [x] minimal single-pass-compiled and running program (main function returning an i32 literal)
- [x] support parameter-free function calls
- [x] line comments
- [x] negative integer literals and negation
- [x] arithmetic operators: `+`, `-`, `*`, `/`, `%`
- [x] comparison operators: `==`, `!=`, `>`, `<`, `>=`, `<=`
- [x] logical operators: `&&`, `||`, `!`
- [x] statements in functions; `__builtin_trap` intrinsic
- [x] if/else expessions and basic type checking
- [x] remove semicolons; they're not needed
- [x] bitwise operators: `~`, `&`, `|`, `^`, `<<`, `>>`
- [x] hex, octal, and binary integer literals
- [x] write output to tmp file then move into place on success
- [x] self-correcting file-driven build failure tests
- [x] use spans to improve error messages
- [x] symbol table and optional function returns
- [x] local `let` variables
- [x] never type
- [x] basic vscode language support
- [x] `u1`-`u64`, `i1`-`i64` types with [subtype](type_system.md) checking
- [x] default to i64, and always use 64-bit ops, for now
- [x] inline type annotation / coersion
- [x] add `let _ =` as explicit drop
- [x] const int types with compile-time evaluation of const ops
  - [x] fix nesting of live code within dead code; need `ctx->is_dead_code = was_dead_code | is_dead_code` instead of just `ctx->is_dead_code = is_dead_code`
  - [x] fix trap on `if (1:i64 <= 0) { __builtin_trap() }`
  - [x] fix `if` without else returning TY_NEVER; should be TY_UNIT always
  - [x] hoist `require_subtype,emit_type_coersion,.type =` pattern into a helpr
  - [x] fix `error: expected an integer type, not const int:` on `0 && __builtin_trap()`
  - [x] fix `error: expected an integer type, not const int:` on `1 || __builtin_trap()`
  - [x] test every pair of CONST_INT, INT binary ops?
  - [x] test and fix ops applied to TY_NEVER
- [x] test and fix `if v { __builtin_trap() } else { 1 }`
- [ ] test deadcode elim after `__builtin_trap()` in middle of a block
- [ ] add semicolons back?
  - needed to distinguish `let a = b - 0` from `let a = b;  -0`...
  - that's kinda dumb, though, since `let a = b;  -0` is useless outside of tests...
  - maybe we can just check for this and provide a helpful error message?
- [ ] change `const` into a modifier, e.g. `const let`, `const <expr>`, `const if`
  - `const` enforces that the result is compile-time known
  - `const let` and `const <expr>` enforce that the rvalue is a `TY_CONST_FN` or `TY_CONST_INT`, etc.
  - `const if` enforces that only one branch's code is emitted (but all are parsed / type checked)
  - `const for` can be used for compile-time loop unrolling
- [ ] test `fail_invalid_type_for_arithmetic`
- [ ] test `fail_expected_subtype`
- [ ] test `fail_const_int_div_by_zero`
- [ ] test `fail_const_int_rem_by_zero`
- [ ] test const eval of `&&`, `||`, and `if`
- [ ] test const eval of all other ops
- [ ] test parsing / emission of i64 minimum value
- [ ] test that "0" { "0" } dec_literal is reserved
- [ ] test that 0b0 has type `u1`
- [ ] test that 0o0 has type `u3`
- [ ] test that 0x0 has type `u4`
- [ ] test `2:u1` fails
- [ ] test `-1:u1` fails
- [ ] test `-1:i1` succeeds
- [ ] test `2:u32 + 2:u32 == 4:u32` succeeds
- [ ] test `if` dead code elim; both ways


- [ ] measure test coverage
- [ ] `let` type annotations
- [ ] array type, array literals
- [ ] slice type, slice literals
- [ ] byte-slice string literals, i.e. `b"Hello"`
- [ ] up to 8 (single-word) function (in) parameters (i.e. passed via registers)
- [ ] inout (`name: <->type`) and out (`name: ->type`) parameters
- [ ] support calling `ssize_t write(int fd, const void *buf, size_t count)`
  - e.g. `const write = extern fn(fd: i32, buf: *u8, count: usize) -> isize`
- [ ] nullable and non-nullable pointer types, with coersion
  - we'll need them eventually, but perhaps not yet?
  - and perhaps we'll only allow dereferencing in an `unsafe` block?
- [ ] reserve builtin type idents, e.g. `unit`, `never`, `u1`-`u64`, `i1`-`i64`
- [ ] rename types to start with capital letter?
- [ ] add line numbers to error messages
- [ ] support multi-line span error messages
- [ ] explicit tail calls?
- [ ] `return` with value
- [ ] `loop`, `continue`, and `break` with optional label and value
- [ ] `match` expressions
- [ ] function pointers and indirect calls
- [ ] bool type; `if`, `&&`, `||` expect bool
- [ ] more than 8 (single-word) function parameters (i.e. passed via the stack)
- [ ] struct types
- [ ] `let` pattern matching assignment; returns bool: true on match, false otherwise
- [ ] non-copy, non-move, non-drop types
- [ ] copy, move, drop methods
- [ ] automatic type promotion?
- [ ] reference types?
- [ ] local `const`
- [ ] `const` def aliases
- [ ] `const` def returns symbol to allow chained defs?
- [ ] `const` def with compile-time evaluated arithmetic
- [ ] `const if` for compile-time conditional definitions?
- [ ] `const loop` for compile-time codegen?
- [ ] arithmetic fuzz testing (comparing against C with `-ftrapv`)
- [ ] explicit type conversion
- [ ] byte character literals
- [ ] byte string literals
- [ ] printing via printf?
- [ ] warn on unused variable?
- [ ] compile-time optimization: defer line and col calculation until error?
    - or store all line idx's in a separate persistent list?
- [ ] compile-time optimization: intern idents?
    - using a trie?
    - benchmark before and after
- [ ] run-time optimization: push-pop annihilation
- [ ] output to stdout on `-o -`
