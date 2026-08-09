#include "test_support.h"

typedef struct {
    Noc_Workspace workspace;
    Noc_Document_Snapshot snapshot;
    Noc_C_Parse_Tree tree;
    Noc_C_Ast ast;
} Completion_Fixture;

static bool completion_fixture_build(Completion_Fixture *fixture,
                                     const char *path,
                                     const char *source)
{
    noc_workspace_init(&fixture->workspace);
    if (noc_workspace_open_document(&fixture->workspace,
                                    path,
                                    source,
                                    strlen(source),
                                    NOC_SOURCE_CLASS_PROJECT,
                                    &fixture->snapshot) != NOC_WORKSPACE_OK) {
        return false;
    }
    if (noc_c_parse_tree_build(&fixture->snapshot,
                               noc_c_parse_default_options(),
                               &fixture->tree) != NOC_C_PARSE_OK) {
        return false;
    }
    return noc_c_ast_build(&fixture->tree,
                           noc_c_ast_default_options(),
                           &fixture->ast) == NOC_C_AST_OK;
}

static void completion_fixture_free(Completion_Fixture *fixture)
{
    noc_c_ast_free(&fixture->ast);
    noc_c_parse_tree_free(&fixture->tree);
    noc_document_snapshot_free(&fixture->snapshot);
    noc_workspace_deinit(&fixture->workspace);
}

static size_t first_missing_node(const Noc_C_Ast *ast)
{
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        if (node && (node->flags & NOC_C_AST_NODE_MISSING) != 0) return index;
    }
    return NOC_C_AST_NODE_NONE;
}

static void test_complete_source_positions_have_stable_context(void)
{
    static const char source[] =
        "int value;\n"
        "int read(void) { return value; }\n";
    Completion_Fixture fixture = {0};
    Noc_C_Ast_Completion_Context beginning = {0};
    Noc_C_Ast_Completion_Context inside = {0};
    Noc_C_Ast_Completion_Context end = {0};
    const char *value = strstr(source, "return value") + sizeof("return ") - 1;
    size_t inside_offset = (size_t)(value - source) + 2;
    size_t position;

    CHECK(completion_fixture_build(&fixture, "ast/completion.c", source));
    for (position = 0; position <= sizeof(source) - 1; ++position) {
        Noc_C_Ast_Completion_Context every = {0};
        CHECK(noc_c_ast_completion_context(&fixture.ast, position, &every));
        CHECK(every.left_node ==
              (position == 0
                   ? NOC_C_AST_NODE_NONE
                   : noc_c_ast_node_at_offset(&fixture.ast, position - 1)));
        CHECK(every.right_node ==
              (position == sizeof(source) - 1
                   ? NOC_C_AST_NODE_NONE
                   : noc_c_ast_node_at_offset(&fixture.ast, position)));
    }
    CHECK(noc_c_ast_completion_context(&fixture.ast, 0, &beginning));
    CHECK(beginning.offset == 0);
    CHECK(beginning.node == noc_c_ast_root(&fixture.ast));
    CHECK(beginning.left_node == NOC_C_AST_NODE_NONE);
    CHECK(beginning.right_node != NOC_C_AST_NODE_NONE);
    CHECK(beginning.expected_count == 0);

    CHECK(noc_c_ast_completion_context(&fixture.ast,
                                       inside_offset,
                                       &inside));
    CHECK(inside.left_node != NOC_C_AST_NODE_NONE);
    CHECK(inside.left_node == inside.right_node);
    CHECK(inside.node == inside.left_node);
    CHECK(noc_c_ast_node_at(&fixture.ast, inside.node)->kind ==
          NOC_C_AST_KIND_IDENTIFIER);
    CHECK(slice_equals(noc_c_ast_node_source(&fixture.ast, inside.node),
                       "value"));
    CHECK(inside.generation == noc_c_ast_generation(&fixture.ast));
    CHECK(inside.file_id ==
          noc_document_snapshot_file_id(noc_c_ast_snapshot(&fixture.ast)));
    CHECK(inside.document_generation ==
          noc_c_ast_document_generation(&fixture.ast));
    CHECK(inside.expected_count == 0);
    CHECK(noc_c_ast_completion_next_expected_node(
              &fixture.ast,
              &inside,
              NOC_C_AST_NODE_NONE) ==
          NOC_C_AST_NODE_NONE);

    CHECK(noc_c_ast_completion_context(&fixture.ast,
                                       sizeof(source) - 1,
                                       &end));
    CHECK(end.offset == sizeof(source) - 1);
    CHECK(end.node == noc_c_ast_root(&fixture.ast));
    CHECK(end.left_node != NOC_C_AST_NODE_NONE);
    CHECK(end.right_node == NOC_C_AST_NODE_NONE);
    CHECK(end.expected_count == 0);
    completion_fixture_free(&fixture);
}

