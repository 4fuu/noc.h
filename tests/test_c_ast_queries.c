#include "test_support.h"

typedef struct {
    Noc_Workspace workspace;
    Noc_Document_Snapshot snapshot;
    Noc_C_Parse_Tree tree;
    Noc_C_Ast ast;
} Ast_Query_Fixture;

static bool build_fixture(Ast_Query_Fixture *fixture,
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

static void free_fixture(Ast_Query_Fixture *fixture)
{
    noc_c_ast_free(&fixture->ast);
    noc_c_parse_tree_free(&fixture->tree);
    noc_document_snapshot_free(&fixture->snapshot);
    noc_workspace_deinit(&fixture->workspace);
}

static size_t find_source(const Noc_C_Ast *ast,
                          Noc_C_Ast_Kind kind,
                          const char *source)
{
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        if (node && node->kind == kind &&
            slice_equals(noc_c_ast_node_source(ast, index), source)) {
            return index;
        }
    }
    return NOC_C_AST_NODE_NONE;
}

static size_t find_child_field(const Noc_C_Ast *ast,
                               size_t parent,
                               Noc_C_Ast_Field field)
{
    const Noc_C_Ast_Node *parent_node = noc_c_ast_node_at(ast, parent);
    size_t child = parent_node ? parent_node->first_child : NOC_C_AST_NODE_NONE;
    while (child != NOC_C_AST_NODE_NONE) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, child);
        if (node && node->field == field) return child;
        child = node ? node->next_sibling : NOC_C_AST_NODE_NONE;
    }
    return NOC_C_AST_NODE_NONE;
}

static void test_byte_and_range_queries_find_deepest_physical_node(void)
{
    static const char source[] =
        "int add(int left, int right) {\n"
        "    int sum = left + right;\n"
        "    return sum;\n"
        "}\n";
    Ast_Query_Fixture fixture = {0};
    Noc_Byte_Range range;
    const Noc_C_Ast_Node *binary_node;
    const Noc_C_Ast_Node *result;
    const char *trailing_newline = source + strlen(source) - 1;
    size_t binary;
    size_t function;
    size_t node;

    CHECK(build_fixture(&fixture, "ast/queries.c", source));
    CHECK(noc_c_ast_is_syntax_complete(&fixture.ast));
    binary = find_source(&fixture.ast,
                         NOC_C_AST_KIND_BINARY_EXPRESSION,
                         "left + right");
    function = find_source(&fixture.ast,
                           NOC_C_AST_KIND_FUNCTION_DEFINITION,
                           "int add(int left, int right) {\n"
                           "    int sum = left + right;\n"
                           "    return sum;\n"
                           "}");
    CHECK(binary != NOC_C_AST_NODE_NONE);
    CHECK(function != NOC_C_AST_NODE_NONE);
    binary_node = noc_c_ast_node_at(&fixture.ast, binary);
    CHECK(binary_node != NULL);
    if (binary_node) {
        range = binary_node->bytes;
        CHECK(noc_c_ast_node_covering_range(&fixture.ast, range) == binary);
        node = noc_c_ast_node_at_offset(&fixture.ast,
                                        binary_node->bytes.begin + 1);
        result = noc_c_ast_node_at(&fixture.ast, node);
        CHECK(result != NULL);
        CHECK(result && result->kind == NOC_C_AST_KIND_IDENTIFIER);
        CHECK(result && slice_equals(noc_c_ast_node_source(&fixture.ast, node),
                                     "left"));
    }

    node = noc_c_ast_node_at_offset(&fixture.ast,
                                    (size_t)(trailing_newline - source));
    CHECK(node == noc_c_ast_root(&fixture.ast));

    if (binary_node) {
        const Noc_C_Ast_Node *function_node =
            noc_c_ast_node_at(&fixture.ast, function);
        CHECK(function_node != NULL);
        if (function_node) {
            range.begin = function_node->bytes.begin;
            range.end = binary_node->bytes.end;
            CHECK(noc_c_ast_node_covering_range(&fixture.ast, range) == function);
        }
    }
    free_fixture(&fixture);
}

