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
`macro-invocations`, `macro-expansion`, `logical-source`,
`logical-source-lifecycle`, `logical-source-queries`,
`logical-source-fragments`, `logical-source-fragments-lifecycle`,
`function-macro-expansion`,
`variadic-macro-expansion`, `macro-stringification`, `macro-token-paste`,
`macro-builtins`, `configured-builtins`, `preprocessor-expressions`,
`conditional-groups`, `include-operands`, `include-resolver`,
`include-expansion`, `include-expansion-resolver`,
`include-graph`, `include-graph-limits`, `include-graph-queries`,
`pragma-once`, `include-guard`, `include-control-queries`,
`release-header-runtime`,
`preprocessor`, `lexing`, `syntax`, `c-analysis`, `c-parse-tree`,
`c-parse-recovery`, `c-parse-lifecycle`, `c-parse-corpus`,
`c-parse-malformed-corpus`, `c-parse-completion`, `c-parse-cpp-runtime`,
`c-ast`, `c-ast-details`, `c-ast-c11`, `c-ast-c11-constructs`,
`c-ast-declarators`, `c-ast-completion`, `c-ast-queries`, `c-ast-recovery`,
`tree-sitter-coexistence`, `rewriter`, and `artifacts`, for example:

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
| `src/logical_source.c` | Owning canonical macro-fragment bytes, physical sites, and normalized provenance frames |
| `src/logical_c_parse.c` | Recoverable logical C concrete-syntax adapter over retained macro-fragment source |
| `src/logical_ast.c` | Stable normalized logical C AST and token-to-physical/macro provenance bridge |
| `src/conditional.c`, `src/conditional_groups.c` | C11 condition evaluation and recoverable balanced conditional analysis |
| `src/include_control.c` | Read-only pragma-once and strict canonical include-guard recognition |
| `src/include_resolver.c`, `src/include_expansion.c` | Physical/expanded include operands and host-configurable snapshot resolution |
| `src/include_graph.c` | Bounded conditional include discovery, recursion, cycles, and stable IDE queries |
| `src/parser.c` | Token cursors/arguments and lossless delimiter/C structure analysis |
| `src/ast.c` | Stable normalized physical C AST and typed spelling/recovery details |
| `src/c_parse.c` | Recoverable physical C concrete-syntax adapter and Noc-owned flat node views |
| `src/features.c` | Context, rules, feature controls, and metadata interfaces |
| `src/lower.c`, `src/emit_c.c` | Rewrite/edit lowering and transformation/artifact/CLI emission |
| `third_party/tree-sitter/` | Pinned, namespaced native Tree-sitter runtime and C grammar payload |

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

## Recoverable physical C syntax

`noc_c_parse_tree_build` parses one immutable document snapshot with the pinned
tree-sitter-c grammar. It publishes a Noc-owned flat preorder tree rather than
Tree-sitter objects: each `Noc_C_Parse_Node` has a grammar kind, its field name
within the parent, parent/child/sibling indices, flags, a generation, and an
exact half-open physical byte range. This layer is intended for editor structure
and the next AST-mapping phase. It does not preprocess macros, resolve typedef
names semantically, validate feature policy, or claim that every grammar
extension is accepted by a Noc target profile.

The translation-unit root always spans the complete retained snapshot. If
recovery skips an unrecognized leading or trailing byte, child nodes keep the
grammar engine's exact ranges while the root still provides a whole-document
source view.

```c
Noc_C_Parse_Tree tree = {0};
Noc_C_Parse_Options options = noc_c_parse_default_options();

if (noc_c_parse_tree_build(&document, options, &tree) != NOC_C_PARSE_OK) {
    /* Invalid arguments, cancellation, limits, or adapter/engine failure. */
    return 1;
}

/* Malformed editor text still produces a valid, traversable recovery tree. */
if (noc_c_parse_tree_has_error(&tree)) {
    const Noc_C_Parse_Node *root =
        noc_c_parse_tree_node_at(&tree, noc_c_parse_tree_root(&tree));
    (void)root;
}
noc_c_parse_tree_free(&tree);
```

Tree handles start as `{0}`, own a cloned source snapshot, and must not be
shallow-copied. Failed builds preserve an existing output unchanged. Successful
rebuilds increment the generation and invalidate previous node pointers and
indices. Missing recovery tokens have zero-width `[offset,offset)` ranges;
`ERROR`, `MISSING`, and `HAS_ERROR` flags distinguish recovery structure from a
complete parse. Cancellation is cooperative and published-node/source limits
bound Noc's adapter work; the pinned upstream runtime retains its documented
fail-fast policy for its own internal allocation failure.

