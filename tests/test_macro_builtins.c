#include "macro_expansion_test_support.h"

static void test_builtin_names_and_logical_classification(void)
{
    static const char spliced_line[] = "__LI\\\nNE__";

    CHECK(strcmp(noc_macro_builtin_kind_name(NOC_MACRO_BUILTIN_NONE),
                 "none") == 0);
    CHECK(strcmp(noc_macro_builtin_kind_name(NOC_MACRO_BUILTIN_FILE),
                 "file") == 0);
    CHECK(strcmp(noc_macro_builtin_kind_name(NOC_MACRO_BUILTIN_LINE),
                 "line") == 0);
    CHECK(strcmp(noc_macro_builtin_kind_name(NOC_MACRO_BUILTIN_STDC),
                 "stdc") == 0);
    CHECK(strcmp(noc_macro_builtin_kind_name(NOC_MACRO_BUILTIN_STDC_VERSION),
                 "stdc-version") == 0);
    CHECK(strcmp(noc_macro_builtin_kind_name(NOC_MACRO_BUILTIN_STDC_HOSTED),
                 "stdc-hosted") == 0);
    CHECK(strcmp(noc_macro_builtin_kind_name(NOC_MACRO_BUILTIN_DATE),
                 "date") == 0);
    CHECK(strcmp(noc_macro_builtin_kind_name(NOC_MACRO_BUILTIN_TIME),
                 "time") == 0);
    CHECK(strcmp(noc_macro_builtin_kind_name((Noc_Macro_Builtin_Kind)99),
                 "unknown") == 0);
    CHECK(noc_macro_builtin_kind_from_name(noc_slice_from_cstr("__FILE__")) ==
          NOC_MACRO_BUILTIN_FILE);
    CHECK(noc_macro_builtin_kind_from_name(noc_slice_from_cstr("__LINE__")) ==
          NOC_MACRO_BUILTIN_LINE);
    CHECK(noc_macro_builtin_kind_from_name(
              (Noc_Slice){spliced_line, sizeof(spliced_line) - 1}) ==
          NOC_MACRO_BUILTIN_LINE);
    CHECK(noc_macro_builtin_kind_from_name(noc_slice_from_cstr("__STDC__")) ==
          NOC_MACRO_BUILTIN_STDC);
    CHECK(noc_macro_builtin_kind_from_name(
              noc_slice_from_cstr("__STDC_VERSION__")) ==
          NOC_MACRO_BUILTIN_STDC_VERSION);
    CHECK(noc_macro_builtin_kind_from_name(noc_slice_from_cstr("__DATE__")) ==
          NOC_MACRO_BUILTIN_DATE);
    CHECK(noc_macro_builtin_kind_from_name(noc_slice_from_cstr("__TIME__")) ==
          NOC_MACRO_BUILTIN_TIME);
    CHECK(noc_macro_builtin_kind_from_name(
              noc_slice_from_cstr("__STDC_HOSTED__")) ==
          NOC_MACRO_BUILTIN_STDC_HOSTED);
    CHECK(noc_macro_builtin_kind_from_name(noc_slice_from_cstr("ordinary")) ==
          NOC_MACRO_BUILTIN_NONE);
    CHECK(strcmp(noc_macro_expansion_token_origin_name(
                     NOC_MACRO_EXPANSION_TOKEN_BUILTIN),
                 "builtin") == 0);
}

