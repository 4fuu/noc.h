#include "macro_expansion_test_support.h"

static Noc_Macro_Expansion_Status expand_with_options(
    Macro_Expansion_Fixture *fixture,
    size_t entry_limit,
    Noc_Token_Range range,
    Noc_Macro_Expansion_Options options)
{
    return noc_macro_expansion_build_with_options(&fixture->environment,
                                                  entry_limit,
                                                  &fixture->input,
                                                  range,
                                                  options,
                                                  &fixture->expansion);
}

static Noc_Macro_Expansion_Options hosted_options(char *date, char *time)
{
    Noc_Macro_Expansion_Options options = noc_macro_expansion_default_options();
    options.execution_environment = NOC_EXECUTION_ENVIRONMENT_HOSTED;
    options.translation_date = noc_slice_from_cstr(date);
    options.translation_time = noc_slice_from_cstr(time);
    return options;
}

static void test_unconfigured_defaults_and_legacy_wrapper(void)
{
    static const char input[] =
        "__FILE__ __STDC_HOSTED__ __DATE__ __TIME__\n";
    static const char expected[] =
        "\"macro-input.c\" __STDC_HOSTED__ __DATE__ __TIME__\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Options options = noc_macro_expansion_default_options();

    macro_fixture_init(&fixture, "", input);
    CHECK(options.limits.max_depth ==
          noc_macro_expansion_default_limits().max_depth);
    CHECK(options.limits.max_output_tokens ==
          noc_macro_expansion_default_limits().max_output_tokens);
    CHECK(options.limits.max_expansions ==
          noc_macro_expansion_default_limits().max_expansions);
    CHECK(options.execution_environment ==
          NOC_EXECUTION_ENVIRONMENT_UNSPECIFIED);
    CHECK(options.translation_date.data == NULL);
    CHECK(options.translation_date.count == 0);
    CHECK(options.translation_time.data == NULL);
    CHECK(options.translation_time.count == 0);

    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_FILE));
    CHECK(noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_STDC_VERSION));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_STDC_HOSTED));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_DATE));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_TIME));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_NONE));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, (Noc_Macro_Builtin_Kind)99));
    CHECK(!noc_macro_expansion_builtin_is_available(
        NULL, NOC_MACRO_BUILTIN_FILE));

    macro_fixture_deinit(&fixture);
}

static void test_independent_translation_inputs(void)
{
    static const char input[] =
        "__STDC_HOSTED__ __DATE__ __TIME__\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Options options = noc_macro_expansion_default_options();

    macro_fixture_init(&fixture, "", input);

    options.execution_environment = NOC_EXECUTION_ENVIRONMENT_FREESTANDING;
    CHECK(expand_with_options(&fixture,
                              0,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(
        &fixture, "0 __DATE__ __TIME__\n"));
    CHECK(noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_STDC_HOSTED));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_DATE));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_TIME));

    options = noc_macro_expansion_default_options();
    options.translation_date = noc_slice_from_cstr("Mar 10 2025");
    CHECK(expand_with_options(&fixture,
                              0,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(
        &fixture, "__STDC_HOSTED__ \"Mar 10 2025\" __TIME__\n"));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_STDC_HOSTED));
    CHECK(noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_DATE));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_TIME));

    options = noc_macro_expansion_default_options();
    options.translation_time = noc_slice_from_cstr("01:02:03");
    CHECK(expand_with_options(&fixture,
                              0,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(
        &fixture, "__STDC_HOSTED__ __DATE__ \"01:02:03\"\n"));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_STDC_HOSTED));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_DATE));
    CHECK(noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_TIME));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, (Noc_Macro_Builtin_Kind)-1));

    macro_fixture_deinit(&fixture);
}

