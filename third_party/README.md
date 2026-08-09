# Third-party dependencies

Normal builds and the generated `release/noc.h` do not download dependencies.
The checked-in private amalgamation under `tree-sitter/` contains everything
needed by Noc's recoverable physical C parser.

## Tree-sitter runtime

- Upstream: <https://github.com/tree-sitter/tree-sitter>
- Tag: `v0.26.12`
- Commit: `808e4b1fc06e269a107c4bd8bd936cc6fde18b00`
- Release archive SHA-256:
  `428e2b182fe38eddc100d8bd851e47c96921a69281b66abafc25ba4b0aaeeeab`
- License: MIT; copied to `tree-sitter/LICENSE.runtime`
- Bundled Unicode/ICU support-file license: preserved in full in the generated
  `tree-sitter/tree_sitter.c` payload.

## tree-sitter-c grammar

- Upstream: <https://github.com/tree-sitter/tree-sitter-c>
- Tag: `v0.24.2`
- Commit: `b780e47fc780ddc8da13afa35a3f4ed5c157823d`
- Release archive SHA-256:
  `2eeb4db31f8fa0865e45488503d13403923bcb485a1bdb637abff8c42dd97364`
- Generated language ABI: 15
- License: MIT; copied to `tree-sitter/LICENSE.grammar`

Noc applies the minimal downstream patch
`noc-c11-required-grammar-v3` before regenerating the parser. The patch adds
the ISO C11 `_Bool`, `_Complex`, `_Thread_local`, and `_Static_assert`
productions absent from the pinned upstream grammar and gives the valid
`_Atomic(type-name)` form its own node instead of recovering it as a cast
expression. It also accepts standard anonymous bit-fields and recognizes the
C23 `static_assert` alias without making that alias part of the C11 profile.
Direct AST, recovery, and parser-corpus tests cover these downstream rules.

Regeneration uses Tree-sitter CLI `v0.25.4` and preserves language ABI 15. The
generator downloads the matching official compressed executable for Linux
x64/arm64, macOS x64/arm64, or Windows x64 and verifies its platform-specific
SHA-256 before executing it. The archive SHA-256 values remain the upstream
authentication roots; the commit values record the peeled upstream tags for
provenance. Exact CLI asset names and hashes live next to the dependency pins in
`tools/vendor_tree_sitter.py`.

## Local transformations and updates

`tools/vendor_tree_sitter.py` authenticates both complete upstream archives,
applies the exact downstream grammar transformation to a temporary copy,
regenerates it with the authenticated CLI, checks the runtime/grammar ABI and
matching `tree_sitter/parser.h`, recursively inlines the fixed native runtime
and generated grammar include graph, and emits:

- `tree-sitter/tree_sitter_private.h`: namespaced implementation-only ABI;
- `tree-sitter/tree_sitter.c`: deterministic native runtime/grammar payload;
- `tree-sitter/LICENSE.runtime`: authenticated runtime license; and
- `tree-sitter/LICENSE.grammar`: authenticated grammar license.

Tree-sitter API/type names, generated grammar identifiers, and upstream macros
are prefixed so they do not become Noc public API or collide with another linked
Tree-sitter. Consumer definitions of Tree-sitter feature/debug switches,
portable-endian `HAVE_*` probes, and bundled ICU `U_*` configuration cannot
configure the private payload; WASM remains disabled by its undefined private
feature switch. The generator rejects unprefixed upstream macro mutations
outside an audited standard-library allowlist. Complete license notices and
exact hashes of every selected upstream input are preserved in the generated C
payload and therefore in the standalone release header.

The generator also supplies private, standard-C UTF-16 endian helpers and the
standard POSIX `fdopen` prototype used only by upstream debug-graph code. This
keeps strict C11 consumers independent of feature-test macros such as
`_DEFAULT_SOURCE`; it strips an upstream NetBSD workaround that undefines
`_POSIX_C_SOURCE`, and does not set or change feature-test macros in consumer
code.

To update or reproduce from the pinned archives:

```console
$ python3 tools/vendor_tree_sitter.py
$ ./nob header
$ ./nob test
```

This maintainer-only operation requires Python 3, Node.js (used to evaluate the
pinned `grammar.js`), and network access. Normal builds and all consumers of
`release/noc.h` need none of them.

For an already extracted, audited pair of source trees:

```console
$ python3 tools/vendor_tree_sitter.py \
    --runtime-root /path/to/tree-sitter-0.26.12 \
    --grammar-root /path/to/tree-sitter-c-0.24.2
```

`--check` performs the same generation and fails instead of replacing stale
outputs. Updating a pin requires updating the constants in the generator and
this manifest, reviewing upstream licenses and platform requirements, then
running all release-header, module, sanitizer, and CI gates.