static void test_builtin_context_rescan_and_provenance(void)
{
    static const char definitions[] =
        "#define LINE __LINE__\n"
        "#define FILE __FILE__\n"
        "#define INNER __LINE__\n"
        "#define OUTER INNER\n"
        "#define ID(x) x\n"
        "#define CAT(a,b) a##b\n"
        "#define STR(x) #x\n";
    static const char input[] =
        "__FILE__ __LINE__ __STDC__ __STDC_VERSION__\n"
        "LINE FILE\n"
        "OUTER\n"
        "ID(__LINE__)\n"
        "__LI\\\nNE__\n"
        "CAT(__LI,NE__)\n"
        "STR(__LINE__)\n";
    static const char expected[] =
        "\"macro-input.c\" 1 1 201112L\n"
        "2 \"macro-input.c\"\n"
        "3\n"
        "4\n"
        "5\n"
        "7\n"
        "\"__LINE__\"\n";
    Macro_Expansion_Fixture fixture;
    size_t builtin_count = 0;
    size_t direct_count = 0;
    size_t pasted_line_count = 0;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_input_line(&fixture, 7).begin != NOC_TOKEN_INDEX_NONE);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(fixture.expansion.generated_spelling_count == 12);
    for (index = 0; index < fixture.expansion.count; ++index) {
        const Noc_Macro_Expansion_Token *token =
            noc_macro_expansion_token_at(&fixture.expansion, index);
        CHECK(token != NULL);
        if (!token || token->origin != NOC_MACRO_EXPANSION_TOKEN_BUILTIN) continue;
        CHECK(token->builtin_kind != NOC_MACRO_BUILTIN_NONE);
        CHECK(token->generated_spelling_index <
              fixture.expansion.generated_spelling_count);
        CHECK(token->token.text.data ==
              fixture.expansion.generated_spellings[
                  token->generated_spelling_index].data);
        CHECK(token->token.text.count ==
              fixture.expansion.generated_spellings[
                  token->generated_spelling_index].count);
        if (token->frame_index == NOC_TOKEN_INDEX_NONE) {
            CHECK(token->unit == &fixture.input);
            direct_count += 1;
        }
        if (token->builtin_kind == NOC_MACRO_BUILTIN_LINE &&
            noc_token_is_punct(
                token->unit->preprocessing_tokens[
                    token->preprocessing_token_index].token,
                "##")) {
            CHECK(token->frame_index < fixture.expansion.frame_count);
            pasted_line_count += 1;
        }
        builtin_count += 1;
    }
    CHECK(builtin_count == 10);
    CHECK(direct_count == 5);
    CHECK(pasted_line_count == 1);
    CHECK(fixture.diagnostics.errors == 0);

    macro_fixture_deinit(&fixture);
}