static void test_recovery_expectations_are_enumerated_at_insertion_position(void)
{
    static const char source[] = "int f(void) { return 1 }\n";
    Completion_Fixture fixture = {0};
    Noc_C_Ast_Completion_Context context = {0};
    const Noc_C_Ast_Node *missing;
    Noc_C_Ast_Expected expected;
    size_t missing_index = NOC_C_AST_NODE_NONE;

    CHECK(completion_fixture_build(&fixture,
                                   "ast/completion-recovery.c",
                                   source));
    missing_index = first_missing_node(&fixture.ast);
    CHECK(missing_index != NOC_C_AST_NODE_NONE);
    missing = noc_c_ast_node_at(&fixture.ast, missing_index);
    CHECK(missing != NULL);
    if (!missing) {
        completion_fixture_free(&fixture);
        return;
    }
    CHECK(noc_c_ast_completion_context(&fixture.ast,
                                       missing->bytes.begin,
                                       &context));
    CHECK(context.offset == missing->bytes.begin);
    CHECK(context.left_node != NOC_C_AST_NODE_NONE);
    CHECK(context.right_node != NOC_C_AST_NODE_NONE);
    CHECK(context.node == missing->parent);
    CHECK(context.expected_count == 1);
    CHECK(noc_c_ast_completion_next_expected_node(
              &fixture.ast,
              &context,
              NOC_C_AST_NODE_NONE) ==
          missing_index);
    CHECK(noc_c_ast_completion_next_expected_node(&fixture.ast,
                                                  &context,
                                                  missing_index) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_completion_next_expected_node(&fixture.ast,
                                                  &context,
                                                  noc_c_ast_root(&fixture.ast)) ==
          NOC_C_AST_NODE_NONE);
    expected = noc_c_ast_node_expected(&fixture.ast, missing_index);
    CHECK(expected.kind == NOC_C_AST_EXPECTED_PUNCTUATOR);
    CHECK(slice_equals(expected.spelling, ";"));
    completion_fixture_free(&fixture);
}

static void test_eof_recovery_context_overrides_document_edge_root(void)
{
    static const char source[] =
        "int function(void) {\n"
        "    int value = 1;";
    Completion_Fixture fixture = {0};
    Noc_C_Ast_Completion_Context context = {0};
    size_t previous = NOC_C_AST_NODE_NONE;
    bool saw_closing_brace = false;
    size_t enumerated = 0;

    CHECK(completion_fixture_build(&fixture, "ast/completion-eof.c", source));
    CHECK(noc_c_ast_completion_context(&fixture.ast,
                                       sizeof(source) - 1,
                                       &context));
    CHECK(context.right_node == NOC_C_AST_NODE_NONE);
    CHECK(context.expected_count != 0);
    CHECK(context.node != noc_c_ast_root(&fixture.ast));
    for (;;) {
        Noc_C_Ast_Expected expected;
        size_t node = noc_c_ast_completion_next_expected_node(&fixture.ast,
                                                              &context,
                                                              previous);
        if (node == NOC_C_AST_NODE_NONE) break;
        expected = noc_c_ast_node_expected(&fixture.ast, node);
        if (slice_equals(expected.spelling, "}")) saw_closing_brace = true;
        previous = node;
        enumerated += 1;
    }
    CHECK(enumerated == context.expected_count);
    CHECK(saw_closing_brace);
    completion_fixture_free(&fixture);
}

