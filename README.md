# single-pass-compiler

A single-pass compiler, for learning, that targets the Apple M1 chip.

## Building

To build the compiler, run:

```sh
make spc
```

To build the examples, run:

```sh
make examples
```

## Grammar

The currently supported grammar, in extended Backus–Naur form (EBNF):

```ebnf
program = { static_stmnt } ;
static_stmnt = static_let_stmnt ;
static_let_stmnt = "let" ident "=" fn_def ;
fn_def = "fn" "(" ")" "->" type_expr block ;
type_expr = ident ;
block = "{" { expr } "}" ;
expr = block | if_expr | "(" expr ")" | [ "-" ] int_literal | fn_call | op_expr ;
if_expr = "if" expr block_expr { "else" "if" expr block_expr } [ "else" block_expr ] ;
fn_call = ident "(" ")" ;
op_expr = prefix_op expr | expr infix_op expr ;

ident = alpha { alphanumeric } ;
int_literal = digit { "_" | digit } ;

alphanumeric = alpha | digit ;
alpha = "_" | "A" .. "Z" | "a" .. "z" ;
digit = "0" .. "9" ;

prefix_op = "!" | "-" ;
infix_op = "||" | "&&" | "==" | "!=" | ">" | "<" | ">=" | "<=" | "+" | "-" | "*" | "/" | "%" ;

line_comment ::= "//" { ? any character except '\n' ? } "\n"
```

## References

- [Arm A-profile A64 Instruction Set Architecture](https://developer.arm.com/documentation/ddi0602/latest/?lang=en)
- [Simple but Powerful Pratt Parsing](https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
- [Wikipedia: Operators in C and C++: Operator precedence](https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence)
- [HelloSilicon](https://github.com/below/HelloSilicon)
- [A Gentle Introduction to Assembly Language Programming](https://github.com/pkivolowitz/asm_book)

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
- [ ] bitwise operators: `~`, `&`, `|`, `^`, `<<`, `>>`
- [ ] self-correcting tests
- [ ] symbol table
- [ ] never type
- [ ] use spans to improve error messages
- [ ] write to tmp file then move into place on success to fix make behavior
- [ ] local variables
- [ ] named return slots a la go?
- [ ] u1 / bool type
- [ ] u8 type, integer literal suffixes, type promotion
- [ ] explicit type conversion
- [ ] byte character literals
- [ ] reference types?
- [ ] byte string literals
- [ ] printing via printf
- [ ] optimization: push-pop annihilation
- [ ] boolean arithmetic
- [ ] if/else branches
- [ ] unconditional loops, continue, and break
- [ ] output to stdout on `-o -`
- [ ] `-g` / `--debug` option