static void test_depth_and_common_ancestor_follow_ast_generation(void)
{
    static const char source[] =
        "int choose(int left, int right) {\n"
        "    return left + right;\n"
        "}\n";
    Ast_Query_Fixture fixture = {0};
    const Noc_C_Ast_Node *binary_node;
    size_t binary;
    size_t left;
    size_t right;
    size_t root;

    CHECK(build_fixture(&fixture, "ast/ancestors.c", source));
    root = noc_c_ast_root(&fixture.ast);
    binary = find_source(&fixture.ast,
                         NOC_C_AST_KIND_BINARY_EXPRESSION,
                         "left + right");
    binary_node = noc_c_ast_node_at(&fixture.ast, binary);
    left = find_child_field(&fixture.ast, binary, NOC_C_AST_FIELD_LEFT);
    right = find_child_field(&fixture.ast, binary, NOC_C_AST_FIELD_RIGHT);
    CHECK(root != NOC_C_AST_NODE_NONE);
    CHECK(binary_node != NULL);
    CHECK(left != NOC_C_AST_NODE_NONE);
    CHECK(right != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_depth(&fixture.ast, root) == 0);
    CHECK(noc_c_ast_depth(&fixture.ast, binary) > 0);
    CHECK(noc_c_ast_depth(&fixture.ast, left) ==
          noc_c_ast_depth(&fixture.ast, binary) + 1);
    CHECK(noc_c_ast_common_ancestor(&fixture.ast, left, right) == binary);
    CHECK(noc_c_ast_common_ancestor(&fixture.ast, left, root) == root);
    CHECK(noc_c_ast_common_ancestor(&fixture.ast, right, right) == right);
    free_fixture(&fixture);
}

static void test_invalid_queries_and_zero_width_recovery_are_unambiguous(void)
{
    static const char source[] = "int f(void) { return 1 }\n";
    Ast_Query_Fixture fixture = {0};
    Ast_Query_Fixture empty = {0};
    Noc_C_Ast invalid = {0};
    Noc_Byte_Range range = {0, 0};
    size_t missing = NOC_C_AST_NODE_NONE;
    size_t index;
    size_t source_count = strlen(source);

    CHECK(build_fixture(&fixture, "ast/query-recovery.c", source));
    for (index = 0; index < noc_c_ast_node_count(&fixture.ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(&fixture.ast, index);
        if (node && (node->flags & NOC_C_AST_NODE_MISSING) != 0) {
            missing = index;
            break;
        }
    }
    CHECK(missing != NOC_C_AST_NODE_NONE);
    if (missing != NOC_C_AST_NODE_NONE) {
        const Noc_C_Ast_Node *missing_node =
            noc_c_ast_node_at(&fixture.ast, missing);
        size_t owner = noc_c_ast_node_at_offset(&fixture.ast,
                                                missing_node->bytes.begin);
        CHECK(missing_node->bytes.begin == missing_node->bytes.end);
        CHECK(owner != NOC_C_AST_NODE_NONE);
        CHECK(owner != missing);
    }

    CHECK(noc_c_ast_node_at_offset(&fixture.ast, source_count) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_at_offset(&fixture.ast, SIZE_MAX) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_at_offset(&invalid, 0) == NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_covering_range(&fixture.ast, range) ==
          NOC_C_AST_NODE_NONE);
    range.begin = 3;
    range.end = 2;
    CHECK(noc_c_ast_node_covering_range(&fixture.ast, range) ==
          NOC_C_AST_NODE_NONE);
    range.begin = 0;
    range.end = source_count + 1;
    CHECK(noc_c_ast_node_covering_range(&fixture.ast, range) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_covering_range(&invalid, range) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_depth(&fixture.ast, SIZE_MAX) == NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_depth(&invalid, 0) == NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_common_ancestor(&fixture.ast, 0, SIZE_MAX) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_common_ancestor(&invalid, 0, 0) ==
          NOC_C_AST_NODE_NONE);
    free_fixture(&fixture);

    range.begin = 0;
    range.end = 1;
    CHECK(build_fixture(&empty, "ast/empty-query.c", ""));
    CHECK(noc_c_ast_node_at_offset(&empty.ast, 0) == NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_covering_range(&empty.ast, range) ==
          NOC_C_AST_NODE_NONE);
    free_fixture(&empty);
}

int main(void)
{
    test_byte_and_range_queries_find_deepest_physical_node();
    test_depth_and_common_ancestor_follow_ast_generation();
    test_invalid_queries_and_zero_width_recovery_are_unambiguous();
    return finish_suite("C AST physical queries");
}
