# noc.h roadmap

This file is the long-running implementation checklist. A milestone is checked
only after its interfaces have tests or examples, the complete local suite
passes, and the milestone has been committed and pushed.

The current 0.19 implementation remains the compatibility baseline while the
compiler frontend is built. Dialect inputs continue to use ordinary `.c` and
`.h` names. Development code may be split into normal C modules, but releases
must be generated reproducibly as one self-contained `noc.h`.

## Milestone 0 — portable baseline and continuous integration

- [x] Keep dialect inputs on ordinary `.c` and `.h` suffixes.
- [x] Provide the single-header lexer, rule registry, rewriter, diagnostics, CLI,
      atomic output, and `@embed` reference module.
- [x] Add root build/test documentation and regression tests.
- [x] Add GitHub Actions coverage for Linux, macOS, and Windows.
- [x] Make the `nob.c` harness build with GCC/Clang-style compilers and MSVC.

## Milestone 1 — public token stream and cursor APIs

- [x] Add an owning public token stream with tokenize/free operations.
- [x] Add reusable raw/significant token cursors independent of rewrite callbacks.
- [x] Add token range and source-span helpers.
- [x] Add balanced range and comma-separated argument parsing.
- [x] Cover every public operation with focused unit tests.

## Milestone 2 — lossless syntax tree

- [x] Add a generic lossless syntax tree for translation units, tokens, and
      `()`, `[]`, and `{}` groups.
- [x] Preserve trivia and exact source ranges in every node.
- [x] Add traversal, parent/child lookup, source extraction, and destruction APIs.
- [x] Add deepest token/range lookup, depth, and common-ancestor queries.
- [x] Diagnose unmatched and mismatched delimiters with source locations.
- [x] Add tree construction, traversal, malformed-input, and round-trip tests.

## Milestone 3 — reusable C structure analysis

- [x] Add lightweight top-level declaration/function discovery without claiming
      to be a complete semantic C frontend.
- [x] Expose declaration kind, name token, signature range, body group, and source
      location where determinable.
- [x] Add function parameter and compound-statement helpers.
- [x] Test functions, prototypes, variables, typedefs, structs/enums, attributes,
      nested declarators, and deliberately unsupported ambiguity.

## Milestone 4 — AST-assisted rewriting and composition

- [x] Add transactional non-overlapping edits for token ranges and syntax nodes.
- [x] Let rules consume syntax nodes and token ranges safely.
- [x] Add transformed emission for nested rule composition.
- [x] Add newline/source-map restoration helpers.
- [x] Add dependency reporting so rules such as `@embed` expose build inputs.
- [x] Add examples for expression, statement, declaration, and attribute rules.

## Milestone 5 — IDE artifacts (lower priority)

- [x] Define an IDE-generation options structure and stable public API.
- [x] Generate transformed header overlays from dialect `.h` files.
- [x] Generate a dialect metadata header containing registered rule names,
      scopes, syntax, and descriptions for tooling/autocomplete integrations.
- [x] Evaluate and document clangd/compile_commands integration using generated
      `.c` files and `#line` mappings.
- [x] Add golden-file tests and an end-to-end IDE-header example.

## Later work

- [x] Split unit coverage into independently buildable lexer, syntax, C analysis,
      rewriter, and artifact/file-I/O suites with on-demand build targets.
- [x] Multi-file API/CLI and mirrored source/output directory trees.
- [x] Add Make/Ninja depfile API and CLI output.
- [x] Add exact compiler-command signature API and CLI output.
- [x] Add optional, conservative inactive-preprocessor-branch handling without
      guessing macro-dependent conditions.
- [x] Normalize phase-2 splices inside identifiers, preprocessing numbers,
      literal prefixes, and punctuators while preserving exact source slices
      for lossless syntax APIs.
- [x] Fuzz lexer, token cursor, syntax tree/query, preprocessor activity, and
      rewrite callback APIs with deterministic smoke and Clang libFuzzer modes.
- [x] Add explicit token-pattern rule triggers and per-rule feature controls
      while preserving the existing `@name` registration contract.
