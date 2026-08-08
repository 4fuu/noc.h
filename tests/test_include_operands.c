#include "include_test_support.h"

static void test_names_and_recoverable_operand_states(void)
{
    static const Noc_Include_Operand_Status statuses[] = {
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED,
        NOC_INCLUDE_OPERAND_EMPTY,
        NOC_INCLUDE_OPERAND_MISSING,
        NOC_INCLUDE_OPERAND_MALFORMED,
        NOC_INCLUDE_OPERAND_INCOMPLETE,
    };
    static const char *const status_names[] = {
        "direct", "expansion-required", "empty", "missing", "malformed",
        "incomplete",
    };
    static const Noc_Include_Form forms[] = {
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_QUOTED,
        NOC_INCLUDE_FORM_ANGLED,
    };
    static const char *const form_names[] = {"none", "quoted", "angled"};
    static const Noc_Include_Operand_Build_Status build_statuses[] = {
        NOC_INCLUDE_OPERAND_BUILD_OK,
        NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT,
        NOC_INCLUDE_OPERAND_BUILD_STALE,
        NOC_INCLUDE_OPERAND_BUILD_GENERATION_EXHAUSTED,
        NOC_INCLUDE_OPERAND_BUILD_OUT_OF_MEMORY,
    };
    static const char *const build_names[] = {
        "ok", "invalid-argument", "stale", "generation-exhausted",
        "out-of-memory",
    };
    static const char source[] =
        "#include \"local.h\"\n"
        "#include <sys/types.h>\n"
        "#include \"split\\\n/path.h\"\n"
        "#include <>\n"
        "#include \"\"\n"
        "#include HEADER\n"
        "#include WRAP(NAME)\n"
        "#include\n"
        "#include \"x.h\" extra\n"
        "#include \"x.h\" 123\n"
        "#include <a.h> <b.h>\n"
        "#include <CLOSE\n"
        "#include \"unfinished\n"
        "#define VALUE 1\n";
    static const Noc_Include_Operand_Status expected_statuses[] = {
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_DIRECT,
        NOC_INCLUDE_OPERAND_EMPTY,
        NOC_INCLUDE_OPERAND_EMPTY,
        NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED,
        NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED,
        NOC_INCLUDE_OPERAND_MISSING,
        NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED,
        NOC_INCLUDE_OPERAND_MALFORMED,
        NOC_INCLUDE_OPERAND_MALFORMED,
        NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED,
        NOC_INCLUDE_OPERAND_INCOMPLETE,
    };
    static const Noc_Include_Form expected_forms[] = {
        NOC_INCLUDE_FORM_QUOTED,
        NOC_INCLUDE_FORM_ANGLED,
        NOC_INCLUDE_FORM_QUOTED,
        NOC_INCLUDE_FORM_ANGLED,
        NOC_INCLUDE_FORM_QUOTED,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_NONE,
        NOC_INCLUDE_FORM_NONE,
    };
    static const char *const expected_names[] = {
        "local.h", "sys/types.h", "split/path.h", "", "", "", "", "",
        "", "", "", "", "",
    };
    Include_Fixture fixture;
    Noc_Include_Operand operand = {0};
    size_t index;

    for (index = 0; index < sizeof(statuses) / sizeof(statuses[0]); ++index) {
        CHECK(strcmp(noc_include_operand_status_name(statuses[index]),
                     status_names[index]) == 0);
    }
    CHECK(strcmp(noc_include_operand_status_name(
                     (Noc_Include_Operand_Status)99),
                 "unknown") == 0);
    for (index = 0; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        CHECK(strcmp(noc_include_form_name(forms[index]), form_names[index]) == 0);
    }
    CHECK(strcmp(noc_include_form_name((Noc_Include_Form)99), "unknown") == 0);
    for (index = 0;
         index < sizeof(build_statuses) / sizeof(build_statuses[0]);
         ++index) {
        CHECK(strcmp(noc_include_operand_build_status_name(build_statuses[index]),
                     build_names[index]) == 0);
    }
    CHECK(strcmp(noc_include_operand_build_status_name(
                     (Noc_Include_Operand_Build_Status)99),
                 "unknown") == 0);

    include_fixture_init(&fixture, "src/main.c", source);
    CHECK(fixture.unit.count == 14);
    for (index = 0; index < 13; ++index) {
        size_t previous_generation = operand.generation;
        CHECK(noc_include_operand_build(&fixture.unit, index, &operand) ==
              NOC_INCLUDE_OPERAND_BUILD_OK);
        CHECK(noc_include_operand_is_valid(&operand));
        CHECK(operand.generation == previous_generation + 1);
        CHECK(operand.directive_index == index);
        CHECK(operand.status == expected_statuses[index]);
        CHECK(operand.form == expected_forms[index]);
        CHECK(slice_equals(operand.logical_name, expected_names[index]));
        if (operand.body_tokens.begin != NOC_TOKEN_INDEX_NONE) {
            CHECK(operand.body_tokens.begin < operand.body_tokens.end);
            CHECK(operand.body_tokens.end <=
                  fixture.unit.preprocessing_token_count);
        } else {
            CHECK(operand.status == NOC_INCLUDE_OPERAND_MISSING);
            CHECK(operand.body_tokens.end == NOC_TOKEN_INDEX_NONE);
        }
        if (operand.status == NOC_INCLUDE_OPERAND_DIRECT) {
            CHECK(operand.header_token_index < operand.body_tokens.end);
            CHECK(operand.problem_token_index == NOC_TOKEN_INDEX_NONE);
            CHECK(operand.logical_name.data[
                      operand.logical_name.count] == '\0');
        } else if (operand.status != NOC_INCLUDE_OPERAND_MISSING) {
            CHECK(operand.problem_token_index < operand.body_tokens.end);
        }
    }

    CHECK(noc_include_operand_build(&fixture.unit, 13, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT);
    CHECK(noc_include_operand_build(&fixture.unit,
                                    fixture.unit.count,
                                    &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT);
    CHECK(noc_include_operand_build(NULL, 0, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT);
    CHECK(noc_include_operand_build(&fixture.unit, 0, NULL) ==
          NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT);

    noc_include_operand_free(&operand);
    include_fixture_deinit(&fixture);
}

static void test_macro_replacement_can_complete_physical_operands(void)
{
    static const char source[] =
        "#define EMPTY\n"
        "#include \"x.h\" EMPTY\n"
        "#define CLOSE >\n"
        "#include <x.h CLOSE\n";
    Include_Fixture fixture;
    Noc_Include_Operand operand = {0};

    include_fixture_init(&fixture, "src/macros.c", source);
    CHECK(fixture.unit.count == 4);
    CHECK(noc_include_operand_build(&fixture.unit, 1, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(operand.status == NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED);
    CHECK(operand.problem_token_index > operand.body_tokens.begin);
    CHECK(noc_include_operand_build(&fixture.unit, 3, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(operand.status == NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED);
    CHECK(noc_include_operand_is_valid(&operand));

    noc_include_operand_free(&operand);
    include_fixture_deinit(&fixture);
}

static void test_ownership_validation_staleness_and_transactionality(void)
{
    static const char source[] =
        "#include \"stable.h\"\n"
        "#include <other.h>\n";
    Include_Fixture fixture;
    Noc_Include_Operand operand = {0};
    const char *owned_name;
    size_t saved_generation;
    size_t saved_stream_generation;

    include_fixture_init(&fixture, "src/owner.c", source);
    CHECK(noc_include_operand_build(&fixture.unit, 0, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(noc_include_operand_is_valid(&operand));
    owned_name = operand.logical_name.data;
    saved_generation = operand.generation;

    operand.form = NOC_INCLUDE_FORM_NONE;
    CHECK(!noc_include_operand_is_valid(&operand));
    operand.form = NOC_INCLUDE_FORM_QUOTED;
    CHECK(noc_include_operand_is_valid(&operand));
    operand.header_token_index = NOC_TOKEN_INDEX_NONE;
    CHECK(!noc_include_operand_is_valid(&operand));
    operand.header_token_index = operand.body_tokens.begin;
    CHECK(noc_include_operand_is_valid(&operand));

    operand.generation = SIZE_MAX;
    CHECK(noc_include_operand_build(&fixture.unit, 1, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_GENERATION_EXHAUSTED);
    CHECK(operand.logical_name.data == owned_name);
    CHECK(slice_equals(operand.logical_name, "stable.h"));
    operand.generation = saved_generation;
    CHECK(noc_include_operand_is_valid(&operand));

    saved_stream_generation = fixture.unit.stream.generation;
    fixture.unit.stream.generation += 1;
    CHECK(!noc_include_operand_is_valid(&operand));
    CHECK(noc_include_operand_build(&fixture.unit, 1, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_STALE);
    CHECK(operand.logical_name.data == owned_name);
    fixture.unit.stream.generation = saved_stream_generation;
    CHECK(noc_include_operand_is_valid(&operand));

    CHECK(noc_preprocessor_unit_build(&fixture.context,
                                      &fixture.snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture.unit));
    CHECK(!noc_include_operand_is_valid(&operand));
    CHECK(slice_equals(operand.logical_name, "stable.h"));
    CHECK(noc_include_operand_build(&fixture.unit, 1, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(noc_include_operand_is_valid(&operand));
    CHECK(slice_equals(operand.logical_name, "other.h"));
    CHECK(operand.generation == saved_generation + 1);

    owned_name = operand.logical_name.data;
    saved_generation = operand.generation;
    noc_preprocessor_unit_free(&fixture.unit);
    CHECK(noc_include_operand_build(&fixture.unit, 0, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_STALE);
    CHECK(operand.logical_name.data == owned_name);
    CHECK(operand.generation == saved_generation);

    noc_include_operand_free(&operand);
    CHECK(!noc_include_operand_is_valid(&operand));
    noc_include_operand_free(&operand);
    include_fixture_deinit(&fixture);
}

int main(void)
{
    test_names_and_recoverable_operand_states();
    test_macro_replacement_can_complete_physical_operands();
    test_ownership_validation_staleness_and_transactionality();
    return finish_suite("include-operands");
}