No Tree-sitter type or numeric grammar symbol is part of the public API. The
runtime and C grammar are pinned, renamed into Noc's private namespace, and
amalgamated into `release/noc.h`, including their license notices. Normal builds
are offline and require no external library. Dependency updates are an explicit
maintainer operation described in `third_party/README.md`. Because the embedded
runtime is C, compile the one translation unit that defines
`NOC_IMPLEMENTATION` as C11; C++ consumers may include the declaration-only
header and link that C object normally.

The pinned upstream C grammar is reproducibly regenerated with a small,
authenticated-toolchain downstream patch for the required C11 `_Bool`,
`_Complex`, `_Thread_local`, `_Static_assert`, and `_Atomic(type-name)`
spellings plus anonymous bit-fields. Noc's normalized AST exposes these through
stable primitive/type flags, storage specifiers, `STATIC_ASSERT_DECLARATION`,
`ATOMIC_TYPE_SPECIFIER`, `BITFIELD_CLAUSE`, and typed fields rather than parser
symbol IDs. ISO C11 spellings have no extension marker; the C23 `static_assert`
alias is distinguished for later feature-policy validation. Run focused
spelling, construct, and corpus coverage with `./nob test c-ast-c11`, `./nob
test c-ast-c11-constructs`, and `./nob test c-parse-corpus`.

The normalized AST also supports physical byte navigation without exposing
Tree-sitter objects. `noc_c_ast_node_at_offset` and
`noc_c_ast_node_covering_range` return the deepest retained node for editor
selection, while `noc_c_ast_depth` and `noc_c_ast_common_ancestor` support
context reconstruction. These queries intentionally exclude the EOF insertion
position and zero-width missing nodes; expected-symbol and completion queries
remain explicit recovery operations rather than ambiguous byte ownership.

`noc_c_ast_completion_context` accepts insertion positions including document
edges and EOF. It reports the adjacent physical AST nodes, their syntactic
context, AST/document generations, and every zero-width recovery expectation at
that position. `noc_c_ast_completion_next_expected_node` enumerates those
expected nodes in one allocation-free linear scan. This is parser recovery
context for IDE clients, not an exhaustive grammar-lookahead or semantic symbol
completion result; a missing hint must never exclude another candidate.

`noc_c_parse_grammar_candidates_build` complements that normalized AST context
with an owning, bounded set of retained-parser-state hints. It works at every
physical insertion offset, including BOF and EOF. Token interiors use whole
grammar-leaf replacement semantics; trivia uses the next physical leaf; ERROR
recovery uses its first leaf; and a MISSING symbol uses the previous non-extra
leaf. These rules are deterministic heuristics because a materialized
Tree-sitter tree does not retain one authoritative LR stack at every byte.
Results are sorted and deduplicated by stable Noc categories and copied
spellings, survive parse-tree rebuild/free, merge materialized MISSING origins,
mark non-C11 exact spellings, and carry document/tree generations. They are not
semantic completion and do not apply typedef, macro, target, or feature-policy
filtering; callers combine them with AST context and later semantic/index data.
Run the focused coverage with `./nob test c-parse-completion`.

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
cases such as `ID(ID(3))` retain standard rescan behavior. C11 `#` and its `%:`
digraph stringify the raw, unprescanned argument, normalize intervening
whitespace/comments, escape string/character literal spellings, and then rejoin
normal rescan. C11 `##` and `%:%:` paste raw adjacent arguments, model empty
arguments with placemarkers, re-tokenize to exactly one preprocessing token, and
rescan the result. Noc resolves paste chains left-to-right for reproducibility;
portable source must not depend on that order because C11 leaves it unspecified.
The deterministic predefined macros `__FILE__`, `__LINE__`, `__STDC__`, and
`__STDC_VERSION__` also participate in normal argument prescan and replacement
rescan. File and line values follow the nearest physical token or invocation in
the expansion input until `#line` mapping is implemented. Active explicit
definitions in the selected macro-environment prefix take precedence over these
predefined macros; after an effective `#undef`, predefined fallback is eligible
again.

