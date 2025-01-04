#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>         // For open()
#include <sys/mman.h>      // For mmap(), munmap()
#include <sys/stat.h>      // For fstat()
#include <sys/syslimits.h> // For PATH_MAX
#include <unistd.h>        // For getopt(), close()
#include <errno.h>         // For errno
#include <string.h>        // For strerror()
#include <assert.h>        // For assert()
#include <limits.h>        // For ULLONG_MAX, LLONG_MAX

#define MAX_TOKEN_LOOKAHEAD 2
#define MAX_INT_LITERAL_LEN 64

enum TOKEN {
    TOKEN_EOF,
    TOKEN_EXCLAMATION,
    TOKEN_EXCLAMATION_EQUAL,
    TOKEN_PERCENT,
    TOKEN_AMPERSAND,
    TOKEN_AMPERSAND_AMPERSAND,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_ASTERISK,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_RIGHT_ARROW,
    TOKEN_SLASH,
    TOKEN_LESS_THAN,
    TOKEN_LESS_EQUAL,
    TOKEN_LESS_LESS,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER_THAN,
    TOKEN_GREATER_EQUAL,
    TOKEN_GREATER_GREATER,
    TOKEN_CARET,
    TOKEN_LEFT_BRACE,
    TOKEN_BAR,
    TOKEN_BAR_BAR,
    TOKEN_RIGHT_BRACE,
    TOKEN_TILDE,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_FN,
    TOKEN_KEYWORD_LET,
    TOKEN_KEYWORD_ELSE,
    TOKEN_IDENT,
    TOKEN_INT_LITERAL,
    TOKEN_RESERVED_NUM,
    TOKEN_UNKNOWN,
};

static char *TOKEN_NAMES[] = {
    "EOF",
    "EXCLAMATION",
    "EXCLAMATION_EQUAL",
    "PERCENT",
    "AMPERSAND",
    "AMPERSAND_AMPERSAND",
    "LEFT_PAREN",
    "RIGHT_PAREN",
    "ASTERISK",
    "PLUS",
    "MINUS",
    "RIGHT_ARROW",
    "SLASH",
    "LESS_THAN",
    "LESS_EQUAL",
    "LESS_LESS",
    "EQUAL",
    "EQUAL_EQUAL",
    "GREATER_THAN",
    "GREATER_EQUAL",
    "GREATER_GREATER",
    "CARET",
    "LEFT_BRACE",
    "BAR",
    "BAR_BAR",
    "RIGHT_BRACE",
    "TILDE",
    "KEYWORD_IF",
    "KEYWORD_FN",
    "KEYWORD_LET",
    "KEYWORD_ELSE",
    "IDENT",
    "INT_LITERAL",
    "RESERVED_NUM"
    "UNKNOWN",
};

enum UNARY_OP {
    UNARY_OP_NEG,
    UNARY_OP_BITWISE_NOT,
    UNARY_OP_LOGICAL_NOT,
};

enum BINARY_OP {
    BINARY_OP_MUL,
    BINARY_OP_DIV,
    BINARY_OP_REM,
    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_SHL,
    BINARY_OP_SHR,
    BINARY_OP_LT,
    BINARY_OP_LE,
    BINARY_OP_GT,
    BINARY_OP_GE,
    BINARY_OP_EQ,
    BINARY_OP_NE,
    BINARY_OP_BITWISE_AND,
    BINARY_OP_BITWISE_XOR,
    BINARY_OP_BITWISE_OR,
    BINARY_OP_LOGICAL_AND,
    BINARY_OP_LOGICAL_OR,
};

static int UNARY_OP_RIGHT_BINDING_POWERS[] = {
    [UNARY_OP_LOGICAL_NOT]  = 110,
    [UNARY_OP_BITWISE_NOT]  = 110,
    [UNARY_OP_NEG]          = 110,
};

static int BINARY_OP_LEFT_BINDING_POWERS[] = {
    [BINARY_OP_MUL]         = 100,
    [BINARY_OP_DIV]         = 100,
    [BINARY_OP_REM]         = 100,
    [BINARY_OP_ADD]         =  90,
    [BINARY_OP_SUB]         =  90,
    [BINARY_OP_SHL]         =  80,
    [BINARY_OP_SHR]         =  80,
    [BINARY_OP_LT]          =  70,
    [BINARY_OP_LE]          =  70,
    [BINARY_OP_GT]          =  70,
    [BINARY_OP_GE]          =  70,
    [BINARY_OP_EQ]          =  60,
    [BINARY_OP_NE]          =  60,
    [BINARY_OP_BITWISE_AND] =  50,
    [BINARY_OP_BITWISE_XOR] =  40,
    [BINARY_OP_BITWISE_OR]  =  30,
    [BINARY_OP_LOGICAL_AND] =  20,
    [BINARY_OP_LOGICAL_OR]  =  10,
};

static int BINARY_OP_RIGHT_BINDING_POWERS[] = {
    // +1 for left-to-right associativity, -1 for right-to-left associativity
    [BINARY_OP_MUL]         = 100 + 1,
    [BINARY_OP_DIV]         = 100 + 1,
    [BINARY_OP_REM]         = 100 + 1,
    [BINARY_OP_ADD]         =  90 + 1,
    [BINARY_OP_SUB]         =  90 + 1,
    [BINARY_OP_SHL]         =  80 + 1,
    [BINARY_OP_SHR]         =  80 + 1,
    [BINARY_OP_LT]          =  70 + 1,
    [BINARY_OP_LE]          =  70 + 1,
    [BINARY_OP_GT]          =  70 + 1,
    [BINARY_OP_GE]          =  70 + 1,
    [BINARY_OP_EQ]          =  60 + 1,
    [BINARY_OP_NE]          =  60 + 1,
    [BINARY_OP_BITWISE_AND] =  50 + 1,
    [BINARY_OP_BITWISE_XOR] =  40 + 1,
    [BINARY_OP_BITWISE_OR]  =  30 + 1,
    [BINARY_OP_LOGICAL_AND] =  20 + 1,
    [BINARY_OP_LOGICAL_OR]  =  10 + 1,
};