static void test_configured_expansion_rescan_provenance_and_ownership(void)
{
    static const char definitions[] =
        "#define ID(x) x\n"
        "#define CAT(a,b) a##b\n"
        "#define STR(x) #x\n";
    static const char input[] =
        "__STDC_HOSTED__ __DATE__ __TIME__\n"
        "ID(__DATE__) CAT(__STDC_,HOSTED__)\n"
        "STR(__TIME__)\n";
    static const char expected[] =
        "1 \"Aug  8 2026\" \"23:59:60\"\n"
        "\"Aug  8 2026\" 1\n"
        "\"__TIME__\"\n";
    char date[] = "Aug  8 2026";
    char time[] = "23:59:60";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Options options = hosted_options(date, time);
    size_t hosted_count = 0;
    size_t date_count = 0;
    size_t time_count = 0;
    size_t pasted_hosted_count = 0;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(expand_with_options(&fixture,
                              fixture.environment.count,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_STDC_HOSTED));
    CHECK(noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_DATE));
    CHECK(noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_TIME));

    for (index = 0; index < fixture.expansion.count; ++index) {
        const Noc_Macro_Expansion_Token *token =
            noc_macro_expansion_token_at(&fixture.expansion, index);
        CHECK(token != NULL);
        if (!token || token->origin != NOC_MACRO_EXPANSION_TOKEN_BUILTIN) {
            continue;
        }
        CHECK(token->generated_spelling_index <
              fixture.expansion.generated_spelling_count);
        if (token->builtin_kind == NOC_MACRO_BUILTIN_STDC_HOSTED) {
            Noc_Token source = token->unit->preprocessing_tokens[
                token->preprocessing_token_index].token;
            hosted_count += 1;
            CHECK(token->token.kind == NOC_TOKEN_NUMBER);
            if (noc_token_is_punct(source, "##")) pasted_hosted_count += 1;
        } else if (token->builtin_kind == NOC_MACRO_BUILTIN_DATE) {
            date_count += 1;
            CHECK(token->token.kind == NOC_TOKEN_STRING);
        } else if (token->builtin_kind == NOC_MACRO_BUILTIN_TIME) {
            time_count += 1;
            CHECK(token->token.kind == NOC_TOKEN_STRING);
        }
    }
    CHECK(hosted_count == 2);
    CHECK(date_count == 2);
    CHECK(time_count == 1);
    CHECK(pasted_hosted_count == 1);

    /* The successful build copied both translation spellings. */
    memset(date, 'X', sizeof(date) - 1);
    memset(time, 'Y', sizeof(time) - 1);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));

    /* Availability is part of the validated result contract. */
    fixture.expansion.available_builtin_mask &=
        ~(UINT32_C(1) << (unsigned)NOC_MACRO_BUILTIN_DATE);
    CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(!noc_macro_expansion_builtin_is_available(
        &fixture.expansion, NOC_MACRO_BUILTIN_FILE));
    fixture.expansion.available_builtin_mask |=
        UINT32_C(1) << (unsigned)NOC_MACRO_BUILTIN_DATE;
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    fixture.expansion.available_builtin_mask |= UINT32_C(1) << 31;
    CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
    fixture.expansion.available_builtin_mask &= ~(UINT32_C(1) << 31);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    fixture.expansion.available_builtin_mask |= UINT32_C(1);
    CHECK(!noc_macro_expansion_is_valid(&fixture.expansion));
    fixture.expansion.available_builtin_mask &= ~UINT32_C(1);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));

    macro_fixture_deinit(&fixture);
}

static void test_freestanding_and_explicit_definition_precedence(void)
{
    static const char definitions[] =
        "#define __DATE__ \"override-date\"\n"
        "#undef __DATE__\n"
        "#define __STDC_HOSTED__ 9\n"
        "#undef __STDC_HOSTED__\n";
    static const char input[] = "__DATE__ __STDC_HOSTED__\n";
    char date[] = "Jan  1 2000";
    char time[] = "00:00:00";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Options options = hosted_options(date, time);

    macro_fixture_init(&fixture, definitions, input);
    CHECK(fixture.environment.count == 4);
    CHECK(expand_with_options(&fixture,
                              1,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\"override-date\" 1\n"));
    CHECK(expand_with_options(&fixture,
                              2,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\"Jan  1 2000\" 1\n"));
    CHECK(expand_with_options(&fixture,
                              3,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\"Jan  1 2000\" 9\n"));
    CHECK(expand_with_options(&fixture,
                              4,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\"Jan  1 2000\" 1\n"));

    options.execution_environment = NOC_EXECUTION_ENVIRONMENT_FREESTANDING;
    CHECK(expand_with_options(&fixture,
                              4,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\"Jan  1 2000\" 0\n"));

    macro_fixture_deinit(&fixture);
}

static void expect_invalid_options_preserve(
    Macro_Expansion_Fixture *fixture,
    Noc_Macro_Expansion_Options options,
    Noc_Macro_Expansion_Token *items,
    Noc_Slice *spellings,
    size_t generation)
{
    CHECK(expand_with_options(fixture,
                              fixture->environment.count,
                              macro_fixture_full_input(fixture),
                              options) == NOC_MACRO_EXPANSION_INVALID_ARGUMENT);
    CHECK(fixture->expansion.items == items);
    CHECK(fixture->expansion.generated_spellings == spellings);
    CHECK(fixture->expansion.generation == generation);
    CHECK(noc_macro_expansion_is_valid(&fixture->expansion));
    CHECK(macro_fixture_render_equals(fixture, "1\n"));
}

