#define TREE_SITTER_FEATURE_WASM 1
#define TREE_SITTER_HIDE_SYMBOLS 1
#define DEBUG_QUERY_STEPS 301
#define DEBUG_ANALYZE_QUERY 302
#define DEBUG_EXECUTE_QUERY 303
#define DEBUG_GET_CHANGED_RANGES 304
#define HAVE_ENDIAN_H 305
#define HAVE_SYS_ENDIAN_H 306
#define HAVE_SYS_PARAM_H 307
#define U_INT64_T_UNAVAILABLE 308
#define U_COMBINED_IMPLEMENTATION 309
#define U_COMMON_IMPLEMENTATION 310
#define U_I18N_IMPLEMENTATION 311
#define U_IO_IMPLEMENTATION 312
#define U_IN_DOXYGEN 313
#define U_PLATFORM 314
#define U_SIZEOF_WCHAR_T 315
#define U_GCC_MAJOR_MINOR 316
#define U_CPLUSPLUS_VERSION 317
#define U_IS_SURROGATE(value) ((value) == 318)
#define TRUE 101
#define FALSE 102
#define UCHAR_TYPE unsigned short
#define Array(type) struct { type consumer_item; }

typedef struct Length {
    int consumer_value;
} Length;

typedef struct Lexer {
    int consumer_value;
} Lexer;

typedef struct Stack {
    int consumer_value;
} Stack;

enum {
    StackStatusActive = 201,
    TreeCursorStepVisible = 202,
};

static const unsigned int LENGTH_MAX = 203;

static int length_add(int left, int right)
{
    return left + right;
}

#define NOC_IMPLEMENTATION
#include "../release/noc.h"

#include <stdio.h>
#include <string.h>

static int failed = 0;

#define REQUIRE(condition)                                                      \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: requirement failed: %s\n",                \
                    __FILE__, __LINE__, #condition);                            \
            failed = 1;                                                         \
        }                                                                       \
    } while (0)

static Noc_Include_Resolve_Status release_not_found(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *output)
{
    (void)user_data;
    (void)request;
    (void)output;
    return NOC_INCLUDE_RESOLVE_NOT_FOUND;
}

