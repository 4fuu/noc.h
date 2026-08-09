#include "test_support.h"

typedef struct {
    const char *name;
    const char *source;
    const char *expected_spelling;
} Malformed_Case;

typedef struct {
    Noc_Document_Snapshot snapshot;
    Noc_C_Parse_Tree tree;
    Noc_C_Ast ast;
} Malformed_Parse;

static bool slices_equal(Noc_Slice left, Noc_Slice right)
{
    return left.count == right.count &&
           (left.count == 0 ||
            (left.data && right.data &&
             memcmp(left.data, right.data, left.count) == 0));
}

static void malformed_parse(Noc_Workspace *workspace,
                            const char *path,
                            const char *source,
                            size_t source_count,
                            Malformed_Parse *output)
{
    memset(output, 0, sizeof(*output));
    CHECK(noc_workspace_open_document(workspace,
                                      path,
                                      source,
                                      source_count,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &output->snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&output->snapshot,
                                 noc_c_parse_default_options(),
                                 &output->tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&output->tree,
                          noc_c_ast_default_options(),
                          &output->ast) == NOC_C_AST_OK);
}

static void malformed_parse_free(Malformed_Parse *parsed)
{
    noc_c_ast_free(&parsed->ast);
    noc_c_parse_tree_free(&parsed->tree);
    noc_document_snapshot_free(&parsed->snapshot);
}

static bool ast_has_expected_spelling(const Noc_C_Ast *ast,
                                      const char *spelling)
{
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        Noc_C_Ast_Expected expected = noc_c_ast_node_expected(ast, index);
        if (slice_equals(expected.spelling, spelling)) return true;
    }
    return false;
}

static void check_recovery_tree(const char *case_name,
                                const char *source,
                                const Noc_C_Parse_Tree *tree)
{
    size_t source_count = strlen(source);
    size_t recovery_count = 0;
    size_t index;
    CHECK(noc_c_parse_tree_is_valid(tree));
    if (!noc_c_parse_tree_has_error(tree)) {
        fprintf(stderr, "malformed corpus case `%s` parsed as complete\n",
                case_name);
    }
    CHECK(noc_c_parse_tree_has_error(tree));
    CHECK(slice_equals(noc_c_parse_node_source(
                           tree,
                           noc_c_parse_tree_root(tree)),
                       source));
    for (index = 0; index < noc_c_parse_tree_node_count(tree); ++index) {
        const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, index);
        CHECK(node != NULL);
        if (!node) continue;
        CHECK(node->bytes.begin <= node->bytes.end);
        CHECK(node->bytes.end <= source_count);
        if ((node->flags & (NOC_C_PARSE_NODE_ERROR |
                            NOC_C_PARSE_NODE_MISSING |
                            NOC_C_PARSE_NODE_SKIPPED_SOURCE)) != 0) {
            recovery_count += 1;
        }
    }
    CHECK(recovery_count != 0);
}

static void check_recovery_ast(const char *case_name,
                               size_t source_count,
                               const Noc_C_Ast *ast)
{
    size_t index;
    CHECK(noc_c_ast_is_valid(ast));
    if (noc_c_ast_is_syntax_complete(ast)) {
        fprintf(stderr, "malformed corpus case `%s` has a complete AST\n",
                case_name);
    }
    CHECK(!noc_c_ast_is_syntax_complete(ast));
    CHECK((noc_c_ast_issues(ast) &
           (NOC_C_AST_ISSUE_PARSE_ERROR |
            NOC_C_AST_ISSUE_MISSING |
            NOC_C_AST_ISSUE_SKIPPED_SOURCE)) != 0);
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        CHECK(node != NULL);
        if (!node) continue;
        CHECK(node->kind != NOC_C_AST_KIND_UNKNOWN);
        CHECK(node->field != NOC_C_AST_FIELD_UNKNOWN);
        CHECK(node->bytes.begin <= node->bytes.end);
        CHECK(node->bytes.end <= source_count);
    }
}

static void compare_recovery_trees(const Noc_C_Parse_Tree *left,
                                   const Noc_C_Parse_Tree *right)
{
    size_t count = noc_c_parse_tree_node_count(left);
    size_t index;
    CHECK(count == noc_c_parse_tree_node_count(right));
    for (index = 0; index < count; ++index) {
        const Noc_C_Parse_Node *left_node =
            noc_c_parse_tree_node_at(left, index);
        const Noc_C_Parse_Node *right_node =
            noc_c_parse_tree_node_at(right, index);
        CHECK(left_node != NULL);
        CHECK(right_node != NULL);
        if (!left_node || !right_node) continue;
        CHECK(left_node->bytes.begin == right_node->bytes.begin);
        CHECK(left_node->bytes.end == right_node->bytes.end);
        CHECK(left_node->parent == right_node->parent);
        CHECK(left_node->first_child == right_node->first_child);
        CHECK(left_node->last_child == right_node->last_child);
        CHECK(left_node->next_sibling == right_node->next_sibling);
        CHECK(left_node->child_count == right_node->child_count);
        CHECK(left_node->flags == right_node->flags);
        CHECK(slices_equal(left_node->kind, right_node->kind));
        CHECK(slices_equal(left_node->field, right_node->field));
        CHECK(slices_equal(noc_c_parse_node_source(left, index),
                           noc_c_parse_node_source(right, index)));
    }
}

