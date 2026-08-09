#include "preprocessor_logical_source_test_support.h"

typedef struct {
    size_t calls;
    size_t cancel_at;
} Driver_Cancel;

static bool cancel_driver(void *user_data)
{
    Driver_Cancel *cancel = (Driver_Cancel *)user_data;
    cancel->calls += 1;
    return cancel->calls >= cancel->cancel_at;
}

static void check_preserved(const Noc_Logical_Source *source,
                            Noc_Logical_Source_Impl *implementation,
                            size_t generation,
                            const char *text)
{
    CHECK(source->impl == implementation);
    CHECK(source->generation == generation);
    CHECK(noc_logical_source_text(source).data == text);
    CHECK(noc_logical_source_is_valid(source));
}

static void test_validation_limits_cancellation_and_transactionality(void)
{
    static const char input[] =
        "int first = EXPAND;\n"
        "#define LOCAL 2\n"
        "int second = LOCAL;\n";
    Preprocessor_Logical_Source_Fixture fixture;
    Preprocessor_Logical_Source_Fixture aggregate_fixture;
    Noc_Preprocessor_Logical_Source_Options defaults =
        noc_preprocessor_logical_source_default_options();
    Noc_Preprocessor_Logical_Source_Options options;
    Noc_Preprocessor_Logical_Source_Result result;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source exhausted = {0};
    Noc_Logical_Source_Impl *implementation;
    const char *text;
    size_t generation;
    Driver_Cancel cancel = {0, 1};

    preprocessor_logical_source_fixture_init(&fixture,
                                             "#define EXPAND 123\n",
                                             input,
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_FULL);
    result = noc_preprocessor_logical_source_build(&fixture.groups,
                                                   defaults,
                                                   &source);
    CHECK(result.status == NOC_PREPROCESSOR_LOGICAL_SOURCE_OK);
    CHECK(!slice_equals(noc_logical_source_text(&source), ""));
    CHECK(preprocessor_logical_source_text_contains(&source, "first = 123"));
    CHECK(preprocessor_logical_source_text_contains(&source, "second = 2"));
    implementation = source.impl;
    generation = source.generation;
    text = noc_logical_source_text(&source).data;

#define CHECK_FAILURE_PRESERVES(expected_status)                                \
    do {                                                                        \
        result = noc_preprocessor_logical_source_build(&fixture.groups,         \
                                                       options,                 \
                                                       &source);                \
        CHECK(result.status == (expected_status));                              \
        check_preserved(&source, implementation, generation, text);             \
    } while (0)

    options = defaults;
    options.logical_source.max_fragments = 1;
    CHECK_FAILURE_PRESERVES(
        NOC_PREPROCESSOR_LOGICAL_SOURCE_LIMIT_EXCEEDED);

    preprocessor_logical_source_fixture_init(&aggregate_fixture,
                                             "",
                                             "left\n#define LOCAL 2\nright\n",
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_FULL);
    options = defaults;
    options.logical_source.max_tokens = 1;
    result = noc_preprocessor_logical_source_build(&aggregate_fixture.groups,
                                                   options,
                                                   &source);
    CHECK(result.status ==
          NOC_PREPROCESSOR_LOGICAL_SOURCE_LIMIT_EXCEEDED);
    CHECK(result.problem_tokens.begin != NOC_TOKEN_INDEX_NONE);
    check_preserved(&source, implementation, generation, text);

    options = defaults;
    options.logical_source.max_input_bytes_examined = strlen(input) - 1;
    CHECK_FAILURE_PRESERVES(
        NOC_PREPROCESSOR_LOGICAL_SOURCE_LIMIT_EXCEEDED);

    options = defaults;
    options.macro_expansion.limits.max_output_tokens = 1;
    CHECK_FAILURE_PRESERVES(
        NOC_PREPROCESSOR_LOGICAL_SOURCE_EXPANSION_FAILED);
    CHECK(result.expansion_status == NOC_MACRO_EXPANSION_OUTPUT_LIMIT);

    options = defaults;
    options.logical_source.max_source_bytes = 2;
    CHECK_FAILURE_PRESERVES(
        NOC_PREPROCESSOR_LOGICAL_SOURCE_LIMIT_EXCEEDED);
    CHECK(result.logical_source_status ==
          NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);

    options = defaults;
    options.logical_source.should_cancel = cancel_driver;
    options.logical_source.cancel_user_data = &cancel;
    CHECK_FAILURE_PRESERVES(NOC_PREPROCESSOR_LOGICAL_SOURCE_CANCELLED);
    CHECK(cancel.calls == 1);

    options = defaults;
    options.logical_source.max_fragments = 0;
    CHECK_FAILURE_PRESERVES(
        NOC_PREPROCESSOR_LOGICAL_SOURCE_INVALID_ARGUMENT);

    result = noc_preprocessor_logical_source_build(NULL, defaults, &source);
    CHECK(result.status ==
          NOC_PREPROCESSOR_LOGICAL_SOURCE_INVALID_ARGUMENT);
    check_preserved(&source, implementation, generation, text);
    result = noc_preprocessor_logical_source_build(&fixture.groups,
                                                   defaults,
                                                   NULL);
    CHECK(result.status ==
          NOC_PREPROCESSOR_LOGICAL_SOURCE_INVALID_ARGUMENT);
    check_preserved(&source, implementation, generation, text);

    exhausted.generation = SIZE_MAX;
    result = noc_preprocessor_logical_source_build(&fixture.groups,
                                                   defaults,
                                                   &exhausted);
    CHECK(result.status ==
          NOC_PREPROCESSOR_LOGICAL_SOURCE_LOGICAL_SOURCE_FAILED);
    CHECK(result.logical_source_status ==
          NOC_LOGICAL_SOURCE_GENERATION_EXHAUSTED);
    CHECK(exhausted.impl == NULL && exhausted.generation == SIZE_MAX);

#undef CHECK_FAILURE_PRESERVES
    noc_logical_source_free(&exhausted);
    noc_logical_source_free(&source);
    preprocessor_logical_source_fixture_deinit(&aggregate_fixture);
    preprocessor_logical_source_fixture_deinit(&fixture);
}

