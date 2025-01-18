// Copyright © 2025 by Scott J Maddox. All rights reserved.
// https://github.com/scottjmaddox/single-pass-compiler

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>         // For open()
#include <sys/mman.h>      // For mmap()
#include <sys/stat.h>      // For fstat()
#include <sys/syslimits.h> // For PATH_MAX
#include <unistd.h>        // For getopt(), close()
#include <errno.h>         // For errno
#include <string.h>        // For strerror()
#include <assert.h>        // For assert()
#include <limits.h>        // For ULLONG_MAX, LLONG_MAX

#define MAX_TOKEN_LOOKAHEAD 2
#define HEAP_COMMIT_SIZE (1024 * 1024) // 1 MiB

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
    TOKEN_COLON,
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
    TOKEN_KEYWORD_CONST,
    TOKEN_IDENT,
    TOKEN_INT_LITERAL,
    TOKEN_RESERVED,
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
    "COLON",
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
    "KEYWORD_CONST",
    "IDENT",
    "INT_LITERAL",
    "RESERVED",
    "UNKNOWN",
};

enum UNARY_OP {
    UNARY_OP_NEG,
    UNARY_OP_BITWISE_NOT,
    UNARY_OP_LOGICAL_NOT,
};

enum BINARY_OP {
    BINARY_OP_TYPE_ANNO,
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

static char *BINARY_OP_DISPLAY[] = {
    [BINARY_OP_TYPE_ANNO]   = ":",
    [BINARY_OP_MUL]         = "*",
    [BINARY_OP_DIV]         = "/",
    [BINARY_OP_REM]         = "%",
    [BINARY_OP_ADD]         = "+",
    [BINARY_OP_SUB]         = "-",
    [BINARY_OP_SHL]         = "<<",
    [BINARY_OP_SHR]         = ">>",
    [BINARY_OP_LT]          = "<",
    [BINARY_OP_LE]          = "<=",
    [BINARY_OP_GT]          = ">",
    [BINARY_OP_GE]          = ">=",
    [BINARY_OP_EQ]          = "==",
    [BINARY_OP_NE]          = "!=",
    [BINARY_OP_BITWISE_AND] = "&",
    [BINARY_OP_BITWISE_XOR] = "^",
    [BINARY_OP_BITWISE_OR]  = "|",
    [BINARY_OP_LOGICAL_AND] = "&&",
    [BINARY_OP_LOGICAL_OR]  = "||",
};

static int UNARY_OP_RIGHT_BINDING_POWERS[] = {
    [UNARY_OP_NEG]          = 110,
    [UNARY_OP_BITWISE_NOT]  = 110,
    [UNARY_OP_LOGICAL_NOT]  = 110,
};

static int BINARY_OP_LEFT_BINDING_POWERS[] = {
    [BINARY_OP_TYPE_ANNO]   = 120,
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
    [BINARY_OP_TYPE_ANNO]   = 120 + 1,
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

struct str {
    size_t len;
    char *ptr;
};

static struct str IF_STR = {.len = sizeof "if" - 1, .ptr = "if"};
static struct str FN_STR = {.len = sizeof "fn" - 1, .ptr = "fn"};
static struct str LET_STR = {.len = sizeof "let" - 1, .ptr = "let"};
static struct str ELSE_STR = {.len = sizeof "else" - 1, .ptr = "else"};
static struct str CONST_STR = {.len = sizeof "const" - 1, .ptr = "const"};

static struct str WILDCARD_STR = {.len = sizeof "_" - 1, .ptr = "_"};

static struct str UNIT_STR = {.len = sizeof "unit" - 1, .ptr = "unit"};
static struct str NEVER_STR = {.len = sizeof "never" - 1, .ptr = "never"};

static struct str BUILTIN_STR = {.len = sizeof "__builtin" - 1, .ptr = "__builtin"};
static struct str BUILTIN_TRAP_STR = {.len = sizeof "__builtin_trap" - 1, .ptr = "__builtin_trap"};

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

struct span {
    struct location start;
    struct location end;
};

// Type kinds
enum TY {
    TY_UNIT,
    TY_NEVER,
    TY_CONST_FN,
    TY_CONST_INT,
    TY_INT,
};

struct type {
    enum TY kind;
    union {
        // `arg_list` stores type constructor arguments.
        // For TY_CONST_FN, that's the parameter types followed by the return type.
        struct type_node *arg_list;
        // For TY_CONST_INT, `value` stores the constant value.
        __int128_t value;
        // For TY_INT, `bits` stores the number of bits.
        struct {
            bool sgnd; // true for signed, false for unsigned
            unsigned int bits;
        };
    };
};


struct type_span {
    struct type type;
    struct span span;
};

struct type_node {
    struct type_node *next;
    struct type_span tysp;
};

struct symbol {
    struct token ident_tok;
    struct type_span tysp;
    int var_stack_offset;
};

struct symbol_node {
    struct symbol_node *next;
    struct symbol sym;
};

struct scope_node {
    struct scope_node *next;
    struct symbol_node *symbol_list;
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
    // Heap state
    uintptr_t heap;
    uintptr_t heap_end;
    size_t commited_heap_size;
    size_t max_heap_size;
    struct type_node *free_type_nodes;
    struct symbol_node *free_symbol_nodes;
    struct scope_node *free_scope_nodes;
    // Compiler state
    int stack_offset; // current stack offset from the frame pointer in x29
    size_t local_label_count;
    struct scope_node *scope_stack;
    bool is_dead_code;
};

///////////
// Utils //
///////////

static void*
alloc(struct context *ctx, size_t size, size_t align) {
    // NOTE: assume align is a non-zero, positive power of 2
    uintptr_t ptr = (ctx->heap_end + align - 1) & ~(align - 1);
    ctx->heap_end = ptr + size;
    size_t new_heap_size = ctx->heap_end - ctx->heap;
    if (new_heap_size > ctx->commited_heap_size) {
        if (new_heap_size > ctx->max_heap_size) {
            fprintf(stderr, "error: heap size limit exceeded\n");
            exit(EXIT_FAILURE);
        }
        // NOTE: assume HEAP_COMMIT_SIZE is a non-zero, positive power of 2
        size_t new_commited_heap_size = (new_heap_size + HEAP_COMMIT_SIZE - 1) & ~(HEAP_COMMIT_SIZE - 1);
        size_t commit_size = new_commited_heap_size - ctx->commited_heap_size;
        void *result = mmap((void *)(ctx->heap + ctx->commited_heap_size), commit_size,
                            PROT_READ | PROT_WRITE, MAP_ANON | MAP_FIXED | MAP_PRIVATE, -1, 0);
        if (result == MAP_FAILED) {
            fprintf(stderr, "internal compiler error while expanding the heap: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        ctx->commited_heap_size = new_commited_heap_size;
    }
    return (void*)ptr;
}

static struct type_node*
alloc_type_node(struct context *ctx) {
    struct type_node *node;
    if (ctx->free_type_nodes == NULL) {
        node = alloc(ctx, sizeof(struct type_node), __alignof__(struct type_node));
    } else {
        node = ctx->free_type_nodes;
        ctx->free_type_nodes = node->next;
    }
    node->next = NULL;
    return node;
}

static struct symbol_node*
alloc_symbol_node(struct context *ctx) {
    struct symbol_node *node;
    if (ctx->free_symbol_nodes == NULL) {
        node = alloc(ctx, sizeof(struct symbol_node), __alignof__(struct symbol_node));
    } else {
        node = ctx->free_symbol_nodes;
        ctx->free_symbol_nodes = node->next;
    }
    node->next = NULL;
    return node;
}

static struct scope_node*
alloc_scope_node(struct context *ctx) {
    struct scope_node *node;
    if (ctx->free_scope_nodes == NULL) {
        node = alloc(ctx, sizeof(struct scope_node), __alignof__(struct scope_node));
    } else {
        node = ctx->free_scope_nodes;
        ctx->free_scope_nodes = node->next;
    }
    node->next = NULL;
    return node;
}

static void
free_type_node(struct context *ctx, struct type_node *node) {
    node->next = ctx->free_type_nodes;
    ctx->free_type_nodes = node;
}

static void
free_symbol_node(struct context *ctx, struct symbol_node *node) {
    node->next = ctx->free_symbol_nodes;
    ctx->free_symbol_nodes = node;
}

static void
free_scope_node(struct context *ctx, struct scope_node *node) {
    node->next = ctx->free_scope_nodes;
    ctx->free_scope_nodes = node;
}

static void
push_symbol(struct context *ctx, struct symbol sym) {
    struct symbol_node *node = alloc_symbol_node(ctx);
    *node = (struct symbol_node){ .next = ctx->scope_stack->symbol_list, .sym = sym };
    ctx->scope_stack->symbol_list = node;
}

static void
push_scope(struct context *ctx) {
    struct scope_node *node = alloc_scope_node(ctx);
    *node = (struct scope_node){ .next = ctx->scope_stack, .symbol_list = NULL };
    ctx->scope_stack = node;
}

static void
pop_scope(struct context *ctx) {
    struct scope_node *scope = ctx->scope_stack;
    ctx->scope_stack = scope->next;
    while (scope->symbol_list != NULL) {
        struct symbol_node *node = scope->symbol_list;
        scope->symbol_list = node->next;
        struct type ty = node->sym.tysp.type;
        if (ty.kind == TY_CONST_FN) {
            struct type_node *arg = ty.arg_list;
            while (arg != NULL) {
                struct type_node *next = arg->next;
                free_type_node(ctx, arg);
                arg = next;
            }
        }
        free_symbol_node(ctx, node);
    }
    free_scope_node(ctx, scope);
}

static bool
str_equals(struct str a, struct str b) {
    if (a.len != b.len) { return false; }
    for (size_t i = 0; i < a.len; i++) {
        if (a.ptr[i] != b.ptr[i]) { return false; }
    }
    return true;
}

static bool
str_starts_with(struct str str, struct str prefix) {
    return str.len >= prefix.len && str_equals((struct str){ .len = prefix.len, .ptr = str.ptr }, prefix);
}

static struct str
token_str(struct context *ctx, struct token tok) {
    return (struct str){ .len = tok.len, .ptr = ctx->src + tok.loc.idx };
}

static bool
type_equals(struct type a, struct type b) {
    if (a.kind != b.kind) { return false; }
    switch (a.kind) {
    case TY_UNIT:
    case TY_NEVER:
        return true;
    case TY_CONST_FN:
        {
            struct type_node *a_arg = a.arg_list;
            struct type_node *b_arg = b.arg_list;
            while (a_arg != NULL && b_arg != NULL) {
                if (!type_equals(a_arg->tysp.type, b_arg->tysp.type)) { return false; }
                a_arg = a_arg->next;
                b_arg = b_arg->next;
            }
            return a_arg == NULL && b_arg == NULL;
        }
    case TY_CONST_INT:
        return a.value == b.value;
    case TY_INT:
        return a.sgnd == b.sgnd && a.bits == b.bits;
    }
    assert(!"unreachable");
}

static struct symbol_node*
find_symbol_node(struct context *ctx, struct str ident) {
    for(struct scope_node *scope = ctx->scope_stack; scope != NULL; scope = scope->next) {
        for(struct symbol_node *node = scope->symbol_list; node != NULL; node = node->next) {
            if (str_equals(token_str(ctx, node->sym.ident_tok), ident)) {
                return node;
            }
        }
    }
    return NULL;
}

//////////////////////
// Lexical analysis //
//////////////////////

static struct token
lex_number(struct context *ctx, struct token tok) {
    char *src = ctx->src;
    size_t len = ctx->src_len;
    size_t i = tok.loc.idx;
    assert(i < len);

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
        bool starts_with_zero = false;
        if (src[i] == '0') { starts_with_zero = true; }
        has_digit = true;
        i += 1;
        bool has_non_zero_digit = false;
        while (i < len) {
            switch (src[i]) {
            case '1' ... '9': has_non_zero_digit = true; // fallthrough
            case '_':
            case '0': i += 1; continue;
            }
            break;
        }
        is_reserved |= starts_with_zero && has_non_zero_digit;
    }
    if (i < len) {
        switch (src[i]) {
        case '.':
        case 'A' ... 'Z':
        case 'a' ... 'z': i += 1; is_reserved = true; break;
        }
    }
    is_reserved = is_reserved || !has_digit;
    tok.kind = is_reserved ? TOKEN_RESERVED : TOKEN_INT_LITERAL;
    tok.len = i - tok.loc.idx;
    return tok;
}

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
        case ':': tok.kind = TOKEN_COLON; return tok;
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
                if (str_equals(token_str(ctx, tok), IF_STR)) { tok.kind = TOKEN_KEYWORD_IF; return tok; }
                if (str_equals(token_str(ctx, tok), FN_STR)) { tok.kind = TOKEN_KEYWORD_FN; return tok; }
                break;
            case 3:
                if (str_equals(token_str(ctx, tok), LET_STR)) { tok.kind = TOKEN_KEYWORD_LET; return tok; }
                break;
            case 4:
                if (str_equals(token_str(ctx, tok), ELSE_STR)) { tok.kind = TOKEN_KEYWORD_ELSE; return tok; }
                break;
            case 5:
                if (str_equals(token_str(ctx, tok), CONST_STR)) { tok.kind = TOKEN_KEYWORD_CONST; return tok; }
                break;
            }
            tok.kind = TOKEN_IDENT;
            return tok;
        case '0' ... '9': return lex_number(ctx, tok);
        }
        tok.kind = TOKEN_UNKNOWN;
        return tok;
    }
    tok.kind = TOKEN_EOF;
    tok.len = 0;
    return tok;
}

