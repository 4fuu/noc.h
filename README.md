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

Run one independently buildable suite on demand with `./nob test <suite>`.
The suite names are `header-c`, `header-cpp`, `public-header-c`,
`public-header-cpp`, `modules`, `workspace`,
`preprocessing-tokens`, `macro-directives`, `macro-environment`,
`macro-invocations`, `macro-expansion`, `function-macro-expansion`,
`variadic-macro-expansion`, `preprocessor`, `lexing`, `syntax`, `c-analysis`,
`rewriter`, and `artifacts`, for example:

```console
$ ./nob test c-analysis
```

Other targets:

```console
$ ./nob example
$ ./nob describe
$ ./nob fuzz
$ ./nob verify-header
$ ./nob clean
```

The test target transforms, compiles, and runs both examples. `examples/embed`
shows a built-in expression rule with a file dependency; `examples/rules` is a
single project dialect covering expression, statement, declaration, and
attribute scopes. Both dialect inputs and applications use ordinary `.c` files.

## Developing the single header

`include/noc/noc.h` is the standalone, declaration-only public header.
`release/noc.h` is the checked-in quick-start header that emits bodies when
`NOC_IMPLEMENTATION` is defined. Editable implementations live in focused,
independently compilable `src/*.c` compiler-phase modules. `nob.c` builds the
standalone C11 `tools/amalgamate.c` tool and supplies its ordered source
manifest. Do not edit the release header.

```console
$ ./nob header          # regenerate release/noc.h and build/generated/noc.h
$ ./nob verify-header   # fail if release/noc.h is stale or generation varies
```

Generation has no timestamp or host-specific data. `verify-header` generates
the payload twice and compares both byte-for-byte before checking the release
artifact. Normal tests compile against `build/generated/noc.h`, not accidentally
against the release copy. CI checks the release artifact before running tests.

Public declarations remain above the `NOC_IMPLEMENTATION` bodies; private
types and genuine cross-module helper contracts live in `src/internal.h`.
Add a source module to the coherent phase-ordered amalgamation manifest,
regenerate, run its focused suite and the full suite, and commit both source and
generated header together. The published file remains self-contained and
dialect inputs keep normal `.c` and `.h` suffixes.

The manifest controls presentation, not C visibility or dependencies. Every
module compiles as its own translation unit and cross-module dependencies must
be declared by `src/internal.h`; module-local helpers remain static. Current ownership is:

| Module | Responsibility |
| --- | --- |
| `include/noc/noc.h` | Central public declarations |
| `src/internal.h` | Private shared types and cross-module contracts |
| `src/source.c` | Workspace identities, snapshots, edits, and line maps |
| `src/lexer.c` | Slices, lexer, buffers, and shared lexical primitives |
| `src/preprocessor.c` | Token streams, activity, policy, directive inventory, and PP units |
| `src/macro_directives.c` | Recoverable `#define`/`#undef` grammar and queries |
| `src/macro_invocations.c` | Lossless, recoverable function-like invocation/argument syntax |
| `src/macro_environment.c`, `src/macro_expansion.c` | Effective macro state and bounded expansion |
| `src/parser.c`, `src/ast.c` | Token cursors/arguments and syntax/C structure analysis |
| `src/features.c` | Context, rules, feature controls, and metadata interfaces |
| `src/lower.c`, `src/emit_c.c` | Rewrite/edit lowering and transformation/artifact/CLI emission |

New compiler phases get their own domain module and focused test suite; they are
not appended to an unrelated implementation merely because the
release is amalgamated. In particular, macro environment/expansion, includes,
C parsing, semantic analysis, lowering, emission, and IDE indexing remain
separate ownership boundaries. Shared helpers move downward only when at least
two modules genuinely own the same primitive—one-use wrappers and circular
private dependencies are not accepted.

## Incremental workspace snapshots

`Noc_Workspace` is the in-memory source foundation for parser and LSP work. A
successful open copies the path and source, assigns a workspace-local stable file
ID, and returns an immutable owning snapshot. Updates create a new generation;
older snapshots remain valid until their owners free them, including after the
workspace itself is deinitialized.

```c
Noc_Workspace workspace = {0};
Noc_Document_Snapshot document = {0};

noc_workspace_init(&workspace);
if (noc_workspace_open_document(&workspace,
                                "src/app.c",
                                source,
                                source_count,
                                NOC_SOURCE_CLASS_PROJECT,
                                &document) != NOC_WORKSPACE_OK) {
    return 1;
}

/* output may alias the exact current snapshot for an in-place editor update. */
if (noc_workspace_update_document(&workspace,
                                  &document,
                                  edited_source,
                                  edited_source_count,
                                  &document) != NOC_WORKSPACE_OK) {
    return 1;
}

noc_workspace_deinit(&workspace);
/* document still owns its immutable path, bytes, class, and physical line map. */
noc_document_snapshot_free(&document);
```

