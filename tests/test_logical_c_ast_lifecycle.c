#include "macro_expansion_test_support.h"

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

static void check_preserved(const Noc_Logical_C_Ast *ast,
                            Noc_Logical_C_Ast_Impl *implementation,
                            size_t generation,
                            size_t node_count,
                            size_t source_generation,
                            const char *text)
{
    CHECK(ast->impl == implementation);
    CHECK(ast->generation == generation);
    CHECK(noc_logical_c_ast_is_valid(ast));
    CHECK(noc_logical_c_ast_node_count(ast) == node_count);
    CHECK(noc_logical_c_ast_source_generation(ast) == source_generation);
    CHECK(slice_equals(noc_logical_c_ast_node_source(ast, 0), text));
}

static void test_retention_transactionality_and_rebuild(void)
{
    Macro_Expansion_Fixture first_fixture;
    Macro_Expansion_Fixture second_fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    Noc_Logical_C_Ast ast = {0};
    Noc_C_Ast_Options options = noc_c_ast_default_options();
    Noc_Logical_C_Ast_Impl *implementation;
    size_t generation;
    size_t node_count;
    size_t source_generation;
    size_t cancel_calls = 0;
    Delayed_Cancel publish_cancel = {0, 5};

    macro_fixture_init(&first_fixture, "", "int old_value;\n");
    CHECK(macro_fixture_expand(&first_fixture,
                               macro_fixture_full_input(&first_fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &first_fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    CHECK(noc_logical_c_ast_build(&tree, options, &ast) == NOC_C_AST_OK);
    implementation = ast.impl;
    generation = ast.generation;
    node_count = noc_logical_c_ast_node_count(&ast);
    source_generation = noc_logical_c_ast_source_generation(&ast);

    options.max_nodes = 1;
    CHECK(noc_logical_c_ast_build(&tree, options, &ast) ==
          NOC_C_AST_LIMIT_EXCEEDED);
    check_preserved(&ast,
                    implementation,
                    generation,
                    node_count,
                    source_generation,
                    "int old_value ;\n");

    options = noc_c_ast_default_options();
    options.should_cancel = cancel_immediately;
    options.cancel_user_data = &cancel_calls;
    CHECK(noc_logical_c_ast_build(&tree, options, &ast) ==
          NOC_C_AST_CANCELLED);
    CHECK(cancel_calls == 1);
    check_preserved(&ast,
                    implementation,
                    generation,
                    node_count,
                    source_generation,
                    "int old_value ;\n");

    options.should_cancel = cancel_after_delay;
    options.cancel_user_data = &publish_cancel;
    CHECK(noc_logical_c_ast_build(&tree, options, &ast) ==
          NOC_C_AST_CANCELLED);
    CHECK(publish_cancel.calls == publish_cancel.cancel_after);
    check_preserved(&ast,
                    implementation,
                    generation,
                    node_count,
                    source_generation,
                    "int old_value ;\n");

    memset(&options, 0, sizeof(options));
    CHECK(noc_logical_c_ast_build(&tree, options, &ast) ==
          NOC_C_AST_INVALID_ARGUMENT);
    CHECK(noc_logical_c_ast_build(NULL,
                                  noc_c_ast_default_options(),
                                  &ast) == NOC_C_AST_INVALID_ARGUMENT);
    CHECK(noc_logical_c_ast_build(&tree,
                                  noc_c_ast_default_options(),
                                  NULL) == NOC_C_AST_INVALID_ARGUMENT);
    check_preserved(&ast,
                    implementation,
                    generation,
                    node_count,
                    source_generation,
                    "int old_value ;\n");

    /* The AST retains source text and token provenance independently of every
       preprocessing object, caller source handle, and input CST. */
    noc_logical_c_parse_tree_free(&tree);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&first_fixture);
    CHECK(noc_logical_c_ast_is_valid(&ast));
    CHECK(slice_equals(noc_logical_c_ast_node_source(&ast, 0),
                       "int old_value ;\n"));

    macro_fixture_init(&second_fixture,
                       "#define VALUE 9\n",
                       "int new_value = VALUE;\n");
    CHECK(macro_fixture_expand(&second_fixture,
                               macro_fixture_full_input(&second_fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &second_fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    CHECK(noc_logical_c_ast_build(&tree,
                                  noc_c_ast_default_options(),
                                  &ast) == NOC_C_AST_OK);
    CHECK(ast.impl != implementation);
    CHECK(ast.generation == generation + 1);
    CHECK(slice_equals(noc_logical_c_ast_node_source(&ast, 0),
                       "int new_value = 9 ;\n"));

    noc_logical_c_parse_tree_free(&tree);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&second_fixture);
    CHECK(noc_logical_c_ast_is_valid(&ast));
    noc_logical_c_ast_free(&ast);
}

static void test_generation_exhaustion_and_empty_queries(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    Noc_Logical_C_Ast exhausted = {0};
    Noc_Logical_C_Ast empty = {0};
    Noc_C_Ast_Type_Spelling type_spelling = {0};
    Noc_C_Ast_Array_Detail array_detail = {0};
    Noc_C_Ast_Expected expected;
    Noc_Logical_Token_Range range = {31, 32};
    Noc_Logical_Byte_Range bytes = {0, 1};

    CHECK(!noc_logical_c_ast_is_valid(&empty));
    CHECK(!noc_logical_c_ast_is_syntax_complete(&empty));
    CHECK(noc_logical_c_ast_issues(&empty) == 0);
    CHECK(noc_logical_c_ast_generation(&empty) == 0);
    CHECK(noc_logical_c_ast_source_generation(&empty) == 0);
    CHECK(noc_logical_c_ast_source(&empty) == NULL);
    CHECK(noc_logical_c_ast_node_count(&empty) == 0);
    CHECK(noc_logical_c_ast_root(&empty) == NOC_C_AST_NODE_NONE);
    CHECK(noc_logical_c_ast_node_at(&empty, 0) == NULL);
    CHECK(noc_logical_c_ast_node_source(&empty, 0).data == NULL);
    CHECK(noc_logical_c_ast_node_location(&empty, 0).line == 0);
    CHECK(noc_logical_c_ast_node_at_offset(&empty, 0) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_logical_c_ast_node_covering_range(&empty, bytes) ==
          NOC_C_AST_NODE_NONE);
    CHECK(noc_logical_c_ast_depth(&empty, 0) == NOC_C_AST_NODE_NONE);
    CHECK(noc_logical_c_ast_common_ancestor(&empty, 0, 0) ==
          NOC_C_AST_NODE_NONE);
    CHECK(!noc_logical_c_ast_node_token_range(&empty, 0, &range));
    CHECK(range.begin == 31 && range.end == 32);
    CHECK(noc_logical_c_ast_node_operator(&empty, 0) ==
          NOC_C_AST_OPERATOR_NONE);
    CHECK(noc_logical_c_ast_node_specifier(&empty, 0) ==
          NOC_C_AST_SPECIFIER_NONE);
    CHECK(noc_logical_c_ast_node_qualifier(&empty, 0) ==
          NOC_C_AST_QUALIFIER_NONE);
    CHECK(!noc_logical_c_ast_node_type_spelling(&empty,
                                                0,
                                                &type_spelling));
    CHECK(!noc_logical_c_ast_node_array_detail(&empty, 0, &array_detail));
    CHECK(noc_logical_c_ast_node_extension(&empty, 0) ==
          NOC_C_AST_EXTENSION_NONE);
    expected = noc_logical_c_ast_node_expected(&empty, 0);
    CHECK(expected.kind == NOC_C_AST_EXPECTED_NONE);
    CHECK(expected.spelling.data == NULL && expected.spelling.count == 0);
    noc_logical_c_ast_free(&empty);
    noc_logical_c_ast_free(NULL);

    macro_fixture_init(&fixture, "", "int value;\n");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    exhausted.generation = SIZE_MAX;
    CHECK(noc_logical_c_ast_build(&tree,
                                  noc_c_ast_default_options(),
                                  &exhausted) ==
          NOC_C_AST_GENERATION_EXHAUSTED);
    CHECK(exhausted.impl == NULL && exhausted.generation == SIZE_MAX);
    noc_logical_c_ast_free(&exhausted);

    noc_logical_c_parse_tree_free(&tree);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_retention_transactionality_and_rebuild();
    test_generation_exhaustion_and_empty_queries();
    return finish_suite("logical normalized C AST lifecycle");
}
