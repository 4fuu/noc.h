#include "test_support.h"

typedef struct {
    Noc_Workspace workspace;
    Noc_Document_Snapshot snapshot;
    Noc_C_Parse_Tree tree;
    Noc_C_Ast ast;
} C11_Ast_Fixture;

static void fixture_build(C11_Ast_Fixture *fixture,
                          const char *path,
                          const char *source,
                          size_t source_count)
{
    memset(fixture, 0, sizeof(*fixture));
    noc_workspace_init(&fixture->workspace);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      path,
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

static void fixture_free(C11_Ast_Fixture *fixture)
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

static size_t find_kind(const Noc_C_Ast *ast, Noc_C_Ast_Kind kind)
{
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        if (node && node->kind == kind) return index;
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

static void check_type(const Noc_C_Ast *ast,
                       const char *source,
                       Noc_C_Ast_Primitive primitive,
                       unsigned int flags,
                       size_t long_count)
{
    Noc_C_Ast_Type_Spelling spelling = {0};
    Noc_C_Ast_Kind kind = flags == 0 && long_count == 0
                              ? NOC_C_AST_KIND_PRIMITIVE_TYPE
                              : NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER;
    size_t node = find_source(ast, kind, source);
    CHECK(node != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_type_spelling(ast, node, &spelling));
    CHECK(spelling.primitive == primitive);
    CHECK(spelling.flags == flags);
    CHECK(spelling.long_count == long_count);
    CHECK(noc_c_ast_node_extension(ast, node) == NOC_C_AST_EXTENSION_NONE);
}

static void check_static_assert(const Noc_C_Ast *ast,
                                const char *source,
                                const char *condition,
                                const char *message,
                                Noc_C_Ast_Extension extension)
{
    size_t declaration = find_source(ast,
                                     NOC_C_AST_KIND_STATIC_ASSERT_DECLARATION,
                                     source);
    size_t condition_node;
    size_t message_node;
    CHECK(declaration != NOC_C_AST_NODE_NONE);
    condition_node = find_child_field(ast,
                                      declaration,
                                      NOC_C_AST_FIELD_CONDITION);
    message_node = find_child_field(ast, declaration, NOC_C_AST_FIELD_MESSAGE);
    CHECK(condition_node != NOC_C_AST_NODE_NONE);
    CHECK(slice_equals(noc_c_ast_node_source(ast, condition_node), condition));
    CHECK(noc_c_ast_node_extension(ast, declaration) == extension);
    if (message) {
        CHECK(message_node != NOC_C_AST_NODE_NONE);
        CHECK(slice_equals(noc_c_ast_node_source(ast, message_node), message));
    } else {
        CHECK(message_node == NOC_C_AST_NODE_NONE);
    }
}

static void test_required_c11_spellings(void)
{
    static const char source[] =
        "_Bool flag;\n"
        "_Complex implicit_double;\n"
        "float _Complex float_complex;\n"
        "double _Complex double_complex;\n"
        "long double _Complex long_double_complex;\n"
        "_Complex long double reordered_complex;\n"
        "_Thread_local int thread_value;\n"
        "_Static_assert(1, \"translation \" \"unit\");\n"
        "struct Checked {\n"
        "    _Static_assert(sizeof(int) >= 2, \"member\");\n"
        "    int value;\n"
        "};\n"
        "void check_block(void) {\n"
        "    _Static_assert(1, \"block\");\n"
        "}\n"
        "static_assert(1);\n";
    C11_Ast_Fixture fixture;
    size_t storage;

    fixture_build(&fixture,
                  "ast/c11-required.c",
                  source,
                  sizeof(source) - 1);
    CHECK(noc_c_ast_is_syntax_complete(&fixture.ast));
    CHECK(noc_c_ast_issues(&fixture.ast) == 0);
    check_type(&fixture.ast,
               "_Bool",
               NOC_C_AST_PRIMITIVE_C11_BOOL,
               0,
               0);
    check_type(&fixture.ast,
               "_Complex",
               NOC_C_AST_PRIMITIVE_NONE,
               NOC_C_AST_TYPE_COMPLEX,
               0);
    check_type(&fixture.ast,
               "float _Complex",
               NOC_C_AST_PRIMITIVE_FLOAT,
               NOC_C_AST_TYPE_COMPLEX,
               0);
    check_type(&fixture.ast,
               "double _Complex",
               NOC_C_AST_PRIMITIVE_DOUBLE,
               NOC_C_AST_TYPE_COMPLEX,
               0);
    check_type(&fixture.ast,
               "long double _Complex",
               NOC_C_AST_PRIMITIVE_DOUBLE,
               NOC_C_AST_TYPE_COMPLEX,
               1);
    check_type(&fixture.ast,
               "_Complex long double",
               NOC_C_AST_PRIMITIVE_DOUBLE,
               NOC_C_AST_TYPE_COMPLEX,
               1);

    storage = find_source(&fixture.ast,
                          NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                          "_Thread_local");
    CHECK(storage != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_specifier(&fixture.ast, storage) ==
          NOC_C_AST_SPECIFIER_C11_THREAD_LOCAL);
    CHECK(noc_c_ast_node_extension(&fixture.ast, storage) ==
          NOC_C_AST_EXTENSION_NONE);

    check_static_assert(&fixture.ast,
                        "_Static_assert(1, \"translation \" \"unit\");",
                        "1",
                        "\"translation \" \"unit\"",
                        NOC_C_AST_EXTENSION_NONE);
    check_static_assert(&fixture.ast,
                        "_Static_assert(sizeof(int) >= 2, \"member\");",
                        "sizeof(int) >= 2",
                        "\"member\"",
                        NOC_C_AST_EXTENSION_NONE);
    check_static_assert(&fixture.ast,
                        "_Static_assert(1, \"block\");",
                        "1",
                        "\"block\"",
                        NOC_C_AST_EXTENSION_NONE);
    check_static_assert(&fixture.ast,
                        "static_assert(1);",
                        "1",
                        NULL,
                        NOC_C_AST_EXTENSION_C23_STATIC_ASSERT);
    CHECK(strcmp(noc_c_ast_kind_name(
                     NOC_C_AST_KIND_STATIC_ASSERT_DECLARATION),
                 "static_assert_declaration") == 0);
    CHECK(strcmp(noc_c_ast_field_name(NOC_C_AST_FIELD_MESSAGE), "message") ==
          0);
    CHECK(strcmp(noc_c_ast_extension_name(
                     NOC_C_AST_EXTENSION_C23_STATIC_ASSERT),
                 "c23-static-assert") == 0);
    fixture_free(&fixture);
}

static void test_static_assert_recovery(void)
{
    static const struct {
        const char *source;
        bool preserves_declaration;
    } malformed[] = {
        {"_Static_assert(, \"missing condition\");\n", false},
        {"_Static_assert(1, );\n", false},
        {"_Static_assert(1, \"missing close\";\n", false},
        {"_Static_assert(1, \"missing semicolon\")\n", true},
    };
    size_t case_index;
    for (case_index = 0;
         case_index < sizeof(malformed) / sizeof(malformed[0]);
         ++case_index) {
        C11_Ast_Fixture fixture;
        fixture_build(&fixture,
                      "ast/c11-static-assert-recovery.c",
                      malformed[case_index].source,
                      strlen(malformed[case_index].source));
        CHECK(!noc_c_ast_is_syntax_complete(&fixture.ast));
        CHECK((noc_c_ast_issues(&fixture.ast) &
               (NOC_C_AST_ISSUE_PARSE_ERROR | NOC_C_AST_ISSUE_MISSING)) != 0);
        if (malformed[case_index].preserves_declaration) {
            size_t declaration = find_kind(
                &fixture.ast,
                NOC_C_AST_KIND_STATIC_ASSERT_DECLARATION);
            size_t condition;
            size_t message;
            CHECK(declaration != NOC_C_AST_NODE_NONE);
            condition = find_child_field(&fixture.ast,
                                         declaration,
                                         NOC_C_AST_FIELD_CONDITION);
            message = find_child_field(&fixture.ast,
                                       declaration,
                                       NOC_C_AST_FIELD_MESSAGE);
            CHECK(condition != NOC_C_AST_NODE_NONE);
            CHECK(message != NOC_C_AST_NODE_NONE);
            CHECK(slice_equals(noc_c_ast_node_source(&fixture.ast, condition),
                               "1"));
            CHECK(slice_equals(noc_c_ast_node_source(&fixture.ast, message),
                               "\"missing semicolon\""));
        }
        fixture_free(&fixture);
    }
}

int main(void)
{
    test_required_c11_spellings();
    test_static_assert_recovery();
    return finish_suite("C AST required C11 spellings");
}