Owning workspace/snapshot handles start as `{0}` and must not be shallow-copied;
use `noc_document_snapshot_clone` for another owner. Open/update/lookups replace
their output only on success. Stale, closed, and foreign snapshots cannot update
or close another workspace's current revision. Paths use exact case-sensitive
byte identity on every platform—callers choose any filesystem canonicalization.

`noc_document_snapshot_location` and `noc_document_snapshot_offset` round-trip
physical byte positions including EOF. Lines and columns are 1-based; CRLF is
one newline while its two original bytes keep separate columns. This is a byte
source map, not the UTF-16 position conversion that an LSP transport will add.

`noc_workspace_edit_document` applies an ordered, non-overlapping batch of
half-open byte edits against one expected generation. Replacement slices may
borrow bytes from that old snapshot. Invalid/overlapping edits, allocation
failure, and stale snapshots leave both the workspace and output unchanged. An
empty batch intentionally creates a new generation with identical content. The
focused suite is available as `./nob test workspace`.

## Preprocessing tokens, directives, and macro policy

`noc_preprocessor_unit_build` produces a lossless preprocessing-token view and
inventories every directive from an immutable document snapshot. Unlike the
compatibility lexer stream, this view does not collapse a directive into one
opaque token: markers, keywords, body tokens, comments, whitespace, and physical
newlines remain independently queryable. `#include` header names are identified
as `NOC_TOKEN_HEADER_NAME` (including empty spellings, whose validity is checked
later), while otherwise unmatched non-whitespace characters use
`NOC_TOKEN_OTHER`. C11 universal-character-name spellings remain part of their
identifier or preprocessing-number token. Every token retains an exact source
slice and physical location plus a semantic role and owning directive index;
each directive publishes its half-open preprocessing-token range.
An incomplete directive may publish `NOC_TOKEN_INVALID` inside an otherwise
queryable unit, allowing editor clients to retain structure while text is typed.

`noc_preprocessor_token_at` and `noc_preprocessing_token_role_name` are suitable
for IDE semantic tokenization and later macro provenance queries. Directive
records also retain exact spelling, keyword, payload, source class, file ID, and
document generation. Null, unknown, C11, and selected newer directive names are
recognized without pretending to expand macros yet. The unit owns its copied
source and token storage, so queries remain valid after the source snapshot or
workspace is released.

Every `#define` and `#undef` also has a structured `Noc_Macro_Directive` record
queryable through `noc_macro_directive_at`. It distinguishes object-like,
function-like, and undef operations; exposes the macro name, parameter and
replacement ranges, C11 variadic slots, and a valid/incomplete/malformed syntax
status. Function-like detection follows phase 2: a line splice alone between a
name and `(` is accepted, while a comment or actual whitespace makes the macro
object-like. `noc_macro_parameter_at` exposes each identifier or unnamed `...`
slot without copying source text.

Building a unit does not diagnose recoverable macro syntax, so IDEs retain the
partial record. Batch callers may invoke
`noc_preprocessor_unit_validate_macro_directives` to report those statuses.
This is structural parsing only: expansion, duplicate-parameter constraints,
replacement `#`/`##` validation, and built-ins remain separate semantic or
expansion work. Run this coverage independently with
`./nob test macro-directives`.

`noc_macro_invocation_parse` is the corresponding physical-source syntax query.
Given an identifier and an exclusive token bound, it recognizes a following `(`
across trivia, balances nested parentheses, and publishes exact argument token ranges.
Empty parentheses have zero syntactic arguments; expansion interprets that
spelling against the selected macro definition. Missing `)`
is a successful `INCOMPLETE` editor result with partial arguments, not a fabricated
complete invocation. The owning result borrows its preprocessing unit, rejects a
rebuilt/stale owner, and is replaced only on successful parsing. Run this layer
independently with `./nob test macro-invocations`.

The caller must bound a query to its containing source/directive range; this API
does not infer that boundary. It also deliberately does not represent invocations
assembled during macro rescan, where a name, `(`, and argument tokens may have
different provenance owners. Function expansion therefore uses a shared private
logical-token collector and retains per-token provenance while applying the same
balancing rules.

