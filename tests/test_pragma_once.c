#include "include_control_test_support.h"

static void check_status(const Include_Control_Fixture *fixture,
                         size_t directive_index,
                         Noc_Pragma_Once_Status expected)
{
    Noc_Pragma_Once pragma_once = {0};
    CHECK(noc_pragma_once_build(&fixture->unit,
                                directive_index,
                                &pragma_once) == NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(noc_pragma_once_is_valid(&pragma_once));
    CHECK(pragma_once.status == expected);
    CHECK(pragma_once.directive_index == directive_index);
    CHECK(pragma_once.body_tokens.begin ==
          noc_preprocessor_directive_body_tokens(&fixture->unit,
                                                 directive_index).begin);
    CHECK(pragma_once.body_tokens.end ==
          noc_preprocessor_directive_body_tokens(&fixture->unit,
                                                 directive_index).end);
}

static void test_exact_trivia_and_splices(void)
{
    static const char source[] =
        "#pragma once\n"
        "#pragma /* before */ once /* after */\n"
        "#pragma on\\\nce\n";
    Include_Control_Fixture fixture;
    Noc_Pragma_Once pragma_once = {0};
    size_t index;

    include_control_fixture_init(&fixture,
                                 "pragma-once.c",
                                 source,
                                 NOC_MACROS_DISABLED);
    CHECK(fixture.unit.count == 3);
    for (index = 0; index < fixture.unit.count; ++index) {
        check_status(&fixture, index, NOC_PRAGMA_ONCE_VALID);
    }
    CHECK(noc_pragma_once_build(&fixture.unit, 1, &pragma_once) ==
          NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(pragma_once.once_token_index != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_token_is_identifier(
        fixture.unit.preprocessing_tokens[pragma_once.once_token_index].token,
        "once"));
    CHECK(slice_equals(
        fixture.unit.preprocessing_tokens[pragma_once.once_token_index].token.text,
        "once"));
    CHECK(pragma_once.problem_token_index == NOC_TOKEN_INDEX_NONE);
    CHECK(pragma_once.body_tokens.begin <= pragma_once.once_token_index);
    CHECK(pragma_once.once_token_index < pragma_once.body_tokens.end);
    include_control_fixture_deinit(&fixture);
}

static void test_other_recovery_and_case_sensitivity(void)
{
    static const char source[] =
        "#pragma other tokens\n"
        "#pragma Once\n"
        "#pragma\n"
        "#pragma once trailing\n"
        "#pragma once \"unterminated\n"
        "#pragma \"unterminated\n"
        "#pragma other \"unterminated\n";
    static const Noc_Pragma_Once_Status expected[] = {
        NOC_PRAGMA_ONCE_OTHER,
        NOC_PRAGMA_ONCE_OTHER,
        NOC_PRAGMA_ONCE_MISSING,
        NOC_PRAGMA_ONCE_MALFORMED,
        NOC_PRAGMA_ONCE_INCOMPLETE,
        NOC_PRAGMA_ONCE_INCOMPLETE,
        NOC_PRAGMA_ONCE_INCOMPLETE,
    };
    Include_Control_Fixture fixture;
    Noc_Pragma_Once pragma_once = {0};
    size_t index;

    include_control_fixture_init(&fixture,
                                 "pragma-recovery.c",
                                 source,
                                 NOC_MACROS_FULL);
    CHECK(fixture.unit.count == sizeof(expected) / sizeof(expected[0]));
    for (index = 0; index < fixture.unit.count; ++index) {
        check_status(&fixture, index, expected[index]);
    }
    CHECK(noc_pragma_once_build(&fixture.unit, 3, &pragma_once) ==
          NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(pragma_once.once_token_index != NOC_TOKEN_INDEX_NONE);
    CHECK(pragma_once.problem_token_index != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_token_is_identifier(
        fixture.unit.preprocessing_tokens[pragma_once.problem_token_index].token,
        "trailing"));
    CHECK(noc_pragma_once_build(&fixture.unit, 2, &pragma_once) ==
          NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(pragma_once.once_token_index == NOC_TOKEN_INDEX_NONE);
    CHECK(pragma_once.problem_token_index == NOC_TOKEN_INDEX_NONE);
    include_control_fixture_deinit(&fixture);
}

int main(void)
{
    test_exact_trivia_and_splices();
    test_other_recovery_and_case_sensitivity();
    return finish_suite("pragma-once");
}
