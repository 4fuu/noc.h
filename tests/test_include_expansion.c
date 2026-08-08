#include "include_expansion_test_support.h"

static void test_expanded_forms_names_and_provenance(void)
{
    static const char definitions[] =
        "#define QUOTED \"quoted.h\"\n"
        "#define ANGLED <sys/types.h>\n"
        "#define EMPTY\n"
        "#define TRAIL \"tail.h\" EMPTY\n"
        "#define CLOSE >\n"
        "#define BAD value\n"
        "#define PREFIX L\"wide.h\"\n"
        "#define SPACED < spaced.h >\n"
        "#define COMMENTED <dir/**/file.h>\n"
        "#define OPEN <\n"
        "#define PAD /**/\n";
    static const char input[] =
        "#include QUOTED\n"
        "#include ANGLED\n"
        "#include EMPTY\n"
        "#include TRAIL\n"
        "#include <closed.h CLOSE\n"
        "#include <dir file CLOSE\n"
        "#include <dir/**/file CLOSE\n"
        "#include BAD\n"
        "#include <unfinished\n"
        "#include PREFIX\n"
        "#include \"\" EMPTY\n"
        "#include <> EMPTY\n"
        "#include ANGLED extra\n"
        "#include SPACED\n"
        "#include COMMENTED\n"
        "#include OPEN a PAD b CLOSE\n";
    static const Noc_Include_Operand_Status expected_statuses[] = {
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_EMPTY,
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_MALFORMED,
        NOC_INCLUDE_OPERAND_INCOMPLETE,
        NOC_INCLUDE_OPERAND_MALFORMED,
        NOC_INCLUDE_OPERAND_EMPTY,
        NOC_INCLUDE_OPERAND_EMPTY,
        NOC_INCLUDE_OPERAND_MALFORMED,
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_DIRECT,
    };
    static const Noc_Include_Form expected_forms[] = {
        NOC_INCLUDE_FORM_QUOTED,
        NOC_INCLUDE_FORM_ANGLED,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_QUOTED,
        NOC_INCLUDE_FORM_ANGLED,
        NOC_INCLUDE_FORM_ANGLED,
        NOC_INCLUDE_FORM_ANGLED,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_QUOTED,
        NOC_INCLUDE_FORM_ANGLED,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_ANGLED,
        NOC_INCLUDE_FORM_ANGLED,
        NOC_INCLUDE_FORM_ANGLED,
    };
    static const char *const expected_names[] = {
        "quoted.h", "sys/types.h", "", "tail.h", "closed.h", "dir file",
        "dir file", "", "", "", "", "", "", " spaced.h ", "dir file.h",
        "a b",
    };
    Macro_Expansion_Fixture fixture;
    Noc_Include_Expansion expansion = {0};
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(fixture.input.count == 16);
    for (index = 0; index < fixture.input.count; ++index) {
        size_t previous_generation = expansion.generation;
        CHECK(include_expansion_build_at(&fixture, index, &expansion) ==
              NOC_MACRO_EXPANSION_OK);
        CHECK(noc_include_expansion_is_valid(&expansion));
        CHECK(expansion.generation == previous_generation + 1);
        CHECK(expansion.directive_index == index);
        CHECK(expansion.status == expected_statuses[index]);
        CHECK(expansion.form == expected_forms[index]);
        CHECK(slice_equals(expansion.logical_name, expected_names[index]));
        CHECK(noc_macro_expansion_is_valid(&expansion.macro_expansion));
        CHECK(expansion.macro_expansion.input_unit == &fixture.input);
        if (expansion.status == NOC_INCLUDE_OPERAND_DIRECT) {
            CHECK(expansion.header_tokens.begin < expansion.header_tokens.end);
            CHECK(expansion.header_tokens.end <= expansion.macro_expansion.count);
            CHECK(expansion.problem_token_index == NOC_TOKEN_INDEX_NONE);
            CHECK(expansion.logical_name.data[expansion.logical_name.count] == '\0');
        } else if (expansion.status == NOC_INCLUDE_OPERAND_EMPTY &&
                   expansion.form == NOC_INCLUDE_FORM_NONE) {
            CHECK(expansion.header_tokens.begin == NOC_TOKEN_INDEX_NONE);
            CHECK(expansion.problem_token_index == NOC_TOKEN_INDEX_NONE);
        } else {
            CHECK(expansion.problem_token_index < expansion.macro_expansion.count);
        }
    }

    expansion.form = NOC_INCLUDE_FORM_NONE;
    CHECK(!noc_include_expansion_is_valid(&expansion));
    expansion.form = NOC_INCLUDE_FORM_ANGLED;
    CHECK(noc_include_expansion_is_valid(&expansion));
    expansion.header_tokens.end = expansion.macro_expansion.count + 1;
    CHECK(!noc_include_expansion_is_valid(&expansion));
    expansion.header_tokens.end = expansion.macro_expansion.count;
    CHECK(noc_include_expansion_is_valid(&expansion));

    noc_include_expansion_free(&expansion);
    CHECK(!noc_include_expansion_is_valid(&expansion));
    noc_include_expansion_free(&expansion);
    macro_fixture_deinit(&fixture);
}