static struct span
join_spans(struct span left, struct span right) {
    return (struct span){ .start = left.start, .end = right.end };
}

static struct location
token_end(struct context *ctx, struct token tok) {
    // NOTE: assume tokens do not span multiple lines, for now
    return (struct location){
        .idx = tok.loc.idx + tok.len,
        .line = tok.loc.line,
        .col = tok.loc.col + tok.len,
    };
}

static struct span
token_span(struct context *ctx, struct token tok) {
    return (struct span){ .start = tok.loc, .end = token_end(ctx, tok) };
}

static void
push_token(struct context *ctx, struct token tok) {
    assert(ctx->token_count < MAX_TOKEN_LOOKAHEAD);
    ctx->tokens[(ctx->token_offset + ctx->token_count) % MAX_TOKEN_LOOKAHEAD] = tok;
    ctx->token_count += 1;
    ctx->src_loc = token_end(ctx, tok);
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

static enum TOKEN
peek_token_kind_at(struct context *ctx, size_t i) {
    assert(i < MAX_TOKEN_LOOKAHEAD);
    if (i >= ctx->token_count) { fill_tokens(ctx); }
    return ctx->tokens[(ctx->token_offset + i) % MAX_TOKEN_LOOKAHEAD].kind;
}

static enum TOKEN
peek_token_kind(struct context *ctx) {
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    return ctx->tokens[ctx->token_offset].kind;
}

static struct location
peek_token_loc(struct context *ctx) {
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    return ctx->tokens[ctx->token_offset].loc;
}

static struct span
peek_token_span(struct context *ctx) {
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    return token_span(ctx, ctx->tokens[ctx->token_offset]);
}

///////////////////////////////
// Diagnostics and Utilities //
///////////////////////////////

static void
eprint_type(struct type ty) {
    switch (ty.kind) {
    case TY_UNIT: fprintf(stderr, "unit"); break;
    case TY_NEVER: fprintf(stderr, "never"); break;
    case TY_CONST_FN: assert(!"not implemented"); break; // TODO: implement
    case TY_CONST_INT: fprintf(stderr, "const int"); break;
    case TY_INT:
        if (ty.sgnd) {
            fprintf(stderr, "i%hu", ty.bits);
        } else {
            fprintf(stderr, "u%hu", ty.bits);
        };
        break;
    }
}

static struct str
get_span_line(struct context *ctx, struct span span) {
    size_t idx = 0;
    size_t line = 1;
    while (line < span.start.line) {
        switch (ctx->src[idx]) {
        case '\n': idx += 1; line += 1; break;
        default: idx += 1; break;
        }
    }
    size_t len = 0;
    while (idx + len < ctx->src_len && ctx->src[idx + len] != '\n') {
        len += 1;
    }
    return (struct str){ .len = len, .ptr = ctx->src + idx };
}

static void
eprint_span(struct context *ctx, struct span span) {
    // TODO: handle multi-line spans
    struct str token_line = get_span_line(ctx, span);
    fprintf(stderr,
        " --> %s:%zu:%zu\n"
        "  |\n"
        "  | %.*s\n",
        ctx->input_file_path, span.start.line, span.start.col,
        (int)token_line.len, token_line.ptr
    );
    fprintf(stderr, "  | ");
    for (size_t i = 0; i < span.start.col - 1; i++) {
        if (token_line.ptr[i] == '\t') {
            fputc('\t', stderr);
        } else {
            fputc(' ', stderr);
        }
    }
    size_t len = span.end.idx - span.start.idx;
    for (size_t i = 0; i < len; i++) { fputc('^', stderr); }
    fprintf(stderr, "\n");
}

static void
fail_expected_token_kind(struct context *ctx, enum TOKEN expected_token_kind, struct token tok) {
    fprintf(stderr, "error: unexpected token: %s\n", TOKEN_NAMES[tok.kind]);
    eprint_span(ctx, token_span(ctx, tok));
    fprintf(stderr, "  expected: %s\n", TOKEN_NAMES[expected_token_kind]);
    exit(EXIT_FAILURE);
}

static void
fail_expected(struct context *ctx, char *str) {
    struct token tok;
    take_token(ctx, &tok);
    fprintf(stderr, "error: unexpected token: %s\n", TOKEN_NAMES[tok.kind]);
    eprint_span(ctx, token_span(ctx, tok));
    fprintf(stderr, "  expected: %s.\n", str);
    exit(EXIT_FAILURE);
}

static void
fail_reserved_ident(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: reserved identifier\n");
    eprint_span(ctx, token_span(ctx, tok));
    fprintf(stderr, "  note: identifiers starting with '__builtin' are reserved.\n");
    exit(EXIT_FAILURE);
}

static void
fail_unknown_builtin(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: unknown builtin function\n");
    eprint_span(ctx, token_span(ctx, tok));
    exit(EXIT_FAILURE);
}

static void
fail_undefined_fn(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: undefined function\n");
    eprint_span(ctx, token_span(ctx, tok));
    exit(EXIT_FAILURE);
}

static void
fail_undefined_type(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: undefined type\n");
    eprint_span(ctx, token_span(ctx, tok));
    exit(EXIT_FAILURE);
}

static void
fail_undefined_var(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: undefined variable\n");
    eprint_span(ctx, token_span(ctx, tok));
    exit(EXIT_FAILURE);
}

static void
fail_fn_call_non_fn(struct context *ctx, struct token fn_call_tok, struct token fn_ident_tok, struct type_span ident_tysp) {
    fprintf(stderr, "error: function call to non-function:\n");
    eprint_span(ctx, token_span(ctx, fn_call_tok));
    fprintf(stderr, "`%.*s` has type `", (int)fn_call_tok.len, ctx->src + fn_call_tok.loc.idx);
    eprint_type(ident_tysp.type);
    fprintf(stderr, "` and is defined here:\n");
    eprint_span(ctx, token_span(ctx, fn_ident_tok));
    exit(EXIT_FAILURE);
}

static void
fail_expected_type_int(struct context *ctx, struct type_span actual) {
    fprintf(stderr, "error: expected an integer type, not `");
    eprint_type(actual.type);
    fprintf(stderr, "`:\n");
    eprint_span(ctx, actual.span);
    exit(EXIT_FAILURE);
}

static void
fail_incompatible_binary_op_types(struct context *ctx, enum BINARY_OP op, struct type_span left, struct type_span right) {
    // TODO: incorporate `op` into the error message
    fprintf(stderr, "error: bitwise operation type mismatch between `");
    eprint_type(left.type);
    fprintf(stderr, "` from:\n");
    eprint_span(ctx, left.span);
    fprintf(stderr, "  and `");
    eprint_type(right.type);
    fprintf(stderr, "` from:\n");
    eprint_span(ctx, right.span);
    exit(EXIT_FAILURE);
}

static void
fail_invalid_type_for_arithmetic(struct context *ctx, enum BINARY_OP op, struct type_span tysp) {
    fprintf(stderr, "error: invalid type for `%s` operator `", BINARY_OP_DISPLAY[op]);
    eprint_type(tysp.type);
    fprintf(stderr, "` from:\n");
    eprint_span(ctx, tysp.span);
    fprintf(stderr, "  note: the `%s` operator currently only supports `u64`, and `i64`.\n", BINARY_OP_DISPLAY[op]);
    exit(EXIT_FAILURE);
}

static void
fail_expected_subtype(struct context *ctx, struct type_span sub, struct type_span sup) {
    fprintf(stderr, "error: expected type `");
    eprint_type(sub.type);
    fprintf(stderr, "` from:\n");
    eprint_span(ctx, sub.span);
    fprintf(stderr, "  to be a subtype of type `");
    eprint_type(sup.type);
    fprintf(stderr, "` from:\n");
    eprint_span(ctx, sup.span);
    fprintf(stderr,
        "  note: type A is a subtype of type B if and only if:\n"
        "  - every value that A can represent is representable by B, and\n"
        "  - type A supports all the operations that type B supports\n"
    );
    exit(EXIT_FAILURE);
}

static void
fail_if_without_else_non_unit(struct context *ctx, struct type_span then) {
    fprintf(stderr, "error: if without else has non-unit type `");
    eprint_type(then.type);
    fprintf(stderr, "`:\n");
    eprint_span(ctx, then.span);
    fprintf(stderr, "  expected type `unit` or a matching else type.\n");
    exit(EXIT_FAILURE);
}

static void
fail_const_int_out_of_range(struct context *ctx, struct span span) {
    fprintf(stderr, "error: const integer out of range:\n");
    eprint_span(ctx, span);
    fprintf(stderr, "  expected range: -2¹²⁷ to 2¹²⁷ - 1\n");
    exit(EXIT_FAILURE);
}

static void
fail_const_int_div_by_zero(struct context *ctx, struct span span) {
    fprintf(stderr, "error: const integer division by zero:\n");
    eprint_span(ctx, span);
    exit(EXIT_FAILURE);
}

static void
fail_const_int_rem_by_zero(struct context *ctx, struct span span) {
    fprintf(stderr, "error: const integer remainder by zero:\n");
    eprint_span(ctx, span);
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
    if (ctx->is_dead_code) { return; }
    fprintf(ctx->output_file,
        "\t.section\t__TEXT,__text,regular,pure_instructions\n"
    );
}

static void
emit_program_epilogue(struct context *ctx) {
    if (ctx->is_dead_code) { return; }
    fprintf(ctx->output_file, "\n.subsections_via_symbols\n");
}

static void
emit_comment(struct context *ctx, char *comment) {
    if (ctx->is_dead_code) { return; }
    fprintf(ctx->output_file, "\t; %s\n", comment);
}

static void
emit_push(struct context *ctx, char *reg) {
    if (ctx->is_dead_code) { return; }
    // NOTE: using 16-byte slot, for now, to comply with stack pointer alignment restrictions
    fprintf(ctx->output_file, "\tstr\t%s, [sp, #-16]!\t; push\n", reg);
}

static void
emit_pop(struct context *ctx, char *reg) {
    if (ctx->is_dead_code) { return; }
    // NOTE: using 16-byte slot, for now, to comply with stack pointer alignment restrictions
    fprintf(ctx->output_file, "\tldr\t%s, [sp], #16\t; pop\n", reg);
}

static void
emit_drop_type(struct context *ctx, enum TY ty) {
    if (ctx->is_dead_code) { return; }
    switch (ty) {
        case TY_UNIT:
        case TY_NEVER:
        case TY_CONST_FN:
        case TY_CONST_INT:
            break;
        // NOTE: using 16-byte slot, for now, to comply with stack pointer alignment restrictions
        case TY_INT: fprintf(ctx->output_file, "\tadd\tsp, sp, #16\n; pop\n");
    }
}

static void
emit_load_var(struct context *ctx, int var_stack_offset) {
    if (ctx->is_dead_code) { return; }
    emit_comment(ctx, "load var");
    fprintf(ctx->output_file, "\tldr\tx0, [x29, #%d]\n", var_stack_offset);
    emit_push(ctx, "x0");
}

static void
emit_clear_reg(struct context *ctx, char *reg) {
    if (ctx->is_dead_code) { return; }
    fprintf(ctx->output_file, "\tmov\t%s, #0\n", reg);
}

static void
emit_fn_prologue(struct context *ctx, struct str name) {
    if (ctx->is_dead_code) { return; }
    emit_comment(ctx, "fn prologue");
    fprintf(ctx->output_file,
        "\t.globl\t_%.*s\n"
        "\t.p2align\t2\n"
        "_%.*s:\n"
        "\t.cfi_startproc\n"
        "\tstp\tx29, x30, [sp, #-16]!\n"
        "\tmov\tx29, sp"
        "\n",
        (int)name.len, name.ptr,
        (int)name.len, name.ptr
    );
    ctx->stack_offset = 0;
}

static void
emit_fn_epilogue(struct context *ctx, enum TY return_type) {
    if (ctx->is_dead_code) { return; }
    emit_comment(ctx, "pop return value");
    switch (return_type) {
    case TY_UNIT:
    case TY_NEVER:
        emit_clear_reg(ctx, "x0"); break;
    case TY_CONST_FN:
    case TY_CONST_INT:
        assert(!"unreachable"); break;
    case TY_INT: emit_pop(ctx, "x0"); break;
    }
    emit_comment(ctx, "fn epilogue");
    fprintf(ctx->output_file,
        "\tsub\tsp, sp, #%d\n"
        "\tldp\tx29, x30, [sp], #16\n"
        "\tret\n"
        "\t.cfi_endproc\n",
        ctx->stack_offset
    );
}

static void
emit_fn_call(struct context *ctx, char *name, size_t name_len, enum TY return_type) {
    if (ctx->is_dead_code) { return; }
    emit_comment(ctx, "fn call");
    fprintf(ctx->output_file, "\tbl\t_%.*s\n", (int)name_len, name);
    switch (return_type) {
        case TY_UNIT:
        case TY_NEVER: break;
        case TY_CONST_FN:
        case TY_CONST_INT: assert(!"unreachable");
        case TY_INT: emit_push(ctx, "x0"); break;
    }
}

static void
emit_local_label(struct context *ctx, size_t label) {
    if (ctx->is_dead_code) { return; }
    fprintf(ctx->output_file, "%zu:\n", label);
}

static void
emit_local_forward_branch(struct context *ctx, size_t label) {
    if (ctx->is_dead_code) { return; }
    fprintf(ctx->output_file, "\tb\t%zuf\n", label);
}

static void
emit_local_forward_branch_if_zero(struct context *ctx, size_t label) {
    if (ctx->is_dead_code) { return; }
    emit_pop(ctx, "x0");
    fprintf(ctx->output_file, "\tcbz\tx0, %zuf\n", label);
}

static void
emit_local_forward_branch_if_nonzero(struct context *ctx, size_t label) {
    if (ctx->is_dead_code) { return; }
    emit_pop(ctx, "x0");
    fprintf(ctx->output_file, "\tcbnz\tx0, %zuf\n", label);
}

static void
emit_push_int(struct context *ctx, uint64_t value) {
    if (ctx->is_dead_code) { return; }
    // push the value onto the stack
    fprintf(ctx->output_file, "\tldr\tx0, =0x%llx\n", value);
    emit_push(ctx, "x0");
}

static void
emit_not_equal_zero(struct context *ctx) {
    if (ctx->is_dead_code) { return; }
    emit_comment(ctx, "int to bool");
    emit_pop(ctx, "x0");
    fprintf(ctx->output_file, "\tcmp\tx0, #0\n\tcset\tx0, ne\n");
    emit_push(ctx, "x0");
}

static void
emit_unary_op(struct context *ctx, enum UNARY_OP op) {
    if (ctx->is_dead_code) { return; }
    emit_comment(ctx, "unary op");
    emit_pop(ctx, "x0");
    switch (op) {
    // TODO: abort on neg wrapping
    case UNARY_OP_NEG: fprintf(ctx->output_file, "\tneg\tx0, x0\n"); break;
    case UNARY_OP_BITWISE_NOT: fprintf(ctx->output_file, "\tmvn\tx0, x0\n"); break;
    case UNARY_OP_LOGICAL_NOT: fprintf(ctx->output_file, "\tcmp\tx0, #0\n\tcset\tx0, eq\n" ); break;
    }
    emit_push(ctx, "x0");
}

static void
emit_binary_op(struct context *ctx, enum BINARY_OP op, struct type left, struct type right, bool swap) {
    if (ctx->is_dead_code) { return; }
    // TODO: abort on wrapping with standard operators
    // TODO: add explicit wrapping and saturating operators
    emit_comment(ctx, "binary op");
    assert(left.kind == TY_INT && right.kind == TY_INT);
    if (swap) {
        emit_pop(ctx, "x0");
        emit_pop(ctx, "x1");
    } else {
        emit_pop(ctx, "x1");
        emit_pop(ctx, "x0");
    }
    switch (op) {
    case BINARY_OP_TYPE_ANNO: assert(!"unreachable");
    case BINARY_OP_MUL: fprintf(ctx->output_file, "\tmul\tx0, x0, x1\n"); break;
    case BINARY_OP_DIV:
        assert(left.sgnd == right.sgnd);
        if (left.sgnd) {
            fprintf(ctx->output_file, "\tsdiv\tx0, x0, x1\n");
        } else {
            fprintf(ctx->output_file, "\tudiv\tx0, x0, x1\n");
        }
        break;
    case BINARY_OP_REM:
        assert(left.sgnd == right.sgnd);
        if (left.sgnd) {
            fprintf(ctx->output_file, "\tsdiv\tx2, x0, x1\n");
            fprintf(ctx->output_file, "\tmul\tx2, x2, x1\n");
            fprintf(ctx->output_file, "\tsub\tx0, x0, x2\n");
        } else {
            fprintf(ctx->output_file, "\tudiv\tx2, x0, x1\n");
            fprintf(ctx->output_file, "\tmul\tx2, x2, x1\n");
            fprintf(ctx->output_file, "\tsub\tx0, x0, x2\n");
        }
        break;
    case BINARY_OP_ADD: fprintf(ctx->output_file, "\tadd\tx0, x0, x1\n"); break;
    case BINARY_OP_SUB: fprintf(ctx->output_file, "\tsub\tx0, x0, x1\n"); break;
    case BINARY_OP_SHL:
        fprintf(ctx->output_file, "\tlsl\tx0, x0, x1\n");
        // TODO: mask overflow? or abort?
        break;
    case BINARY_OP_SHR:
        if (left.sgnd) {
            fprintf(ctx->output_file, "\tasr\tx0, x0, x1\n");
        } else {
            fprintf(ctx->output_file, "\tlsr\tx0, x0, x1\n");
        }
        break;
    case BINARY_OP_LT: fprintf(ctx->output_file, "\tcmp\tx0, x1\n\tcset\tx0, lt\n"); break;
    case BINARY_OP_LE: fprintf(ctx->output_file, "\tcmp\tx0, x1\n\tcset\tx0, le\n"); break;
    case BINARY_OP_GT: fprintf(ctx->output_file, "\tcmp\tx0, x1\n\tcset\tx0, gt\n"); break;
    case BINARY_OP_GE: fprintf(ctx->output_file, "\tcmp\tx0, x1\n\tcset\tx0, ge\n"); break;
    case BINARY_OP_EQ: fprintf(ctx->output_file, "\tcmp\tx0, x1\n\tcset\tx0, eq\n"); break;
    case BINARY_OP_NE: fprintf(ctx->output_file, "\tcmp\tx0, x1\n\tcset\tx0, ne\n"); break;
    case BINARY_OP_BITWISE_AND: fprintf(ctx->output_file, "\tand\tx0, x0, x1\n"); break;
    case BINARY_OP_BITWISE_XOR: fprintf(ctx->output_file, "\teor\tx0, x0, x1\n"); break;
    case BINARY_OP_BITWISE_OR: fprintf(ctx->output_file, "\torr\tx0, x0, x1\n"); break;
    case BINARY_OP_LOGICAL_AND:
    case BINARY_OP_LOGICAL_OR:
        assert(!"unreachable");
    }
    emit_push(ctx, "x0");
}

static void
emit_builtin_trap(struct context *ctx) {
    if (ctx->is_dead_code) { return; }
    fprintf(ctx->output_file, "\tbrk\t#0\n");
}

///////////////////////
// Compiler Frontend //
///////////////////////

static void
require_subtype_of_int(struct context *ctx, struct type_span tysp) {
    switch (tysp.type.kind) {
    case TY_UNIT:
    case TY_CONST_FN:
        fail_expected_type_int(ctx, tysp);
    case TY_NEVER:
    case TY_CONST_INT:
    case TY_INT:
        return;
    }
}

static bool
is_subtype(struct type left, struct type right) {
    // Type A is a subtype of type B if and only if:
    // - every value that A can represent is representable by B, and
    // - type A supports all the operations that type B supports
    switch (left.kind) {
    case TY_UNIT: if (right.kind == TY_UNIT) { return true; } break;
    case TY_NEVER: return true;
    case TY_CONST_FN:
        if (right.kind == TY_CONST_FN) {
            assert(!"not implemented"); // TODO: implement
        }
        break;
    case TY_CONST_INT:
        if (right.kind == TY_CONST_INT) { return true; }
        if (right.kind == TY_INT) {
            if (!right.sgnd && left.value < 0) { return false; }
            // TODO: check if the constant is within the bounds of the integer type
            __int128 tmp_value = left.value;
            unsigned int bits = 0;
            if (tmp_value < 0) {
                while (tmp_value != -1) { tmp_value >>= 1; bits++; }
                if (bits == 0) { bits = 1; }
                assert(right.sgnd);
                if (bits <= right.bits) { return true; }
            } else {
                while (tmp_value != 0) { tmp_value >>= 1; bits++; }
                if (right.sgnd) {
                    if (bits < right.bits) { return true; }
                } else {
                    if (bits <= right.bits) { return true; }
                }
            }
        }
        break;
    case TY_INT:
        if (right.kind == TY_INT) {
            if (left.sgnd == right.sgnd && left.bits <= right.bits) { return true; }
            if (!left.sgnd && left.bits < right.bits) { return true; }
        }
        break;
    }
    return false;
}

static void
require_subtype(struct context *ctx, struct type_span left, struct type_span right) {
    if (!is_subtype(left.type, right.type)) {
        fail_expected_subtype(ctx, left, right);
    }
}

static struct type
require_subtype_coerce(struct context *ctx, struct type_span from, struct type_span to) {
    if (type_equals(from.type, to.type)) { return to.type; }
    require_subtype(ctx, from, to);
    switch (from.type.kind) {
    case TY_UNIT: break;
    case TY_NEVER: break;
    case TY_CONST_FN: assert(!"not implemented"); // TODO: implement
    case TY_CONST_INT:
        if (to.type.kind == TY_INT) { emit_push_int(ctx, (uint64_t)from.type.value); }
         break;
    case TY_INT:
        // NOTE: currently, no code to emit, since all integers are stored in 64 bits
         break;
    }
    return to.type;
}

static void compile_program(struct context *ctx);
static void compile_const_def(struct context *ctx);
static void compile_fn_def(struct context *ctx, struct token *name_tok);
static struct type_span compile_let_expr(struct context *ctx);
static struct type_span compile_var_expr(struct context *ctx);
static struct type_span compile_block(struct context *ctx, bool create_scope);
static struct type_span compile_expr(struct context *ctx, int min_binding_power);
static struct type_span compile_if_expr(struct context *ctx);
static struct type_span compile_prefix_op_expr(struct context *ctx);
static struct type_span compile_fn_call(struct context *ctx);
static struct type_span compile_int_literal(struct context *ctx);

static void
compile_program(struct context *ctx) {
    // EBNF: program = { const_def } ;
    emit_program_prologue(ctx);
    push_scope(ctx);
    while (peek_token_kind(ctx) != TOKEN_EOF) {
        compile_const_def(ctx);
    }
    // NOTE: no need to pop_scope, since this is the top level
    emit_program_epilogue(ctx);
}

static void
compile_const_def(struct context *ctx) {
    // EBNF: const_def = "const" ident "=" fn_def ;
    take_token_expect_kind(ctx, NULL, TOKEN_KEYWORD_CONST);
    struct token name_tok;
    take_token_expect_kind(ctx, &name_tok, TOKEN_IDENT);
    if (str_starts_with(token_str(ctx, name_tok), BUILTIN_STR)) {
        fail_reserved_ident(ctx, name_tok);
    }
    take_token_expect_kind(ctx, NULL, TOKEN_EQUAL);
    compile_fn_def(ctx, &name_tok);
}

static struct type_span
parse_type(struct context *ctx) {
    // TODO: allow arbitrary type expressions
    struct token type_tok;
    take_token_expect_kind(ctx, &type_tok, TOKEN_IDENT);

    struct str type_str = token_str(ctx, type_tok);
    struct span span = token_span(ctx, type_tok);
    if (str_equals(type_str, UNIT_STR)) {
        return (struct type_span){ .type = { .kind = TY_UNIT }, .span = span };
    }
    if (str_equals(type_str, NEVER_STR)) {
        return (struct type_span){ .type = { .kind = TY_NEVER }, .span = span };
    }
    assert(type_str.len > 0);
    if (type_str.len == 2 && (type_str.ptr[0] == 'i' || type_str.ptr[0] == 'u')
    && type_str.ptr[1] >= '0' && type_str.ptr[1] <= '9') {
        bool sgnd = type_str.ptr[0] == 'i';
        uint8_t bits = type_str.ptr[1] - '0';
        if (bits > 0 || !sgnd) {
            return (struct type_span){ .type = { .kind = TY_INT, .sgnd = sgnd, .bits = bits }, .span = span };
        }
    }
    if (type_str.len == 3 && (type_str.ptr[0] == 'i' || type_str.ptr[0] == 'u')
    && type_str.ptr[1] >= '1' && type_str.ptr[1] <= '6'
    && type_str.ptr[2] >= '0' && type_str.ptr[2] <= '9') {
        bool sgnd = type_str.ptr[0] == 'i';
        uint8_t bits = (type_str.ptr[1] - '0') * 10 + (type_str.ptr[2] - '0');
        if (bits <= 64) {
            return (struct type_span){ .type = { .kind = TY_INT, .sgnd = sgnd, .bits = bits }, .span = span };
        }
    }
    fail_undefined_type(ctx, type_tok);
    return (struct type_span){ 0 };
}

static void
compile_fn_def(struct context *ctx, struct token *name_tok) {
    // EBNF:
    // fn_def = "fn" "(" ")" "->" type_expr block ;
    // type_expr = ident ;
    struct token fn_tok;
    struct token params_end_tok;
    struct type_span declared_return_tysp = { 0 };
    take_token_expect_kind(ctx, &fn_tok, TOKEN_KEYWORD_FN);
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
    // TODO: create type and symbol list for parameters
    take_token_expect_kind(ctx, &params_end_tok, TOKEN_RIGHT_PAREN);
    if (peek_token_kind(ctx) == TOKEN_RIGHT_ARROW) {
        struct token right_arrow_tok;
        take_token_expect_kind(ctx, &right_arrow_tok, TOKEN_RIGHT_ARROW);
        declared_return_tysp = parse_type(ctx);
    } else {
        declared_return_tysp.type.kind = TY_UNIT;
        declared_return_tysp.span.start = token_end(ctx, params_end_tok);
        declared_return_tysp.span.end = peek_token_loc(ctx);
    }

    struct span fn_span = (struct span){ .start = fn_tok.loc, .end = declared_return_tysp.span.end };
    struct type_node *fn_arg_list = alloc_type_node(ctx);
    *fn_arg_list = (struct type_node){ .next = NULL, .tysp = declared_return_tysp };
    struct type_span fn_tysp = { .type = { .kind = TY_CONST_FN, .arg_list = fn_arg_list }, .span = fn_span };
    struct symbol fn_sym = { .ident_tok = *name_tok, .tysp = fn_tysp };
    // TODO: check for an existing definition or a conflicting forward declaration
    push_symbol(ctx, fn_sym);
    ctx->local_label_count = 0;
    push_scope(ctx);
    // TODO: push parameter symbols
    emit_fn_prologue(ctx, token_str(ctx, *name_tok));
    struct type_span block_return_tysp = compile_block(ctx, false);
    block_return_tysp.type = require_subtype_coerce(ctx, block_return_tysp, declared_return_tysp);
    emit_fn_epilogue(ctx, block_return_tysp.type.kind);
    pop_scope(ctx);
}

static struct type_span
compile_let_expr(struct context *ctx) {
    // EBNF: let_expr = "let" ident "=" expr ;
    struct token let_tok;
    take_token_expect_kind(ctx, &let_tok, TOKEN_KEYWORD_LET);
    struct token name_tok;
    take_token_expect_kind(ctx, &name_tok, TOKEN_IDENT);
    if (str_starts_with(token_str(ctx, name_tok), BUILTIN_STR)) {
        fail_reserved_ident(ctx, name_tok);
    }
    // TODO: check for shadowed var in the same scope, and drop their value (and reuse the stack slot?)
    take_token_expect_kind(ctx, NULL, TOKEN_EQUAL);
    struct type_span expr_tysp = compile_expr(ctx, 0);
    struct span let_span = { .start = let_tok.loc, .end = expr_tysp.span.end };
    struct type_span return_tysp = { .type = { .kind = TY_UNIT }, .span = let_span };
    if (expr_tysp.type.kind == TY_NEVER) {
        return_tysp.type = (struct type){ .kind = TY_NEVER };
    } else if (str_equals(token_str(ctx, name_tok), WILDCARD_STR)) {
        emit_drop_type(ctx, expr_tysp.type.kind);
    } else  {
        switch (expr_tysp.type.kind) {
        case TY_UNIT:
        case TY_NEVER:
        case TY_CONST_FN: assert(!"not implemented"); // TODO: implement
        case TY_CONST_INT: {
            struct symbol var_sym = { .ident_tok = name_tok, .tysp = expr_tysp };
            push_symbol(ctx, var_sym);
            break;
        }
        case TY_INT: {
            ctx->stack_offset -= 16;
            struct symbol var_sym = { .ident_tok = name_tok, .tysp = expr_tysp, .var_stack_offset = ctx->stack_offset };
            push_symbol(ctx, var_sym);
            break;
        }
        }
    }
    return return_tysp;
}

static struct type_span
compile_var_expr(struct context *ctx) {
    // EBNF: var_expr = ident ;
    struct token name_tok;
    take_token_expect_kind(ctx, &name_tok, TOKEN_IDENT);
    struct str name_str = token_str(ctx, name_tok);
    struct symbol_node *sym_node = find_symbol_node(ctx, name_str);
    if (sym_node == NULL) { fail_undefined_var(ctx, name_tok); }
    struct type result_type = sym_node->sym.tysp.type;
    switch (result_type.kind) {
    case TY_UNIT:
    case TY_NEVER:
    case TY_CONST_FN: assert(!"not implemented"); // TODO: implement
    case TY_CONST_INT: break;
    case TY_INT: emit_load_var(ctx, sym_node->sym.var_stack_offset); break;
    }
    return (struct type_span){ .type = result_type, .span = token_span(ctx, name_tok) };
}

static struct type_span
compile_block(struct context *ctx, bool create_scope) {
    // EBNF: block = "{" { expr } "}" ;
    if (create_scope) { push_scope(ctx); }
    struct token left_brace_tok;
    take_token_expect_kind(ctx, &left_brace_tok, TOKEN_LEFT_BRACE);
    struct type_span result_tysp = { .type = { .kind = TY_UNIT } };
    struct token right_brace_tok;
    bool was_dead_code = ctx->is_dead_code;
    if (peek_token_kind(ctx) != TOKEN_RIGHT_BRACE) {
        result_tysp = compile_expr(ctx, 0);
        while (peek_token_kind(ctx) != TOKEN_RIGHT_BRACE) {
            if (result_tysp.type.kind == TY_NEVER) {
                // TODO: warn about dead code
                ctx->is_dead_code = true;
                while (peek_token_kind(ctx) != TOKEN_RIGHT_BRACE) {
                    compile_expr(ctx, 0);
                }
            } else {
                emit_drop_type(ctx, result_tysp.type.kind);
                result_tysp = compile_expr(ctx, 0);
            }
        }
        take_token_expect_kind(ctx, &right_brace_tok, TOKEN_RIGHT_BRACE);
    } else {
        take_token_expect_kind(ctx, &right_brace_tok, TOKEN_RIGHT_BRACE);
        result_tysp.span = (struct span){.start = left_brace_tok.loc, .end = token_end(ctx, right_brace_tok)};
    }
    ctx->is_dead_code = was_dead_code;
    // TODO: chain of spans for how a type was derived?
    // result_tysp.span = (struct span){.start = left_brace_tok.loc, .end = token_end(ctx, right_brace_tok)};
    if (create_scope) { pop_scope(ctx); }
    return result_tysp;
}

static struct type
eval_const_int_binary_op(struct context *ctx, enum BINARY_OP op, struct type_span left, struct type_span right, struct span span) {
    assert(left.type.kind == TY_CONST_INT && right.type.kind == TY_CONST_INT);
    struct type result_type = { .kind = TY_CONST_INT };
    switch (op) {
    case BINARY_OP_TYPE_ANNO: assert(!"unreachable");
    case BINARY_OP_MUL:
        if (__builtin_mul_overflow(left.type.value, right.type.value, &result_type.value)) {
            fail_const_int_out_of_range(ctx, span);
        }
        break;
    case BINARY_OP_DIV:
        if (right.type.value == 0) { fail_const_int_div_by_zero(ctx, right.span); }
        result_type.value = left.type.value / right.type.value;
        break;
    case BINARY_OP_REM:
        if (right.type.value == 0) { fail_const_int_rem_by_zero(ctx, right.span); }
        result_type.value = left.type.value % right.type.value;
        break;
    case BINARY_OP_ADD:
        if (__builtin_add_overflow(left.type.value, right.type.value, &result_type.value)) {
            fail_const_int_out_of_range(ctx, span);
        }
        break;
    case BINARY_OP_SUB:
        if (__builtin_sub_overflow(left.type.value, right.type.value, &result_type.value)) {
            fail_const_int_out_of_range(ctx, span);
        }
        break;
    case BINARY_OP_SHL:
        // TODO: check for overflow
        result_type.value = left.type.value << right.type.value;
        break;
    case BINARY_OP_SHR: result_type.value = left.type.value >> right.type.value; break;
    case BINARY_OP_LT: result_type.value = left.type.value < right.type.value; break;
    case BINARY_OP_LE: result_type.value = left.type.value <= right.type.value; break;
    case BINARY_OP_GT: result_type.value = left.type.value > right.type.value; break;
    case BINARY_OP_GE: result_type.value = left.type.value >= right.type.value; break;
    case BINARY_OP_EQ: result_type.value = left.type.value == right.type.value; break;
    case BINARY_OP_NE: result_type.value = left.type.value != right.type.value; break;
    case BINARY_OP_BITWISE_AND: result_type.value = left.type.value & right.type.value; break;
    case BINARY_OP_BITWISE_XOR: result_type.value = left.type.value ^ right.type.value; break;
    case BINARY_OP_BITWISE_OR: result_type.value = left.type.value | right.type.value; break;
    case BINARY_OP_LOGICAL_AND: assert(!"unreachable");
    case BINARY_OP_LOGICAL_OR: assert(!"unreachable");
    }
    return result_type;
}

static struct type
calc_binary_op_type(struct context *ctx, enum BINARY_OP op, struct type_span left, struct type_span right) {
    assert(left.type.kind == TY_INT && right.type.kind == TY_INT);
    switch (op) {
    case BINARY_OP_TYPE_ANNO: assert(!"unreachable");
    case BINARY_OP_MUL:
    case BINARY_OP_DIV:
    case BINARY_OP_REM:
    case BINARY_OP_ADD:
    case BINARY_OP_SUB:
        if (left.type.bits != 64) { fail_invalid_type_for_arithmetic(ctx, op, left); }
        if (right.type.bits != 64) { fail_invalid_type_for_arithmetic(ctx, op, right); }
        if (left.type.sgnd != right.type.sgnd || left.type.bits != right.type.bits) {
            fail_incompatible_binary_op_types(ctx, op, left, right);
        }
        return left.type;
    case BINARY_OP_SHL:
    case BINARY_OP_SHR:
        return left.type;
    case BINARY_OP_LT:
    case BINARY_OP_LE:
    case BINARY_OP_GT:
    case BINARY_OP_GE:
    case BINARY_OP_EQ:
    case BINARY_OP_NE:
        return (struct type){ .kind = TY_INT, .sgnd = false, .bits = 1 };
    case BINARY_OP_BITWISE_AND:
    case BINARY_OP_BITWISE_XOR:
    case BINARY_OP_BITWISE_OR:
        if (left.type.bits != right.type.bits) {
            fail_incompatible_binary_op_types(ctx, op, left, right);
        }
        return left.type;
    case BINARY_OP_LOGICAL_AND: assert(!"unreachable");
    case BINARY_OP_LOGICAL_OR: assert(!"unreachable");
    }
}

static struct type_span
compile_expr(struct context *ctx, int min_binding_power) {
    // EBNF: expr = block | if_expr | let_expr | "(" expr ")" | int_literal | fn_call | var_expr | op_expr ;
    struct type_span left_tysp = { 0 };
    struct type_span right_tysp = { 0 };
    switch (peek_token_kind(ctx)) {
    case TOKEN_RIGHT_BRACE: return (struct type_span){ .type = { .kind = TY_UNIT }, .span = peek_token_span(ctx) };
    case TOKEN_LEFT_BRACE: left_tysp = compile_block(ctx, true); break;
    case TOKEN_KEYWORD_IF: left_tysp = compile_if_expr(ctx); break;
    case TOKEN_KEYWORD_LET: left_tysp = compile_let_expr(ctx); break;
    case TOKEN_LEFT_PAREN:
        take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
        left_tysp = compile_expr(ctx, 0);
        take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_PAREN);
        break;
    case TOKEN_INT_LITERAL: left_tysp = compile_int_literal(ctx); break;
    case TOKEN_IDENT:
        if (peek_token_kind_at(ctx, 1) == TOKEN_LEFT_PAREN) {
            left_tysp = compile_fn_call(ctx);
        } else {
            left_tysp = compile_var_expr(ctx);
        }
        break;
    case TOKEN_MINUS:
    case TOKEN_TILDE:
    case TOKEN_EXCLAMATION:
        left_tysp = compile_prefix_op_expr(ctx);
        break;
    default: fail_expected(ctx, "an expression");
    }
    // EBNF: infix_op = "*" | "/" | "%" | "+" | "-" | "<<" | ">>"
    //                | "<" | "<=" | ">" | ">=" | "==" | "!=" | "&&"  | "||" ;
    for (;;) {
        enum BINARY_OP op;
        switch (peek_token_kind(ctx)) {
        case TOKEN_COLON:               op = BINARY_OP_TYPE_ANNO;   break;
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
        default: return left_tysp;
        }
        if (BINARY_OP_LEFT_BINDING_POWERS[op] < min_binding_power) {
            break;
        }
        struct token op_tok;
        take_token(ctx, &op_tok);
        struct type result_type = { 0 };
        switch (op) {
        case BINARY_OP_LOGICAL_OR: {
            require_subtype_of_int(ctx, left_tysp);
            if (left_tysp.type.kind == TY_NEVER) {
                bool was_dead_code = ctx->is_dead_code;
                ctx->is_dead_code = true;
                right_tysp = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
                ctx->is_dead_code = was_dead_code;
                // TODO: warn about dead code
                require_subtype_of_int(ctx, right_tysp);
                result_type = (struct type){ .kind = TY_NEVER };
            } else if (left_tysp.type.kind == TY_CONST_INT) {
                if (left_tysp.type.value != 0) {
                    // NOTE: LHS is statically true; compile RHS as dead code
                    bool was_dead_code = ctx->is_dead_code;
                    ctx->is_dead_code = true;
                    right_tysp = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
                    ctx->is_dead_code = was_dead_code;
                    // TODO: warn about dead code
                    require_subtype_of_int(ctx, right_tysp);
                    result_type = (struct type){ .kind = TY_CONST_INT, .value = 1 };
                } else {
                    // NOTE: LHS is statically false; compile the RHS
                    right_tysp = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
                    require_subtype_of_int(ctx, right_tysp);
                    if (right_tysp.type.kind == TY_NEVER) {
                        result_type = (struct type){ .kind = TY_NEVER };
                    } else if (right_tysp.type.kind == TY_CONST_INT) {
                        result_type = (struct type){ .kind = TY_CONST_INT, .value = (right_tysp.type.value != 0) };
                    } else if (right_tysp.type.kind == TY_INT) {
                        emit_not_equal_zero(ctx);
                        result_type = (struct type){ .kind = TY_INT, .sgnd = false, .bits = 1 };
                    } else {
                        assert(!"unreachable");
                    }
                }
            } else if (left_tysp.type.kind == TY_INT) {
                size_t true_label = ctx->local_label_count++;
                size_t done_label = ctx->local_label_count++;
                emit_local_forward_branch_if_nonzero(ctx, true_label);
                right_tysp = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
                require_subtype_of_int(ctx, right_tysp);
                if (right_tysp.type.kind == TY_NEVER
                || (right_tysp.type.kind == TY_CONST_INT && right_tysp.type.value != 0)) {
                    // NOTE: RHS is statically true (or never); only emit the true case code
                    emit_local_label(ctx, true_label);
                    emit_push_int(ctx, 1);
                } else if (right_tysp.type.kind == TY_CONST_INT && right_tysp.type.value == 0) {
                    // NOTE: RHS is statically false; skip the RHS's branch_if_nonzero
                    emit_push_int(ctx, 0);
                    emit_local_forward_branch(ctx, done_label);
                    emit_local_label(ctx, true_label);
                    emit_push_int(ctx, 1);
                    emit_local_label(ctx, done_label);
                } else if (right_tysp.type.kind == TY_INT) {
                    emit_local_forward_branch_if_nonzero(ctx, true_label);
                    emit_push_int(ctx, 0);
                    emit_local_forward_branch(ctx, done_label);
                    emit_local_label(ctx, true_label);
                    emit_push_int(ctx, 1);
                    emit_local_label(ctx, done_label);
                } else {
                    assert(!"unreachable");
                }
                result_type = (struct type){ .kind = TY_INT, .sgnd = false, .bits = 1 };
            } else {
                assert(!"unreachable");
            }
            break;
        }
        case BINARY_OP_LOGICAL_AND: {
            require_subtype_of_int(ctx, left_tysp);
            if (left_tysp.type.kind == TY_NEVER) {
                bool was_dead_code = ctx->is_dead_code;
                ctx->is_dead_code = true;
                right_tysp = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
                ctx->is_dead_code = was_dead_code;
                // TODO: warn about dead code
                require_subtype_of_int(ctx, right_tysp);
                result_type = (struct type){ .kind = TY_NEVER };
            } else if (left_tysp.type.kind == TY_CONST_INT) {
                if (left_tysp.type.value == 0) {
                    // NOTE: LHS is statically false; compile RHS as dead code
                    bool was_dead_code = ctx->is_dead_code;
                    ctx->is_dead_code = true;
                    right_tysp = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
                    ctx->is_dead_code = was_dead_code;
                    // TODO: warn about dead code
                    require_subtype_of_int(ctx, right_tysp);
                    result_type = (struct type){ .kind = TY_CONST_INT, .value = 0 };
                } else {
                    // NOTE: LHS is statically true; compile the RHS
                    right_tysp = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
                    require_subtype_of_int(ctx, right_tysp);
                    if (right_tysp.type.kind == TY_NEVER) {
                        result_type = (struct type){ .kind = TY_NEVER };
                    } else if (right_tysp.type.kind == TY_CONST_INT) {
                        result_type = (struct type){ .kind = TY_CONST_INT, .value = (right_tysp.type.value != 0) };
                    } else if (right_tysp.type.kind == TY_INT) {
                        // NOTE: RHS is a non-const int; convert to boolean
                        emit_not_equal_zero(ctx);
                        result_type = (struct type){ .kind = TY_INT, .sgnd = false, .bits = 1 };
                    } else {
                        assert(!"unreachable");
                    }
                }
            } else if (left_tysp.type.kind == TY_INT) {
                size_t false_label = ctx->local_label_count++;
                size_t done_label = ctx->local_label_count++;
                emit_local_forward_branch_if_zero(ctx, false_label);
                right_tysp = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
                require_subtype_of_int(ctx, right_tysp);
                if (right_tysp.type.kind == TY_NEVER
                || (right_tysp.type.kind == TY_CONST_INT && right_tysp.type.value == 0)) {
                    // NOTE: RHS is statically false (or never); only emit the false case code
                    emit_local_label(ctx, false_label);
                    emit_push_int(ctx, 0);
                } else if (right_tysp.type.kind == TY_CONST_INT && right_tysp.type.value != 0) {
                    // NOTE: RHS is statically true; skip the RHS's branch_if_zero
                    emit_push_int(ctx, 1);
                    emit_local_forward_branch(ctx, done_label);
                    emit_local_label(ctx, false_label);
                    emit_push_int(ctx, 0);
                    emit_local_label(ctx, done_label);
                } else if (right_tysp.type.kind == TY_INT) {
                    emit_local_forward_branch_if_zero(ctx, false_label);
                    emit_push_int(ctx, 1);
                    emit_local_forward_branch(ctx, done_label);
                    emit_local_label(ctx, false_label);
                    emit_push_int(ctx, 0);
                    emit_local_label(ctx, done_label);
                } else {
                    assert(!"unreachable");
                }
                result_type = (struct type){ .kind = TY_INT, .sgnd = false, .bits = 1 };
            } else {
                assert(!"unreachable");
            }
            break;
        }
        case BINARY_OP_TYPE_ANNO: {
            right_tysp = parse_type(ctx);
            result_type = require_subtype_coerce(ctx, left_tysp, right_tysp);
            break;
        }
        default: {
            require_subtype_of_int(ctx, left_tysp);
            bool was_dead_code = ctx->is_dead_code;
            if (left_tysp.type.kind == TY_NEVER) {
                ctx->is_dead_code = true;
            }
            right_tysp = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
            ctx->is_dead_code = was_dead_code;
            require_subtype_of_int(ctx, right_tysp);
            struct span result_span = join_spans(left_tysp.span, right_tysp.span);
            if (left_tysp.type.kind == TY_NEVER) {
                // TODO: warn about dead code
                result_type = (struct type){ .kind = TY_NEVER };
            } else if (right_tysp.type.kind == TY_NEVER) {
                emit_drop_type(ctx, left_tysp.type.kind);
                result_type = (struct type){ .kind = TY_NEVER };
            } else if (left_tysp.type.kind == TY_CONST_INT && right_tysp.type.kind == TY_CONST_INT) {
                result_type = eval_const_int_binary_op(ctx, op, left_tysp, right_tysp, result_span);
            } else {
                bool swap = false;
                // TODO: do we need to drop anything if one or both are TY_NEVER?
                if (left_tysp.type.kind == TY_CONST_INT) {
                    left_tysp.type = require_subtype_coerce(ctx, left_tysp, right_tysp);
                    swap = true;
                }
                if (right_tysp.type.kind == TY_CONST_INT) {
                    right_tysp.type = require_subtype_coerce(ctx, right_tysp, left_tysp);
                }
                result_type = calc_binary_op_type(ctx, op, left_tysp, right_tysp);
                emit_binary_op(ctx, op, left_tysp.type, right_tysp.type, swap);
            }
            break;
        }
        }
        struct type_span result_tysp = {
            .type = result_type,
            .span = join_spans(left_tysp.span, right_tysp.span)};
        left_tysp = result_tysp;
    }
    return left_tysp;
}

