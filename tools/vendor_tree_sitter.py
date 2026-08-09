#!/usr/bin/env python3
"""Build Noc's deterministic, private Tree-sitter C amalgamation.

Normal builds use the checked-in generated files and do not need Python or
network access. Maintainers run this script only when updating the pinned
dependencies. The complete upstream archives are authenticated before their
selected native runtime and generated C grammar sources are amalgamated.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import os
from pathlib import Path
import re
import sys
import tarfile
import tempfile
import urllib.request


RUNTIME_VERSION = "0.26.12"
RUNTIME_COMMIT = "808e4b1fc06e269a107c4bd8bd936cc6fde18b00"
RUNTIME_URL = (
    "https://github.com/tree-sitter/tree-sitter/archive/refs/tags/"
    f"v{RUNTIME_VERSION}.tar.gz"
)
RUNTIME_ARCHIVE_SHA256 = (
    "428e2b182fe38eddc100d8bd851e47c96921a69281b66abafc25ba4b0aaeeeab"
)

GRAMMAR_VERSION = "0.24.2"
GRAMMAR_COMMIT = "b780e47fc780ddc8da13afa35a3f4ed5c157823d"
GRAMMAR_URL = (
    "https://github.com/tree-sitter/tree-sitter-c/archive/refs/tags/"
    f"v{GRAMMAR_VERSION}.tar.gz"
)
GRAMMAR_ARCHIVE_SHA256 = (
    "2eeb4db31f8fa0865e45488503d13403923bcb485a1bdb637abff8c42dd97364"
)

LANGUAGE_ABI = 15
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
QUOTED_INCLUDE = re.compile(
    r'^(?P<indent>\s*)#\s*include\s*"(?P<name>[^"]+)"(?P<tail>[^\r\n]*)$',
    re.MULTILINE,
)
MACRO_DEFINITION = re.compile(
    r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)", re.MULTILINE
)

# Configuration queried, but not necessarily defined, by upstream must still be
# private. Prefixes cover Tree-sitter's debug switches, portable-endian build
# probes, and the bundled ICU compatibility layer. Host/compiler feature macros
# such as _WIN32, __GNUC__, and NDEBUG intentionally remain visible.
PRIVATE_CONFIGURATION_MACROS = {
    "TREE_SITTER_FEATURE_WASM",
    "TREE_SITTER_HIDE_SYMBOLS",
    "UCHAR_TYPE",
}
PRIVATE_CONFIGURATION_MACRO_PREFIXES = ("DEBUG_", "HAVE_", "U_")

# These standard <stdint.h> macros may have upstream fallback definitions after
# earlier uses. Keep their standard names and normal system-header ownership.
STANDARD_LIBRARY_MACROS = {
    "INT8_MAX",
    "INT8_MIN",
    "INT16_MAX",
    "INT16_MIN",
    "INT32_MAX",
    "INT32_MIN",
    "INT64_C",
    "UINT8_MAX",
    "UINT16_MAX",
    "UINT32_MAX",
    "UINT64_C",
}

# This manifest closes the same-translation-unit C namespace for the pinned
# inputs. It contains upstream file-scope typedefs/tags/enumerators, constants,
# and static helpers whose names do not follow Tree-sitter's TS/ts_ convention.
# Updating either pin requires re-auditing this list and the hostile-name test.
PRIVATE_IDENTIFIERS = {
    "AnalysisState",
    "AnalysisStateEntry",
    "AnalysisStateSet",
    "AnalysisSubgraph",
    "AnalysisSubgraphArray",
    "AnalysisSubgraphNode",
    "BYTE_ORDER_MARK",
    "CAPTURE_LIST_NONE",
    "CaptureList",
    "CaptureListPool",
    "CaptureQuantifiers",
    "ColumnData",
    "CursorChildIterator",
    "DEFAULT_RANGE",
    "DecodeFunction",
    "Edit",
    "EditEntry",
    "ErrorComparison",
    "ErrorComparisonNone",
    "ErrorComparisonPreferLeft",
    "ErrorComparisonPreferRight",
    "ErrorComparisonTakeLeft",
    "ErrorComparisonTakeRight",
    "ErrorStatus",
    "ExternalScannerState",
    "Iterator",
    "IteratorComparison",
    "IteratorDiffers",
    "IteratorMatches",
    "IteratorMayDiffer",
    "LENGTH_MAX",
    "LENGTH_UNDEFINED",
    "Length",
    "Lexer",
    "LookaheadIterator",
    "MAX_COST_DIFFERENCE",
    "MAX_SUMMARY_DEPTH",
    "MAX_VERSION_COUNT",
    "MAX_VERSION_COUNT_OVERFLOW",
    "MutableSubtree",
    "MutableSubtreeArray",
    "NONE",
    "NodeChildIterator",
    "OP_COUNT_PER_PARSER_CALLBACK_CHECK",
    "OP_COUNT_PER_QUERY_CALLBACK_CHECK",
    "OldUChar",
    "PARENT_DONE",
    "PATTERN_DONE_MARKER",
    "PatternEntry",
    "QueryAnalysis",
    "QueryPattern",
    "QueryState",
    "QueryStateList",
    "QueryStep",
    "ROOT_FIELD",
    "ReduceAction",
    "ReduceActionSet",
    "ReusableNode",
    "Slice",
    "Stack",
    "StackAction",
    "StackActionNone",
    "StackActionPop",
    "StackActionStop",
    "StackCallback",
    "StackEntry",
    "StackHead",
    "StackIterator",
    "StackLink",
    "StackNode",
    "StackNodeArray",
    "StackSlice",
    "StackSliceArray",
    "StackStatus",
    "StackStatusActive",
    "StackStatusHalted",
    "StackStatusPaused",
    "StackSummary",
    "StackSummaryEntry",
    "StackVersion",
    "StatePredecessorMap",
    "StepOffset",
    "Stream",
    "StringData",
    "Subtree",
    "SubtreeArray",
    "SubtreeHeapData",
    "SubtreeInlineData",
    "SubtreePool",
    "SummarizeStackSession",
    "SymbolTable",
    "TableEntry",
    "TokenCache",
    "TreeCursor",
    "TreeCursorEntry",
    "TreeCursorStep",
    "TreeCursorStepHidden",
    "TreeCursorStepNone",
    "TreeCursorStepVisible",
    "UBool",
    "UChar",
    "UChar32",
    "WILDCARD_SYMBOL",
    "_array__assign",
    "_array__erase",
    "_array__grow",
    "_array__reserve",
    "_array__splice",
    "_array__swap",
    "_ts_dup",
    "analysis_state__compare",
    "analysis_state__has_supertype",
    "analysis_state__recursion_depth",
    "analysis_state__top",
    "analysis_state_pool__clone_or_reuse",
    "analysis_state_set__clear",
    "analysis_state_set__delete",
    "analysis_state_set__insert_sorted",
    "analysis_state_set__push",
    "analysis_subgraph_node__compare",
    "atomic_dec",
    "atomic_inc",
    "atomic_load",
    "be16toh",
    "callback__abort",
    "callback__debug_message",
    "callback__lexer_advance",
    "callback__lexer_eof",
    "callback__lexer_get_column",
    "callback__lexer_is_at_included_range_start",
    "callback__lexer_mark_end",
    "callback__noop",
    "capture_list_pool_acquire",
    "capture_list_pool_delete",
    "capture_list_pool_get",
    "capture_list_pool_get_mut",
    "capture_list_pool_is_empty",
    "capture_list_pool_new",
    "capture_list_pool_release",
    "capture_list_pool_reset",
    "capture_quantifier_for_id",
    "capture_quantifiers_add_all",
    "capture_quantifiers_add_for_id",
    "capture_quantifiers_clear",
    "capture_quantifiers_delete",
    "capture_quantifiers_join_all",
    "capture_quantifiers_mul",
    "capture_quantifiers_new",
    "capture_quantifiers_replace",
    "copy",
    "copy_string",
    "copy_strings",
    "copy_unsized_static_array",
    "delete_partially_loaded_language",
    "finished_state_erase",
    "finished_state_pop",
    "finished_state_precedes",
    "finished_state_sift_down",
    "finished_state_sift_up",
    "finished_state_swap",
    "get_builtin_extern",
    "iterator_advance",
    "iterator_ascend",
    "iterator_compare",
    "iterator_descend",
    "iterator_done",
    "iterator_end_position",
    "iterator_get_visible_state",
    "iterator_new",
    "iterator_print_state",
    "iterator_start_position",
    "iterator_tree_is_visible",
    "le16toh",
    "length_add",
    "length_backtrack",
    "length_is_undefined",
    "length_min",
    "length_saturating_sub",
    "length_sub",
    "length_zero",
    "name_eq",
    "point__new",
    "point_add",
    "point_eq",
    "point_gt",
    "point_gte",
    "point_lt",
    "point_lte",
    "point_sub",
    "pop_all_callback",
    "pop_count_callback",
    "pop_error_callback",
    "pop_pending_callback",
    "quantifier_add",
    "quantifier_join",
    "quantifier_mul",
    "query_analysis__delete",
    "query_analysis__new",
    "query_step__add_capture",
    "query_step__new",
    "query_step__remove_capture",
    "range_intersects",
    "range_within",
    "read_u8",
    "read_uleb128",
    "reusable_node_advance",
    "reusable_node_advance_past_leaf",
    "reusable_node_byte_offset",
    "reusable_node_clear",
    "reusable_node_delete",
    "reusable_node_descend",
    "reusable_node_new",
    "reusable_node_reset",
    "reusable_node_tree",
    "set_contains",
    "stack__iter",
    "stack__subtree_is_equivalent",
    "stack__subtree_node_count",
    "stack_head_delete",
    "stack_node_add_link",
    "stack_node_new",
    "stack_node_release",
    "stack_node_retain",
    "state_predecessor_map_add",
    "state_predecessor_map_delete",
    "state_predecessor_map_get",
    "state_predecessor_map_new",
    "stream_advance",
    "stream_is_ident_start",
    "stream_new",
    "stream_offset",
    "stream_reset",
    "stream_scan_identifier",
    "stream_skip_whitespace",
    "summarize_stack_callback",
    "symbol_table_delete",
    "symbol_table_id_for_name",
    "symbol_table_insert_name",
    "symbol_table_name_for_id",
    "symbol_table_new",
    "wasm_dylink_info__parse",
    "wasm_functype_new_4_0",
    "wasm_memory__contains",
    "wasm_memory__read",
    "wasm_memory__string_length",
}

DECLARATIONS_GUARD = "NOC__VENDOR_TREE_SITTER_DECLARATIONS_INCLUDED"
IMPLEMENTATION_GUARD = "NOC__VENDOR_TREE_SITTER_IMPLEMENTATION_INCLUDED"

# query.c removes a consumer feature-test macro on NetBSD so portable/endian.h
# can expose non-standard helpers. Noc supplies its own private UTF-16 endian
# adapters, so that process-wide preprocessor mutation is neither needed nor
# acceptable in a single-header implementation.
NETBSD_POSIX_WORKAROUND = """/*
 * On NetBSD, defining standard requirements like this removes symbols
 * from the namespace; however, we need non-standard symbols for
 * endian.h.
 */
