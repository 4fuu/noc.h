#include "test_support.h"

static bool cancel_immediately(void *user_data)
{
    size_t *calls = (size_t *)user_data;
    *calls += 1;
    return true;
}

typedef struct {
    size_t calls;
    size_t cancel_after;
} Delayed_Cancel;

static bool cancel_after_delay(void *user_data)
{
    Delayed_Cancel *state = (Delayed_Cancel *)user_data;
    state->calls += 1;
    return state->calls >= state->cancel_after;
}

static size_t find_flag(const Noc_C_Ast *ast, unsigned int flag)
{
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        if (node && (node->flags & flag) != 0) return index;
    }
    return NOC_C_AST_NODE_NONE;
}

static size_t find_kind(const Noc_C_Ast *ast, Noc_C_Ast_Kind kind)
{
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        if (node && node->kind == kind) return index;
    }
    return NOC_C_AST_NODE_NONE;
}

static void test_missing_token_keeps_expected_spelling(void)
{
    static const char source[] = "int f(void) { return 1 }\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Ast ast = {0};
    const Noc_C_Ast_Node *missing;
    Noc_C_Ast_Expected expected;
    Noc_Location location;
    size_t missing_index;

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "ast/missing.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&tree, noc_c_ast_default_options(), &ast) ==
          NOC_C_AST_OK);
    CHECK(noc_c_ast_is_valid(&ast));
    CHECK(!noc_c_ast_is_syntax_complete(&ast));
    CHECK((noc_c_ast_issues(&ast) & NOC_C_AST_ISSUE_MISSING) != 0);
    CHECK((noc_c_ast_issues(&ast) & NOC_C_AST_ISSUE_PARSE_ERROR) != 0);
    missing_index = find_flag(&ast, NOC_C_AST_NODE_MISSING);
    CHECK(missing_index != NOC_C_AST_NODE_NONE);
    missing = noc_c_ast_node_at(&ast, missing_index);
    CHECK(missing != NULL);
    CHECK(missing && missing->kind == NOC_C_AST_KIND_MISSING);
    CHECK(missing && missing->bytes.begin == missing->bytes.end);
    CHECK(noc_c_ast_node_source(&ast, missing_index).count == 0);
    expected = noc_c_ast_node_expected(&ast, missing_index);
    CHECK(expected.kind == NOC_C_AST_EXPECTED_PUNCTUATOR);
    CHECK(slice_equals(expected.spelling, ";"));
    location = noc_c_ast_node_location(&ast, missing_index);
    CHECK(location.line == 1);
    CHECK(location.column == 23);

    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    expected = noc_c_ast_node_expected(&ast, missing_index);
    CHECK(slice_equals(expected.spelling, ";"));
    noc_c_ast_free(&ast);
}

static void test_missing_named_symbol_keeps_only_stable_category(void)
{
    static const char source[] = "int f(void) { if () return 1; }\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Ast ast = {0};
    Noc_C_Ast_Expected expected;
    size_t missing_index;

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "ast/missing-named.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&tree, noc_c_ast_default_options(), &ast) ==
          NOC_C_AST_OK);
    missing_index = find_flag(&ast, NOC_C_AST_NODE_MISSING);
    CHECK(missing_index != NOC_C_AST_NODE_NONE);
    expected = noc_c_ast_node_expected(&ast, missing_index);
    CHECK(expected.kind == NOC_C_AST_EXPECTED_IDENTIFIER);
    CHECK(expected.spelling.data == NULL);
    CHECK(expected.spelling.count == 0);
    CHECK((noc_c_ast_node_at(&ast, missing_index)->flags &
           NOC_C_AST_NODE_UNKNOWN_DETAIL) == 0);

    noc_c_ast_free(&ast);
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static void test_error_and_skipped_source_recovery(void)
{
    static const char incomplete[] =
        "int unfinished(int x) {\n"
        "    if (x > 0) return x + ;\n"
        "}\n";
    static const char skipped[] = "\vint value;\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Ast ast = {0};

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "ast/incomplete.c",
                                      incomplete,
                                      sizeof(incomplete) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&tree, noc_c_ast_default_options(), &ast) ==
          NOC_C_AST_OK);
    CHECK(!noc_c_ast_is_syntax_complete(&ast));
    CHECK((noc_c_ast_issues(&ast) & NOC_C_AST_ISSUE_PARSE_ERROR) != 0);
    CHECK(find_kind(&ast, NOC_C_AST_KIND_FUNCTION_DEFINITION) !=
          NOC_C_AST_NODE_NONE);
    CHECK(find_kind(&ast, NOC_C_AST_KIND_IF_STATEMENT) != NOC_C_AST_NODE_NONE);
    CHECK(find_flag(&ast, NOC_C_AST_NODE_ERROR) != NOC_C_AST_NODE_NONE ||
          find_flag(&ast, NOC_C_AST_NODE_MISSING) != NOC_C_AST_NODE_NONE);

    noc_c_ast_free(&ast);
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    CHECK(noc_workspace_open_document(&workspace,
                                      "ast/skipped.c",
                                      skipped,
                                      sizeof(skipped) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&tree, noc_c_ast_default_options(), &ast) ==
          NOC_C_AST_OK);
    CHECK(!noc_c_ast_is_syntax_complete(&ast));
    CHECK((noc_c_ast_issues(&ast) & NOC_C_AST_ISSUE_SKIPPED_SOURCE) != 0);
    CHECK(noc_c_ast_node_at(&ast, 0)->bytes.begin == 0);
    CHECK(noc_c_ast_node_at(&ast, 0)->bytes.end == sizeof(skipped) - 1);
    CHECK(slice_equals(noc_c_ast_node_source(&ast, 0), skipped));

    noc_c_ast_free(&ast);
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static void check_preserved(const Noc_C_Ast *ast,
                            Noc_C_Ast_Impl *implementation,
                            size_t generation,
                            size_t node_count,
                            const char *source)
{
    CHECK(ast->impl == implementation);
    CHECK(ast->generation == generation);
    CHECK(noc_c_ast_is_valid(ast));
    CHECK(noc_c_ast_node_count(ast) == node_count);
    CHECK(slice_equals(noc_c_ast_node_source(ast, 0), source));
}