static struct type_span
compile_if_expr(struct context *ctx) {
    // EBNF: if_expr = "if" expr block_expr { "else" "if" expr block_expr } [ "else" block_expr ] ;
    struct token if_tok;
    take_token_expect_kind(ctx, &if_tok, TOKEN_KEYWORD_IF);
    struct type_span condition_tysp = compile_expr(ctx, 0);
    require_subtype_of_int(ctx, condition_tysp);
    bool then_is_dead_code = condition_tysp.type.kind == TY_NEVER
        || (condition_tysp.type.kind == TY_CONST_INT && condition_tysp.type.value == 0);
    bool else_is_dead_code = condition_tysp.type.kind == TY_NEVER
        || (condition_tysp.type.kind == TY_CONST_INT && condition_tysp.type.value != 0);
    // TODO: warn about dead code
    size_t false_label = ctx->local_label_count++;
    size_t done_label = ctx->local_label_count++;
    bool was_dead_code = ctx->is_dead_code;
    ctx->is_dead_code = was_dead_code | then_is_dead_code | else_is_dead_code;
    emit_local_forward_branch_if_zero(ctx, false_label);
    ctx->is_dead_code = was_dead_code | then_is_dead_code;
    struct type_span then_tysp = compile_block(ctx, true);
    ctx->is_dead_code = was_dead_code | then_is_dead_code | else_is_dead_code;
    emit_local_forward_branch(ctx, done_label);
    emit_local_label(ctx, false_label);
    ctx->is_dead_code = was_dead_code | else_is_dead_code;
    struct type_span result_tysp = then_tysp;
    if (peek_token_kind(ctx) == TOKEN_KEYWORD_ELSE) {
        struct token else_tok;
        take_token_expect_kind(ctx, &else_tok, TOKEN_KEYWORD_ELSE);
        struct type_span else_tysp = { 0 };
        if (peek_token_kind(ctx) == TOKEN_KEYWORD_IF) {
            else_tysp = compile_if_expr(ctx);
        } else {
            else_tysp = compile_block(ctx, true);
        }
        if (then_tysp.type.kind == TY_NEVER) {
            result_tysp = else_tysp;
        } else {
            // TODO: improve error message if not a subtype
            require_subtype_coerce(ctx, else_tysp, then_tysp);
        }
    } else {
        if (!is_subtype(then_tysp.type, (struct type){ .kind = TY_UNIT })) {
            fail_if_without_else_non_unit(ctx, then_tysp);
        }
        result_tysp.type = (struct type){ .kind = TY_UNIT };
        result_tysp.span = (struct span){ .start = if_tok.loc, .end = then_tysp.span.end };
    }
    ctx->is_dead_code = was_dead_code | then_is_dead_code | else_is_dead_code;
    emit_local_label(ctx, done_label);
    ctx->is_dead_code = was_dead_code;
    if (condition_tysp.type.kind == TY_NEVER) {
        result_tysp = condition_tysp;
    }
    return result_tysp;
}