- [ ] Versioning, changelog, release packaging, and vendored `nob.h` update policy.

## Compiler rewrite contract

- The first complete language target is ISO C11. ISO C17 and C23 are explicit
  follow-up input/output profiles; GCC, Clang, and MSVC extensions are explicit
  target profiles rather than silently accepted syntax.
- Parsing support and permission are separate. The frontend should recognize a
  disabled construct and issue a feature-disabled diagnostic rather than a
  misleading parse error.
- The safe default macro policy is trusted-only: project `.c`/`.h` files cannot
  define or undefine macros, while an explicitly trusted prelude and system
  headers may use them. Disabled, project, and full macro modes remain explicit
  alternatives. Expanded tokens retain definition and invocation provenance,
  and expanded AST is still subject to feature validation.
- Parse, validate, lower, and emit are distinct phases. Extensions attach to
  parser/AST hooks and lower into a Core C AST; arbitrary text replacement is
  not a compiler feature boundary. Existing `@name` and token-pattern rules
  remain available as a compatibility layer.
- Public ranges are half-open `[begin,end)`. Owning outputs are initialized with
  `{0}` and are published only on success. Every token, syntax, AST, symbol, and
  index handle records document generation so stale IDE objects can be rejected.
- No phase may publish partial output, dependencies, or an index after failure.
  Diagnostics and fix-its may be returned from failed/incomplete IDE parses.
- Core library APIs must support in-memory documents and cancellation; the LSP
  executable is a consumer of the generated header, not hidden state inside it.

## Milestone 6 — modular source and reproducible single-header release

- [ ] Define public/internal module boundaries without breaking the 0.19 API.
- [ ] Add deterministic amalgamation and make the checked-in root `noc.h` a
      generated artifact with a clear generated-file banner.
- [ ] Verify two generations are byte-identical and CI fails on a stale header.
- [ ] Compile all tests/examples against the generated header with GCC, Clang,
      and MSVC; add C and C++ include-only smoke tests where supported.
- [ ] Document the module contribution workflow and release generation command.

## Milestone 7 — source manager and incremental workspace

- [ ] Add stable file identities, immutable document snapshots, physical/logical
      locations, line maps, include stacks, and generated-to-source mappings.
- [ ] Add in-memory document open/update/close APIs and generation-aware handles.
- [ ] Add include search paths, trusted/system/project file classification, and
      a dependency graph usable by the build system and IDE index.
- [ ] Define allocator, ownership, thread-safety, cancellation, and bounded-work
      contracts for compiler and editor callers.
- [ ] Test disk files, unsaved overlays, stale handles, CRLF/splices, cancellation,
      and updates that preserve unaffected snapshots.

## Milestone 8 — policy-aware complete C preprocessor

- [ ] Tokenize preprocessing tokens and directives without collapsing each
      directive into one opaque token; preserve whitespace/comments/provenance.
- [ ] Implement object/function macros, variadics, argument prescan, recursive
      expansion with hide sets, stringification, token pasting, and built-ins.
- [ ] Implement includes, include guards/pragma-once behavior, conditionals,
      integer constant evaluation, diagnostics, and target predefined macros.
- [ ] Implement disabled, trusted-only, project, and full macro policies without
      leaking system-header implementation macros into project source by default.
- [ ] Add directive/expansion query APIs for IDE hover, definition, references,
      semantic highlighting, and expansion preview.
- [ ] Add focused tests for every directive/operator, malformed and incomplete
      editor input, conformance fixtures, provenance, and macro-policy bypasses.

## Milestone 9 — complete lossless ISO C11 parser and AST

- [ ] Replace lightweight C structure analysis with a grammar-complete parser for
      declarations/declarators, initializers, statements, and expressions.
- [ ] Parse structs/unions/enums, bit-fields, compound literals, designated
      initializers, `_Generic`, `_Atomic`, `_Alignas`, and static assertions.
- [ ] Preserve trivia and spelling ranges while recording expanded/logical ranges
      and physical macro/include provenance on AST nodes.