enum TYPE {
    TYPE_UNIT,
    TYPE_I32,
};

static bool TYPE_SIGNED[] = {
    [TYPE_I32] = true,
};

static char *TYPE_NAMES[] = {
    "unit",
    "i32",
};

struct str {
    size_t len;
    char *ptr;
};

struct location {
    size_t idx;
    size_t line;
    size_t col;
};

struct token {
    enum TOKEN kind;
    struct location loc;
    size_t len;
};

struct context {
    FILE *output_file;
    char *input_file_path;
    char *src;
    size_t src_len;
    struct location src_loc;
    // Token ring buffer
    size_t token_offset;
    size_t token_count;
    struct token tokens[MAX_TOKEN_LOOKAHEAD];
    // Compiler state
    size_t local_label_count;
};

//////////////////////
// Lexical analysis //
//////////////////////

static struct token
lex(struct context *ctx) {
    char *src = ctx->src;
    size_t len = ctx->src_len;
    struct token tok = { .kind = TOKEN_UNKNOWN, .len = 1, .loc = ctx->src_loc };
    size_t i;
    while (tok.loc.idx < len) {
        switch (src[tok.loc.idx]) {
        case ' ':
        case '\t':
            tok.loc.idx += 1;
            tok.loc.col += 1;
            continue;
        case '\n':
            tok.loc.idx += 1;
            tok.loc.line += 1;
            tok.loc.col = 1;
            continue;
        case '!':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '=': tok.kind = TOKEN_EXCLAMATION_EQUAL; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_EXCLAMATION;
            return tok;
        case '%': tok.kind = TOKEN_PERCENT; return tok;
        case '&':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '&': tok.kind = TOKEN_AMPERSAND_AMPERSAND; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_AMPERSAND;
            return tok;
        case '(': tok.kind = TOKEN_LEFT_PAREN; return tok;
        case ')': tok.kind = TOKEN_RIGHT_PAREN; return tok;
        case '+': tok.kind = TOKEN_PLUS; return tok;
        case '*': tok.kind = TOKEN_ASTERISK; return tok;
        case '-':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '>': tok.kind = TOKEN_RIGHT_ARROW; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_MINUS;
            return tok;
        case '/':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '/':
                    // line comment
                    i = tok.loc.idx + 2;
                    while (i < len && src[i] != '\n') { i += 1; }
                    tok.loc.col += i - tok.loc.idx;
                    tok.loc.idx = i;
                    continue;
                }
            }
            tok.kind = TOKEN_SLASH;
            return tok;
        case '<':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '=': tok.kind = TOKEN_LESS_EQUAL; tok.len = 2; return tok;
                case '<': tok.kind = TOKEN_LESS_LESS; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_LESS_THAN;
            return tok;
        case '=':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '=': tok.kind = TOKEN_EQUAL_EQUAL; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_EQUAL;
            return tok;
        case '>':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '=': tok.kind = TOKEN_GREATER_EQUAL; tok.len = 2; return tok;
                case '>': tok.kind = TOKEN_GREATER_GREATER; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_GREATER_THAN;
            return tok;
        case '^': tok.kind = TOKEN_CARET; return tok;
        case '{': tok.kind = TOKEN_LEFT_BRACE; return tok;
        case '|':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '|': tok.kind = TOKEN_BAR_BAR; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_BAR;
            return tok;
        case '}': tok.kind = TOKEN_RIGHT_BRACE; return tok;
        case '~': tok.kind = TOKEN_TILDE; return tok;
        case '_':
        case 'A' ... 'Z':
        case 'a' ... 'z':
            i = tok.loc.idx + 1;
            while (i < len) {
                switch (src[i]) {
                case '_':
                case 'A' ... 'Z':
                case 'a' ... 'z':
                case '0' ... '9':
                    i += 1;
                    continue;
                }
                break;
            }
            tok.len = i - tok.loc.idx;
            switch (tok.len) {
            case 2:
                if (strncmp(src + tok.loc.idx, "if", 2) == 0) { tok.kind = TOKEN_KEYWORD_IF; return tok; }
                if (strncmp(src + tok.loc.idx, "fn", 2) == 0) { tok.kind = TOKEN_KEYWORD_FN; return tok; }
                break;
            case 3:
                if (strncmp(src + tok.loc.idx, "let", 3) == 0) { tok.kind = TOKEN_KEYWORD_LET; return tok; }
                break;
            case 4:
                if (strncmp(src + tok.loc.idx, "else", 4) == 0) { tok.kind = TOKEN_KEYWORD_ELSE; return tok; }
                break;
            }
            tok.kind = TOKEN_IDENT;
            return tok;
        case '0' ... '9':
            i = tok.loc.idx;
            bool has_digit = false;
            bool is_reserved = false;
            if (src[i] == '0' && i + 1 < len
            && (src[i + 1] == 'b' || src[i + 1] == 'o' || src[i + 1] == 'x')) {
                i += 1;
                switch (src[i]) {
                case 'b':
                    // binary literal
                    i += 1;
                    while (i < len) {
                        switch (src[i]) {
                        case '_': i += 1; continue;
                        case '0' ... '1': i += 1; has_digit = true; continue;
                        case '2' ... '9': i += 1; is_reserved = true; break;
                        }
                        break;
                    }
                    break;
                case 'o':
                    // octal literal
                    i += 1;
                    while (i < len) {
                        switch (src[i]) {
                        case '_': i += 1; continue;
                        case '0' ... '7': i += 1; has_digit = true; continue;
                        case '8' ... '9': i += 1; is_reserved = true; break;
                        }
                        break;
                    }
                    break;
                case 'x':
                    // hexadecimal literal
                    i += 1;
                    while (i < len) {
                        switch (src[i]) {
                        case '_': i += 1; continue;
                        case '0' ... '9':
                        case 'A' ... 'F':
                        case 'a' ... 'f': i += 1; has_digit = true; continue;
                        }
                        break;
                    }
                    break;
                default: assert(!"unreachable");
                }
            } else {
                // decimal literal
                has_digit = true;
                i += 1;
                while (i < len) {
                    switch (src[i]) {
                    case '_':
                    case '0' ... '9': i += 1; continue;
                    }
                    break;
                }
            }
            if (i + 1 < len) {
                switch (src[i + 1]) {
                case '.':
                case 'A' ... 'Z':
                case 'a' ... 'z': i += 1; is_reserved = true; break;
                }
            }
            is_reserved = is_reserved || !has_digit;
            tok.kind = is_reserved ? TOKEN_RESERVED_NUM : TOKEN_INT_LITERAL;
            tok.len = i - tok.loc.idx;
            return tok;
        }
        tok.kind = TOKEN_UNKNOWN;
        return tok;
    }
    tok.kind = TOKEN_EOF;
    tok.len = 0;
    return tok;
}