static struct type
eval_const_int_unary_op(struct context *ctx, enum UNARY_OP op, struct type operand, struct span span) {
    assert(operand.kind == TY_CONST_INT);
    struct type result_type = { .kind = TY_CONST_INT };
    switch (op) {
    case UNARY_OP_NEG:
        if (__builtin_sub_overflow(0, operand.value, &result_type.value)) {
            fail_const_int_out_of_range(ctx, span);
        }
        break;
    case UNARY_OP_BITWISE_NOT:
        result_type.value = ~operand.value;
        break;
    case UNARY_OP_LOGICAL_NOT:
        result_type.value = !operand.value;
        break;
    }
    return result_type;
}

static struct type
calc_unary_op_type(struct context *ctx, enum UNARY_OP op, struct type_span tysp) {
    assert(tysp.type.kind == TY_INT);
    switch (op) {
    case UNARY_OP_NEG:
    case UNARY_OP_BITWISE_NOT: return tysp.type;
    case UNARY_OP_LOGICAL_NOT: return (struct type){ .kind = TY_INT, .sgnd = false, .bits = 1 };
    }
}

static struct type_span
compile_prefix_op_expr(struct context *ctx) {
    // EBNF: prefix_op = "-" | "~" | "!" ;
    enum UNARY_OP op;
    switch (peek_token_kind(ctx)) {
    case TOKEN_MINUS:       op = UNARY_OP_NEG;         break;
    case TOKEN_TILDE:       op = UNARY_OP_BITWISE_NOT; break;
    case TOKEN_EXCLAMATION: op = UNARY_OP_LOGICAL_NOT; break;
    default: assert(!"unreachable");
    }
    struct token op_tok;
    take_token(ctx, &op_tok);
    struct type_span expr_tysp = compile_expr(ctx, UNARY_OP_RIGHT_BINDING_POWERS[UNARY_OP_NEG]);
    require_subtype_of_int(ctx, expr_tysp);
    struct span result_span = { .start = op_tok.loc, .end = expr_tysp.span.end };
    struct type result_type = { 0 };
    if (expr_tysp.type.kind == TY_NEVER) {
        result_type = (struct type){ .kind = TY_NEVER };
    } else if (expr_tysp.type.kind == TY_CONST_INT) {
        result_type = eval_const_int_unary_op(ctx, op, expr_tysp.type, result_span);
    } else if (expr_tysp.type.kind == TY_INT) {
        result_type = calc_unary_op_type(ctx, op, expr_tysp);
        emit_unary_op(ctx, op);
    } else {
        assert(!"unreachable");
    }
    return (struct type_span){ .type = result_type, .span = result_span };
}