typedef struct {
    Noc_Preprocessor_Conditional_Groups *groups;
    size_t calls;
    size_t mutate_at;
} Stale_During_Scan;

static bool stale_groups_during_scan(void *user_data)
{
    Stale_During_Scan *state = (Stale_During_Scan *)user_data;
    state->calls += 1;
    if (state->calls == state->mutate_at) {
        noc_preprocessor_conditional_groups_free(state->groups);
    }
    return false;
}

static void test_stale_inputs_and_dependency_mutation(void)
{
    Preprocessor_Logical_Source_Fixture fixture;
    Noc_Preprocessor_Conditional_Groups stale;
    Noc_Preprocessor_Logical_Source_Options options =
        noc_preprocessor_logical_source_default_options();
    Noc_Preprocessor_Logical_Source_Result result;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source_Impl *implementation;
    const char *text;
    size_t generation;
    Stale_During_Scan mutation;

    preprocessor_logical_source_fixture_init(&fixture,
                                             "",
                                             "#define VALUE 1\nint stable = VALUE;\n",
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_FULL);
    result = noc_preprocessor_logical_source_build(&fixture.groups,
                                                   options,
                                                   &source);
    CHECK(result.status == NOC_PREPROCESSOR_LOGICAL_SOURCE_OK);
    implementation = source.impl;
    generation = source.generation;
    text = noc_logical_source_text(&source).data;

    stale = fixture.groups;
    stale.generation += 1;
    result = noc_preprocessor_logical_source_build(&stale, options, &source);
    CHECK(result.status == NOC_PREPROCESSOR_LOGICAL_SOURCE_STALE);
    check_preserved(&source, implementation, generation, text);

    mutation.groups = &fixture.groups;
    mutation.calls = 0;
    mutation.mutate_at = fixture.input.preprocessing_token_count + 2;
    options.logical_source.should_cancel = stale_groups_during_scan;
    options.logical_source.cancel_user_data = &mutation;
    result = noc_preprocessor_logical_source_build(&fixture.groups,
                                                   options,
                                                   &source);
    CHECK(result.status == NOC_PREPROCESSOR_LOGICAL_SOURCE_STALE);
    CHECK(mutation.calls == mutation.mutate_at);
    check_preserved(&source, implementation, generation, text);

    noc_logical_source_free(&source);
    preprocessor_logical_source_fixture_deinit(&fixture);
}

int main(void)
{
    test_validation_limits_cancellation_and_transactionality();
    test_stale_inputs_and_dependency_mutation();
    return finish_suite("preprocessor logical source lifecycle");
}