static void test_strict_option_validation_and_transactionality(void)
{
    static const char *const valid_dates[] = {
        "Jan  1 2000", "Jan  9 2000", "Jan 10 2000", "Dec 31 9999",
        /* Validation is intentionally lexical, not Gregorian-calendar based. */
        "Feb 30 2024",
    };
    static const char *const valid_times[] = {
        "00:00:00", "23:59:59", "23:59:60",
    };
    static const char *const invalid_dates[] = {
        "Xxx  1 2000", "Jan  0 2000", "Jan 08 2000", "Jan 32 2000",
        "Jan  1 20x0", "Jan  1 200", "Jan- 1 2000",
    };
    static const char *const invalid_times[] = {
        "24:00:00", "23:60:00", "23:59:61", "1:00:00",
        "12-00-00", "12:00:x0",
    };
    Macro_Expansion_Fixture fixture;
    char date[] = "Dec 31 9999";
    char time[] = "23:59:60";
    Noc_Macro_Expansion_Options options = hosted_options(date, time);
    Noc_Macro_Expansion_Token *items;
    Noc_Slice *spellings;
    size_t generation;
    size_t index;

    macro_fixture_init(&fixture, "", "__STDC_HOSTED__\n");
    CHECK(expand_with_options(&fixture,
                              0,
                              macro_fixture_full_input(&fixture),
                              options) == NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "1\n"));
    items = fixture.expansion.items;
    spellings = fixture.expansion.generated_spellings;
    generation = fixture.expansion.generation;

    memset(&options, 0, sizeof(options));
    expect_invalid_options_preserve(&fixture,
                                    options,
                                    items,
                                    spellings,
                                    generation);
    options = hosted_options(date, time);
    options.execution_environment = (Noc_Execution_Environment)99;
    expect_invalid_options_preserve(&fixture,
                                    options,
                                    items,
                                    spellings,
                                    generation);
    options = hosted_options(date, time);
    options.translation_date = (Noc_Slice){NULL, 11};
    expect_invalid_options_preserve(&fixture,
                                    options,
                                    items,
                                    spellings,
                                    generation);
    options = hosted_options(date, time);
    options.translation_time = (Noc_Slice){NULL, 8};
    expect_invalid_options_preserve(&fixture,
                                    options,
                                    items,
                                    spellings,
                                    generation);
    for (index = 0;
         index < sizeof(invalid_dates) / sizeof(invalid_dates[0]);
         ++index) {
        options = hosted_options(date, time);
        options.translation_date = noc_slice_from_cstr(invalid_dates[index]);
        expect_invalid_options_preserve(&fixture,
                                        options,
                                        items,
                                        spellings,
                                        generation);
    }
    for (index = 0;
         index < sizeof(invalid_times) / sizeof(invalid_times[0]);
         ++index) {
        options = hosted_options(date, time);
        options.translation_time = noc_slice_from_cstr(invalid_times[index]);
        expect_invalid_options_preserve(&fixture,
                                        options,
                                        items,
                                        spellings,
                                        generation);
    }
    for (index = 0;
         index < sizeof(valid_dates) / sizeof(valid_dates[0]);
         ++index) {
        options = noc_macro_expansion_default_options();
        options.execution_environment = NOC_EXECUTION_ENVIRONMENT_HOSTED;
        options.translation_date = noc_slice_from_cstr(valid_dates[index]);
        CHECK(expand_with_options(&fixture,
                                  0,
                                  macro_fixture_full_input(&fixture),
                                  options) == NOC_MACRO_EXPANSION_OK);
        CHECK(macro_fixture_render_equals(&fixture, "1\n"));
    }
    for (index = 0;
         index < sizeof(valid_times) / sizeof(valid_times[0]);
         ++index) {
        options = noc_macro_expansion_default_options();
        options.execution_environment = NOC_EXECUTION_ENVIRONMENT_HOSTED;
        options.translation_time = noc_slice_from_cstr(valid_times[index]);
        CHECK(expand_with_options(&fixture,
                                  0,
                                  macro_fixture_full_input(&fixture),
                                  options) == NOC_MACRO_EXPANSION_OK);
        CHECK(macro_fixture_render_equals(&fixture, "1\n"));
    }

    macro_fixture_deinit(&fixture);
}