Every result token records its physical source unit, preprocessing-token index,
input/argument/replacement/stringification/paste/builtin origin, and expansion
frame. Frames link nested expansions to both their invocation token and
environment definition, providing the provenance needed by later diagnostics
and IDE expansion previews.
Stringified tokens use stable generated spellings owned by the expansion and
retain the physical `#` operator as provenance; pasted tokens likewise retain the
immediately generating `##` operator. The generated spelling arena may also own
intermediate chain results no longer present in final output. Expansion limits
bound provenance depth, every live logical token sequence, and total expansion
frames. Limit failures, incomplete or mismatched calls, malformed definitions,
and invalid paste results preserve the previous owning result rather than
emitting a known-incorrect approximation. `noc_macro_expansion_render`
concatenates each result token's stored physical or generated spelling for
inspection. Run the coverage independently with
`./nob test macro-expansion`, `./nob test function-macro-expansion`, and
`./nob test variadic-macro-expansion`, `./nob test macro-stringification`, or
`./nob test macro-token-paste`; predefined-macro coverage is
`./nob test macro-builtins`, while explicit translation-input coverage is
`./nob test configured-builtins`.

`noc_macro_expansion_default_options` starts an options-aware translation with
the existing deterministic `__FILE__`, `__LINE__`, `__STDC__`, and
`__STDC_VERSION__` fallbacks. Callers can explicitly select hosted or
freestanding execution and provide fixed C11 `Mmm dd yyyy` / `hh:mm:ss` values
for reproducible `__STDC_HOSTED__`, `__DATE__`, and `__TIME__` expansion. Noc
never reads the compiler host or wall clock. Empty date/time values and an
unspecified execution environment leave those fallbacks unavailable; this is a
deliberate analysis/configuration state rather than the behavior of a complete
conforming C11 implementation, which requires all seven macros. The options are
threaded through normal expansion, condition expansion, `defined`, direct
`#ifdef`, and conditional-group analysis. Generated spellings are copied into
the owning expansion, and `noc_macro_expansion_builtin_is_available` exposes the
exact configured fallback set to IDE clients without interpreting private bits.

`noc_logical_source_build_macro_expansion` is the durable parser bridge for one
successful macro-expansion fragment. It removes phase-2 line splices from each
token spelling and gives the result a separate logical byte coordinate domain;
existing physical CST/AST byte ranges are never reinterpreted. When two
significant preprocessing tokens have no nonempty logical trivia between them,
the serializer inserts an explicitly marked ASCII-space token. That conservative
rule preserves token boundaries on re-lexing (`/` plus `*` cannot become a
comment, and separate punctuators or identifiers cannot merge); paste and
stringification results already arrive as one generated token.

The result owns its canonical NUL-terminated text, logical token ranges, copied
file paths and document identities, immediate physical token sites, and nested
definition/invocation frame chain. It therefore remains queryable after the
temporary expansion, environment, preprocessing units, snapshots, and workspace
are destroyed. Input scanning, output bytes, token/frame/file counts, copied path
bytes, and cancellation are explicitly bounded, and every failure preserves the
previous generation. Its owned logical line map supports EOF-inclusive
offset/line/byte-column conversion with the same CRLF convention as physical
snapshots. A binary-search byte-range query maps future logical CST/AST ranges
back to the minimal token interval, where callers can inspect each token's
physical and macro provenance without conflating coordinate domains. These
records are suitable for diagnostics and expansion preview.
`noc_logical_source_clone` retains one immutable revision without copying its
bytes, token maps, source-file table, or macro frames.

`noc_logical_source_build_macro_expansions` composes multiple caller-ordered
expansion fragments in that same owning domain. It interns shared physical unit
identities, rebases each fragment's nested macro-frame indices, and applies the
canonical anti-token-pasting separator rule across fragment boundaries. Its
combined fragment/token/frame/file/path/input-byte budgets, cancellation, and
transactional publication make it the composition boundary for a later
conditional/include preprocessing driver; it does not itself decide which
fragments are active or traverse includes.

`noc_logical_c_parse_tree_build` parses that retained logical text with the same
embedded recoverable C grammar as the physical CST while publishing a separate
`Noc_Logical_C_Parse_Node` topology. Its ranges never enter the physical
`Noc_Byte_Range` domain. Node source/location queries stay logical, and
`noc_logical_c_parse_node_token_range` maps a node to the smallest contributing
token interval; callers then use the retained logical source to inspect each
token's physical anchor and nested macro frames. The parse tree survives source
rebuild/free, is transactional, generation-scoped, bounded, and cancellable.
This bridge still does not execute directives, choose conditional branches,
traverse includes, or claim that an arbitrary macro fragment is a complete
preprocessed translation unit.

`noc_logical_c_ast_build` maps the logical CST into the same stable Noc-owned
kinds, fields, operators, spelling details, and recovery vocabulary as the
physical normalized AST. Its nodes retain the distinct logical byte-range type,
and every node can be mapped to contributing logical tokens and from there to
copied physical sites and nested macro frames. The AST owns its retained logical
revision, is bounded/cancellable/transactional, and remains valid after the CST
and all preprocessing inputs are released. It is still a syntax AST for one
expanded fragment, not typedef/type resolution or complete translation-unit
preprocessing.