static void
push_token(struct context *ctx, struct token tok) {
    assert(ctx->token_count < MAX_TOKEN_LOOKAHEAD);
    ctx->tokens[(ctx->token_offset + ctx->token_count) % MAX_TOKEN_LOOKAHEAD] = tok;
    ctx->token_count += 1;
    ctx->src_loc = (struct location){
        .idx = tok.loc.idx + tok.len,
        .line = tok.loc.line,
        .col = tok.loc.col + tok.len,
    };
}

static void
fill_tokens(struct context *ctx) {
    while (ctx->token_count < MAX_TOKEN_LOOKAHEAD) {
        struct token tok = lex(ctx);
        push_token(ctx, tok);
    }
}

static void
take_token(struct context *ctx, struct token* token) {
    assert(token != NULL);
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    *token = ctx->tokens[ctx->token_offset];
    ctx->token_offset = (ctx->token_offset + 1) % MAX_TOKEN_LOOKAHEAD;
    ctx->token_count -= 1;
}

// static enum TOKEN
// peek_token_kind_at(struct context *ctx, size_t i) {
//     assert(i < MAX_TOKEN_LOOKAHEAD);
//     if (i >= ctx->token_count) { fill_tokens(ctx); }
//     return ctx->tokens[(ctx->token_offset + i) % MAX_TOKEN_LOOKAHEAD].kind;
// }

static enum TOKEN
peek_token_kind(struct context *ctx) {
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    return ctx->tokens[ctx->token_offset].kind;
}

///////////////////////////////
// Diagnostics and Utilities //
///////////////////////////////

static bool
token_starts_with(struct context *ctx, struct token tok, char *str) {
    return tok.len >= strlen(str) && strncmp(ctx->src + tok.loc.idx, str, strlen(str)) == 0;
}

static bool
token_equals(struct context *ctx, struct token tok, char *str) {
    return tok.len == strlen(str) && strncmp(ctx->src + tok.loc.idx, str, strlen(str)) == 0;
}

static struct str
get_token_line(struct context *ctx, struct token tok) {
    size_t idx = 0;
    size_t line = 1;
    while (line < tok.loc.line) {
        switch (ctx->src[idx]) {
        case '\n':
            idx += 1;
            line += 1;
            break;
        default:
            idx += 1;
            break;
        }
    }
    size_t len = 0;
    while (idx + len < ctx->src_len && ctx->src[idx + len] != '\n') {
        len += 1;
    }
    return (struct str){ .len = len, .ptr = ctx->src + idx };;
}

static void
eprint_token_line(struct context *ctx, struct token tok) {
    struct str token_line = get_token_line(ctx, tok);
    fprintf(stderr,
        " --> %s:%zu:%zu\n"
        "  |\n"
        "  | %.*s\n",
        ctx->input_file_path, tok.loc.line, tok.loc.col,
        (int)token_line.len, token_line.ptr
    );
    fprintf(stderr, "  | ");
    for (size_t i = 0; i < tok.loc.col - 1; i++) {
        if (token_line.ptr[i] == '\t') {
            fputc('\t', stderr);
        } else {
            fputc(' ', stderr);
        }
    }
    for (size_t i = 0; i < tok.len; i++) { fputc('^', stderr); }
    fprintf(stderr, "\n");
}

static void
fail_expected_token_kind(struct context *ctx, enum TOKEN expected_token_kind, struct token tok) {
    fprintf(stderr, "error: unexpected token: %s\n", TOKEN_NAMES[tok.kind]);
    eprint_token_line(ctx, tok);
    fprintf(stderr, "Expected: %s\n", TOKEN_NAMES[expected_token_kind]);
    exit(EXIT_FAILURE);
}

static void
fail_expected(struct context *ctx, char *str) {
    struct token tok;
    take_token(ctx, &tok);
    fprintf(stderr, "error: unexpected token: %s\n", TOKEN_NAMES[tok.kind]);
    eprint_token_line(ctx, tok);
    fprintf(stderr, "Expected %s.\n", str);
    exit(EXIT_FAILURE);
}