static void test_builtin_options_and_transactional_failures(void)
{
    static const char definitions[] = "#define HEADER <one/two.h>\n";
    static const char input[] =
        "#include HEADER\n"
        "#include __FILE__\n"
        "#include \"direct.h\"\n";
    Macro_Expansion_Fixture fixture;
    Noc_Include_Operand operand = {0};
    Noc_Include_Expansion expansion = {0};
    Noc_Macro_Expansion_Options options = noc_macro_expansion_default_options();
    Noc_Macro_Expansion_Limits limits = noc_macro_expansion_default_limits();
    Noc_Macro_Expansion_Token *preserved_items;
    const char *preserved_name;
    size_t preserved_generation;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(include_expansion_build_at(&fixture, 0, &expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(slice_equals(expansion.logical_name, "one/two.h"));
    preserved_items = expansion.macro_expansion.items;
    preserved_name = expansion.logical_name.data;
    preserved_generation = expansion.generation;

    CHECK(noc_include_operand_build(&fixture.input, 1, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(noc_include_expansion_build_with_options(&fixture.environment,
                                                   fixture.environment.count,
                                                   &operand,
                                                   options,
                                                   &expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(slice_equals(expansion.logical_name, "macro-input.c"));
    CHECK(expansion.macro_expansion.items[0].origin ==
          NOC_MACRO_EXPANSION_TOKEN_BUILTIN);
    noc_include_operand_free(&operand);

    preserved_items = expansion.macro_expansion.items;
    preserved_name = expansion.logical_name.data;
    preserved_generation = expansion.generation;
    CHECK(noc_include_operand_build(&fixture.input, 0, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    limits.max_output_tokens = 1;
    CHECK(noc_include_expansion_build(&fixture.environment,
                                      fixture.environment.count,
                                      &operand,
                                      limits,
                                      &expansion) ==
          NOC_MACRO_EXPANSION_OUTPUT_LIMIT);
    CHECK(expansion.macro_expansion.items == preserved_items);
    CHECK(expansion.logical_name.data == preserved_name);
    CHECK(expansion.generation == preserved_generation);
    noc_include_operand_free(&operand);

    CHECK(noc_include_operand_build(&fixture.input, 2, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(operand.status == NOC_INCLUDE_OPERAND_DIRECT);
    CHECK(noc_include_expansion_build(&fixture.environment,
                                      fixture.environment.count,
                                      &operand,
                                      noc_macro_expansion_default_limits(),
                                      &expansion) ==
          NOC_MACRO_EXPANSION_INVALID_ARGUMENT);
    CHECK(expansion.macro_expansion.items == preserved_items);
    noc_include_operand_free(&operand);

    expansion.generation = SIZE_MAX;
    CHECK(noc_include_operand_build(&fixture.input, 0, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(noc_include_expansion_build(&fixture.environment,
                                      fixture.environment.count,
                                      &operand,
                                      noc_macro_expansion_default_limits(),
                                      &expansion) ==
          NOC_MACRO_EXPANSION_GENERATION_EXHAUSTED);
    CHECK(expansion.macro_expansion.items == preserved_items);
    expansion.generation = preserved_generation;
    noc_include_operand_free(&operand);

    CHECK(noc_include_operand_build(&fixture.input, 0, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(noc_preprocessor_unit_build(&fixture.context,
                                      &fixture.input_snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture.input));
    CHECK(noc_include_expansion_build(&fixture.environment,
                                      fixture.environment.count,
                                      &operand,
                                      noc_macro_expansion_default_limits(),
                                      &expansion) == NOC_MACRO_EXPANSION_STALE);
    CHECK(!noc_include_expansion_is_valid(&expansion));
    CHECK(expansion.macro_expansion.items == preserved_items);
    CHECK(expansion.logical_name.data == preserved_name);
    CHECK(expansion.generation == preserved_generation);

    noc_include_operand_free(&operand);
    noc_include_expansion_free(&expansion);
    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_expanded_forms_names_and_provenance();
    test_builtin_options_and_transactional_failures();
    return finish_suite("include-expansion");
}