#if defined(__NetBSD__) && defined(_POSIX_C_SOURCE)
#undef _POSIX_C_SOURCE
#endif
"""
NETBSD_POSIX_REPLACEMENT = (
    "/* noc vendor: omitted upstream NetBSD _POSIX_C_SOURCE mutation; "
    "private endian adapters are used. */\n"
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def download_and_extract(url: str, expected_sha256: str, destination: Path) -> Path:
    print(f"vendor-tree-sitter: downloading {url}", file=sys.stderr)
    with urllib.request.urlopen(url) as response:
        archive = response.read()
    actual = sha256(archive)
    if actual != expected_sha256:
        raise RuntimeError(
            f"archive hash mismatch for {url}: expected {expected_sha256}, got {actual}"
        )
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as tar:
        members = tar.getmembers()
        for member in members:
            member_path = Path(member.name)
            if member_path.is_absolute() or ".." in member_path.parts:
                raise RuntimeError(f"unsafe archive member: {member.name}")
        tar.extractall(destination, members=members)
    roots = [entry for entry in destination.iterdir() if entry.is_dir()]
    if len(roots) != 1:
        raise RuntimeError(f"expected one archive root in {destination}")
    return roots[0]


class Inliner:
    def __init__(self, roots: list[Path], labels: dict[Path, str]) -> None:
        self.roots = [root.resolve() for root in roots]
        self.labels = {root.resolve(): label for root, label in labels.items()}
        self.seen_content: set[str] = set()
        self.input_hashes: list[tuple[str, str]] = []

    def label(self, path: Path) -> str:
        resolved = path.resolve()
        for root in self.roots:
            try:
                relative = resolved.relative_to(root)
            except ValueError:
                continue
            return f"{self.labels[root]}/{relative.as_posix()}"
        raise RuntimeError(f"input escaped allowed roots: {path}")

    def resolve_include(self, current: Path, name: str) -> Path | None:
        candidates = [current.parent / name, current.parent / "wasm" / name]
        candidates.extend(root / name for root in self.roots)
        for candidate in candidates:
            if candidate.is_file():
                resolved = candidate.resolve()
                if any(
                    resolved == root or root in resolved.parents for root in self.roots
                ):
                    return resolved
        return None

    def expand(self, path: Path) -> str:
        path = path.resolve()
        raw = path.read_bytes()
        digest = sha256(raw)
        label = self.label(path)
        if digest in self.seen_content:
            return f"/* noc vendor: duplicate upstream input omitted: {label} */\n"
        self.seen_content.add(digest)
        self.input_hashes.append((label, digest))
        text = raw.decode("utf-8").replace("\r\n", "\n")
        text = re.sub(r"[ \t]+(?=\n|$)", "", text)
        pieces: list[str] = [f"/* noc vendor: begin {label} */\n"]
        cursor = 0
        for match in QUOTED_INCLUDE.finditer(text):
            pieces.append(text[cursor : match.start()])
            included = self.resolve_include(path, match.group("name"))
            if included is None:
                raise RuntimeError(
                    f"cannot resolve quoted include {match.group('name')!r} in {label}"
                )
            pieces.append(self.expand(included))
            cursor = match.end()
        pieces.append(text[cursor:])
        if pieces[-1] and not pieces[-1].endswith("\n"):
            pieces.append("\n")
        pieces.append(f"/* noc vendor: end {label} */\n")
        return "".join(pieces)


def is_private_configuration_macro(name: str) -> bool:
    return name in PRIVATE_CONFIGURATION_MACROS or name.startswith(
        PRIVATE_CONFIGURATION_MACRO_PREFIXES
    )


def identifier_mapping(
    text: str,
) -> tuple[dict[str, str], set[str], set[str]]:
    identifiers = set(IDENTIFIER.findall(text))
    macro_names = set(MACRO_DEFINITION.findall(text))
    configuration_names = {
        name for name in identifiers if is_private_configuration_macro(name)
    }
    mapping: dict[str, str] = {}

    for name in identifiers:
        if name.startswith("TS"):
            mapping[name] = f"Noc__Vendor_{name}"
        elif name.startswith(("ts_", "_ts_")) or name == "tree_sitter_c":
            mapping[name] = f"noc__vendor_{name}"
        elif name.startswith(("sym_", "anon_sym_", "aux_sym_", "alias_sym_", "field_")):
            mapping[name] = f"noc__vendor_{name}"
        elif name in PRIVATE_IDENTIFIERS:
            mapping[name] = f"noc__vendor_{name}"

    for name in configuration_names:
        mapping[name] = f"NOC__VENDOR_MACRO_{name}"

    for name in macro_names:
        if name in STANDARD_LIBRARY_MACROS | {"be16toh", "le16toh"}:
            continue
        mapping[name] = f"NOC__VENDOR_MACRO_{name}"

    return mapping, macro_names, configuration_names


def rewrite_identifiers(text: str, mapping: dict[str, str]) -> str:
    """Rewrite C identifier tokens while leaving comments and literals intact."""
    output: list[str] = []
    index = 0
    count = len(text)
    while index < count:
        if text.startswith("//", index):
            end = text.find("\n", index + 2)
            if end < 0:
                end = count
            output.append(text[index:end])
            index = end
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            if end < 0:
                raise RuntimeError("unterminated comment in upstream input")
            end += 2
            output.append(text[index:end])
            index = end
            continue
        character = text[index]
        if character in {'"', "'"}:
            quote = character
            end = index + 1
            while end < count:
                if text[end] == "\\":
                    end += 2
                    continue
                if text[end] == quote:
                    end += 1
                    break
                end += 1
            else:
                raise RuntimeError("unterminated literal in upstream input")
            output.append(text[index:end])
            index = end
            continue
        match = IDENTIFIER.match(text, index)
        if match:
            name = match.group(0)
            output.append(mapping.get(name, name))
            index = match.end()
            continue
        output.append(character)
        index += 1
    return "".join(output)


def license_comment(title: str, version: str, commit: str, text: str) -> str:
    # Some bundled notices quote source comments. Keep the complete notice while
    # preventing its examples from terminating this generated C comment.
    escaped = text.replace("/*", "/ *").replace("*/", "* /")
    body = "\n".join(
        f"   {line}" if line else "" for line in escaped.rstrip().split("\n")
    )
    return (
        f"/* {title} {version}, commit {commit}\n"
        f"\n{body}\n"
        "*/\n\n"
    )


def validate_inputs(runtime_root: Path, grammar_root: Path) -> None:
    api = read_text(runtime_root / "lib/include/tree_sitter/api.h")
    grammar = read_text(grammar_root / "src/parser.c")
    runtime_parser = (runtime_root / "lib/src/parser.h").read_bytes()
    grammar_parser = (grammar_root / "src/tree_sitter/parser.h").read_bytes()
    if f"#define TREE_SITTER_LANGUAGE_VERSION {LANGUAGE_ABI}" not in api:
        raise RuntimeError("unexpected Tree-sitter runtime language ABI")
    if f"#define LANGUAGE_VERSION {LANGUAGE_ABI}" not in grammar:
        raise RuntimeError("unexpected tree-sitter-c generated language ABI")
    if runtime_parser != grammar_parser:
        raise RuntimeError("runtime and grammar copies of tree_sitter/parser.h differ")
    if "tree_sitter_c(void)" not in grammar:
        raise RuntimeError("tree-sitter-c grammar entry point is missing")


def generate(runtime_root: Path, grammar_root: Path) -> tuple[str, str, str, str]:
    validate_inputs(runtime_root, grammar_root)
    runtime_src = runtime_root / "lib/src"
    runtime_include = runtime_root / "lib/include"
    grammar_src = grammar_root / "src"
    roots = [runtime_src, runtime_include, grammar_src]
    labels = {
        runtime_src: f"tree-sitter-{RUNTIME_VERSION}/lib/src",
        runtime_include: f"tree-sitter-{RUNTIME_VERSION}/lib/include",
        grammar_src: f"tree-sitter-c-{GRAMMAR_VERSION}/src",
    }
    inliner = Inliner(roots, labels)
    runtime = inliner.expand(runtime_src / "lib.c")
    grammar = inliner.expand(grammar_src / "parser.c")
    combined = runtime + "\n" + grammar
    if combined.count(NETBSD_POSIX_WORKAROUND) != 1:
        raise RuntimeError("unexpected NetBSD _POSIX_C_SOURCE workaround")
    combined = combined.replace(
        NETBSD_POSIX_WORKAROUND, NETBSD_POSIX_REPLACEMENT, 1
    )
    mapping, macro_names, configuration_names = identifier_mapping(combined)
    rewritten = rewrite_identifiers(combined, mapping)

    api_inliner = Inliner(roots, labels)
    api = api_inliner.expand(runtime_include / "tree_sitter/api.h")
    rewritten_api = rewrite_identifiers(api, mapping)

    runtime_license = read_text(runtime_root / "LICENSE")
    grammar_license = read_text(grammar_root / "LICENSE")
    icu_license = read_text(runtime_src / "unicode/LICENSE")
    notices = (
        license_comment(
            "Tree-sitter runtime", RUNTIME_VERSION, RUNTIME_COMMIT, runtime_license
        )
        + license_comment(
            "tree-sitter-c", GRAMMAR_VERSION, GRAMMAR_COMMIT, grammar_license
        )
        + license_comment(
            "Unicode/ICU support files bundled by Tree-sitter",
            "upstream copy",
            RUNTIME_COMMIT,
            icu_license,
        )
    )

    private_header = (
        "/* Generated by tools/vendor_tree_sitter.py; do not edit.\n"
        "   This private ABI is intentionally absent from Noc's public API. */\n"
        f"#ifndef {DECLARATIONS_GUARD}\n"
        f"#define {DECLARATIONS_GUARD} 1\n"
        "#define NOC__VENDOR_MACRO_TREE_SITTER_HIDE_SYMBOLS 1\n"
        f"{rewritten_api}\n"
        "const Noc__Vendor_TSLanguage *noc__vendor_tree_sitter_c(void);\n"
        f"#endif /* {DECLARATIONS_GUARD} */\n"
    )

    # The first expanded api.h is replaced with the wrapped private declaration
    # block so the same declarations serve both the standalone module and the
    # final single-header implementation.
    api_begin = (
        "/* noc vendor: begin "
        f"tree-sitter-{RUNTIME_VERSION}/lib/include/tree_sitter/api.h */"
    )
    api_end = (
        "/* noc vendor: end "
        f"tree-sitter-{RUNTIME_VERSION}/lib/include/tree_sitter/api.h */"
    )
    rewritten_api_block = rewrite_identifiers(
        combined[combined.find(api_begin) : combined.find(api_end) + len(api_end) + 1],
        mapping,
    )
    if not rewritten_api_block or rewritten_api_block not in rewritten:
        raise RuntimeError("could not locate expanded Tree-sitter API block")
    rewritten = rewritten.replace(rewritten_api_block, private_header, 1)

    cleanup_names = sorted(
        {mapping[name] for name in macro_names if name in mapping}
        | {mapping["TREE_SITTER_HIDE_SYMBOLS"]}
    )
    cleanup = "".join(f"#undef {name}\n" for name in cleanup_names)

    input_manifest = "\n".join(
        f"   {digest}  {label}" for label, digest in sorted(inliner.input_hashes)
    )
    portability = (
        "/* Noc portability adapters. Strict C11 modes on some Unix systems hide\n"
        "   fdopen and endian conversion declarations behind feature-test macros.\n"
        "   Keep consumer flags untouched: only the two private UTF-16 helpers\n"
        "   are replaced, and the POSIX function receives its standard prototype. */\n"
        "#include <assert.h>\n"
        "#include <ctype.h>\n"
        "#include <inttypes.h>\n"
        "#include <limits.h>\n"
        "#include <stdarg.h>\n"
        "#include <stdbool.h>\n"
        "#include <stddef.h>\n"
        "#include <stdint.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "#include <wctype.h>\n"
        "#if defined(_WIN32)\n"
        "#include <io.h>\n"
        "#include <windows.h>\n"
        "#else\n"
        "#include <unistd.h>\n"
        "#endif\n"
        "#define NOC__VENDOR_MACRO_TREE_SITTER_HIDE_SYMBOLS 1\n"
        "#if !defined(_WIN32)\n"
        "extern FILE *fdopen(int file_descriptor, const char *mode);\n"
        "static inline uint16_t noc__vendor_swap_u16(uint16_t value) {\n"
        "  return (uint16_t)((value << 8) | (value >> 8));\n"
        "}\n"
        "static inline uint16_t noc__vendor_le16toh(uint16_t value) {\n"
        "  const uint16_t one = 1;\n"
        "  return *(const unsigned char *)&one\n"
        "    ? value : noc__vendor_swap_u16(value);\n"
        "}\n"
        "static inline uint16_t noc__vendor_be16toh(uint16_t value) {\n"
        "  const uint16_t one = 1;\n"
        "  return *(const unsigned char *)&one\n"
        "    ? noc__vendor_swap_u16(value) : value;\n"
        "}\n\n"
        "#endif\n"
        "#if defined(__GNUC__) || defined(__clang__)\n"
        "#pragma GCC visibility push(hidden)\n"
        "#endif\n"
        "#if defined(__clang__)\n"
        "#pragma clang diagnostic push\n"
        "#pragma clang diagnostic ignored \"-Wunused-function\"\n"
        "#endif\n"
        "#if defined(_MSC_VER)\n"
        "#pragma warning(push)\n"
        "#pragma warning(disable : 4018 4244 4701)\n"
        "#endif\n"
    )
    source = (
        "/* Generated by tools/vendor_tree_sitter.py; do not edit.\n"
        "\n"
        f"   Tree-sitter runtime v{RUNTIME_VERSION} ({RUNTIME_COMMIT})\n"
        f"   tree-sitter-c v{GRAMMAR_VERSION} ({GRAMMAR_COMMIT}), ABI {LANGUAGE_ABI}\n"
        "\n"
        "   Local transformations: recursively inline the fixed native source\n"
        "   graph, prefix Tree-sitter APIs/types/generated grammar identifiers,\n"
        "   prefix upstream macros, normalize trailing horizontal whitespace,\n"
        "   disable exported visibility, and omit WASM by leaving\n"
        "   TREE_SITTER_FEATURE_WASM undefined. Upstream source hashes:\n"
        f"{input_manifest}\n"
        "*/\n\n"
        f"{notices}"
        "#ifndef NOC_INTERNAL_H_INCLUDED\n"
        "#define NOC__INDIVIDUAL_SOURCE 1\n"
        "#endif\n"
        "#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)\n"
        f"#ifndef {IMPLEMENTATION_GUARD}\n"
        f"#define {IMPLEMENTATION_GUARD} 1\n"
        "#if defined(__cplusplus)\n"
        '#error "NOC_IMPLEMENTATION with the embedded C parser must be compiled as C11"\n'
        "#endif\n"
        f"{portability}"
        f"{rewritten}\n"
        "#if defined(_MSC_VER)\n"
        "#pragma warning(pop)\n"
        "#endif\n"
        "#if defined(__clang__)\n"
        "#pragma clang diagnostic pop\n"
        "#endif\n"
        "#if defined(__GNUC__) || defined(__clang__)\n"
        "#pragma GCC visibility pop\n"
        "#endif\n"
        f"{cleanup}"
        f"#endif /* {IMPLEMENTATION_GUARD} */\n"
        "#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */\n"
    )
    forbidden_marker = "NOC__VENDOR_FORBIDDEN_CONFIGURATION_TOKEN"
    configuration_probe = rewrite_identifiers(
        source, {name: forbidden_marker for name in configuration_names}
    )
    if forbidden_marker in configuration_probe:
        raise RuntimeError("unprefixed Tree-sitter configuration token")
    raw_mutations = {
        name
        for name in re.findall(
            r"^\s*#\s*(?:define|undef)\s+([A-Za-z_][A-Za-z0-9_]*)",
            source,
            re.MULTILINE,
        )
        if not name.startswith(("NOC__VENDOR_", "noc__vendor_"))
        and name not in STANDARD_LIBRARY_MACROS
        and name != "NOC__INDIVIDUAL_SOURCE"
    }
    if raw_mutations:
        names = ", ".join(sorted(raw_mutations))
        raise RuntimeError(f"unprefixed upstream macro mutation: {names}")
    return private_header, source, runtime_license, grammar_license


def write_if_changed(path: Path, content: str, check: bool) -> bool:
    encoded = content.encode("utf-8")
    current = path.read_bytes() if path.exists() else None
    if current == encoded:
        return True
    if check:
        print(f"vendor-tree-sitter: stale generated file: {path}", file=sys.stderr)
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(encoded)
    os.replace(temporary, path)
    print(f"vendor-tree-sitter: wrote {path}", file=sys.stderr)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-root", type=Path)
    parser.add_argument("--grammar-root", type=Path)
    parser.add_argument("--check", action="store_true")
    parser.add_argument(
        "--output-directory", type=Path, default=Path("third_party/tree-sitter")
    )
    arguments = parser.parse_args()
    if (arguments.runtime_root is None) != (arguments.grammar_root is None):
        parser.error("--runtime-root and --grammar-root must be supplied together")

    try:
        if arguments.runtime_root is not None:
            private_header, source, runtime_license, grammar_license = generate(
                arguments.runtime_root.resolve(), arguments.grammar_root.resolve()
            )
        else:
            with tempfile.TemporaryDirectory(prefix="noc-tree-sitter-") as temporary:
                temporary_path = Path(temporary)
                runtime_root = download_and_extract(
                    RUNTIME_URL,
                    RUNTIME_ARCHIVE_SHA256,
                    temporary_path / "runtime",
                )
                grammar_root = download_and_extract(
                    GRAMMAR_URL,
                    GRAMMAR_ARCHIVE_SHA256,
                    temporary_path / "grammar",
                )
                private_header, source, runtime_license, grammar_license = generate(
                    runtime_root, grammar_root
                )
        output = arguments.output_directory
        ok = write_if_changed(
            output / "tree_sitter_private.h", private_header, arguments.check
        )
        ok = write_if_changed(output / "tree_sitter.c", source, arguments.check) and ok
        ok = write_if_changed(
            output / "LICENSE.runtime", runtime_license, arguments.check
        ) and ok
        ok = write_if_changed(
            output / "LICENSE.grammar", grammar_license, arguments.check
        ) and ok
        return 0 if ok else 1
    except (OSError, RuntimeError, tarfile.TarError) as error:
        print(f"vendor-tree-sitter: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