`Noc_Macro_Environment` is the separate state layer used by future conditional,
include, and expansion processing. A caller applies valid, policy-enabled macro
directives in the order they are actually active; the environment never guesses
whether an unresolved conditional branch executes. Entries may come from
different preprocessing units, which lets an include traversal preserve one
definition history. `#undef` events remain in that history, while
`noc_macro_environment_lookup` returns only the currently active definition.
`noc_macro_environment_lookup_before` provides a half-open historical query for
IDE expansion previews and definition navigation.

The environment borrows its referenced units. Those owning unit objects must
outlive it and must not be rebuilt; generation checks reject a stale environment
after a legal rebuild. Apply is transactional, increments the environment
generation only on success, and rejects malformed or policy-disabled directives
without changing prior state. Run this layer independently with
`./nob test macro-environment`. It still does not perform macro expansion or
conditional evaluation.

`noc_macro_expansion_build` performs bounded object-like, fixed-arity, and strict
C11 variadic function-like macro substitution and recursive rescan against a
selected environment history prefix. Function arguments are collected from the
logical token stream, prescanned, substituted, and rescanned, including
invocations whose name and parentheses were assembled across replacement/input
boundaries. Multiple variable arguments preserve their separating commas and
substitute through `__VA_ARGS__`; reserved-name and omitted-variable-argument
constraints are rejected rather than treated as GNU extensions. Direct and
indirect recursion use token hide sets rather than provenance ancestry, so nested
cases such as `ID(ID(3))` retain standard rescan behavior.

Every result token records its physical source unit, preprocessing-token index,
input/argument/replacement origin, and expansion frame. Frames link nested
expansions to both their invocation token and environment definition, providing
the provenance needed by later diagnostics and IDE expansion previews. Expansion
limits bound provenance depth, every live logical token sequence, and total
expansion frames. Limit failures, incomplete or mismatched calls, malformed
definitions, and unsupported `#`/`%:`/`##`/`%:%:` operators preserve
the previous owning result rather than emitting a known-incorrect approximation.
`noc_macro_expansion_render` concatenates exact physical token spellings for
inspection. Run the object and function coverage independently with
`./nob test macro-expansion`, `./nob test function-macro-expansion`, and
`./nob test variadic-macro-expansion`.

Macro definition permission is explicit:

- `NOC_MACROS_DISABLED`: no source class may define or undefine macros.
- `NOC_MACROS_TRUSTED_ONLY`: trusted preludes and system headers may do so.
- `NOC_MACROS_PROJECT`: project, trusted, and system sources may do so.
- `NOC_MACROS_FULL`: all source classes may do so.

Generated source is deliberately not trusted by implication. Build and policy
validation are separate: the inventory always records a recognized `#define` or
`#undef`, including when disabled, while
`noc_preprocessor_unit_validate_macro_policy` emits precise feature-disabled
diagnostics. This prevents tooling from reporting a policy violation as a parse
error. Conditional policy application, `#`/`##`, include loading, and broader
expansion provenance queries remain subsequent preprocessor milestones. Run the
token and recovery coverage independently with
`./nob test preprocessing-tokens`, or the directive/policy coverage with
`./nob test preprocessor`.

## Fuzzing

`tests/fuzz_noc.c` is both a deterministic cross-platform smoke runner and a
Clang libFuzzer target. The smoke campaign is part of `./nob test` and exercises
lexer byte coverage, token streams/cursors, lossless syntax trees and lookup
invariants, C analysis, preprocessor activity maps, and rewrite callbacks.
Run it independently with `./nob fuzz`.

For a bounded sanitizer-backed libFuzzer campaign:

```console
$ clang -std=c11 -DNOC_LIBFUZZER \
    -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -I. \
    -o noc-fuzz tests/fuzz_noc.c
$ ASAN_OPTIONS=detect_leaks=1 ./noc-fuzz -runs=20000 -max_len=2048
```

CI runs the deterministic harness on Linux, macOS, and Windows, then runs the
bounded libFuzzer campaign in a dedicated Linux Clang job.

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

Run `dialect --describe` to inspect every registered trigger, its enabled state,
and its declared scope, syntax, and description.

## IDE metadata header

`noc_generate_ide_metadata_header` serializes the same registry into a
standalone C header for editor extensions, completion generators, and other
tooling. The output records a schema version, the `noc.h` version, dialect name,
rule count, and each rule's name, trigger kind/text, enabled state,
numeric/string scope, syntax, and description:

Schema 2 names the trigger kinds `at-name` and `pattern`, matching
`NOC_RULE_TRIGGER_AT_NAME` and `NOC_RULE_TRIGGER_PATTERN`.

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
header alone cannot make an ordinary C parser understand raw dialect syntax;
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
preorder, inner-range, source, location, and token lookup helpers.
`noc_syntax_node_at_token` maps an ordinary token to its leaf and an opening or
closing delimiter to the group that owns it. `noc_syntax_node_covering_range`
finds the deepest node containing a non-empty token range; depth and lowest
common-ancestor queries support structural selections without rebuilding an
index. A group range includes its delimiters; `noc_syntax_inner_range` excludes
them. Tree builds are transactional: malformed delimiters diagnose their exact
source location and leave any prior tree intact. Trees borrow their token stream
and become invalid after that stream is freed or successfully retokenized;
failed retokenization preserves both the stream and its trees.

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

## Conservative preprocessor activity

`noc_preprocessor_map_build` labels every token as active, inactive, or unknown
across nested conditional directives. It resolves exact `#if 0`, `#if 1`, and
equivalent `#elif` branches, including `%:` directive markers. Conditions that
depend on macros, `defined`, or other expressions stay unknown; the API does
not pretend to run the C preprocessor. Maps are transactional, diagnose
unbalanced or misplaced conditional directives, borrow a token-stream
generation, and provide constant-time lookup with
`noc_preprocessor_activity_at`.

Transforms preserve their historical behavior by default. Projects that want
known dead branches left untouched can opt in:

```c
noc.options.skip_inactive_preprocessor_branches = true;
```

Only rule triggers in definitely inactive branches are skipped. Unknown
branches are still transformed and unknown `@name` triggers there still produce
diagnostics, so build-configuration-dependent code is never silently guessed.
Fragments passed recursively through `noc_rw_emit_transformed` do not run an
independent activity analysis because a captured slice may cross conditional
boundaries owned by its enclosing source.

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

`noc_register_rule` retains the legacy `@name` spelling. A rule can instead
use an explicit C-lexer-token trigger:

```c
noc_register_rule_pattern(&noc, "checked add", checked_add_rule);
noc_register_rule_pattern(&noc, "unless (", unless_rule);
noc_set_rule_enabled(&noc, noc_slice_from_cstr("unless"), false);
```

Patterns match token kinds and phase-2 logical spellings. Trivia between their
tokens is ignored (and remains available through `noc_rw_trigger_range`), but
trivia outside the trigger is not consumed. At a token, the pattern containing
the most significant tokens wins regardless of registration order or enabled
state. Leading `@` is reserved for legacy registration. Pattern strings are
borrowed and must outlive the context, like the other strings in `Noc_Rule`.
Rule names remain globally unique identities and `noc_rule_is_enabled` queries
their state.

Pattern matching is deliberately lexical, not C scope-aware: a trigger such as
`defer` also matches a label, member name, or any other identifier token with
that spelling. `Noc_Rule.scope` is descriptive metadata, not a matching
constraint. Patterns do not search inside comments, strings, or preprocessor
tokens; a pattern can nevertheless explicitly match one complete string token.
Definitely inactive preprocessor branches are skipped when that option is
enabled. Generated C should be used for ordinary IDE C parsing; raw dialect
spellings are not necessarily understood by C parsers.

Rules begin enabled. With the default `disabled_rule_is_error = true`, use of a
disabled selected rule fails transactionally. Setting it false preserves the
complete matched trigger byte-for-byte and continues after it, without parsing
its following arguments or body. Registry and enable changes are rejected
during transforms, including nested transforms. Version 0.19 changes the public
`Noc_Context` layout: rebuild applications and libraries together; it is not
ABI-compatible with objects built against earlier headers.

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

`noc_generate_depfile` turns that result into a transactional,
Make/Ninja-compatible dependency buffer:

```c
Noc_Transform_Result result = {0};
Noc_Buffer depfile = {0};

if (!noc_transform_source(&noc, input, source, source_count, &result)) return 1;
if (!noc_generate_depfile(&noc, output, input, &result, &depfile)) return 1;

fwrite(depfile.items, 1, depfile.count, stdout);
noc_buffer_free(&depfile);
noc_transform_result_free(&result);
```

The primary source is emitted first, its duplicate is suppressed from reported
dependencies, and ordering otherwise remains first-seen. Spaces, tabs, `#`,
`$`, colons, and backslashes are escaped for depfile readers, including Windows
drive paths. Empty paths and embedded newlines are rejected without changing an
existing destination buffer.