static void
fail_reserved_ident(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: reserved identifier\n");
    eprint_token_line(ctx, tok);
    fprintf(stderr, "Identifiers starting with '__builtin' are reserved.\n");
    exit(EXIT_FAILURE);
}

static void
fail_unknown_builtin(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: unknown builtin function\n");
    eprint_token_line(ctx, tok);
    exit(EXIT_FAILURE);
}

static void
fail_type_mismatch(struct context *ctx, struct token tok, enum TYPE expected_type, enum TYPE actual_type) {
    fprintf(stderr, "error: type mismatch:\n");
    eprint_token_line(ctx, tok);
    fprintf(stderr, "Expected type: %s\n", TYPE_NAMES[expected_type]);
    fprintf(stderr, "Actual type: %s\n", TYPE_NAMES[actual_type]);
    exit(EXIT_FAILURE);
}

static void
fail_int_literal_out_of_range(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: integer literal out of range\n");
    eprint_token_line(ctx, tok);
    fprintf(stderr, "Range: %lld to %lld\n", LLONG_MIN, LLONG_MAX);
    exit(EXIT_FAILURE);
}

static void
take_token_expect_kind(struct context *ctx, struct token *tok_out, enum TOKEN token_kind) {
    struct token tok;
    take_token(ctx, &tok);
    if (tok.kind != token_kind) {
        fail_expected_token_kind(ctx, token_kind, tok);
    }
    if (tok_out != NULL) {
        *tok_out = tok;
    }
}

//////////////////////
// Compiler Backend //
//////////////////////

static void
emit_program_prologue(struct context *ctx) {
    fprintf(ctx->output_file,
        "\t.section\t__TEXT,__text,regular,pure_instructions\n"
    );
}

static void
emit_program_epilogue(struct context *ctx) {
    fprintf(ctx->output_file, "\n.subsections_via_symbols\n");
}

static void
emit_push(struct context *ctx, char *reg) {
    // NOTE: using 16-byte slot, for now, to comply with stack pointer alignment restrictions
    fprintf(ctx->output_file, "\tstr\t%s, [sp, #-16]!\n", reg);
}

static void
emit_pop(struct context *ctx, char *reg) {
    // NOTE: using 16-byte slot, for now, to comply with stack pointer alignment restrictions
    fprintf(ctx->output_file, "\tldr\t%s, [sp], #16\n", reg);
}

static void
emit_clear_reg(struct context *ctx, char *reg) {
    fprintf(ctx->output_file, "\tmov\t%s, #0\n", reg);
}

static void
emit_fn_prologue(struct context *ctx, char *name, size_t name_len) {
    fprintf(ctx->output_file,
        "\n"
        "\t.globl\t_%.*s\n"
        "\t.p2align\t2\n"
        "_%.*s:\n"
        "\t.cfi_startproc\n"
        "\tstp\tx29, x30, [sp, #-16]!\n"
        "\n",
        (int)name_len, name,
        (int)name_len, name
    );
}

static void
emit_fn_epilogue(struct context *ctx, enum TYPE return_type) {
    switch (return_type) {
    case TYPE_UNIT:
        emit_clear_reg(ctx, "w0");
        break;
    case TYPE_I32:
        emit_pop(ctx, "w0");
        break;
    }
    fprintf(ctx->output_file,
        "\n"
        "\tldp\tx29, x30, [sp], #16\n"
        "\tret\n"
        "\t.cfi_endproc\n"
    );
}

static void
emit_drop_type(struct context *ctx, enum TYPE ty) {
    if (ty != TYPE_UNIT) {
        emit_pop(ctx, "w0");
    }
}

static void
emit_fn_call(struct context *ctx, char *name, size_t name_len, enum TYPE return_type) {
    fprintf(ctx->output_file, "\tbl\t_%.*s\n", (int)name_len, name);
    if (return_type != TYPE_UNIT) { emit_push(ctx, "w0"); }
}

static void
emit_local_label(struct context *ctx, size_t label) {
    fprintf(ctx->output_file, "%zu:\n", label);
}

static void
emit_local_forward_branch(struct context *ctx, size_t label) {
    fprintf(ctx->output_file, "\tb\t%zuf\n", label);
}

static void
emit_local_forward_branch_if_zero(struct context *ctx, size_t label) {
    emit_pop(ctx, "w0");
    fprintf(ctx->output_file, "\tcbz\tw0, %zuf\n", label);
}

static void
emit_local_forward_branch_if_nonzero(struct context *ctx, size_t label) {
    emit_pop(ctx, "w0");
    fprintf(ctx->output_file, "\tcbnz\tw0, %zuf\n", label);
}

static void
emit_unary_op(struct context *ctx, enum UNARY_OP op) {
    emit_pop(ctx, "w0");
    switch (op) {
    case UNARY_OP_NEG: fprintf(ctx->output_file, "\tneg\tw0, w0\n"); break;
    case UNARY_OP_BITWISE_NOT: fprintf(ctx->output_file, "\tmvn\tw0, w0\n"); break;
    case UNARY_OP_LOGICAL_NOT: fprintf(ctx->output_file, "\tcmp\tw0, #0\n\tcset\tw0, eq\n" ); break;
    default: assert(!"unreachable");
    }
    emit_push(ctx, "w0");
}