int main(void)
{
    static const char source[] =
        "#define CLOSE >\n"
        "#include <closed.h CLOSE\n"
        "#pragma once\n";
    static const char guard_source[] =
        "#ifndef RELEASE_RUNTIME_H\n"
        "#define RELEASE_RUNTIME_H\n"
        "int release_runtime;\n"
        "#endif\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Document_Snapshot guard_snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Noc_Preprocessor_Unit guard_unit = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Macro_Environment guard_environment = {0};
    Noc_Preprocessor_Conditional_Groups guard_groups = {0};
    Noc_Pragma_Once pragma_once = {0};
    Noc_Include_Guard guard = {0};
    Noc_Include_Operand operand = {0};
    Noc_Include_Expansion expansion = {0};
    Noc_Include_Graph graph = {0};
    Noc_C_Parse_Tree parse_tree = {0};
    Noc_C_Ast ast = {0};
    Noc_Include_Graph_Options graph_options =
        noc_include_graph_default_options();
    Noc_Include_Resolver resolver = {release_not_found, NULL};

    REQUIRE(sizeof(Length) != 0 && sizeof(Lexer) != 0 && sizeof(Stack) != 0);
    REQUIRE(length_add(TRUE, FALSE) == 203);
    REQUIRE(LENGTH_MAX == 203);
    REQUIRE(StackStatusActive == 201 && TreeCursorStepVisible == 202);
    REQUIRE(TREE_SITTER_FEATURE_WASM == 1);
    REQUIRE(TREE_SITTER_HIDE_SYMBOLS == 1);
    REQUIRE(DEBUG_QUERY_STEPS == 301);
    REQUIRE(DEBUG_ANALYZE_QUERY == 302);
    REQUIRE(DEBUG_EXECUTE_QUERY == 303);
    REQUIRE(DEBUG_GET_CHANGED_RANGES == 304);
    REQUIRE(HAVE_ENDIAN_H == 305);
    REQUIRE(HAVE_SYS_ENDIAN_H == 306);
    REQUIRE(HAVE_SYS_PARAM_H == 307);
    REQUIRE(U_INT64_T_UNAVAILABLE == 308);
    REQUIRE(U_COMBINED_IMPLEMENTATION == 309);
    REQUIRE(U_COMMON_IMPLEMENTATION == 310);
    REQUIRE(U_I18N_IMPLEMENTATION == 311);
    REQUIRE(U_IO_IMPLEMENTATION == 312);
    REQUIRE(U_IN_DOXYGEN == 313);
    REQUIRE(U_PLATFORM == 314);
    REQUIRE(U_SIZEOF_WCHAR_T == 315);
    REQUIRE(U_GCC_MAJOR_MINOR == 316);
    REQUIRE(U_CPLUSPLUS_VERSION == 317);
    REQUIRE(U_IS_SURROGATE(318));
    noc_context_init(&context);
    noc_workspace_init(&workspace);
    REQUIRE(noc_workspace_open_document(&workspace,
                                        "release-runtime.c",
                                        source,
                                        sizeof(source) - 1,
                                        NOC_SOURCE_CLASS_PROJECT,
                                        &snapshot) == NOC_WORKSPACE_OK);
    REQUIRE(noc_preprocessor_unit_build(&context,
                                        &snapshot,
                                        NOC_MACROS_FULL,
                                        &unit));
    REQUIRE(unit.count == 3);
    REQUIRE(unit.macro_directive_count == 1);
    REQUIRE(noc_pragma_once_build(&unit, 2, &pragma_once) ==
            NOC_INCLUDE_CONTROL_BUILD_OK);
    REQUIRE(noc_pragma_once_is_valid(&pragma_once));
    REQUIRE(pragma_once.status == NOC_PRAGMA_ONCE_VALID);
    REQUIRE(noc_macro_environment_apply(&environment, &unit, 0) ==
            NOC_MACRO_ENVIRONMENT_OK);
    REQUIRE(noc_include_operand_build(&unit, 1, &operand) ==
            NOC_INCLUDE_OPERAND_BUILD_OK);
    REQUIRE(operand.status == NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED);
    REQUIRE(noc_include_expansion_build(&environment,
                                        environment.count,
                                        &operand,
                                        noc_macro_expansion_default_limits(),
                                        &expansion) == NOC_MACRO_EXPANSION_OK);
    REQUIRE(noc_include_expansion_is_valid(&expansion));
    REQUIRE(expansion.status == NOC_INCLUDE_OPERAND_DIRECT);
    REQUIRE(expansion.form == NOC_INCLUDE_FORM_ANGLED);
    REQUIRE(expansion.logical_name.count == sizeof("closed.h") - 1);
    REQUIRE(expansion.logical_name.data != NULL &&
            memcmp(expansion.logical_name.data,
                   "closed.h",
                   sizeof("closed.h") - 1) == 0);
    graph_options.macro_policy = NOC_MACROS_FULL;
    REQUIRE(noc_include_graph_build(&context,
                                    &snapshot,
                                    NULL,
                                    0,
                                    resolver,
                                    graph_options,
                                    &graph) == NOC_INCLUDE_GRAPH_OK);
    REQUIRE(noc_include_graph_is_valid(&graph));
    REQUIRE(noc_include_graph_node_count(&graph) == 1);
    REQUIRE(noc_include_graph_edge_count(&graph) == 1);
    REQUIRE(noc_include_graph_edge_at(&graph, 0)->macro_expanded);
    REQUIRE(noc_include_graph_edge_at(&graph, 0)->status ==
            NOC_INCLUDE_GRAPH_EDGE_NOT_FOUND);

    REQUIRE(noc_workspace_open_document(&workspace,
                                        "release-runtime.h",
                                        guard_source,
                                        sizeof(guard_source) - 1,
                                        NOC_SOURCE_CLASS_PROJECT,
                                        &guard_snapshot) == NOC_WORKSPACE_OK);
    REQUIRE(noc_preprocessor_unit_build(&context,
                                        &guard_snapshot,
                                        NOC_MACROS_FULL,
                                        &guard_unit));
    REQUIRE(noc_preprocessor_conditional_groups_build(
                &guard_environment,
                0,
                &guard_unit,
                noc_macro_expansion_default_limits(),
                &guard_groups) == NOC_CONDITIONAL_GROUPS_OK);
    REQUIRE(noc_include_guard_build(&guard_unit, &guard_groups, &guard) ==
            NOC_INCLUDE_CONTROL_BUILD_OK);
    REQUIRE(noc_include_guard_is_valid(&guard));
    REQUIRE(guard.status == NOC_INCLUDE_GUARD_CANONICAL);
    REQUIRE(guard.definition_allowed);
    REQUIRE(guard.guard_name.count == sizeof("RELEASE_RUNTIME_H") - 1);
    REQUIRE(guard.guard_name.data != NULL &&
            memcmp(guard.guard_name.data,
                   "RELEASE_RUNTIME_H",
                   sizeof("RELEASE_RUNTIME_H") - 1) == 0);
    REQUIRE(noc_c_parse_tree_build(&guard_snapshot,
                                   noc_c_parse_default_options(),
                                   &parse_tree) == NOC_C_PARSE_OK);
    REQUIRE(noc_c_parse_tree_is_valid(&parse_tree));
    REQUIRE(!noc_c_parse_tree_has_error(&parse_tree));
    REQUIRE(noc_c_parse_tree_node_count(&parse_tree) > 1);
    REQUIRE(noc_c_parse_tree_root(&parse_tree) == 0);
    REQUIRE(noc_c_ast_build(&parse_tree,
                            noc_c_ast_default_options(),
                            &ast) == NOC_C_AST_OK);
    REQUIRE(noc_c_ast_is_valid(&ast));
    REQUIRE(noc_c_ast_is_syntax_complete(&ast));
    REQUIRE(noc_c_ast_node_at(&ast, noc_c_ast_root(&ast))->kind ==
            NOC_C_AST_KIND_TRANSLATION_UNIT);
    REQUIRE(noc_c_ast_node_source(&ast, 0).count ==
            sizeof(guard_source) - 1);

    noc_c_parse_tree_free(&parse_tree);
    REQUIRE(noc_c_ast_is_valid(&ast));
    noc_c_ast_free(&ast);
    noc_preprocessor_conditional_groups_free(&guard_groups);
    noc_macro_environment_free(&guard_environment);
    noc_preprocessor_unit_free(&guard_unit);
    noc_document_snapshot_free(&guard_snapshot);
    noc_include_graph_free(&graph);
    noc_include_expansion_free(&expansion);
    noc_include_operand_free(&operand);
    noc_macro_environment_free(&environment);
    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
    return failed;
}