static void compare_recovery_asts(const Noc_C_Ast *left,
                                  const Noc_C_Ast *right)
{
    size_t count = noc_c_ast_node_count(left);
    size_t index;
    CHECK(noc_c_ast_issues(left) == noc_c_ast_issues(right));
    CHECK(count == noc_c_ast_node_count(right));
    for (index = 0; index < count; ++index) {
        const Noc_C_Ast_Node *left_node = noc_c_ast_node_at(left, index);
        const Noc_C_Ast_Node *right_node = noc_c_ast_node_at(right, index);
        Noc_C_Ast_Expected left_expected;
        Noc_C_Ast_Expected right_expected;
        CHECK(left_node != NULL);
        CHECK(right_node != NULL);
        if (!left_node || !right_node) continue;
        CHECK(left_node->kind == right_node->kind);
        CHECK(left_node->field == right_node->field);
        CHECK(left_node->bytes.begin == right_node->bytes.begin);
        CHECK(left_node->bytes.end == right_node->bytes.end);
        CHECK(left_node->parent == right_node->parent);
        CHECK(left_node->first_child == right_node->first_child);
        CHECK(left_node->last_child == right_node->last_child);
        CHECK(left_node->next_sibling == right_node->next_sibling);
        CHECK(left_node->child_count == right_node->child_count);
        CHECK(left_node->flags == right_node->flags);
        left_expected = noc_c_ast_node_expected(left, index);
        right_expected = noc_c_ast_node_expected(right, index);
        CHECK(left_expected.kind == right_expected.kind);
        CHECK(slices_equal(left_expected.spelling,
                           right_expected.spelling));
        CHECK(slices_equal(noc_c_ast_node_source(left, index),
                           noc_c_ast_node_source(right, index)));
    }
}

static void test_malformed_corpus_recovery_is_stable(void)
{
    static const Malformed_Case cases[] = {
        {
            "missing-semicolon",
            "int function(void) { return 1 }\n",
            ";",
        },
        {
            "incomplete-compound",
            "int function(void) {\n    int value = 1;\n",
            "}",
        },
        {
            "missing-expression",
            "int function(int value) { return value + ; }\n",
            NULL,
        },
        {
            "broken-declarator",
            "int (*handler)(int;\n",
            ")",
        },
        {
            "incomplete-enum",
            "enum State { STATE_A, STATE_B = 2;\n",
            "}",
        },
        {
            "incomplete-atomic-type",
            "_Atomic(int atomic_value;\n",
            ")",
        },
        {
            "skipped-edge-byte",
            "\vint value;\n",
            NULL,
        },
    };
    size_t case_index;

    for (case_index = 0;
         case_index < sizeof(cases) / sizeof(cases[0]);
         ++case_index) {
        Noc_Workspace workspace = {0};
        Malformed_Parse first;
        Malformed_Parse second;
        Noc_Slice physical;
        char first_path[128];
        char second_path[128];
        size_t source_count = strlen(cases[case_index].source);

        snprintf(first_path,
                 sizeof(first_path),
                 "malformed/%s.c",
                 cases[case_index].name);
        snprintf(second_path,
                 sizeof(second_path),
                 "malformed/%s-round-trip.c",
                 cases[case_index].name);
        noc_workspace_init(&workspace);
        malformed_parse(&workspace,
                        first_path,
                        cases[case_index].source,
                        source_count,
                        &first);
        physical = noc_c_parse_node_source(&first.tree,
                                           noc_c_parse_tree_root(&first.tree));
        malformed_parse(&workspace,
                        second_path,
                        physical.data,
                        physical.count,
                        &second);
        check_recovery_tree(cases[case_index].name,
                            cases[case_index].source,
                            &first.tree);
        check_recovery_tree(cases[case_index].name,
                            cases[case_index].source,
                            &second.tree);
        check_recovery_ast(cases[case_index].name,
                           source_count,
                           &first.ast);
        check_recovery_ast(cases[case_index].name,
                           source_count,
                           &second.ast);
        compare_recovery_trees(&first.tree, &second.tree);
        compare_recovery_asts(&first.ast, &second.ast);
        if (cases[case_index].expected_spelling) {
            CHECK(ast_has_expected_spelling(&first.ast,
                                            cases[case_index].expected_spelling));
            CHECK(ast_has_expected_spelling(&second.ast,
                                            cases[case_index].expected_spelling));
        }
        malformed_parse_free(&first);
        malformed_parse_free(&second);
        noc_workspace_deinit(&workspace);
    }
}

int main(void)
{
    test_malformed_corpus_recovery_is_stable();
    return finish_suite("malformed C parser corpus");
}
