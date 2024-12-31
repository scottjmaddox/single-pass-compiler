# single-pass-compiler

A single-pass compiler, for learning. The syntax is based on Rust. The target is the Apple M1 chip.

## Step 1

A minimal grammar, in extended Backus–Naur form (EBNF):

```
program = { fn_def } EOF ;
fn_def = "fn" ident "(" ")" "->" ident block ;
block = "{" expr "}" ;
expr = literal ;

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
./build.sh && ./spc example.spc > example.s
```

To build and run the resulting assembly:

```sh
clang -o example example.s && ./example
```

To confirm the expected exit code:

```sh
echo $? # should output 42
```

## References

- [Simple but Powerful Pratt Parsing](https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
- [A Gentle Introduction to Assembly Language Programming](https://github.com/pkivolowitz/asm_book)