`noc_logical_c_ast_completion_context` provides the allocation-free logical
insertion-point counterpart of the physical AST completion context. It reports
adjacent normalized nodes, their common syntax context, and every zero-width
recovery expectation at that logical offset. Owner, AST generation, and logical
source generation reject stale or foreign contexts; expectation nodes retain
the normal token-to-physical/macro provenance bridge. These parser recovery
hints remain separate from later semantic and feature-policy completion.

Run source-map/serialization coverage with `./nob test logical-source`,
ownership/cancellation/generation/limit coverage with `./nob test
logical-source-lifecycle`, coordinate/range coverage with `./nob test
logical-source-queries`, ordered composition/provenance coverage with `./nob
test logical-source-fragments`, and its combined limit/stale/transactional
coverage with `./nob test logical-source-fragments-lifecycle`. Run logical
grammar/provenance coverage with `./nob test logical-c-parse`, and
retained-revision/transactional coverage with `./nob test
logical-c-parse-lifecycle`. Run normalized logical AST/provenance coverage with
`./nob test logical-c-ast` and its ownership/limit/generation coverage with
`./nob test logical-c-ast-lifecycle`. Run logical insertion-context and recovery
expectation coverage with `./nob test logical-c-ast-completion`.

Conditional preprocessing is staged rather than hidden inside a monolithic
driver. `noc_preprocessor_directive_body_tokens` returns the significant
`#if`/`#elif` body span while retaining internal trivia.
`noc_macro_expansion_build_condition` expands that range with ordinary macro
provenance and limits but protects `defined` operands. Expansion-generated
`defined` follows the deterministic GCC/Clang extension for otherwise undefined
C11 input. `noc_preprocessor_expression_evaluate` then applies C11 precedence,
intmax/uintmax conversions, remaining-identifier-to-zero behavior,
short-circuiting, and the conditional operator. It reports malformed input,
division by zero, signed overflow, invalid shifts, bounded-depth failures, and
target-dependent operations at exact expansion-token indices. Numeric octal and
hexadecimal character escapes up to 127 are deterministic; execution-character-
set constants and negative signed right shifts remain target-dependent until
translation-target options exist. Run this layer independently with
`./nob test preprocessor-expressions`.

`noc_preprocessor_conditional_groups_build` layers recoverable balanced
`#if`/`#ifdef`/`#ifndef`/`#elif`/`#else`/`#endif` groups over that evaluator. It
publishes exact group and branch ranges, parent-branch relationships, structural
issues, per-preprocessing-token activity, and the concrete macro prefix visible
at each token. The result owns a cloned initial macro prefix and appends only
definitely active `#define`/`#undef` events. If an unknown path can change macro
state, later concrete-prefix queries return `NOC_TOKEN_INDEX_NONE` and conditions
are not guessed. This distinction lets IDEs inspect incomplete source without
presenting uncertain branches as semantic success. C23 `#elifdef`/`#elifndef`
are inventoried but reported as unsupported by the current C11 analysis. Run
this layer independently with `./nob test conditional-groups`.

Physical include syntax and host resolution are separate APIs.
`noc_include_operand_build` classifies each inventoried `#include` as a direct
quoted/angled header, macro-expansion-required input, or a recoverable
empty/missing/malformed/incomplete editor state. Direct logical names own their
phase-2-splice-normalized bytes while retaining the exact physical token range
and problem index. `noc_include_resolve` then passes only valid direct operands
to a host callback. The host owns search order, path canonicalization, unsaved
overlays, virtual filesystems, and source classification; Noc performs no hidden
filesystem access and validates the returned owning snapshot transactionally.
Run these layers independently with `./nob test include-operands` and
`./nob test include-resolver`. `noc_include_expansion_build` performs bounded
normal macro expansion when physical classification requires it, then publishes
an owning logical name with expansion-relative ranges and full token provenance;
`noc_include_expansion_resolve` sends only a valid final name to the same host
policy. Its focused suites are `include-expansion` and
`include-expansion-resolver`.

`noc_include_graph_build` composes those phases into bounded, deterministic
depth-first discovery using heap-backed traversal frames rather than recursive C
calls. The graph owns immutable snapshots and per-inclusion
preprocessor/conditional contexts, records inactive and unknown edges without
calling the host resolver, detects ancestor cycles, and exposes stable node,
edge, operand, expansion, and phase-object queries for IDE/LSP indexing. The
same snapshot may be represented by multiple nodes because each occurrence can
have a different macro prefix. Since child macro effects are not yet executed
back into the parent, later affected edges are explicitly
`UNKNOWN_MACRO_STATE` rather than guessed. Noc still performs no filesystem I/O:
the host resolver owns search and overlays. Exercise traversal, bounds and
transactionality, or query/provenance independently with `./nob test
include-graph`, `./nob test include-graph-limits`, and `./nob test
include-graph-queries`.