static void test_context_owner_rejects_equal_foreign_generations(void)
{
    static const char source[] = "int f(void) { return 1 }\n";
    Completion_Fixture left = {0};
    Completion_Fixture right = {0};
    Noc_C_Ast_Completion_Context context = {0};
    const Noc_C_Ast_Node *missing;
    size_t missing_index;

    CHECK(completion_fixture_build(&left, "ast/owner-left.c", source));
    CHECK(completion_fixture_build(&right, "ast/owner-right.c", source));
    CHECK(noc_c_ast_generation(&left.ast) == noc_c_ast_generation(&right.ast));
    CHECK(noc_c_ast_document_generation(&left.ast) ==
          noc_c_ast_document_generation(&right.ast));
    CHECK(noc_document_snapshot_file_id(noc_c_ast_snapshot(&left.ast)) ==
          noc_document_snapshot_file_id(noc_c_ast_snapshot(&right.ast)));
    missing_index = first_missing_node(&left.ast);
    missing = noc_c_ast_node_at(&left.ast, missing_index);
    CHECK(missing != NULL);
    if (missing) {
        CHECK(noc_c_ast_completion_context(&left.ast,
                                           missing->bytes.begin,
                                           &context));
        CHECK(noc_c_ast_completion_next_expected_node(
                  &left.ast,
                  &context,
                  NOC_C_AST_NODE_NONE) == missing_index);
        CHECK(noc_c_ast_completion_next_expected_node(
                  &right.ast,
                  &context,
                  NOC_C_AST_NODE_NONE) == NOC_C_AST_NODE_NONE);
    }
    completion_fixture_free(&left);
    completion_fixture_free(&right);
}

static void test_failures_preserve_output_and_rebuilds_reject_stale_context(void)
{
    static const char source[] = "int value;\n";
    Completion_Fixture fixture = {0};
    Noc_C_Ast invalid = {0};
    Noc_C_Ast_Completion_Context context;
    Noc_C_Ast_Completion_Context preserved;
    size_t offset = (size_t)(strstr(source, "value") - source) + 1;

    CHECK(completion_fixture_build(&fixture, "ast/completion-stale.c", source));
    memset(&context, 0xA5, sizeof(context));
    preserved = context;
    CHECK(!noc_c_ast_completion_context(&invalid, 0, &context));
    CHECK(memcmp(&context, &preserved, sizeof(context)) == 0);
    CHECK(!noc_c_ast_completion_context(&fixture.ast,
                                        sizeof(source),
                                        &context));
    CHECK(memcmp(&context, &preserved, sizeof(context)) == 0);
    CHECK(!noc_c_ast_completion_context(&fixture.ast, 0, NULL));

    CHECK(noc_c_ast_completion_context(&fixture.ast, offset, &context));
    CHECK(noc_c_ast_build(&fixture.tree,
                          noc_c_ast_default_options(),
                          &fixture.ast) == NOC_C_AST_OK);
    CHECK(context.generation + 1 == noc_c_ast_generation(&fixture.ast));
    CHECK(noc_c_ast_completion_next_expected_node(
              &fixture.ast,
              &context,
              NOC_C_AST_NODE_NONE) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_completion_next_expected_node(
              &invalid,
              &context,
              NOC_C_AST_NODE_NONE) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_completion_next_expected_node(
              &fixture.ast,
              NULL,
              NOC_C_AST_NODE_NONE) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_completion_context(&fixture.ast, offset, &context));
    context.file_id = NOC_FILE_ID_NONE;
    CHECK(noc_c_ast_completion_next_expected_node(
              &fixture.ast,
              &context,
              NOC_C_AST_NODE_NONE) ==
          NOC_C_AST_NODE_NONE);
    completion_fixture_free(&fixture);
}

int main(void)
{
    test_complete_source_positions_have_stable_context();
    test_recovery_expectations_are_enumerated_at_insertion_position();
    test_eof_recovery_context_overrides_document_edge_root();
    test_context_owner_rejects_equal_foreign_generations();
    test_failures_preserve_output_and_rebuilds_reject_stale_context();
    return finish_suite("C AST completion context");
}