static void test_limits_cancellation_and_rebuild_are_transactional(void)
{
    static const char baseline_source[] = "int preserved;\n";
    static const char repeated[] = "int repeated;\n";
    const size_t large_count = 16 * 1024;
    char *large_source = (char *)malloc(large_count);
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot baseline = {0};
    Noc_Document_Snapshot large = {0};
    Noc_C_Parse_Tree baseline_tree = {0};
    Noc_C_Parse_Tree large_tree = {0};
    Noc_C_Ast ast = {0};
    Noc_C_Ast_Options options = noc_c_ast_default_options();
    Noc_C_Ast_Impl *implementation;
    Delayed_Cancel delayed = {0, 3};
    size_t immediate_calls = 0;
    size_t generation;
    size_t node_count;
    size_t offset;

    CHECK(large_source != NULL);
    if (!large_source) return;
    for (offset = 0; offset < large_count;) {
        size_t remaining = large_count - offset;
        size_t copy_count = remaining < sizeof(repeated) - 1
                                ? remaining
                                : sizeof(repeated) - 1;
        memcpy(large_source + offset, repeated, copy_count);
        offset += copy_count;
    }
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "ast/preserved.c",
                                      baseline_source,
                                      sizeof(baseline_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &baseline) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "ast/large.c",
                                      large_source,
                                      large_count,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &large) == NOC_WORKSPACE_OK);
    free(large_source);
    CHECK(noc_c_parse_tree_build(&baseline,
                                 noc_c_parse_default_options(),
                                 &baseline_tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_parse_tree_build(&large,
                                 noc_c_parse_default_options(),
                                 &large_tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&baseline_tree, options, &ast) == NOC_C_AST_OK);
    implementation = ast.impl;
    generation = ast.generation;
    node_count = noc_c_ast_node_count(&ast);

    options.max_nodes = 1;
    CHECK(noc_c_ast_build(&large_tree, options, &ast) ==
          NOC_C_AST_LIMIT_EXCEEDED);
    check_preserved(&ast,
                    implementation,
                    generation,
                    node_count,
                    baseline_source);

    options = noc_c_ast_default_options();
    options.should_cancel = cancel_immediately;
    options.cancel_user_data = &immediate_calls;
    CHECK(noc_c_ast_build(&large_tree, options, &ast) == NOC_C_AST_CANCELLED);
    CHECK(immediate_calls == 1);
    check_preserved(&ast,
                    implementation,
                    generation,
                    node_count,
                    baseline_source);

    options.should_cancel = cancel_after_delay;
    options.cancel_user_data = &delayed;
    CHECK(noc_c_ast_build(&large_tree, options, &ast) == NOC_C_AST_CANCELLED);
    CHECK(delayed.calls == delayed.cancel_after);
    check_preserved(&ast,
                    implementation,
                    generation,
                    node_count,
                    baseline_source);

    memset(&options, 0, sizeof(options));
    CHECK(noc_c_ast_build(&large_tree, options, &ast) ==
          NOC_C_AST_INVALID_ARGUMENT);
    CHECK(noc_c_ast_build(NULL,
                          noc_c_ast_default_options(),
                          &ast) == NOC_C_AST_INVALID_ARGUMENT);
    check_preserved(&ast,
                    implementation,
                    generation,
                    node_count,
                    baseline_source);

    CHECK(noc_c_ast_build(&large_tree,
                          noc_c_ast_default_options(),
                          &ast) == NOC_C_AST_OK);
    CHECK(ast.impl != implementation);
    CHECK(ast.generation == generation + 1);
    CHECK(noc_c_ast_document_generation(&ast) ==
          noc_document_snapshot_generation(&large));

    noc_c_ast_free(&ast);
    noc_c_parse_tree_free(&baseline_tree);
    noc_c_parse_tree_free(&large_tree);
    noc_document_snapshot_free(&baseline);
    noc_document_snapshot_free(&large);
    noc_workspace_deinit(&workspace);
}

