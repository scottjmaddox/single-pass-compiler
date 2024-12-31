# single-pass-compiler

A single-pass compiler, for learning. The syntax is based on Rust. The target is the Apple M1 chip.

## Step 2

A minimal grammar, in extended Backus–Naur form (EBNF):

```
program = { fn_def } EOF ;
fn_def = "fn" ident "(" ")" "->" ident block ;
block = "{" expr "}" ;
expr = fn_call | literal ;
fn_call = ident "(" ")" ;

ident = ident_start { ident_rest } ;
literal = literal_start { literal_rest } ;

ident_start = "_" | "A" .. "Z" | "a" .. "z" ;
ident_rest = ident_start | digit;
literal_start = digit;
literal_rest = "_" | digit;
digit = "0" .. "9" ;
```

To build and run the compiler on `example.spc`:

```sh
./build.sh && ./spc examples/002.spc > example.spc.s
```

You can also build the corresponding C file to assembly, to compare:

```sh
clang -o example.c.s -S examples/002.c
```

To build, link, and run the resulting assembly:

```sh
clang -arch arm64 -o example example.spc.s && ./example
```

To confirm the expected exit code:

```sh
echo $? # should output 42
```

## References

- [Simple but Powerful Pratt Parsing](https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
- [HelloSilicon](https://github.com/below/HelloSilicon)
- [A Gentle Introduction to Assembly Language Programming](https://github.com/pkivolowitz/asm_book)

## TODO

- [x] minimal single-pass-compiled and running program (main function returning a literal)
- [x] support parameter-free function calls
- [x] improve syntax error messages
- [ ] comments
- [ ] arithmetic
- [ ] local variables
- [ ] references
- [ ] type checking
- [ ] add u8 type and type conversion