File-oriented build drivers can use `noc_transform_file_with_result` to retain
the same owning result after the transformed file is atomically written. The
generic CLI wires both operations together without a second transformation:

```console
$ dialect input.c -o build/input.c --depfile build/input.d
$ dialect input.c -o build/input.c --depfile build/input.d \
    --dep-target build/input.o
```

The depfile target defaults to the transformed `-o` path. Input, transformed
output, and depfile paths must be distinct; both generated files use unique
temporary files and atomic replacement.

## Mirrored multi-file transformation

`noc_transform_files` preflights a set of ordinary `.c`/`.h` paths, maps each
path relative to an input root, creates the corresponding output directories,
and transforms them into an isolated mirror:

```c
const char *inputs[] = {
    "src/app.c",
    "src/include/project.h",
};
Noc_Batch_Options batch = {
    .input_root = "src",
    .output_root = "build/generated",
    .emit_depfiles = true,
};

if (!noc_transform_files(&noc, &batch, inputs, 2)) return 1;
```

This produces `build/generated/app.c` and
`build/generated/include/project.h`; optional depfiles are written beside them
as `.c.d`/`.h.d`. The dialect CLI exposes the same operation:

```console
$ dialect --batch src build/generated \
    src/app.c src/include/project.h
$ dialect --batch-depfiles src build/generated \
    src/app.c src/include/project.h
```

All mappings and duplicate output collisions are checked before the first write.
Inputs must reside lexically below `input_root`, use `.c` or `.h`, and cannot
contain `.`/`..` or colon-bearing relative components, preventing output-root
escape and keeping mirrors portable to Windows. Each file is atomically
replaced, but the batch as a whole is intentionally not a cross-file
transaction: outputs completed before a later transformation error remain.
Outputs are also preflighted against every input, so an output tree nested under
the input root cannot overwrite a source that the batch has not read yet.

The current Windows backend uses narrow ANSI filesystem calls. Batch mode
therefore rejects UNC roots, drive-relative forms such as `C:relative`, and
non-ASCII paths rather than silently applying incorrect containment or collision
rules. Rooted drive paths such as `C:/project/src` remain supported. Removing
these restrictions requires a future end-to-end UTF-16 filesystem backend, not
only a different string comparison.

## Exact command signatures

Timestamp checks do not notice a changed compiler path, define, include order,
or flag. `noc_generate_command_signature` serializes the complete argument
vector with decimal byte lengths rather than a collision-prone ordinary hash:

```c
const char *command[] = {
    "cc", "-std=c11", "-Ibuild/generated", "-c", "build/generated/app.c",
};
Noc_Buffer signature = {0};

if (!noc_generate_command_signature(&noc, command, 5, &signature)) return 1;
```

Empty arguments and embedded newlines remain distinct and unambiguous. The
artifact includes a schema number and `noc.h` version, is NUL-terminated for
convenience, and replaces the destination only on success. The CLI can atomically
write it directly:

```console
$ dialect --command-signature build/generated/compile.sig -- \
    cc -std=c11 -Ibuild/generated -c build/generated/app.c
```

Compare the complete signature file with the previous build before deciding to
reuse compiler output. A command signature complements rather than replaces
normal dependencies: the dialect executable/source, transformed inputs, emitted
depfiles, compiler executable, and relevant environment still belong in the
build graph.

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

This version handles explicit token/AST-assisted transformations, a lossless
delimiter tree, lightweight C structure discovery, and bounded
object/fixed-arity/C11-variadic macro inspection expansion. It does not provide a
complete integrated preprocessor, stringify/paste/built-in macro expansion, a
semantic C AST, typedef resolution, or a C type system. Rules inside preprocessor
directives are left untouched. More
structured statement and declaration helpers can be added without changing the
registration model.

The lexer accounts for backslash-newline splicing when recognizing comments,
preprocessor directives, identifiers, and multi-character punctuators. Raw
`Noc_Token.text` remains the exact source slice for lossless trees and rewrites;
`noc_token_is_identifier` and `noc_token_is_punct` compare the phase-2 logical
spelling, and `noc_token_logical_text` copies that spelling into a transactional
NUL-terminated buffer. C trigraphs are rejected explicitly. Preprocessor
activity analysis is conservative and does not expand macros; it can skip only
literal, definitely inactive branches when explicitly enabled. Expansion
callbacks should preserve physical newline counts when exact diagnostics after
an expansion matter; the rewriter source-mapping helpers make that policy
explicit but do not infer it automatically.

The staged implementation plan and current milestone status live in
[`TODO.md`](TODO.md).