static void test_builtin_argument_location_and_environment_precedence(void)
{
    static const char definitions[] =
        "#define INNER __LINE__\n"
        "#define OUTER(x) x\n"
        "#define __LI\\\nNE__ 900\n"
        "#undef __LINE__\n"
        "#define __LINE__ __LINE__\n";
    static const char input[] =
        "OUTER(\n"
        "  INNER\n"
        ")\n"
        "OUTER(\n"
        "  __LINE__\n"
        ")\n"
        "__LINE__\n";
    Macro_Expansion_Fixture fixture;
    Noc_Token_Range range;
    const Noc_Macro_Expansion_Token *token;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(fixture.environment.count == 5);

    range.begin = macro_fixture_input_line(&fixture, 1).begin;
    range.end = macro_fixture_input_line(&fixture, 3).end;
    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    2,
                                    &fixture.input,
                                    range,
                                    noc_macro_expansion_default_limits(),
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\n  2\n\n"));
    token = NULL;
    for (index = 0; index < fixture.expansion.count; ++index) {
        if (fixture.expansion.items[index].origin ==
            NOC_MACRO_EXPANSION_TOKEN_BUILTIN) {
            token = &fixture.expansion.items[index];
            break;
        }
    }
    CHECK(token != NULL);
    if (token) {
        CHECK(token->builtin_kind == NOC_MACRO_BUILTIN_LINE);
        CHECK(token->token.location.line == 1);
        CHECK(token->unit == &fixture.definitions);
        CHECK(token->frame_index < fixture.expansion.frame_count);
    }

    range.begin = macro_fixture_input_line(&fixture, 4).begin;
    range.end = macro_fixture_input_line(&fixture, 6).end;
    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    2,
                                    &fixture.input,
                                    range,
                                    noc_macro_expansion_default_limits(),
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\n  5\n\n"));
    token = NULL;
    for (index = 0; index < fixture.expansion.count; ++index) {
        if (fixture.expansion.items[index].origin ==
            NOC_MACRO_EXPANSION_TOKEN_BUILTIN) {
            token = &fixture.expansion.items[index];
            break;
        }
    }
    CHECK(token != NULL);
    if (token) {
        CHECK(token->builtin_kind == NOC_MACRO_BUILTIN_LINE);
        CHECK(token->token.location.line == 5);
        CHECK(token->unit == &fixture.input);
        CHECK(token->frame_index < fixture.expansion.frame_count);
    }

    range = macro_fixture_input_line(&fixture, 7);
    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    3,
                                    &fixture.input,
                                    range,
                                    noc_macro_expansion_default_limits(),
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "900\n"));
    CHECK(fixture.expansion.frame_count == 1);
    token = noc_macro_expansion_token_at(&fixture.expansion, 0);
    CHECK(token != NULL);
    if (token) {
        CHECK(token->origin == NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT);
        CHECK(token->builtin_kind == NOC_MACRO_BUILTIN_NONE);
    }

    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    4,
                                    &fixture.input,
                                    range,
                                    noc_macro_expansion_default_limits(),
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "7\n"));
    CHECK(fixture.expansion.frame_count == 0);
    token = noc_macro_expansion_token_at(&fixture.expansion, 0);
    CHECK(token != NULL);
    if (token) {
        CHECK(token->origin == NOC_MACRO_EXPANSION_TOKEN_BUILTIN);
        CHECK(token->builtin_kind == NOC_MACRO_BUILTIN_LINE);
    }

    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    5,
                                    &fixture.input,
                                    range,
                                    noc_macro_expansion_default_limits(),
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "__LINE__\n"));
    CHECK(fixture.expansion.frame_count == 1);
    token = noc_macro_expansion_token_at(&fixture.expansion, 0);
    CHECK(token != NULL);
    if (token) {
        CHECK(token->origin == NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT);
        CHECK(token->builtin_kind == NOC_MACRO_BUILTIN_NONE);
    }
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));

    macro_fixture_deinit(&fixture);
}

