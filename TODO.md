# noc.h roadmap

This file is the long-running implementation checklist. A milestone is checked
only after its interfaces have tests or examples, the complete local suite
passes, and the milestone has been committed and pushed.

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
- [x] Normalize phase-2 splices inside identifiers and punctuators while
      preserving exact source slices for lossless syntax APIs.
- [x] Fuzz lexer, token cursor, syntax tree/query, preprocessor activity, and
      rewrite callback APIs with deterministic smoke and Clang libFuzzer modes.
- [ ] Add explicit token-pattern rule triggers and per-rule feature controls
      while preserving the existing `@name` registration contract.
- [ ] Versioning, changelog, release packaging, and vendored `nob.h` update policy.
