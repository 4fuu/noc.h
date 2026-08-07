# noc.h

[![CI](https://github.com/4fuu/noc.h/actions/workflows/ci.yml/badge.svg)](https://github.com/4fuu/noc.h/actions/workflows/ci.yml)

`noc.h` is a single-header toolkit for defining small, explicit, project-local
C dialects. It transforms token-aware C source into readable standard C before
the normal compiler runs.

It is not a predefined language and it is not a complete C compiler frontend.
Each project compiles its own dialect program and explicitly registers the
extensions that program accepts.

## Why sources still use `.c` and `.h`

Dialect sources keep their ordinary `.c` and `.h` suffixes. The enabled syntax
is self-contained in the project's dialect program, and every extension has an
explicit `@name` trigger. Generated files go to a separate build directory so
the original sources are never overwritten:

```text
examples/embed/app.c
        │
        ▼ project dialect
build/generated/embed_app.c
        │
        ▼ C compiler
build/embed-example
```

Do not pass an untransformed source containing dialect syntax directly to the C
compiler.

## Bootstrap and test

```console
$ cc -o nob nob.c
$ ./nob test
```

Other targets:

```console
$ ./nob example
$ ./nob describe
$ ./nob clean
```

## Define a dialect

The embed example is a complete dialect executable:

```c
#define NOC_IMPLEMENTATION
#include "noc.h"

int main(int argc, char **argv)
{
    Noc_Context noc;
    noc_context_init(&noc);
    noc_register_embed_rule(&noc, "embed");
    int result = noc_run_cli(&noc, argc, argv);
    noc_context_deinit(&noc);
    return result;
}
```

Its source remains a normal `.c` file:

```c
static const char message[] = @embed("message.txt");
```

Run `dialect --describe` to inspect every enabled rule and its declared scope,
syntax, and description.

## Token streams and cursors

`noc_tokenize` creates an owning, lossless token stream. The copied source,
path, token text, and token locations remain valid until the stream is freed:

```c
Noc_Token_Stream stream = {0};
if (!noc_tokenize(&noc, "input.c", source, source_count, &stream)) return 1;

Noc_Token_Cursor cursor;
Noc_Token_Range call;
Noc_Token_Range arguments_range;
Noc_Argument_List arguments = {0};

noc_token_cursor_init(&cursor, &stream);
noc_token_cursor_match_identifier(&cursor, "call", NULL);
noc_token_cursor_take_balanced(&cursor, "(", ")", &call, &arguments_range);
noc_parse_arguments(&stream, arguments_range, &arguments);

noc_argument_list_free(&arguments);
noc_token_stream_free(&stream);
```

Token ranges use half-open `[begin, end)` indices and can be trimmed or mapped
back to exact source slices and locations. Cursor matching is transactional: a
failed optional match does not consume trivia or tokens. Balanced capture and
argument splitting honor nested `()`, `[]`, and `{}` groups and reject mixed
mismatches such as `([)]`. General ranges may include the terminal EOF token;
argument ranges never do. Retokenization replaces a stream only on success, so
a failed update preserves all existing tokens and pointers.

## Define a custom rule

All extensions use the same registry and callback interface:

```c
static bool expand_twice(Noc_Rewriter *rw,
                         const Noc_Rule *rule,
                         void *user_data)
{
    Noc_Slice expression;
    (void)rule;
    (void)user_data;

    if (!noc_rw_capture_balanced(rw, "(", ")", &expression)) return false;
    return noc_rw_emit_cstr(rw, "((") &&
           noc_rw_emit_slice(rw, expression) &&
           noc_rw_emit_cstr(rw, ") + (") &&
           noc_rw_emit_slice(rw, expression) &&
           noc_rw_emit_cstr(rw, "))");
}

noc_register_rule(&noc, (Noc_Rule) {
    .name = "twice",
    .scope = NOC_RULE_EXPRESSION,
    .syntax = "@twice(expression)",
    .description = "Duplicate an expression and add both results.",
    .expand = expand_twice,
});
```

The rewriter API currently provides raw and trivia-skipping token lookahead,
matching and expectation helpers, balanced delimiter capture, diagnostics, and
text/C-string emitters. Ordinary source text and preprocessor directives pass
through unchanged. File output uses a uniquely created temporary file in the
destination directory and an atomic replacement, and input/output aliases are
rejected.

## Current boundary

This initial version deliberately handles explicit token-level transformations.
It does not expand C macros, build a full AST, resolve typedefs, or change the C
type system. Rules inside preprocessor directives are left untouched. More
structured statement and declaration helpers can be added without changing the
registration model.

The lexer accounts for backslash-newline splicing when recognizing comments and
preprocessor directives. Splices inside identifiers or multi-character
punctuators are not normalized for callbacks yet. C trigraphs are rejected
explicitly. Because `noc.h` does not evaluate preprocessor conditions, a rule in
an inactive `#if` branch is still visited. Expansion callbacks should preserve
physical newline counts when exact diagnostics after an expansion matter.

The staged implementation plan and current milestone status live in
[`TODO.md`](TODO.md).