static struct type_span
compile_fn_call(struct context *ctx) {
    // EBNF: fn_call = ident "(" ")" ;
    struct token name_tok;
    take_token_expect_kind(ctx, &name_tok, TOKEN_IDENT);
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
    // TODO: collect arguments
    struct token right_paren_tok;
    take_token_expect_kind(ctx, &right_paren_tok, TOKEN_RIGHT_PAREN);
    struct span fn_call_span = { .start = name_tok.loc, .end = token_end(ctx, right_paren_tok) };
    struct str name_str = token_str(ctx, name_tok);
    if (str_starts_with(name_str, BUILTIN_STR)) {
        if (str_equals(name_str, BUILTIN_TRAP_STR)) {
            emit_builtin_trap(ctx);
            return (struct type_span){ .type = { .kind = TY_NEVER }, .span = fn_call_span};
        }
        fail_unknown_builtin(ctx, name_tok);
    }
    struct symbol_node *fn_sym_node = find_symbol_node(ctx, name_str);
    if (fn_sym_node == NULL) { fail_undefined_fn(ctx, name_tok); }
    struct type_span fn_tysp = fn_sym_node->sym.tysp;
    if (fn_tysp.type.kind != TY_CONST_FN) { fail_fn_call_non_fn(ctx, name_tok, fn_sym_node->sym.ident_tok, fn_tysp); }
    // TODO: check argument types against parameter types
    struct type_node *return_type_node = fn_tysp.type.arg_list;
    for (; return_type_node->next != NULL; return_type_node = return_type_node->next) {}
    struct type_span return_tysp = return_type_node->tysp;
    emit_fn_call(ctx, ctx->src + name_tok.loc.idx, name_tok.len, return_tysp.type.kind);
    return_tysp.span = fn_call_span;
    return return_tysp;
}

