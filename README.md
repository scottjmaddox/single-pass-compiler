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
program = { stmnt } ;
stmnt = let_stmnt ;
let_stmnt = "let" ident "=" fn_def ";" ;
fn_def = "fn" "(" ")" "->" type_expr "{" expr "}" ;
expr = "(" expr ")" | prefix_op_expr | int_literal | fn_call | infix_op_expr ;
fn_call = ident "(" ")" ;
type_expr = ident ;

infix_op_expr = expr "+" expr
              | expr "-" expr
              | expr "*" expr
              | expr "/" expr
              | expr "%" expr ;
prefix_op_expr = "-" expr ;

ident = alpha { alphanumeric } ;
int_literal = [ "-" ] digit { "_" | digit } ;

alphanumeric = alpha | digit ;
alpha = "_" | "A" .. "Z" | "a" .. "z" ;
digit = "0" .. "9" ;

line_comment ::= "//" { ? any character except '\n' ? } "\n"
```

## References

- [Arm A-profile A64 Instruction Set Architecture](https://developer.arm.com/documentation/ddi0602/latest/?lang=en)
- [Simple but Powerful Pratt Parsing](https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
- [HelloSilicon](https://github.com/below/HelloSilicon)
- [A Gentle Introduction to Assembly Language Programming](https://github.com/pkivolowitz/asm_book)

## TODO

- [x] minimal single-pass-compiled and running program (main function returning a literal)
- [x] support parameter-free function calls
- [x] line comments
- [x] negative integer literals and negation
- [x] arithmetic
- [ ] local variables
- [ ] references
- [ ] type checking
- [ ] add u8 type and type conversion
- [ ] write to tmp file then move into place on success?
