#include "test_support.h"

static size_t find_kind(const Noc_C_Ast *ast, Noc_C_Ast_Kind kind)
{
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        if (node && node->kind == kind) return index;
    }
    return NOC_C_AST_NODE_NONE;
}

static void check_topology(const Noc_C_Ast *ast, size_t source_count)
{
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        size_t child;
        size_t child_count = 0;
        CHECK(node != NULL);
        if (!node) continue;
        CHECK(node->generation == noc_c_ast_generation(ast));
        CHECK(node->kind != NOC_C_AST_KIND_UNKNOWN);
        CHECK(node->field != NOC_C_AST_FIELD_UNKNOWN);
        CHECK((node->flags & (NOC_C_AST_NODE_UNKNOWN_KIND |
                              NOC_C_AST_NODE_UNKNOWN_FIELD |
                              NOC_C_AST_NODE_UNKNOWN_DETAIL)) == 0);
        CHECK(node->bytes.begin <= node->bytes.end);
        CHECK(node->bytes.end <= source_count);
        CHECK(index == 0 ? node->parent == NOC_C_AST_NODE_NONE
                         : node->parent < index);
        CHECK(noc_c_ast_node_source(ast, index).count ==
              node->bytes.end - node->bytes.begin);
        child = node->first_child;
        while (child != NOC_C_AST_NODE_NONE) {
            const Noc_C_Ast_Node *child_node = noc_c_ast_node_at(ast, child);
            CHECK(child_node != NULL);
            if (!child_node) break;
            CHECK(child > index);
            CHECK(child_node->parent == index);
            CHECK(child_node->bytes.begin >= node->bytes.begin);
            CHECK(child_node->bytes.end <= node->bytes.end);
            child_count += 1;
            CHECK(child_count <= node->child_count);
            child = child_node->next_sibling;
        }
        CHECK(child_count == node->child_count);
        CHECK(node->child_count == 0
                  ? node->first_child == NOC_C_AST_NODE_NONE &&
                        node->last_child == NOC_C_AST_NODE_NONE
                  : node->last_child != NOC_C_AST_NODE_NONE);
    }
}

