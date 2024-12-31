# single-pass-compiler

A single-pass compiler, for learning. The syntax is based on Rust. The target is the Apple M1 chip.

## Step 1

A minimal grammar, in extended Backus–Naur form (EBNF):

```
program = fn_def ;
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

To build and run:

```
./build.sh && ./main example.spc
```
