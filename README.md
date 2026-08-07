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

The test target transforms, compiles, and runs both examples. `examples/embed`
shows a built-in expression rule with a file dependency; `examples/rules` is a
single project dialect covering expression, statement, declaration, and
attribute scopes. Both dialect inputs and applications use ordinary `.c` files.

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

## IDE metadata header

`noc_generate_ide_metadata_header` serializes the same registry into a
standalone C header for editor extensions, completion generators, and other
tooling. The output records a schema version, the `noc.h` version, dialect name,
rule count, and each rule's name, numeric/string scope, syntax, and description:

```c
Noc_Ide_Metadata_Options options = {
    .include_guard = "MY_DIALECT_IDE_H",
    .macro_prefix = "MY_DIALECT_IDE",
    .dialect_name = "my-project",
};
Noc_Buffer header = {0};

if (!noc_generate_ide_metadata_header(&noc, &options, &header)) return 1;
fwrite(header.items, 1, header.count, stdout);
noc_buffer_free(&header);
```

The generic dialect CLI exposes the default form directly:

```console
$ build/rules-dialect --ide-metadata build/generated/ide/rules_metadata.h
```

Options may be `NULL` or zero-initialized for deterministic defaults;
`omit_descriptions` reduces the artifact when descriptions are unnecessary.
The include guard and macro prefix must be C identifiers, arbitrary metadata is
escaped as C strings, and generation replaces the destination buffer only on
success. This metadata enables integrations to discover the dialect, but a
header alone cannot make an ordinary C parser understand `@name` syntax;
indexers should consume transformed source/header overlays.

### clangd and transformed overlays

The [`examples/ide`](examples/ide) build demonstrates the complete indexing
shape. Its ordinary [`math.h`](examples/ide/math.h) contains dialect syntax.
The rules dialect transforms both that header and `app.c` into the same mirrored
output directory, generates `rules_metadata.h`, then compiles and runs the
overlay as standard C. The metadata output is also checked byte-for-byte against
[`tests/golden/rules_metadata.h`](tests/golden/rules_metadata.h).

For clangd, generate a mirrored tree such as `build/generated/ide`, keep source
and header relative paths aligned, and make `compile_commands.json` name the
generated `.c` translation units rather than originals containing `@` syntax.
Put the generated include root before the original include root:

```json
{
  "directory": "/absolute/path/to/project",
  "file": "/absolute/path/to/project/build/generated/ide/app.c",
  "arguments": [
    "cc", "-std=c11",
    "-I/absolute/path/to/project/build/generated/ide",
    "-c", "/absolute/path/to/project/build/generated/ide/app.c"
  ]
}
```

Default `#line` directives point diagnostics from overlays back to the ordinary
`.c`/`.h` dialect paths. Regenerate overlays and the compilation database when
sources, registered rules, dialect binaries, or reported dependencies change.
Editors still open original sources for authoring; completion and semantic
indexing come from the generated mirror. Tools that cannot associate the mirror
with originals may expose generated paths for navigation, which is a clangd
client integration limitation rather than something a metadata header can fix.

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

## Lossless syntax trees

`noc_syntax_tree_build` turns a token stream into a lightweight, lossless tree
without pretending to perform C semantic analysis. The root covers the complete
source except EOF, token nodes retain whitespace and comments, and group nodes
cover their opening and closing `()`, `[]`, or `{}` tokens:

```c
Noc_Syntax_Tree tree = {0};
if (!noc_syntax_tree_build(&noc, &stream, &tree)) return 1;

for (size_t node = noc_syntax_root(&tree);
     node != NOC_SYNTAX_NONE;
     node = noc_syntax_next_preorder(&tree, node)) {
    const Noc_Syntax_Node *syntax = noc_syntax_node(&tree, node);
    Noc_Slice exact_source = noc_syntax_source(&tree, node);
    printf("%s: ", noc_syntax_kind_name(syntax->kind));
    fwrite(exact_source.data, 1, exact_source.count, stdout);
    fputc('\n', stdout);
}

noc_syntax_tree_free(&tree);
```

Nodes are referred to by stable indices and expose parent, child, sibling,
preorder, inner-range, source, location, and token lookup helpers. A group range
includes its delimiters; `noc_syntax_inner_range` excludes them. Tree builds are
transactional: malformed delimiters diagnose their exact source location and
leave any prior tree intact. Trees borrow their token stream and become invalid
after that stream is freed or successfully retokenized; failed retokenization
preserves both the stream and its trees.

## Lightweight C structure analysis

`noc_c_translation_unit_build` discovers top-level declarations and ordinary
prototype-style function definitions without performing macro expansion or
typedef resolution. Each item contains exact token ranges for the complete
item and signature, plus a best-effort name token and parameter/body ranges:

```c
Noc_C_Translation_Unit unit = {0};
if (!noc_c_translation_unit_build(&noc, &tree, &unit)) return 1;

for (size_t i = 0; i < unit.count; ++i) {
    const Noc_C_External_Item *item = noc_c_external_item(&unit, i);
    printf("%s: %s\n",
           noc_c_external_kind_name(item->kind),
           noc_c_declaration_kind_name(item->declaration_kind));

    if (item->parameters.begin != NOC_TOKEN_INDEX_NONE) {
        Noc_C_Parameter_List parameters = {0};
        if (noc_c_parse_parameters(unit.stream, item->parameters, &parameters)) {
            /* Parameter ranges and best-effort name tokens are now available. */
        }
        noc_c_parameter_list_free(&parameters);
    }
}

noc_c_translation_unit_free(&unit);
```

The analysis distinguishes object, function, typedef, tag, and unknown
declarations. Its declarator walker handles parenthesized pointers, functions
returning function pointers, arrays, inline tag definitions, and recognized C,
GNU, and MSVC attribute forms. Compound-statement helpers validate a body range
and return its delimiter-free inner range. Translation-unit results own their
item arrays and borrow only the token stream, so the lossless tree may be freed
after analysis; successful retokenization invalidates the result.

Classification is intentionally conservative. A comma declaration is reported
as unknown because one item can declare mixed object and function entities.
Names that require typedef knowledge, nested abstract parameter declarators,
and unrecognized attribute macros may be left unset. K&R function definitions
are not analyzed as functions; unknown top-level brace ranges are isolated so
they do not consume the following declaration.

## Transactional syntax edits

`Noc_Edit_Set` applies non-overlapping edits from C analysis or lossless syntax
nodes while preserving every untouched source byte. Replacement text is copied
when an edit is added, and edits may be supplied in any order:

```c
Noc_Edit_Set edits = {0};
Noc_Buffer rewritten = {0};

Noc_Token_Range name = {item->name_token, item->name_token + 1};
noc_edit_set_add_cstr(&edits, unit.stream, name, "new_name");
noc_edit_set_add_cstr(&edits, unit.stream, item->body,
                      "{ return new_value; }");

if (!noc_edit_set_apply(&edits, unit.stream, &rewritten)) return 1;
fwrite(rewritten.items, 1, rewritten.count, stdout);

noc_buffer_free(&rewritten);
noc_edit_set_free(&edits);
```

Adding an invalid or overlapping edit leaves the set unchanged. Adjacent
replacements are allowed; insertions at a replacement endpoint are emitted
before the replacement at that boundary, while duplicate insertions and
insertions inside replaced text are rejected. Empty replacement slices delete
their range. `noc_edit_set_add_syntax` accepts a syntax-node index directly.
Applying is transactional and leaves the destination buffer unchanged on
failure. Edit sets borrow a specific token-stream generation and cannot be used
after successful retokenization, including a free-and-reuse cycle.

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

Rules that read additional build inputs call `noc_rw_add_dependency`. Paths are
copied, deduplicated in first-seen order, and returned through
`Noc_Transform_Result.dependencies`; `noc_transform_result_free` releases both
the generated output and dependency list. Failed transforms publish neither
partial output nor partial dependencies. The built-in `@embed` rule reports its
resolved source-relative file path automatically.

Callbacks can pass a captured slice to `noc_rw_emit_transformed` when nested
dialect expressions should be expanded before emission. Nested transforms use
the same registry and source path, merge dependencies into the outer result,
and omit duplicate `#line` prologues. A bounded recursion depth rejects runaway
self-expansion, and any nested error transactionally discards all outer partial
output and dependencies.

Callbacks that replace multiline syntax can call `noc_rw_preserve_newlines` to
emit only the original CR, LF, or CRLF sequences and keep following source on
its physical line. When generated text cannot preserve line counts,
`noc_rw_emit_line_directive` starts a correctly escaped C `#line` directive;
the following output line is mapped to the requested source line and path.

For structured callbacks, `noc_rw_token_stream` and
`noc_rw_remaining_range` expose a callback-lifetime view compatible with the
standalone cursor and parser APIs. `noc_rw_consume_range` advances only across
an exact range beginning at the current raw cursor, so failed speculative parses
cannot skip source accidentally. `noc_rw_syntax_tree` lazily builds one lossless
tree for the current source, and `noc_rw_take_syntax` atomically matches and
consumes its next complete token or delimiter-group node. Borrowed stream, tree,
and node references must not escape the callback.

## Current boundary

This version deliberately handles explicit token/AST-assisted transformations,
a lossless delimiter tree, and lightweight C structure discovery. It does not
expand C macros, build a semantic C AST, resolve typedefs, or change the C type
system. Rules inside preprocessor directives are left untouched. More
structured statement and declaration helpers can be added without changing the
registration model.

The lexer accounts for backslash-newline splicing when recognizing comments and
preprocessor directives. Splices inside identifiers or multi-character
punctuators are not normalized for callbacks yet. C trigraphs are rejected
explicitly. Because `noc.h` does not evaluate preprocessor conditions, a rule in
an inactive `#if` branch is still visited. Expansion callbacks should preserve
physical newline counts when exact diagnostics after an expansion matter; the
rewriter source-mapping helpers make that policy explicit but do not infer it
automatically.

The staged implementation plan and current milestone status live in
[`TODO.md`](TODO.md).
