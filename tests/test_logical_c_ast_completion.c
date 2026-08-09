#include "macro_expansion_test_support.h"

typedef struct {
    Macro_Expansion_Fixture expansion;
    Noc_Logical_Source source;
    Noc_Logical_C_Parse_Tree tree;
    Noc_Logical_C_Ast ast;
} Logical_Completion_Fixture;

static bool completion_fixture_build(Logical_Completion_Fixture *fixture,
                                     const char *definitions,
                                     const char *input)
{
    macro_fixture_init(&fixture->expansion, definitions, input);
    if (macro_fixture_expand(&fixture->expansion,
                             macro_fixture_full_input(&fixture->expansion)) !=
        NOC_MACRO_EXPANSION_OK) {
        return false;
    }
    if (noc_logical_source_build_macro_expansion(
            &fixture->expansion.expansion,
            noc_logical_source_default_options(),
            &fixture->source) != NOC_LOGICAL_SOURCE_OK) {
        return false;
    }
    if (noc_logical_c_parse_tree_build(&fixture->source,
                                       noc_c_parse_default_options(),
                                       &fixture->tree) != NOC_C_PARSE_OK) {
        return false;
    }
    return noc_logical_c_ast_build(&fixture->tree,
                                   noc_c_ast_default_options(),
                                   &fixture->ast) == NOC_C_AST_OK;
}

static void completion_fixture_free(Logical_Completion_Fixture *fixture)
{
    noc_logical_c_ast_free(&fixture->ast);
    noc_logical_c_parse_tree_free(&fixture->tree);
    noc_logical_source_free(&fixture->source);
    macro_fixture_deinit(&fixture->expansion);
}

static size_t first_missing_node(const Noc_Logical_C_Ast *ast)
{
    size_t index;
    for (index = 0; index < noc_logical_c_ast_node_count(ast); ++index) {
        const Noc_Logical_C_Ast_Node *node =
            noc_logical_c_ast_node_at(ast, index);
        if (node && (node->flags & NOC_C_AST_NODE_MISSING) != 0) return index;
    }
    return NOC_C_AST_NODE_NONE;
}

static void test_every_logical_position_has_stable_adjacent_context(void)
{
    Logical_Completion_Fixture fixture = {0};
    Noc_Logical_C_Ast_Completion_Context beginning = {0};
    Noc_Logical_C_Ast_Completion_Context inside = {0};
    Noc_Logical_C_Ast_Completion_Context end = {0};
    Noc_Slice text;
    const char *value;
    size_t inside_offset;
    size_t position;

    CHECK(completion_fixture_build(
        &fixture,
        "#define NAME value\n",
        "int NAME; int read(void) { return NAME; }\n"));
    text = noc_logical_source_text(noc_logical_c_ast_source(&fixture.ast));
    value = strstr(text.data, "return value");
    CHECK(value != NULL);
    if (!value) {
        completion_fixture_free(&fixture);
        return;
    }
    value += sizeof("return ") - 1;
    inside_offset = (size_t)(value - text.data) + 2;
    for (position = 0; position <= text.count; ++position) {
        Noc_Logical_C_Ast_Completion_Context every = {0};
        CHECK(noc_logical_c_ast_completion_context(&fixture.ast,
                                                   position,
                                                   &every));
        CHECK(every.left_node ==
              (position == 0
                   ? NOC_C_AST_NODE_NONE
                   : noc_logical_c_ast_node_at_offset(&fixture.ast,
                                                       position - 1)));
        CHECK(every.right_node ==
              (position == text.count
                   ? NOC_C_AST_NODE_NONE
                   : noc_logical_c_ast_node_at_offset(&fixture.ast, position)));
    }

    CHECK(noc_logical_c_ast_completion_context(&fixture.ast, 0, &beginning));
    CHECK(beginning.node == noc_logical_c_ast_root(&fixture.ast));
    CHECK(beginning.left_node == NOC_C_AST_NODE_NONE);
    CHECK(beginning.right_node != NOC_C_AST_NODE_NONE);
    CHECK(beginning.expected_count == 0);

    CHECK(noc_logical_c_ast_completion_context(&fixture.ast,
                                               inside_offset,
                                               &inside));
    CHECK(inside.left_node == inside.right_node);
    CHECK(inside.node == inside.left_node);
    CHECK(slice_equals(noc_logical_c_ast_node_source(&fixture.ast, inside.node),
                       "value"));
    CHECK(inside.generation == noc_logical_c_ast_generation(&fixture.ast));
    CHECK(inside.source_generation ==
          noc_logical_c_ast_source_generation(&fixture.ast));
    CHECK(inside.expected_count == 0);
    CHECK(noc_logical_c_ast_completion_next_expected_node(
              &fixture.ast,
              &inside,
              NOC_C_AST_NODE_NONE) == NOC_C_AST_NODE_NONE);

    CHECK(noc_logical_c_ast_completion_context(&fixture.ast,
                                               text.count,
                                               &end));
    CHECK(end.node == noc_logical_c_ast_root(&fixture.ast));
    CHECK(end.left_node != NOC_C_AST_NODE_NONE);
    CHECK(end.right_node == NOC_C_AST_NODE_NONE);
    completion_fixture_free(&fixture);
}

