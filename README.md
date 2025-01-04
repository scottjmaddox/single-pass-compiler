# single-pass-compiler

A single-pass compiler, for learning, that targets the Apple M1 chip.

## Building

To build and run the compiler on the examples, and run the run tests:

```sh
make
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

prefix_op = "-" | "!" | "~" ;
infix_op = "*" | "/" | "%" | "+" | "-" | "<<" | ">>"
         | "<" | "<=" | ">" | ">=" | "==" | "!=" | "&&"  | "||" ;

line_comment ::= "//" { ? any character except '\n' ? } "\n"
```

The following lexical forms that ambiguously resemble number literals are disallowed:

```ebnf
reserved_number = bin_literal ( "2" .. "9" )
                | oct_literal ( "8" .. "9" )
                | int_literal ( "." | "a" .. "f" | "A" .. "F" )
                | "0b" { "_" }
                | "0o" { "_" }
                | "0x" { "_" }
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
- [x] bitwise operators: `~`, `&`, `|`, `^`, `<<`, `>>`
- [x] hex, octal, and binary integer literals
- [ ] symbol table
- [ ] local variables
- [ ] unconditional loops, continue, and break
- [ ] while loops
- [ ] arithmetic fuzz testing (comparing against C)
- [ ] optional function returns
- [ ] function parameters and optional return values
- [ ] write to tmp file then move into place on success to fix make behavior
- [ ] self-correcting file-driven build-failure-tests
- [ ] use spans to improve error messages
- [ ] never type
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
- [ ] output to stdout on `-o -`
- [ ] `-g` / `--debug` option
