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
expr = literal | fn_call ;
fn_call = ident "(" ")" ;
type_expr = ident ;

ident = ident_start { ident_rest } ;
literal = literal_start { literal_rest } ;

ident_start = "_" | "A" .. "Z" | "a" .. "z" ;
ident_rest = ident_start | digit;
literal_start = digit;
literal_rest = "_" | digit;
digit = "0" .. "9" ;
```

## References

- [Simple but Powerful Pratt Parsing](https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
- [HelloSilicon](https://github.com/below/HelloSilicon)
- [A Gentle Introduction to Assembly Language Programming](https://github.com/pkivolowitz/asm_book)

## TODO

- [x] minimal single-pass-compiled and running program (main function returning a literal)
- [x] support parameter-free function calls
- [ ] comments
- [ ] arithmetic
- [ ] local variables
- [ ] references
- [ ] type checking
- [ ] add u8 type and type conversion
- [ ] write to tmp file then move into place on success?