static void test_statuses_generation_exhaustion_and_empty_queries(void)
{
    static const char source[] = "int value;\n";
    static const char *const names[] = {
        "ok",
        "invalid-argument",
        "cancelled",
        "limit-exceeded",
        "generation-exhausted",
        "out-of-memory",
    };
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Ast exhausted = {0};
    Noc_C_Ast empty = {0};
    Noc_C_Ast_Expected expected;
    size_t index;

    for (index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        CHECK(strcmp(noc_c_ast_status_name((Noc_C_Ast_Status)index),
                     names[index]) == 0);
    }
    CHECK(strcmp(noc_c_ast_status_name((Noc_C_Ast_Status)999),
                 "unknown") == 0);
    CHECK(noc_c_ast_default_options().max_nodes > 1);
    CHECK(!noc_c_ast_is_valid(&empty));
    CHECK(!noc_c_ast_is_syntax_complete(&empty));
    CHECK(noc_c_ast_issues(&empty) == 0);
    CHECK(noc_c_ast_generation(&empty) == 0);
    CHECK(noc_c_ast_document_generation(&empty) == 0);
    CHECK(noc_c_ast_snapshot(&empty) == NULL);
    CHECK(noc_c_ast_node_count(&empty) == 0);
    CHECK(noc_c_ast_root(&empty) == NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_operator(&empty, 0) == NOC_C_AST_OPERATOR_NONE);
    CHECK(noc_c_ast_node_specifier(&empty, 0) == NOC_C_AST_SPECIFIER_NONE);
    CHECK(noc_c_ast_node_qualifier(&empty, 0) == NOC_C_AST_QUALIFIER_NONE);
    CHECK(noc_c_ast_node_extension(&empty, 0) == NOC_C_AST_EXTENSION_NONE);
    expected = noc_c_ast_node_expected(&empty, 0);
    CHECK(expected.kind == NOC_C_AST_EXPECTED_NONE);
    CHECK(expected.spelling.data == NULL);
    noc_c_ast_free(&empty);

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "ast/exhausted.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    exhausted.generation = SIZE_MAX;
    CHECK(noc_c_ast_build(&tree,
                          noc_c_ast_default_options(),
                          &exhausted) == NOC_C_AST_GENERATION_EXHAUSTED);
    CHECK(exhausted.impl == NULL);
    CHECK(exhausted.generation == SIZE_MAX);
    noc_c_ast_free(&exhausted);
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static void check_pinned_c11_grammar_gap(const char *path,
                                         const char *source)
{
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Ast ast = {0};

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      path,
                                      source,
                                      strlen(source),
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&tree, noc_c_ast_default_options(), &ast) ==
          NOC_C_AST_OK);
    CHECK(!noc_c_ast_is_syntax_complete(&ast));
    CHECK((noc_c_ast_issues(&ast) &
           (NOC_C_AST_ISSUE_PARSE_ERROR | NOC_C_AST_ISSUE_UNKNOWN_DETAIL)) !=
          0);

    noc_c_ast_free(&ast);
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static void test_pinned_c11_grammar_gaps_are_not_silently_classified(void)
{
    /* tree-sitter-c 0.24.2 lacks these required C11 spellings. Test each one
       independently until the grammar is patched or replaced: accepting a
       nearby C23/GNU alias must never be reported as C11 support. */
    check_pinned_c11_grammar_gap("ast/c11-bool-gap.c", "_Bool flag;\n");
    check_pinned_c11_grammar_gap("ast/c11-complex-gap.c",
                                 "_Complex value;\n");
    check_pinned_c11_grammar_gap("ast/c11-thread-gap.c",
                                 "_Thread_local int local_value;\n");
    check_pinned_c11_grammar_gap("ast/c11-assert-gap.c",
                                 "_Static_assert(1, \"ok\");\n");
}

int main(void)
{
    test_missing_token_keeps_expected_spelling();
    test_missing_named_symbol_keeps_only_stable_category();
    test_error_and_skipped_source_recovery();
    test_limits_cancellation_and_rebuild_are_transactional();
    test_statuses_generation_exhaustion_and_empty_queries();
    test_pinned_c11_grammar_gaps_are_not_silently_classified();
    return finish_suite("recoverable physical C AST");
}