static void
emit_binary_op(struct context *ctx, enum BINARY_OP op, enum TYPE left_type, enum TYPE right_type) {
    assert(left_type == TYPE_I32);
    assert(right_type == TYPE_I32);
    emit_pop(ctx, "w1");
    emit_pop(ctx, "w0");
    switch (op) {
    case BINARY_OP_MUL: fprintf(ctx->output_file, "\tmul\tw0, w0, w1\n"); break;
    case BINARY_OP_DIV: fprintf(ctx->output_file, "\tsdiv\tw0, w0, w1\n"); break;
    case BINARY_OP_REM:
        fprintf(ctx->output_file,
            "\tsdiv\tw2, w0, w1\n"
            "\tmul\tw2, w2, w1\n"
            "\tsub\tw0, w0, w2\n"
        );
        break;
    case BINARY_OP_ADD: fprintf(ctx->output_file, "\tadd\tw0, w0, w1\n"); break;
    case BINARY_OP_SUB: fprintf(ctx->output_file, "\tsub\tw0, w0, w1\n"); break;
    case BINARY_OP_SHL: fprintf(ctx->output_file, "\tlsl\tw0, w0, w1\n"); break;
    case BINARY_OP_SHR:
        if (TYPE_SIGNED[left_type]) {
            fprintf(ctx->output_file, "\tasr\tw0, w0, w1\n");
        } else {
            fprintf(ctx->output_file, "\tlsr\tw0, w0, w1\n");
        }
        break;
    case BINARY_OP_LT: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, lt\n"); break;
    case BINARY_OP_LE: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, le\n"); break;
    case BINARY_OP_GT: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, gt\n"); break;
    case BINARY_OP_GE: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, ge\n"); break;
    case BINARY_OP_EQ: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, eq\n"); break;
    case BINARY_OP_NE: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, ne\n"); break;
    case BINARY_OP_BITWISE_AND: fprintf(ctx->output_file, "\tand\tw0, w0, w1\n");; break;
    case BINARY_OP_BITWISE_XOR: fprintf(ctx->output_file, "\teor\tw0, w0, w1\n");; break;
    case BINARY_OP_BITWISE_OR: fprintf(ctx->output_file, "\torr\tw0, w0, w1\n");; break;
    default: assert(!"unreachable");
    }
    emit_push(ctx, "w0");
}

static void
emit_int_literal(struct context *ctx, long long int value) {
    // push the literal onto the stack
    fprintf(ctx->output_file, "\tldr\tw0, =%lld\n", value);
    emit_push(ctx, "w0");
}

static void
emit_builtin_trap(struct context *ctx) {
    fprintf(ctx->output_file, "\tbrk\t#0\n");
}

///////////////////////
// Compiler Frontend //
///////////////////////

static void compile_program(struct context *ctx);
static void compile_static_stmnt(struct context *ctx);
static void compile_static_let_stmnt(struct context *ctx);
static void compile_fn_def(struct context *ctx, struct token *name_tok);
static enum TYPE compile_block(struct context *ctx);
static enum TYPE compile_expr(struct context *ctx, int min_binding_power);
static enum TYPE compile_if_expr(struct context *ctx);
static enum TYPE compile_prefix_op_expr(struct context *ctx);
static enum TYPE compile_fn_call(struct context *ctx);
static enum TYPE compile_int_literal(struct context *ctx, bool negate);

static void
compile_program(struct context *ctx) {
    // EBNF: program = { static_stmnt } ;
    emit_program_prologue(ctx);
    while (peek_token_kind(ctx) != TOKEN_EOF) {
        compile_static_stmnt(ctx);
    }
    emit_program_epilogue(ctx);
}

static void
compile_static_stmnt(struct context *ctx) {
    // EBNF: static_stmnt = static_let_stmnt ;
    switch (peek_token_kind(ctx)) {
    case TOKEN_KEYWORD_LET: compile_static_let_stmnt(ctx); break;
    default: fail_expected(ctx, "a let statement");
    }
}

static void
compile_static_let_stmnt(struct context *ctx) {
    // EBNF: static_let_stmnt = "let" ident "=" fn_def ;
    take_token_expect_kind(ctx, NULL, TOKEN_KEYWORD_LET);
    struct token name_tok;
    take_token_expect_kind(ctx, &name_tok, TOKEN_IDENT);
    if (token_starts_with(ctx, name_tok, "__builtin")) {
        fail_reserved_ident(ctx, name_tok);
    }
    take_token_expect_kind(ctx, NULL, TOKEN_EQUAL);
    compile_fn_def(ctx, &name_tok);
}

static void
compile_fn_def(struct context *ctx, struct token *name_tok) {
    // EBNF:
    // fn_def = "fn" "(" ")" "->" type_expr block ;
    // type_expr = ident ;
    take_token_expect_kind(ctx, NULL, TOKEN_KEYWORD_FN);
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
    take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_PAREN);
    enum TYPE expected_type = TYPE_UNIT;
    struct token type_tok;
    if (peek_token_kind(ctx) == TOKEN_RIGHT_ARROW) {
        take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_ARROW);
        take_token_expect_kind(ctx, &type_tok, TOKEN_IDENT);
        if (token_equals(ctx, type_tok, "i32")) {
            expected_type = TYPE_I32;
        } else {
            // TODO: support unit return type
            fail_expected(ctx, "i32");
        }
    }
    ctx->local_label_count = 0;
    emit_fn_prologue(ctx, ctx->src + name_tok->loc.idx, name_tok->len);
    enum TYPE return_type = compile_block(ctx);
    if (return_type != expected_type) {
        fail_type_mismatch(ctx, type_tok, expected_type, return_type);
    }
    emit_fn_epilogue(ctx, return_type);
}