static void test_conditional_source_definition_and_undef_precedence(void)
{
    static const char source[] =
        "#define __STDC_HOSTED__ 0\n"
        "#if __STDC_HOSTED__ == 0\n"
        "explicit_hosted\n"
        "#endif\n"
        "#undef __STDC_HOSTED__\n"
        "#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 1\n"
        "restored_hosted\n"
        "#endif\n"
        "#ifdef __DATE__\n"
        "configured_date\n"
        "#endif\n"
        "#undef __DATE__\n"
        "#ifndef __DATE__\n"
        "missing_date\n"
        "#endif\n"
        "#define __TIME__ 7\n"
        "#ifdef __TIME__\n"
        "explicit_time\n"
        "#endif\n"
        "#undef __TIME__\n"
        "#ifdef __TIME__\n"
        "restored_time\n"
        "#endif\n";
    char date[] = "Apr  5 2030";
    char time[] = "06:07:08";
    Macro_Expansion_Fixture fixture;
    Noc_Preprocessor_Conditional_Groups groups = {0};
    Noc_Macro_Expansion_Options options = hosted_options(date, time);

    macro_fixture_init(&fixture, "", source);
    CHECK(noc_preprocessor_conditional_groups_build_with_options(
              &fixture.environment,
              0,
              &fixture.input,
              options,
              &groups) == NOC_CONDITIONAL_GROUPS_OK);
    CHECK(noc_preprocessor_conditional_groups_is_fully_resolved(&groups));
    CHECK(groups.group_count == 6);
    CHECK(groups.environment.count == 5);
    CHECK(groups.branches[0].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(groups.branches[1].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(groups.branches[2].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(groups.branches[3].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(groups.branches[4].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(groups.branches[5].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);

    /* Without configured fallback, the same active #undef events leave each
       reserved name unavailable; explicit source definitions still win. */
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture.environment,
              0,
              &fixture.input,
              noc_macro_expansion_default_limits(),
              &groups) == NOC_CONDITIONAL_GROUPS_OK);
    CHECK(noc_preprocessor_conditional_groups_is_fully_resolved(&groups));
    CHECK(groups.branches[0].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(groups.branches[1].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(groups.branches[2].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(groups.branches[3].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(groups.branches[4].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(groups.branches[5].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);

    noc_preprocessor_conditional_groups_free(&groups);
    macro_fixture_deinit(&fixture);
}

static void test_condition_and_conditional_group_consistency(void)
{
    static const char source[] =
        "#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 1 && "
        "defined(__DATE__) && defined(__TIME__)\n"
        "configured_expression\n"
        "#endif\n"
        "#ifdef __STDC_HOSTED__\n"
        "configured_ifdef\n"
        "#endif\n"
        "#ifndef __DATE__\n"
        "missing_date\n"
        "#endif\n";
    char date[] = "Feb 29 2024";
    char time[] = "12:34:56";
    Macro_Expansion_Fixture fixture;
    Noc_Preprocessor_Conditional_Groups groups = {0};
    Noc_Macro_Expansion_Options options = hosted_options(date, time);
    Noc_Token_Range condition;
    bool value = false;
    size_t problem = 123;
    Noc_Preprocessor_Conditional_Group *preserved_groups;
    size_t preserved_generation;

    macro_fixture_init(&fixture, "", source);
    condition = noc_preprocessor_directive_body_tokens(&fixture.input, 0);
    CHECK(noc_macro_expansion_build_condition_with_options(
              &fixture.environment,
              0,
              &fixture.input,
              condition,
              options,
              &fixture.expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(noc_preprocessor_expression_evaluate(&fixture.expansion,
                                               &value,
                                               &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);
    CHECK(problem == NOC_TOKEN_INDEX_NONE);

    CHECK(noc_preprocessor_conditional_groups_build_with_options(
              &fixture.environment,
              0,
              &fixture.input,
              options,
              &groups) == NOC_CONDITIONAL_GROUPS_OK);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&groups));
    CHECK(groups.group_count == 3);
    CHECK(groups.branch_count == 3);
    CHECK(groups.branches[0].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(groups.branches[1].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(groups.branches[2].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(noc_preprocessor_conditional_groups_is_fully_resolved(&groups));

    /* Legacy/default analysis sees the unconfigured fallbacks as absent. */
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture.environment,
              0,
              &fixture.input,
              noc_macro_expansion_default_limits(),
              &groups) == NOC_CONDITIONAL_GROUPS_OK);
    CHECK(groups.branches[0].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(groups.branches[1].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(groups.branches[2].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);

    preserved_groups = groups.groups;
    preserved_generation = groups.generation;
    options.translation_date = noc_slice_from_cstr("Feb 32 2024");
    CHECK(noc_preprocessor_conditional_groups_build_with_options(
              &fixture.environment,
              0,
              &fixture.input,
              options,
              &groups) == NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT);
    CHECK(groups.groups == preserved_groups);
    CHECK(groups.generation == preserved_generation);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&groups));

    noc_preprocessor_conditional_groups_free(&groups);
    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_unconfigured_defaults_and_legacy_wrapper();
    test_independent_translation_inputs();
    test_configured_expansion_rescan_provenance_and_ownership();
    test_freestanding_and_explicit_definition_precedence();
    test_strict_option_validation_and_transactionality();
    test_conditional_source_definition_and_undef_precedence();
    test_condition_and_conditional_group_consistency();
    return finish_suite("configured-builtins");
}