static void test_recovery_expectations_keep_logical_and_macro_provenance(void)
{
    Logical_Completion_Fixture fixture = {0};
    Noc_Logical_C_Ast_Completion_Context context = {0};
    const Noc_Logical_C_Ast_Node *missing;
    const Noc_Logical_Source *source;
    Noc_C_Ast_Expected expected;
    Noc_Logical_Token_Range range = {0};
    Noc_Logical_Token_Macro_Provenance provenance = {0};
    size_t missing_index;

    CHECK(completion_fixture_build(&fixture,
                                   "#define VALUE 1\n",
                                   "int f(void) { return VALUE }\n"));
    missing_index = first_missing_node(&fixture.ast);
    CHECK(missing_index != NOC_C_AST_NODE_NONE);
    missing = noc_logical_c_ast_node_at(&fixture.ast, missing_index);
    CHECK(missing != NULL);
    if (!missing) {
        completion_fixture_free(&fixture);
        return;
    }
    CHECK(noc_logical_c_ast_completion_context(&fixture.ast,
                                               missing->bytes.begin,
                                               &context));
    CHECK(context.node == missing->parent);
    CHECK(context.expected_count == 1);
    CHECK(noc_logical_c_ast_completion_next_expected_node(
              &fixture.ast,
              &context,
              NOC_C_AST_NODE_NONE) == missing_index);
    CHECK(noc_logical_c_ast_completion_next_expected_node(
              &fixture.ast,
              &context,
              missing_index) == NOC_C_AST_NODE_NONE);
    CHECK(noc_logical_c_ast_completion_next_expected_node(
              &fixture.ast,
              &context,
              noc_logical_c_ast_root(&fixture.ast)) == NOC_C_AST_NODE_NONE);
    CHECK(context.left_node != NOC_C_AST_NODE_NONE);
    CHECK(noc_logical_c_ast_node_token_range(&fixture.ast,
                                             context.left_node,
                                             &range));
    CHECK(range.end == range.begin + 1);
    source = noc_logical_c_ast_source(&fixture.ast);
    CHECK(source != NULL);
    CHECK(source && slice_equals(
                        noc_logical_source_token_text(source, range.begin),
                        "1"));
    CHECK(source && noc_logical_source_token_macro_provenance(source,
                                                               range.begin,
                                                               &provenance));
    CHECK(provenance.macro_origin ==
          NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT);
    CHECK(provenance.macro_frame_index != NOC_TOKEN_INDEX_NONE);
    expected = noc_logical_c_ast_node_expected(&fixture.ast, missing_index);
    CHECK(expected.kind == NOC_C_AST_EXPECTED_PUNCTUATOR);
    CHECK(slice_equals(expected.spelling, ";"));
    CHECK(noc_logical_c_ast_node_token_range(&fixture.ast,
                                             missing_index,
                                             &range));
    CHECK(range.begin == range.end);
    completion_fixture_free(&fixture);
}

static void test_empty_logical_source_has_root_context(void)
{
    Logical_Completion_Fixture fixture = {0};
    Noc_Logical_C_Ast_Completion_Context context = {0};

    CHECK(completion_fixture_build(&fixture, "", ""));
    CHECK(noc_logical_source_text(noc_logical_c_ast_source(&fixture.ast)).count ==
          0);
    CHECK(noc_logical_c_ast_completion_context(&fixture.ast, 0, &context));
    CHECK(context.offset == 0);
    CHECK(context.node == noc_logical_c_ast_root(&fixture.ast));
    CHECK(context.left_node == NOC_C_AST_NODE_NONE);
    CHECK(context.right_node == NOC_C_AST_NODE_NONE);
    CHECK(context.expected_count == 0);
    CHECK(noc_logical_c_ast_completion_next_expected_node(
              &fixture.ast,
              &context,
              NOC_C_AST_NODE_NONE) == NOC_C_AST_NODE_NONE);
    completion_fixture_free(&fixture);
}

static void test_eof_expectations_override_root_context(void)
{
    Logical_Completion_Fixture fixture = {0};
    Noc_Logical_C_Ast_Completion_Context context = {0};
    Noc_Slice text;
    size_t previous = NOC_C_AST_NODE_NONE;
    size_t enumerated = 0;
    bool saw_closing_brace = false;

    CHECK(completion_fixture_build(&fixture,
                                   "",
                                   "int function(void) { int value = 1;"));
    text = noc_logical_source_text(noc_logical_c_ast_source(&fixture.ast));
    CHECK(noc_logical_c_ast_completion_context(&fixture.ast,
                                               text.count,
                                               &context));
    CHECK(context.right_node == NOC_C_AST_NODE_NONE);
    CHECK(context.expected_count != 0);
    CHECK(context.node != noc_logical_c_ast_root(&fixture.ast));
    for (;;) {
        size_t node = noc_logical_c_ast_completion_next_expected_node(
            &fixture.ast,
            &context,
            previous);
        Noc_C_Ast_Expected expected;
        if (node == NOC_C_AST_NODE_NONE) break;
        expected = noc_logical_c_ast_node_expected(&fixture.ast, node);
        if (slice_equals(expected.spelling, "}")) saw_closing_brace = true;
        previous = node;
        enumerated += 1;
    }
    CHECK(enumerated == context.expected_count);
    CHECK(saw_closing_brace);
    completion_fixture_free(&fixture);
}