static void test_builtin_spelling_growth_and_validator(void)
{
    static const char input[] =
        "__FILE__ __LINE__ __STDC__ __STDC_VERSION__\n"
        "__LINE__\n__LINE__\n__LINE__\n__LINE__\n__LINE__\n"
        "__LINE__\n__LINE__\n__LINE__\n__LINE__\n__LINE__\n"
        "__LINE__\n__LINE__\n__LINE__\n__LINE__\n__LINE__\n"
        "__LINE__\n__LINE__\n__LINE__\n__LINE__\n__LINE__\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Token *file_token = NULL;
    Noc_Macro_Expansion_Token *line_token = NULL;
    Noc_Macro_Expansion_Token *input_token = NULL;
    const char *first_spelling;
    size_t index;

    macro_fixture_init(&fixture, "__LINE__", input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(fixture.expansion.generated_spelling_count == 24);
    CHECK(fixture.expansion.generated_spelling_capacity >= 24);
    first_spelling = fixture.expansion.items[0].token.text.data;
    for (index = 0; index < fixture.expansion.count; ++index) {
        Noc_Macro_Expansion_Token *candidate = &fixture.expansion.items[index];
        if (!file_token &&
            candidate->builtin_kind == NOC_MACRO_BUILTIN_FILE) {
            file_token = candidate;
        }
        if (!line_token &&
            candidate->builtin_kind == NOC_MACRO_BUILTIN_LINE) {
            line_token = candidate;
        }
        if (!input_token &&
            candidate->origin == NOC_MACRO_EXPANSION_TOKEN_INPUT) {
            input_token = candidate;
        }
    }
    CHECK(file_token != NULL);
    CHECK(line_token != NULL);
    CHECK(input_token != NULL);
    CHECK(first_spelling == fixture.expansion.generated_spellings[0].data);
    CHECK(slice_equals(fixture.expansion.generated_spellings[0],
                       "\"macro-input.c\""));

    if (line_token) {
        Noc_Macro_Builtin_Kind saved_kind = line_token->builtin_kind;
        const Noc_Preprocessor_Unit *saved_unit = line_token->unit;
        size_t saved_unit_generation = line_token->unit_stream_generation;
        size_t saved_token_index = line_token->preprocessing_token_index;
        size_t saved_spelling_index = line_token->generated_spelling_index;
        line_token->builtin_kind = (Noc_Macro_Builtin_Kind)99;
        CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
        line_token->builtin_kind = NOC_MACRO_BUILTIN_STDC_VERSION;
        CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
        line_token->builtin_kind = saved_kind;
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));

        line_token->unit = &fixture.definitions;
        line_token->unit_stream_generation = fixture.definitions.stream.generation;
        line_token->preprocessing_token_index = 0;
        CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
        line_token->unit = saved_unit;
        line_token->unit_stream_generation = saved_unit_generation;
        line_token->preprocessing_token_index = saved_token_index;
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));

        line_token->generated_spelling_index =
            fixture.expansion.generated_spelling_count;
        CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
        line_token->generated_spelling_index = saved_spelling_index;
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    }
    if (file_token) {
        Noc_Token_Kind saved_token_kind = file_token->token.kind;
        file_token->token.kind = NOC_TOKEN_NUMBER;
        CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
        file_token->token.kind = saved_token_kind;
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    }
    if (input_token) {
        input_token->builtin_kind = NOC_MACRO_BUILTIN_LINE;
        CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
        input_token->builtin_kind = NOC_MACRO_BUILTIN_NONE;
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    }

    macro_fixture_deinit(&fixture);
}

static void test_builtin_failure_transactionality_and_staleness(void)
{
    static const char definitions[] = "#define MANY 1 2 3 4 5\n";
    static const char input[] =
        "__LINE__\n"
        "__LINE__ MANY\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Limits limits = noc_macro_expansion_default_limits();
    Noc_Token_Range range;
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Macro_Expansion_Frame *preserved_frames;
    Noc_Slice *preserved_spellings;
    size_t preserved_count;
    size_t preserved_spelling_count;
    size_t preserved_generation;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_input_line(&fixture, 1)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "1\n"));
    preserved_items = fixture.expansion.items;
    preserved_frames = fixture.expansion.frames;
    preserved_spellings = fixture.expansion.generated_spellings;
    preserved_count = fixture.expansion.count;
    preserved_spelling_count = fixture.expansion.generated_spelling_count;
    preserved_generation = fixture.expansion.generation;

    range = macro_fixture_input_line(&fixture, 2);
    limits.max_output_tokens = range.end - range.begin;
    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    fixture.environment.count,
                                    &fixture.input,
                                    range,
                                    limits,
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OUTPUT_LIMIT);
    CHECK(fixture.expansion.items == preserved_items);
    CHECK(fixture.expansion.frames == preserved_frames);
    CHECK(fixture.expansion.generated_spellings == preserved_spellings);
    CHECK(fixture.expansion.count == preserved_count);
    CHECK(fixture.expansion.generated_spelling_count ==
          preserved_spelling_count);
    CHECK(fixture.expansion.generation == preserved_generation);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(macro_fixture_render_equals(&fixture, "1\n"));

    CHECK(noc_preprocessor_unit_build(&fixture.context,
                                      &fixture.input_snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture.input));
    CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(noc_macro_expansion_token_at(&fixture.expansion, 0) == NULL);

    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_builtin_names_and_logical_classification();
    test_builtin_context_rescan_and_provenance();
    test_builtin_argument_location_and_environment_precedence();
    test_builtin_spelling_growth_and_validator();
    test_builtin_failure_transactionality_and_staleness();
    return finish_suite("macro-builtins");
}
