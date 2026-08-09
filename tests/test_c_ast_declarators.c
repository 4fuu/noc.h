#include "test_support.h"

typedef struct {
    Noc_Workspace workspace;
    Noc_Document_Snapshot snapshot;
    Noc_C_Parse_Tree tree;
    Noc_C_Ast ast;
} Declarator_Fixture;

static void fixture_build(Declarator_Fixture *fixture,
                          const char *source,
                          size_t source_count)
{
    memset(fixture, 0, sizeof(*fixture));
    noc_workspace_init(&fixture->workspace);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      "ast/declarators.c",
                                      source,
                                      source_count,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &fixture->snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&fixture->snapshot,
                                 noc_c_parse_default_options(),
                                 &fixture->tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&fixture->tree,
                          noc_c_ast_default_options(),
                          &fixture->ast) == NOC_C_AST_OK);
}

static void fixture_free(Declarator_Fixture *fixture)
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

static void check_parent(const Noc_C_Ast *ast,
                         Noc_C_Ast_Kind child_kind,
                         const char *child_source,
                         Noc_C_Ast_Kind parent_kind)
{
    size_t child = find_source(ast, child_kind, child_source);
    const Noc_C_Ast_Node *child_node = noc_c_ast_node_at(ast, child);
    const Noc_C_Ast_Node *parent = child_node
                                       ? noc_c_ast_node_at(ast,
                                                           child_node->parent)
                                       : NULL;
    if (child == NOC_C_AST_NODE_NONE) {
        fprintf(stderr,
                "missing %s node with source `%s`\n",
                noc_c_ast_kind_name(child_kind),
                child_source);
    } else if (!parent || parent->kind != parent_kind) {
        fprintf(stderr,
                "declarator `%s`: expected parent %s, got %s\n",
                child_source,
                noc_c_ast_kind_name(parent_kind),
                parent ? noc_c_ast_kind_name(parent->kind) : "none");
    }
    CHECK(child != NOC_C_AST_NODE_NONE);
    CHECK(parent != NULL);
    CHECK(parent && parent->kind == parent_kind);
}

static void test_nested_declarator_precedence_and_aliases(void)
{
    static const char source[] =
        "typedef int (*Callback)(const char *, ...);\n"
        "struct Dispatch { int (*invoke)(void); };\n"
        "int (*array_pointer)[4];\n"
        "int *pointer_array[4];\n"
        "int takes_callback(int (*callback)(double), int values[static 4]);\n"
        "int (*returns_callback(void))(int);\n"
        "void abstract_declarators(void) {\n"
        "    sizeof(int (*)(double));\n"
        "    sizeof(int (*)[4]);\n"
        "}\n";
    Declarator_Fixture fixture;
    size_t index;

    fixture_build(&fixture, source, sizeof(source) - 1);
    CHECK(noc_c_ast_is_syntax_complete(&fixture.ast));
    CHECK(noc_c_ast_issues(&fixture.ast) == 0);
    for (index = 0; index < noc_c_ast_node_count(&fixture.ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(&fixture.ast, index);
        CHECK(node != NULL);
        CHECK(node && node->kind != NOC_C_AST_KIND_UNKNOWN);
        CHECK(node && node->field != NOC_C_AST_FIELD_UNKNOWN);
    }

    check_parent(&fixture.ast,
                 NOC_C_AST_KIND_POINTER_DECLARATOR,
                 "*array_pointer",
                 NOC_C_AST_KIND_PARENTHESIZED_DECLARATOR);
    check_parent(&fixture.ast,
                 NOC_C_AST_KIND_PARENTHESIZED_DECLARATOR,
                 "(*array_pointer)",
                 NOC_C_AST_KIND_ARRAY_DECLARATOR);
    check_parent(&fixture.ast,
                 NOC_C_AST_KIND_ARRAY_DECLARATOR,
                 "pointer_array[4]",
                 NOC_C_AST_KIND_POINTER_DECLARATOR);
    CHECK(find_source(&fixture.ast,
                      NOC_C_AST_KIND_FUNCTION_DECLARATOR,
                      "(*invoke)(void)") != NOC_C_AST_NODE_NONE);
    CHECK(find_source(&fixture.ast,
                      NOC_C_AST_KIND_FUNCTION_DECLARATOR,
                      "(*callback)(double)") != NOC_C_AST_NODE_NONE);
    CHECK(find_source(&fixture.ast,
                      NOC_C_AST_KIND_FUNCTION_DECLARATOR,
                      "(*returns_callback(void))(int)") !=
          NOC_C_AST_NODE_NONE);
    CHECK(find_source(&fixture.ast,
                      NOC_C_AST_KIND_ABSTRACT_FUNCTION_DECLARATOR,
                      "(*)(double)") != NOC_C_AST_NODE_NONE);
    CHECK(find_source(&fixture.ast,
                      NOC_C_AST_KIND_ABSTRACT_ARRAY_DECLARATOR,
                      "(*)[4]") != NOC_C_AST_NODE_NONE);
    fixture_free(&fixture);
}

int main(void)
{
    test_nested_declarator_precedence_and_aliases();
    return finish_suite("C AST declarators");
}