static void test_foreign_stale_and_invalid_contexts_are_rejected(void)
{
    Logical_Completion_Fixture left = {0};
    Logical_Completion_Fixture right = {0};
    Noc_Logical_C_Ast invalid = {0};
    Noc_Logical_C_Ast_Completion_Context context;
    Noc_Logical_C_Ast_Completion_Context preserved;
    const Noc_Logical_C_Ast_Node *missing;
    Noc_Slice text;
    size_t missing_index;
    size_t missing_offset;
    size_t generation;

    CHECK(completion_fixture_build(&left,
                                   "",
                                   "int f(void) { return 1 }\n"));
    CHECK(completion_fixture_build(&right,
                                   "",
                                   "int f(void) { return 1 }\n"));
    CHECK(noc_logical_c_ast_generation(&left.ast) ==
          noc_logical_c_ast_generation(&right.ast));
    CHECK(noc_logical_c_ast_source_generation(&left.ast) ==
          noc_logical_c_ast_source_generation(&right.ast));
    missing_index = first_missing_node(&left.ast);
    missing = noc_logical_c_ast_node_at(&left.ast, missing_index);
    CHECK(missing != NULL);
    if (!missing) {
        completion_fixture_free(&left);
        completion_fixture_free(&right);
        return;
    }
    missing_offset = missing->bytes.begin;
    CHECK(noc_logical_c_ast_completion_context(&left.ast,
                                               missing_offset,
                                               &context));
    CHECK(noc_logical_c_ast_completion_next_expected_node(
              &right.ast,
              &context,
              NOC_C_AST_NODE_NONE) == NOC_C_AST_NODE_NONE);

    memset(&preserved, 0xA5, sizeof(preserved));
    context = preserved;
    text = noc_logical_source_text(noc_logical_c_ast_source(&left.ast));
    CHECK(!noc_logical_c_ast_completion_context(&invalid, 0, &context));
    CHECK(memcmp(&context, &preserved, sizeof(context)) == 0);
    CHECK(!noc_logical_c_ast_completion_context(&left.ast,
                                                text.count + 1,
                                                &context));
    CHECK(memcmp(&context, &preserved, sizeof(context)) == 0);
    CHECK(!noc_logical_c_ast_completion_context(&left.ast, 0, NULL));

    CHECK(noc_logical_c_ast_completion_context(&left.ast,
                                               missing_offset,
                                               &context));
    CHECK(noc_logical_c_ast_build(&left.tree,
                                  noc_c_ast_default_options(),
                                  &left.ast) == NOC_C_AST_OK);
    CHECK(context.generation + 1 == noc_logical_c_ast_generation(&left.ast));
    CHECK(noc_logical_c_ast_completion_next_expected_node(
              &left.ast,
              &context,
              NOC_C_AST_NODE_NONE) == NOC_C_AST_NODE_NONE);
    CHECK(noc_logical_c_ast_completion_context(&left.ast,
                                               missing_offset,
                                               &context));
    context.source_generation += 1;
    CHECK(noc_logical_c_ast_completion_next_expected_node(
              &left.ast,
              &context,
              NOC_C_AST_NODE_NONE) == NOC_C_AST_NODE_NONE);

    CHECK(noc_logical_c_ast_completion_context(&left.ast,
                                               missing_offset,
                                               &context));
    generation = left.ast.generation;
    noc_logical_c_ast_free(&left.ast);
    CHECK(left.ast.impl == NULL && left.ast.generation == generation);
    CHECK(noc_logical_c_ast_build(&left.tree,
                                  noc_c_ast_default_options(),
                                  &left.ast) == NOC_C_AST_OK);
    CHECK(left.ast.generation == generation + 1);
    CHECK(noc_logical_c_ast_completion_next_expected_node(
              &left.ast,
              &context,
              NOC_C_AST_NODE_NONE) == NOC_C_AST_NODE_NONE);

    completion_fixture_free(&left);
    completion_fixture_free(&right);
}

int main(void)
{
    test_every_logical_position_has_stable_adjacent_context();
    test_recovery_expectations_keep_logical_and_macro_provenance();
    test_eof_expectations_override_root_context();
    test_empty_logical_source_has_root_context();
    test_foreign_stale_and_invalid_contexts_are_rejected();
    return finish_suite("logical C AST completion context");
}