static enum TYPE
compile_block(struct context *ctx) {
    // EBNF: block = "{" { expr } "}" ;
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_BRACE);
    enum TYPE previous_return_type = TYPE_UNIT;
    enum TYPE final_return_type  = TYPE_UNIT;
    while (peek_token_kind(ctx) != TOKEN_RIGHT_BRACE) {
        emit_drop_type(ctx, previous_return_type);
        enum TYPE return_type = compile_expr(ctx, 0);
        previous_return_type = final_return_type;
        final_return_type = return_type;
    }
    take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_BRACE);
    return final_return_type;
}

static enum TYPE
compile_expr(struct context *ctx, int min_binding_power) {
    // EBNF: expr = block | if_expr | "(" expr ")" | [ "-" ] int_literal | fn_call | op_expr ;
    enum TYPE left_type, right_type;
    switch (peek_token_kind(ctx)) {
    case TOKEN_RIGHT_BRACE: return TYPE_UNIT;
    case TOKEN_LEFT_BRACE: left_type = compile_block(ctx); break;
    case TOKEN_KEYWORD_IF: left_type = compile_if_expr(ctx); break;
    case TOKEN_LEFT_PAREN:
        take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
        left_type = compile_expr(ctx, 0);
        take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_PAREN);
        break;
    case TOKEN_INT_LITERAL: left_type = compile_int_literal(ctx, false); break;
    case TOKEN_IDENT: left_type = compile_fn_call(ctx); break;
    case TOKEN_MINUS:
    case TOKEN_TILDE:
    case TOKEN_EXCLAMATION:
        left_type = compile_prefix_op_expr(ctx);
        break;
    default: fail_expected(ctx, "an expression");
    }
    // EBNF: infix_op = "*" | "/" | "%" | "+" | "-" | "<<" | ">>"
    //                | "<" | "<=" | ">" | ">=" | "==" | "!=" | "&&"  | "||" ;
    for (;;) {
        enum BINARY_OP op;
        switch (peek_token_kind(ctx)) {
        case TOKEN_ASTERISK:            op = BINARY_OP_MUL;         break;
        case TOKEN_SLASH:               op = BINARY_OP_DIV;         break;
        case TOKEN_PERCENT:             op = BINARY_OP_REM;         break;
        case TOKEN_PLUS:                op = BINARY_OP_ADD;         break;
        case TOKEN_MINUS:               op = BINARY_OP_SUB;         break;
        case TOKEN_LESS_LESS:           op = BINARY_OP_SHL;         break;
        case TOKEN_GREATER_GREATER:     op = BINARY_OP_SHR;         break;
        case TOKEN_LESS_THAN:           op = BINARY_OP_LT;          break;
        case TOKEN_LESS_EQUAL:          op = BINARY_OP_LE;          break;
        case TOKEN_GREATER_THAN:        op = BINARY_OP_GT;          break;
        case TOKEN_GREATER_EQUAL:       op = BINARY_OP_GE;          break;
        case TOKEN_EQUAL_EQUAL:         op = BINARY_OP_EQ;          break;
        case TOKEN_EXCLAMATION_EQUAL:   op = BINARY_OP_NE;          break;
        case TOKEN_AMPERSAND:           op = BINARY_OP_BITWISE_AND; break;
        case TOKEN_CARET:               op = BINARY_OP_BITWISE_XOR; break;
        case TOKEN_BAR:                 op = BINARY_OP_BITWISE_OR;  break;
        case TOKEN_AMPERSAND_AMPERSAND: op = BINARY_OP_LOGICAL_AND; break;
        case TOKEN_BAR_BAR:             op = BINARY_OP_LOGICAL_OR;  break;
        default: return left_type;
        }
        if (BINARY_OP_LEFT_BINDING_POWERS[op] < min_binding_power) {
            break;
        }
        struct token op_tok;
        take_token(ctx, &op_tok);
        enum TYPE result_type;
        switch (op) {
        case BINARY_OP_LOGICAL_OR: {
            if (left_type != TYPE_I32) {
                fail_type_mismatch(ctx, op_tok, TYPE_I32, left_type);
            }
            size_t true_label = ctx->local_label_count++;
            size_t done_label = ctx->local_label_count++;
            emit_local_forward_branch_if_nonzero(ctx, true_label);
            right_type = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
            if (right_type != TYPE_I32) {
                fail_type_mismatch(ctx, op_tok, TYPE_I32, right_type);
            }
            emit_local_forward_branch_if_nonzero(ctx, true_label);
            emit_int_literal(ctx, 0);
            emit_local_forward_branch(ctx, done_label);
            emit_local_label(ctx, true_label);
            emit_int_literal(ctx, 1);
            emit_local_label(ctx, done_label);
            result_type = TYPE_I32; // TODO: return TYPE_BOOL
            break;
        }
        case BINARY_OP_LOGICAL_AND: {
            if (left_type != TYPE_I32) {
                fail_type_mismatch(ctx, op_tok, TYPE_I32, left_type);
            }
            size_t false_label = ctx->local_label_count++;
            size_t done_label = ctx->local_label_count++;
            emit_local_forward_branch_if_zero(ctx, false_label);
            right_type = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
            if (right_type != TYPE_I32) {
                fail_type_mismatch(ctx, op_tok, TYPE_I32, right_type);
            }
            emit_local_forward_branch_if_zero(ctx, false_label);
            emit_int_literal(ctx, 1);
            emit_local_forward_branch(ctx, done_label);
            emit_local_label(ctx, false_label);
            emit_int_literal(ctx, 0);
            emit_local_label(ctx, done_label);
            result_type = TYPE_I32; // TODO: return TYPE_BOOL
            break;
        }
        default:
            if (left_type != TYPE_I32) {
                fail_type_mismatch(ctx, op_tok, TYPE_I32, left_type);
            }
            right_type = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
            if (right_type != TYPE_I32) {
                fail_type_mismatch(ctx, op_tok, TYPE_I32, right_type);
            }
            emit_binary_op(ctx, op, left_type, right_type);
            result_type = TYPE_I32; // TODO: infer type
            break;
        }
        left_type = result_type;
    }
    return left_type;
}

