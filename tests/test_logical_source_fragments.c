#include "macro_expansion_test_support.h"

static bool token_is(const Noc_Preprocessor_Unit *unit,
                     size_t index,
                     const char *spelling)
{
    const Noc_Preprocessing_Token *token =
        noc_preprocessor_token_at(unit, index);
    return token && slice_equals(token->token.text, spelling);
}

static size_t nth_token(const Noc_Preprocessor_Unit *unit,
                        const char *spelling,
                        size_t occurrence)
{
    size_t index;
    for (index = 0; index < unit->preprocessing_token_count; ++index) {
        if (!token_is(unit, index, spelling)) continue;
        if (occurrence == 0) return index;
        occurrence -= 1;
    }
    return NOC_TOKEN_INDEX_NONE;
}

static void test_ordered_fragments_rebase_provenance_and_parse(void)
{
    static const char definitions[] =
        "#define TYPE(name) int name\n"
        "#define DECL(name) TYPE(name);\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion second = {0};
    const Noc_Macro_Expansion *fragments[2];
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    Noc_Logical_C_Ast ast = {0};
    const Noc_Logical_Macro_Frame *frame;
    const char *boundary;
    size_t first_begin;
    size_t second_begin;
    size_t input_end;
    size_t token_index;
    size_t boundary_offset;
    size_t int_count = 0;
    size_t separator_count = 0;
    bool saw_boundary_separator = false;

    macro_fixture_init(&fixture, definitions, "DECL(a)DECL(b)");
    first_begin = nth_token(&fixture.input, "DECL", 0);
    second_begin = nth_token(&fixture.input, "DECL", 1);
    input_end = fixture.input.preprocessing_token_count - 1;
    CHECK(first_begin != NOC_TOKEN_INDEX_NONE);
    CHECK(second_begin != NOC_TOKEN_INDEX_NONE);
    CHECK(first_begin < second_begin && second_begin < input_end);
    CHECK(noc_macro_expansion_build(
              &fixture.environment,
              fixture.environment.count,
              &fixture.input,
              (Noc_Token_Range){first_begin, second_begin},
              noc_macro_expansion_default_limits(),
              &fixture.expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_build(
              &fixture.environment,
              fixture.environment.count,
              &fixture.input,
              (Noc_Token_Range){second_begin, input_end},
              noc_macro_expansion_default_limits(),
              &second) == NOC_MACRO_EXPANSION_OK);
    CHECK(fixture.expansion.frame_count == 2);
    CHECK(second.frame_count == 2);
    fragments[0] = &fixture.expansion;
    fragments[1] = &second;

    CHECK(noc_logical_source_build_macro_expansions(
              fragments,
              2,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_is_valid(&source));
    CHECK(slice_equals(noc_logical_source_text(&source),
                       "int a ; int b ;"));
    boundary = strstr(noc_logical_source_text(&source).data, "; int");
    CHECK(boundary != NULL);
    boundary_offset = boundary
                          ? (size_t)(boundary -
                                     noc_logical_source_text(&source).data) +
                                1
                          : SIZE_MAX;
    CHECK(noc_logical_source_file_count(&source) == 2);
    CHECK(noc_logical_source_macro_frame_count(&source) == 4);
    frame = noc_logical_source_macro_frame_at(&source, 0);
    CHECK(frame &&
          frame->parent_macro_frame_index == NOC_TOKEN_INDEX_NONE);
    frame = noc_logical_source_macro_frame_at(&source, 1);
    CHECK(frame && frame->parent_macro_frame_index == 0);
    frame = noc_logical_source_macro_frame_at(&source, 2);
    CHECK(frame &&
          frame->parent_macro_frame_index == NOC_TOKEN_INDEX_NONE);
    frame = noc_logical_source_macro_frame_at(&source, 3);
    CHECK(frame && frame->parent_macro_frame_index == 2);

    for (token_index = 0;
         token_index < noc_logical_source_token_count(&source);
         ++token_index) {
        const Noc_Logical_Token *token =
            noc_logical_source_token_at(&source, token_index);
        Noc_Logical_Token_Macro_Provenance provenance;
        CHECK(token != NULL);
        if (!token) continue;
        if ((token->flags & NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) != 0) {
            separator_count += 1;
            if (token->bytes.begin == boundary_offset &&
                token->bytes.end == boundary_offset + 1) {
                saw_boundary_separator = true;
            }
            CHECK(!noc_logical_source_token_macro_provenance(
                &source,
                token_index,
                &provenance));
            continue;
        }
        if (!slice_equals(noc_logical_source_token_text(&source, token_index),
                          "int")) {
            continue;
        }
        CHECK(noc_logical_source_token_macro_provenance(&source,
                                                        token_index,
                                                        &provenance));
        CHECK(provenance.macro_origin ==
              NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT);
        CHECK(provenance.macro_frame_index == (int_count == 0 ? 1 : 3));
        int_count += 1;
    }
    CHECK(int_count == 2);
    CHECK(separator_count != 0);
    CHECK(saw_boundary_separator);

    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    CHECK(!noc_logical_c_parse_tree_has_error(&tree));
    CHECK(noc_logical_c_ast_build(&tree,
                                  noc_c_ast_default_options(),
                                  &ast) == NOC_C_AST_OK);
    CHECK(noc_logical_c_ast_is_syntax_complete(&ast));
    CHECK(noc_logical_c_ast_node_count(&ast) != 0);

    noc_logical_c_ast_free(&ast);
    noc_logical_c_parse_tree_free(&tree);
    noc_logical_source_free(&source);
    noc_macro_expansion_free(&second);
    macro_fixture_deinit(&fixture);
}

static void test_empty_and_single_fragment_compatibility(void)
{
    Macro_Expansion_Fixture fixture;
    const Noc_Macro_Expansion *fragment;
    Noc_Logical_Source empty = {0};
    Noc_Logical_Source singular = {0};
    Noc_Logical_Source plural = {0};
    Noc_Logical_Source_Options options = noc_logical_source_default_options();
    Noc_Logical_Source_Options legacy_options;
    size_t index;

    CHECK(noc_logical_source_build_macro_expansions(NULL,
                                                    0,
                                                    options,
                                                    &empty) ==
          NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_is_valid(&empty));
    CHECK(noc_logical_source_text(&empty).data != NULL);
    CHECK(noc_logical_source_text(&empty).count == 0);
    CHECK(noc_logical_source_token_count(&empty) == 0);
    CHECK(noc_logical_source_file_count(&empty) == 0);
    CHECK(noc_logical_source_macro_frame_count(&empty) == 0);

    macro_fixture_init(&fixture, "#define VALUE 7\n", "int x=VALUE;");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    fragment = &fixture.expansion;
    legacy_options = options;
    legacy_options.max_fragments = 0;
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    legacy_options,
                                                    &singular) ==
          NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_build_macro_expansions(&fragment,
                                                    1,
                                                    options,
                                                    &plural) ==
          NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_slice_equal(noc_logical_source_text(&singular),
                          noc_logical_source_text(&plural)));
    CHECK(noc_logical_source_token_count(&singular) ==
          noc_logical_source_token_count(&plural));
    CHECK(noc_logical_source_file_count(&singular) ==
          noc_logical_source_file_count(&plural));
    CHECK(noc_logical_source_macro_frame_count(&singular) ==
          noc_logical_source_macro_frame_count(&plural));
    for (index = 0; index < noc_logical_source_token_count(&singular); ++index) {
        const Noc_Logical_Token *left =
            noc_logical_source_token_at(&singular, index);
        const Noc_Logical_Token *right =
            noc_logical_source_token_at(&plural, index);
        CHECK(left != NULL && right != NULL);
        CHECK(left && right && left->kind == right->kind);
        CHECK(left && right && left->flags == right->flags);
        CHECK(noc_slice_equal(noc_logical_source_token_text(&singular, index),
                              noc_logical_source_token_text(&plural, index)));
    }

    noc_logical_source_free(&plural);
    noc_logical_source_free(&singular);
    noc_logical_source_free(&empty);
    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_ordered_fragments_rebase_provenance_and_parse();
    test_empty_and_single_fragment_compatibility();
    return finish_suite("logical-source fragments");
}