static __int128
parse_int_literal(struct context *ctx, struct token tok) {
    __int128 value = 0;
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
                if (__builtin_mul_overflow(value, 2, &value)) {
                    fail_const_int_out_of_range(ctx, token_span(ctx, tok));
                }
                char digit = src[i] - '0';
                assert(digit < 2);
                if (__builtin_add_overflow(value, digit, &value)) {
                    fail_const_int_out_of_range(ctx, token_span(ctx, tok));
                }
            }
            break;
        case 'o':
            // octal literal
            for (i += 2; i < len; i++) {
                if (src[i] == '_') { continue; }
                if (__builtin_mul_overflow(value, 8, &value)) {
                    fail_const_int_out_of_range(ctx, token_span(ctx, tok));
                }
                char digit = src[i] - '0';
                assert(digit < 8);
                if (__builtin_add_overflow(value, digit, &value)) {
                    fail_const_int_out_of_range(ctx, token_span(ctx, tok));
                }
            }
            break;
        case 'x':
            // hexadecimal literal
            for (i += 2; i < len; i++) {
                if (src[i] == '_') { continue; }
                if (__builtin_mul_overflow(value, 16, &value)) {
                    fail_const_int_out_of_range(ctx, token_span(ctx, tok));
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
                if (__builtin_add_overflow(value, digit, &value)) {
                    fail_const_int_out_of_range(ctx, token_span(ctx, tok));
                }
            }
            break;
        default: assert(!"unreachable");
        }
    } else {
        // decimal literal
        for (; i < len; i++) {
            if (src[i] == '_') { continue; }
            if (__builtin_mul_overflow(value, 10, &value)) {
                fail_const_int_out_of_range(ctx, token_span(ctx, tok));
            }
            char digit = src[i] - '0';
            assert(digit < 10);
            if (__builtin_add_overflow(value, digit, &value)) {
                fail_const_int_out_of_range(ctx, token_span(ctx, tok));
            }
        }
    }
    return value;
}