static enum TYPE
compile_if_expr(struct context *ctx) {
    // EBNF: if_expr = "if" expr block_expr { "else" "if" expr block_expr } [ "else" block_expr ] ;
    struct token if_tok;
    take_token_expect_kind(ctx, &if_tok, TOKEN_KEYWORD_IF);
    enum TYPE condition_type = compile_expr(ctx, 0);
    if (condition_type == TYPE_UNIT) {
        fail_type_mismatch(ctx, if_tok, TYPE_I32, condition_type);
    }
    size_t false_label = ctx->local_label_count++;
    size_t done_label = ctx->local_label_count++;
    emit_local_forward_branch_if_zero(ctx, false_label);
    enum TYPE then_type = compile_block(ctx);
    emit_local_forward_branch(ctx, done_label);
    emit_local_label(ctx, false_label);
    if (peek_token_kind(ctx) == TOKEN_KEYWORD_ELSE) {
        struct token else_tok;
        take_token_expect_kind(ctx, &else_tok, TOKEN_KEYWORD_ELSE);
        enum TYPE else_type;
        if (peek_token_kind(ctx) == TOKEN_KEYWORD_IF) {
            else_type = compile_if_expr(ctx);
        } else {
            else_type = compile_block(ctx);
        }
        if (then_type != else_type) {
            fail_type_mismatch(ctx, else_tok, then_type, else_type);
        }
    } else {
        if (then_type != TYPE_UNIT) {
            // TODO: show the then block span, not the if_tok
            fail_type_mismatch(ctx, if_tok, TYPE_UNIT, then_type);
        }
    }
    emit_local_label(ctx, done_label);
    return then_type;
}

static enum TYPE
compile_prefix_op_expr(struct context *ctx) {
    // EBNF: prefix_op = "!" | "-" ;
    enum TYPE return_type;
    switch (peek_token_kind(ctx)) {
    case TOKEN_MINUS:
        take_token_expect_kind(ctx, NULL, TOKEN_MINUS);
        if (peek_token_kind(ctx) == TOKEN_INT_LITERAL) {
            return_type = compile_int_literal(ctx, true);
            if (return_type != TYPE_I32) {
                fail_type_mismatch(ctx, ctx->tokens[ctx->token_offset], TYPE_I32, return_type);
            }
        } else {
            return_type = compile_expr(ctx, UNARY_OP_RIGHT_BINDING_POWERS[UNARY_OP_NEG]);
            if (return_type != TYPE_I32) {
                fail_type_mismatch(ctx, ctx->tokens[ctx->token_offset], TYPE_I32, return_type);
            }
            emit_unary_op(ctx, UNARY_OP_NEG);
        }
        break;
    case TOKEN_TILDE:
        take_token_expect_kind(ctx, NULL, TOKEN_TILDE);
        return_type = compile_expr(ctx, UNARY_OP_RIGHT_BINDING_POWERS[UNARY_OP_BITWISE_NOT]);
        if (return_type != TYPE_I32) {
            fail_type_mismatch(ctx, ctx->tokens[ctx->token_offset], TYPE_I32, return_type);
        }
        emit_unary_op(ctx, UNARY_OP_BITWISE_NOT);
        break;
    case TOKEN_EXCLAMATION:
        take_token_expect_kind(ctx, NULL, TOKEN_EXCLAMATION);
        return_type = compile_expr(ctx, UNARY_OP_RIGHT_BINDING_POWERS[UNARY_OP_LOGICAL_NOT]);
        if (return_type != TYPE_I32) {
            fail_type_mismatch(ctx, ctx->tokens[ctx->token_offset], TYPE_I32, return_type);
        }
        emit_unary_op(ctx, UNARY_OP_LOGICAL_NOT);
        break;
    default: assert(!"unreachable");
    }
    return return_type;
}

static enum TYPE
compile_fn_call(struct context *ctx) {
    // EBNF: fn_call = ident "(" ")" ;
    struct token name_tok;
    take_token_expect_kind(ctx, &name_tok, TOKEN_IDENT);
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
    take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_PAREN);
    if (token_starts_with(ctx, name_tok, "__builtin")) {
        if (token_equals(ctx, name_tok, "__builtin_trap")) {
            emit_builtin_trap(ctx);
            return TYPE_UNIT; // TODO: never type
        }
        fail_unknown_builtin(ctx, name_tok);
    }
    enum TYPE return_type = TYPE_I32; // TODO: lookup return type in symbol table
    emit_fn_call(ctx, ctx->src + name_tok.loc.idx, name_tok.len, return_type);
    return return_type;
}