- [ ] Add error recovery and incomplete-source parsing with expected-token and
      completion-context queries; never invent a successful complete AST.
- [ ] Add parser corpus tests, round-trip checks, malformed-input regression
      suites, and parser fuzzing independently of semantic analysis.

## Milestone 10 — semantic AST, types, constants, and target ABI

- [ ] Implement C scopes/namespaces, symbols, typedef-name disambiguation,
      declarations/definitions, linkage, storage duration, and redeclaration.
- [ ] Implement canonical types, qualifiers, compatibility/composite types,
      conversions, lvalue rules, calls, and variadic function checks.
- [ ] Implement integer/floating constant expressions, initializer validation,
      object layout, enum values, `_Static_assert`, and target ABI queries.
- [ ] Add configurable data models for GCC/Clang-like targets and MSVC without
      conflating host ABI with output-target ABI.
- [ ] Add semantic tests grouped by language chapter plus negative diagnostics,
      ABI fixtures, differential compiler checks, and semantic fuzz invariants.

## Milestone 11 — feature profiles and parser-aware extension SDK

- [ ] Add discoverable feature IDs, enabled/disabled/unsupported states, reasons,
      dependencies/conflicts, and project/file/target profile composition.
- [ ] Gate macros, goto, VLAs, unions, bit-fields, raw pointers, implementation
      extensions, and future Noc features after parsing and macro expansion.
- [ ] Add declaration, statement, expression, type, and attribute parselets with
      validate/lower hooks, extension AST payloads, and explicit lifetimes.
- [ ] Keep legacy `@name` and token-pattern rules source-compatible while routing
      new structured features through parser/AST boundaries.
- [ ] Generate feature/extension metadata for IDE completion and diagnostics;
      test every public registration/query API and every built-in gate.

## Milestone 12 — Core C lowering and deterministic C emitter

- [ ] Define a validated Core C AST accepted by the emitter and target profiles
      such as `c11`, `c17`, `gnu17`, and `msvc`.
- [ ] Lower extensions with hygienic temporaries while preserving evaluation
      order, side effects, qualifiers, atomics, volatile access, and control flow.
- [ ] Emit precedence-correct, deterministic, readable C with `#line` directives,
      a machine-readable source map, dependencies, and generated-node provenance.
- [ ] Reject a target profile that cannot represent an input construct instead of
      emitting subtly different C.
- [ ] Test parse/emit/reparse, compile/run behavior, source-map diagnostics,
      deterministic output, and GCC/Clang/MSVC compilation.

## Milestone 13 — IDE index and LSP support

- [ ] Add cursor-to-token/syntax/AST/symbol queries and a persistent workspace
      index with stable symbol IDs where possible and explicit generations otherwise.
- [ ] Implement document/workspace symbols, definition/declaration, references,
      hover/type information, completion, signature help, and semantic tokens.
- [ ] Implement staged diagnostics/fix-its, safe rename, include graph queries,
      feature state/reason queries, and generated-C preview/source mapping.
- [ ] Support incremental updates, cancellation, bounded work, malformed buffers,
      and deterministic invalidation of dependent documents.
- [ ] Build a reference JSON-RPC/LSP server from the public `noc.h` API and add
      protocol fixtures for initialize, edit, diagnostics, navigation, completion,
      rename, cancellation, and generated-C preview.

## Milestone 14 — conformance, safety features, and release

- [ ] Add licensed C conformance corpora, Csmith/differential testing, sanitizer
      coverage, long fuzz campaigns, malformed-editor corpora, and Windows CI.
- [ ] Add higher-level Noc features only after the C frontend is sound: CFG and
      dataflow APIs, ownership/borrowing, defer, checked arithmetic, and safe profiles.
- [ ] Validate ownership and other flow-sensitive features across macros, goto,
      loops, cleanup, aliases, calls, and separate translation units.
- [ ] Publish the reproducibly generated single `noc.h`, version/changelog,
      compatibility notes, checksums, examples, IDE/LSP tool, and release tests.