`noc_pragma_once_build` and `noc_include_guard_build` provide a separate,
read-only include-control recognition layer for preprocessing tools and IDE/LSP
clients. Pragma recognition accepts only direct, case-sensitive `#pragma once`;
guard recognition deliberately accepts only a file-enclosing `#ifndef NAME`
whose first significant guarded construct is an object-like `#define NAME`, with
no peer branch. Both retain exact half-open preprocessing-token ranges, recovery
states, splice-aware names, macro-policy visibility, and owner generations.
They neither suppress duplicate includes nor mutate macro state. Exercise syntax
and recovery, or query lifetime/transactionality independently with `./nob test
pragma-once`, `./nob test include-guard`, and `./nob test
include-control-queries`. Duplicate suppression and exact cross-file macro
execution remain later preprocessing stages.

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
error. Include loading and broader expansion provenance queries remain
subsequent preprocessor milestones.
Run the token and recovery coverage independently with
`./nob test preprocessing-tokens`, or the directive/policy coverage with
`./nob test preprocessor`.

## Fuzzing

`tests/fuzz_noc.c` and `tests/fuzz_c_parser.c` are deterministic cross-platform
smoke runners and separate Clang libFuzzer targets. The general target covers
lexer bytes, token streams/cursors, lossless syntax queries, C structure
analysis, preprocessing, sampled owning logical-source/provenance invariants,
and rewrite callbacks. The parser target isolates the
recoverable physical CST and normalized AST, checking ranges, topology,
generation-aware rebuilds, typed details, and deterministic recovery for
arbitrary bytes. Both smoke campaigns are part of `./nob test` and run on demand
with `./nob fuzz`.

For a bounded sanitizer-backed libFuzzer campaign:

```console
$ ./nob header
$ clang -std=c11 -DNOC_IMPLEMENTATION -DNOC_LIBFUZZER \
    -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer \
    -include build/generated/noc.h -Iinclude -I. \
    -o noc-fuzz tests/fuzz_noc.c
$ ASAN_OPTIONS=detect_leaks=1 ./noc-fuzz -runs=20000 -max_len=2048

$ clang -std=c11 -DNOC_IMPLEMENTATION -DNOC_LIBFUZZER \
    -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer \
    -include build/generated/noc.h -Iinclude -I. \
    -o noc-c-parse-fuzz tests/fuzz_c_parser.c
$ ASAN_OPTIONS=detect_leaks=1 \
    ./noc-c-parse-fuzz -runs=20000 -max_len=4096
```

CI runs both deterministic harnesses on Linux, macOS, and Windows, then runs
both bounded libFuzzer campaigns in a dedicated Linux Clang job.

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
complete integrated preprocessor, every implementation-specific built-in macro,
typedef resolution, or a C type system. C11 macro
stringification, token pasting, deterministic file/line/standard built-ins, and
preprocessing integer-expression evaluation are supported. Recoverable
conditional-group execution and active-only macro state are available as an
explicit analysis API;
translation-configured hosted/date/time built-ins are available without reading
the host or clock. Durable ordered macro-fragment composition, a recoverable
logical C CST, and a normalized logical AST preserve token-level physical/macro
provenance, but active-fragment selection, complete directive/include
preprocessing, and broader target semantics are not integrated.
Rules inside preprocessor directives are left untouched. More
structured statement and declaration helpers can be added without changing the
registration model.

The lexer accounts for backslash-newline splicing when recognizing comments,
preprocessor directives, identifiers, and multi-character punctuators. Raw
`Noc_Token.text` remains the exact source slice for lossless trees and rewrites;
`noc_token_is_identifier` and `noc_token_is_punct` compare the phase-2 logical
spelling, and `noc_token_logical_text` copies that spelling into a transactional
NUL-terminated buffer. C trigraphs are rejected explicitly. Preprocessor
activity analysis used by the legacy transform path remains conservative and
does not yet call the new macro-aware expression evaluator; it can skip only
literal, definitely inactive branches when explicitly enabled. Expansion
callbacks should preserve physical newline counts when exact diagnostics after
an expansion matter; the rewriter source-mapping helpers make that policy
explicit but do not infer it automatically.

The staged implementation plan and current milestone status live in
[`TODO.md`](TODO.md).