static enum TYPE
compile_int_literal(struct context *ctx, bool negate) {
    struct token tok;
    take_token_expect_kind(ctx, &tok, TOKEN_INT_LITERAL);
    unsigned long long int uvalue = 0;
    char *src = ctx->src + tok.loc.idx;
    size_t len = tok.len;
    size_t i = 0;
    if (src[i] == '0' && i + 1 < len
    && (src[i + 1] == 'b' || src[i + 1] == 'o' || src[i + 1] == 'x')) {
        switch (src[i + 1]) {
        case 'b':
            // binary literal
            for (i += 2; i < len; i++) {
                if (src[i] == '_') { continue; }
                if (__builtin_mul_overflow(uvalue, 2, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
                char digit = src[i] - '0';
                assert(digit < 2);
                if (__builtin_add_overflow(uvalue, digit, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
            }
            break;
        case 'o':
            // octal literal
            for (i += 2; i < len; i++) {
                if (src[i] == '_') { continue; }
                if (__builtin_mul_overflow(uvalue, 8, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
                char digit = src[i] - '0';
                assert(digit < 8);
                if (__builtin_add_overflow(uvalue, digit, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
            }
            break;
        case 'x':
            // hexadecimal literal
            for (i += 2; i < len; i++) {
                if (src[i] == '_') { continue; }
                if (__builtin_mul_overflow(uvalue, 16, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
                char digit;
                if (src[i] >= '0' && src[i] <= '9') {
                    digit = src[i] - '0';
                } else if (src[i] >= 'A' && src[i] <= 'F') {
                    digit = src[i] - 'A' + 10;
                } else if (src[i] >= 'a' && src[i] <= 'f') {
                    digit = src[i] - 'a' + 10;
                } else {
                    assert(!"unreachable");
                }
                assert(digit < 16);
                if (__builtin_add_overflow(uvalue, digit, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
            }
            break;
        default: assert(!"unreachable");
        }
    } else {
        // decimal literal
        for (; i < len; i++) {
            if (src[i] == '_') { continue; }
            if (__builtin_mul_overflow(uvalue, 10, &uvalue)) {
                fail_int_literal_out_of_range(ctx, tok);
            }
            char digit = src[i] - '0';
            assert(digit < 10);
            if (__builtin_add_overflow(uvalue, digit, &uvalue)) {
                fail_int_literal_out_of_range(ctx, tok);
            }
        }
    }
    if (negate && uvalue > LLONG_MAX) {
        fail_int_literal_out_of_range(ctx, tok);
    }
    long long int value;
    if (negate) {
        if (__builtin_sub_overflow(0, uvalue, &value)) {
            fail_int_literal_out_of_range(ctx, tok);
        }
    } else {
        if (__builtin_add_overflow(0, uvalue, &value)) {
            fail_int_literal_out_of_range(ctx, tok);
        }
    }
    emit_int_literal(ctx, value);
    return TYPE_I32;
}

////////////////
// Entrypoint //
////////////////

void // output_file_path should have capacity PATH_MAX + 1
get_default_output_file_path(const char *input_file_path, char *output_file_path) {
    // Find the last dot in input_file_path
    char *dot = strrchr(input_file_path, '.');
    // Calculate the length of the base name
    size_t base_len = dot ? (size_t)(dot - input_file_path) : strlen(input_file_path);
    assert(base_len + 2 <= PATH_MAX);
    // Copy the base name
    strncpy(output_file_path, input_file_path, base_len);
    output_file_path[base_len] = '\0';
    // Append the new extension
    strcat(output_file_path, ".s");
}

int
main(int argc, char *argv[]) {
    // Parse command line options
    int opt;
    char output_file_path_buf[PATH_MAX + 1];
    char *output_file_path = NULL;
    while ((opt = getopt(argc, argv, "o:")) != -1) {
        switch(opt) {
            case 'o':
                output_file_path = optarg;
                break;
            case '?':
                // getopt already prints an error message for unknown options
                fprintf(stderr, "Usage: %s [-o OUT_FILE] FILE\n", argv[0]);
                exit(EXIT_FAILURE);
            default:
                assert(!"unreachable");
        }
    }
    // After option parsing, optind is the index of the first non-option argument
    if (optind >= argc) {
        fprintf(stderr, "Expected FILE argument after options\n");
        fprintf(stderr, "Usage: %s [-o OUT_FILE] FILE\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *input_file_path = argv[optind];
    if (output_file_path == NULL) {
        get_default_output_file_path(input_file_path, output_file_path_buf);
        output_file_path = output_file_path_buf;
    }

    // Open the input file
    int fd = open(input_file_path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "internal compiler error while opening file '%s': %s\n", input_file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Get the input file size
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        fprintf(stderr, "internal compiler error while getting file size for '%s': %s\n", input_file_path, strerror(errno));
        if (close(fd) == -1) { fprintf(stderr, "internal compiler error while closing file '%s': %s\n", input_file_path, strerror(errno)); }
        exit(EXIT_FAILURE);
    }
    size_t input_file_size = sb.st_size;
    void *mapped_input_file = NULL;
    if (input_file_size != 0) {
        // Memory-map the input file
        mapped_input_file = mmap(NULL, input_file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped_input_file == MAP_FAILED) {
            fprintf(stderr, "internal compiler error while mapping file '%s': %s\n", input_file_path, strerror(errno));
            if (close(fd) == -1) { fprintf(stderr, "internal compiler error while closing file '%s': %s\n", input_file_path, strerror(errno)); }
            exit(EXIT_FAILURE);
        }
    }
    if (close(fd) == -1) {
        fprintf(stderr, "internal compiler error while closing file '%s': %s\n", input_file_path, strerror(errno));
        // Continue even if close fails
    }

    // Open the output file
    FILE *output_file = fopen(output_file_path, "w");

    struct context ctx = {
        .output_file = output_file,
        .input_file_path = input_file_path,
        .src = (char *)mapped_input_file,
        .src_len = input_file_size,
        .src_loc = { .idx = 0, .line = 1, .col = 1 },
    };
    compile_program(&ctx);

    // Close the output file
    if (fclose(output_file) == EOF) {
        fprintf(stderr, "internal compiler error while closing file '%s': %s\n", output_file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Unmap the input file
    if (mapped_input_file != NULL && munmap(mapped_input_file, input_file_size) == -1) {
        fprintf(stderr, "internal compiler error while unmapping file '%s': %s\n", input_file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return 0;
}
