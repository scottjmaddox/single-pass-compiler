# single-pass-compiler

A single-pass compiler, for learning, that targets the Apple M1 chip.

## Building

To build and run the compiler on the examples and run the tests:

```sh
make
```

## Grammar

The currently supported grammar, in [extended Backus–Naur form (EBNF)](https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form):

```ebnf
program = { const_let } ;
const_let = const_let_decl | const_let_def ;
const_let_decl = "const" "let" ident ":" fn_decl ;
fn_decl = "fn" "(" fn_decl_params ")" "->" type_expr ;
fn_decl_params = { fn_decl_param "," } [ fn_decl_param ] ;
fn_decl_param = [ ident ":" ] type_expr ;
const_let_def = "const" "let" ident "=" fn_def ;
fn_def = "fn" "(" fn_def_params ")" "->" type_expr block ;
fn_def_params = { fn_def_param "," } [ fn_def_param ] ;
fn_def_param = ident ":" type_expr ;
type_expr = ident ;
block = "{" { expr [ ";" ] } "}" ;
expr = block | if_expr | let_expr  | return_expr | "(" expr ")" | int_literal | fn_call | var_expr | op_expr ;
if_expr = "if" expr block { "else" "if" expr block } [ "else" block ] ;
let_expr = "let" ident [ ":" type_expr ] "=" expr ;
return_expr = "return" expr ;
fn_call = ident "(" fn_args ")" ;
fn_args = { expr "," } [ expr ] ;
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
- [Procedure Call Standard for the Arm® 64-bit Architecture (AArch64)](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst)

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
- [x] inline type annotation / coersion
- [x] add `let _ =` as explicit drop
- [x] const int types with compile-time evaluation of const ops
- [x] change `const main` to `const let main` in preparation for `const` being a modifier
- [x] measure test coverage
- [x] `let` type annotations
- [x] up to 8 (single-word) function parameters (passed via registers)
- [x] more than 8 (single-word) function parameters (passed via the stack)
- [x] `const let name: type` forward function declarations
- [x] `extern fn` declarations; test calling into c
- [x] allow extra comma at the end of `fn_decl_params`/`fn_def_params`
- [x] allow semicolons between expressions in blocks; if at the end of a block, result is `unit`
- [x] early `return` with value
- [x] test self recursion and mutual recursion
- [ ] `loop`, `continue`, and `break` with optional label and value
- [ ] array type, array literals, array copy assignment, array indexing, array index assignment
- [ ] slice type, slice literals, slice copy assignment, array slicing, array slice assignment
- [ ] byte character literals, i.e. `b'a'`
- [ ] byte-slice string literals, i.e. `b"Hello"`
- [ ] `__builtin_print`?
- [ ] wrapping arithmatic operators: `*%`, `+%`, `-%`?
- [ ] saturating arithmatic operators: `*|`, `+|`, `-|`?
- [ ] require explicit overflow/underflow handling for normal operators?
- [ ] function types and indirect calls
- [ ] basic `match` expressions
- [ ] reserve builtin type idents, e.g. `unit`, `never`, `u[0-9]+`, `i[0-9]+`
- [ ] in (`name: <-type`), inout (`name: <->type`) and out (`name: ->type`) parameters, passed as pointers
- [ ] optional in (`name: ?<-type`), inout (`name: ?<->type`), and out (`name: ?->type`) parameters?
- [ ] non-nullable pointer types (`*type`) (no dereferencing, yet)
- [ ] support calling `ssize_t write(int fd, const void *buf, size_t count)`
  - e.g. `const let write = extern fn(fd: i32, buf: *u8, count: usize) -> isize`
- [ ] add line numbers to error messages
- [ ] support multi-line span error messages
- [ ] `let name: type` forward variable declarations
- [ ] nullable pointer types (`?*type`), with coersion
- [ ] `const let` support for `const int`
- [ ] `const let` function aliases
- [ ] local `const let` support
- [ ] `const <expr>`: enforces that the result is a compile-time constant
- [ ] `const if`: enforces that only one branch's code is emitted
  - but both branches are parsed and type checked
- [ ] `const for`: compile-time loop unrolling
- [ ] `const fn`: function that can be evaluated at compile time?
  - or should it just be any function that expects or outputs a `const type`?
    - or should we take the Zig approach of monomorphizing over const arguments?
  - should normal functions be defined with just `let name = fn(...`?
    - that way `const let = ...` is always equivalent to `let = const ...`?
- [ ] explicit tail calls?
- [ ] struct types
- [ ] non-copy, non-move, non-drop types
- [ ] copy, move, drop methods
- [ ] implicitly staticly typed context (`ctx`) parameters/arguments
- [ ] reference types?
- [ ] safe integer downcasting, e.g. `(v:u64 & 0xF):u4`?
- [ ] unsafe type casting?
- [ ] warn on unused variable?
- [ ] compile-time optimization: intern idents?
  - using a trie?
  - benchmark before and after
- [ ] run-time optimization: window push-pop annihilation
- [ ] output to stdout on `-o -`