static struct type_span
compile_int_literal(struct context *ctx) {
    struct token tok;
    take_token_expect_kind(ctx, &tok, TOKEN_INT_LITERAL);
    __int128 value = parse_int_literal(ctx, tok);
    struct type_span result_tysp = {
        .type = { .kind = TY_CONST_INT, .value = value },
        .span = token_span(ctx, tok) };
    return result_tysp;
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

static char tmp_output_file_path[] = "/tmp/spc-XXXXXX";
static int tmp_output_fd = -1;
static FILE *tmp_output_file = NULL;

void cleanup_tmp_output(void) {
    if (tmp_output_file != NULL) {
        fclose(tmp_output_file);
    } else if (tmp_output_fd != -1) {
        close(tmp_output_fd);
    }
    if (tmp_output_file_path[0] != '\0') {
        remove(tmp_output_file_path);
    }
}

int
main(int argc, char *argv[]) {
    // Parse command line options
    int opt;
    char output_file_path_buf[PATH_MAX + 1];
    char *output_file_path = NULL;
    size_t max_heap_size = 1024 * 1024 * 1024; // 1 GiB
    while ((opt = getopt(argc, argv, "o:m:")) != -1) {
        switch(opt) {
            case 'o':
                output_file_path = optarg;
                break;
            case 'm':
                max_heap_size = 0;
                for (size_t i = 0; optarg[i] != '\0'; i++) {
                    if (optarg[i] < '0' || optarg[i] > '9') {
                        fprintf(stderr, "error: invalid number: %s\n", optarg);
                        return EXIT_FAILURE;
                    }
                    if (__builtin_mul_overflow(max_heap_size, 10, &max_heap_size)) {
                        fprintf(stderr, "error: number out of range: %s\n", optarg);
                        return EXIT_FAILURE;
                    }
                    char digit = optarg[i] - '0';
                    if (__builtin_add_overflow(max_heap_size, digit, &max_heap_size)) {
                        fprintf(stderr, "error: number out of range: %s\n", optarg);
                        return EXIT_FAILURE;
                    }
                }
                break;
            case '?':
                // getopt already prints an error message for unknown options
                fprintf(stderr, "Usage: %s [-o OUT_FILE] [-m MAX_HEAP_SIZE] FILE\n", argv[0]);
                return EXIT_FAILURE;
            default:
                assert(!"unreachable");
        }
    }
    // After option parsing, optind is the index of the first non-option argument
    if (optind >= argc) {
        fprintf(stderr, "Expected FILE argument after options\n");
        fprintf(stderr, "Usage: %s [-o OUT_FILE] FILE\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *input_file_path = argv[optind];
    if (output_file_path == NULL) {
        get_default_output_file_path(input_file_path, output_file_path_buf);
        output_file_path = output_file_path_buf;
    }

    // Open the input file
    int input_fd = open(input_file_path, O_RDONLY);
    if (input_fd == -1) {
        fprintf(stderr, "internal compiler error while opening file '%s': %s\n", input_file_path, strerror(errno));
        return EXIT_FAILURE;
    }

    // Get the input file size
    struct stat sb;
    if (fstat(input_fd, &sb) == -1) {
        fprintf(stderr, "internal compiler error while getting file size for '%s': %s\n", input_file_path, strerror(errno));
        if (close(input_fd) == -1) { fprintf(stderr, "internal compiler warning while closing file '%s': %s\n", input_file_path, strerror(errno)); }
        return EXIT_FAILURE;
    }
    size_t input_file_size = sb.st_size;
    if (input_file_size == 0) {
        fprintf(stderr, "error: empty input file '%s'\n", input_file_path);
        return EXIT_FAILURE;
    }
    // Memory-map the input file
    void *mapped_input_file = mmap(NULL, input_file_size, PROT_READ, MAP_PRIVATE, input_fd, 0);
    if (mapped_input_file == MAP_FAILED) {
        fprintf(stderr, "internal compiler error while mapping file '%s': %s\n", input_file_path, strerror(errno));
        if (close(input_fd) == -1) { fprintf(stderr, "internal compiler warning while closing file '%s': %s\n", input_file_path, strerror(errno)); }
        return EXIT_FAILURE;
    }
    if (close(input_fd) == -1) {
        fprintf(stderr, "internal compiler warning while closing file '%s': %s\n", input_file_path, strerror(errno));
        // Continue anyway
    }

    // Prepare a temporary file for the output
    tmp_output_fd = mkstemp(tmp_output_file_path);
    if (tmp_output_fd == -1) {
        fprintf(stderr, "internal compiler error while creating temporary output file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    atexit(cleanup_tmp_output);
    tmp_output_file = fdopen(tmp_output_fd, "w");
    if (tmp_output_file == NULL) {
        fprintf(stderr, "internal compiler error while opening temporary output file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Get the system's page size
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf");
        exit(EXIT_FAILURE);
    }
    // Align max_heap_size to page size
    max_heap_size = (max_heap_size / page_size) * page_size;
    if (max_heap_size == 0) {
        fprintf(stderr, "error: max heap size must be larger than the page size: %ld\n", page_size);
        return EXIT_FAILURE;
    }
    // Align max_heap_size to the commit size
    max_heap_size = (max_heap_size / HEAP_COMMIT_SIZE) * HEAP_COMMIT_SIZE;
    if (max_heap_size == 0) {
        fprintf(stderr, "error: max heap size must be larger than the commit size: %d\n", HEAP_COMMIT_SIZE);
        return EXIT_FAILURE;
    }
    // Reserve a memory block for the heap
    void *heap = mmap(NULL, max_heap_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (heap == MAP_FAILED) {
        fprintf(stderr, "internal compiler error while reserving memory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Compile the program
    struct context ctx = {
        .output_file = tmp_output_file,
        .input_file_path = input_file_path,
        .src = (char *)mapped_input_file,
        .src_len = input_file_size,
        .src_loc = { .idx = 0, .line = 1, .col = 1 },
        .heap = (uintptr_t)heap,
        .heap_end = (uintptr_t)heap,
        .commited_heap_size = 0,
        .max_heap_size = max_heap_size,
        .scope_stack = NULL,
        .free_symbol_nodes = NULL,
    };
    compile_program(&ctx);

    if (fclose(tmp_output_file) != 0) {
        fprintf(stderr, "internal compiler error while closing temporary output file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    tmp_output_file = NULL;
    tmp_output_fd = -1;

    if (rename(tmp_output_file_path, output_file_path) != 0) {
        fprintf(stderr, "internal compiler error while moving temporary output file to '%s': %s\n",
                output_file_path, strerror(errno));
        return EXIT_FAILURE;
    }
    tmp_output_file_path[0] = '\0';

    return 0;
}