static void test_normalized_kinds_fields_and_ranges(void)
{
    static const char source[] =
        "typedef unsigned long Size;\n"
        "struct Pair { int x; unsigned y : 3; };\n"
        "enum Mode { MODE_A = 1, MODE_B };\n"
        "int sum(const int *values, int count) {\n"
        "    int total = 0;\n"
        "    for (int i = 0; i < count; ++i) {\n"
        "        total += values[i];\n"
        "    }\n"
        "    if (total > 0) return (int)total;\n"
        "    return 0;\n"
        "}\n";
    static const Noc_C_Ast_Kind expected_kinds[] = {
        NOC_C_AST_KIND_TRANSLATION_UNIT,
        NOC_C_AST_KIND_TYPE_DEFINITION,
        NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER,
        NOC_C_AST_KIND_TYPE_IDENTIFIER,
        NOC_C_AST_KIND_STRUCT_SPECIFIER,
        NOC_C_AST_KIND_FIELD_DECLARATION_LIST,
        NOC_C_AST_KIND_BITFIELD_CLAUSE,
        NOC_C_AST_KIND_ENUM_SPECIFIER,
        NOC_C_AST_KIND_ENUMERATOR_LIST,
        NOC_C_AST_KIND_FUNCTION_DEFINITION,
        NOC_C_AST_KIND_FUNCTION_DECLARATOR,
        NOC_C_AST_KIND_PARAMETER_LIST,
        NOC_C_AST_KIND_PARAMETER_DECLARATION,
        NOC_C_AST_KIND_POINTER_DECLARATOR,
        NOC_C_AST_KIND_COMPOUND_STATEMENT,
        NOC_C_AST_KIND_INIT_DECLARATOR,
        NOC_C_AST_KIND_FOR_STATEMENT,
        NOC_C_AST_KIND_UPDATE_EXPRESSION,
        NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION,
        NOC_C_AST_KIND_SUBSCRIPT_EXPRESSION,
        NOC_C_AST_KIND_IF_STATEMENT,
        NOC_C_AST_KIND_CAST_EXPRESSION,
        NOC_C_AST_KIND_RETURN_STATEMENT,
    };
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Ast ast = {0};
    const Noc_C_Ast_Node *root;
    Noc_Location root_location;
    size_t index;

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "ast/normalized.c",
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
    CHECK(noc_c_ast_is_syntax_complete(&ast));
    CHECK(noc_c_ast_issues(&ast) == 0);
    CHECK(noc_c_ast_generation(&ast) == 1);
    CHECK(noc_c_ast_document_generation(&ast) ==
          noc_document_snapshot_generation(&snapshot));
    CHECK(noc_c_ast_root(&ast) == 0);
    root = noc_c_ast_node_at(&ast, 0);
    CHECK(root != NULL);
    CHECK(root && root->kind == NOC_C_AST_KIND_TRANSLATION_UNIT);
    CHECK(root && root->bytes.begin == 0);
    CHECK(root && root->bytes.end == sizeof(source) - 1);
    CHECK(slice_equals(noc_c_ast_node_source(&ast, 0), source));
    root_location = noc_c_ast_node_location(&ast, 0);
    CHECK(root_location.line == 1);
    CHECK(root_location.column == 1);
    CHECK(strcmp(root_location.path, "ast/normalized.c") == 0);
    for (index = 0;
         index < sizeof(expected_kinds) / sizeof(expected_kinds[0]);
         ++index) {
        CHECK(find_kind(&ast, expected_kinds[index]) != NOC_C_AST_NODE_NONE);
    }
    CHECK(noc_c_ast_node_at(&ast, SIZE_MAX) == NULL);
    CHECK(noc_c_ast_node_source(&ast, SIZE_MAX).data == NULL);
    CHECK(noc_c_ast_node_location(&ast, SIZE_MAX).path == NULL);
    check_topology(&ast, sizeof(source) - 1);

    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    CHECK(noc_c_ast_is_valid(&ast));
    CHECK(slice_equals(noc_document_snapshot_source(noc_c_ast_snapshot(&ast)),
                       source));
    CHECK(slice_equals(noc_c_ast_node_source(&ast, 0), source));
    noc_c_ast_free(&ast);
    noc_c_ast_free(&ast);
}

static void test_successful_rebuild_changes_only_ast_generation(void)
{
    static const char first_source[] = "int first;\n";
    static const char second_source[] =
        "int second;\n"
        "int read_second(void) { return second; }\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot first = {0};
    Noc_Document_Snapshot second = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Ast ast = {0};
    Noc_C_Ast_Impl *first_implementation;
    size_t first_document_generation;

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "ast/rebuild.c",
                                      first_source,
                                      sizeof(first_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &first) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&first,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&tree, noc_c_ast_default_options(), &ast) ==
          NOC_C_AST_OK);
    first_implementation = ast.impl;
    first_document_generation = noc_c_ast_document_generation(&ast);

    CHECK(noc_workspace_update_document(&workspace,
                                        &first,
                                        second_source,
                                        sizeof(second_source) - 1,
                                        &second) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&second,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&tree, noc_c_ast_default_options(), &ast) ==
          NOC_C_AST_OK);
    CHECK(ast.impl != first_implementation);
    CHECK(noc_c_ast_generation(&ast) == 2);
    CHECK(noc_c_ast_document_generation(&ast) ==
          noc_document_snapshot_generation(&second));
    CHECK(noc_c_ast_document_generation(&ast) ==
          first_document_generation + 1);
    CHECK(slice_equals(noc_c_ast_node_source(&ast, 0), second_source));

    noc_c_ast_free(&ast);
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&first);
    noc_document_snapshot_free(&second);
    noc_workspace_deinit(&workspace);
}

int main(void)
{
    test_normalized_kinds_fields_and_ranges();
    test_successful_rebuild_changes_only_ast_generation();
    return finish_suite("normalized physical C AST");
}
