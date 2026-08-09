#include "macro_expansion_test_support.h"

typedef struct {
    size_t calls;
    size_t cancel_at;
} Cancel_State;

static bool cancel_logical_source(void *user_data)
{
    Cancel_State *state = (Cancel_State *)user_data;
    state->calls += 1;
    return state->calls >= state->cancel_at;
}

static void check_preserved(const Noc_Logical_Source *source,
                            Noc_Logical_Source_Impl *implementation,
                            size_t generation,
                            const char *text)
{
    CHECK(source->impl == implementation);
    CHECK(source->generation == generation);
    CHECK(noc_logical_source_is_valid(source));
    CHECK(noc_logical_source_text(source).data == text);
}

static void check_names_defaults_and_empty_expansion(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source_Options options = noc_logical_source_default_options();
    Noc_Logical_Token_Macro_Provenance preserved;
    Noc_Slice text;

    CHECK(strcmp(noc_logical_source_status_name(NOC_LOGICAL_SOURCE_OK),
                 "ok") == 0);
    CHECK(strcmp(noc_logical_source_status_name(
                     NOC_LOGICAL_SOURCE_INVALID_ARGUMENT),
                 "invalid-argument") == 0);
    CHECK(strcmp(noc_logical_source_status_name(NOC_LOGICAL_SOURCE_STALE),
                 "stale") == 0);
    CHECK(strcmp(noc_logical_source_status_name(NOC_LOGICAL_SOURCE_CANCELLED),
                 "cancelled") == 0);
    CHECK(strcmp(noc_logical_source_status_name(
                     NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED),
                 "limit-exceeded") == 0);
    CHECK(strcmp(noc_logical_source_status_name(
                     NOC_LOGICAL_SOURCE_GENERATION_EXHAUSTED),
                 "generation-exhausted") == 0);
    CHECK(strcmp(noc_logical_source_status_name(
                     NOC_LOGICAL_SOURCE_OUT_OF_MEMORY),
                 "out-of-memory") == 0);
    CHECK(strcmp(noc_logical_source_status_name(
                     (Noc_Logical_Source_Status)99),
                 "unknown") == 0);
    CHECK(options.max_source_bytes > 0);
    CHECK(options.max_source_bytes < SIZE_MAX);
    CHECK(options.max_input_bytes_examined > options.max_source_bytes);
    CHECK(options.max_tokens >=
          2 * noc_macro_expansion_default_limits().max_output_tokens - 1);
    CHECK(options.max_macro_frames >=
          noc_macro_expansion_default_limits().max_expansions);
    CHECK(options.max_source_files > 1);
    CHECK(options.max_path_bytes > 1);
    CHECK(options.max_fragments > 1);
    CHECK(!noc_logical_source_is_valid(NULL));
    CHECK(noc_logical_source_generation(NULL) == 0);
    CHECK(noc_logical_source_token_count(NULL) == 0);
    CHECK(noc_logical_source_file_count(NULL) == 0);
    CHECK(noc_logical_source_macro_frame_count(NULL) == 0);
    CHECK(noc_logical_source_token_at(NULL, 0) == NULL);
    CHECK(noc_logical_source_file_at(NULL, 0) == NULL);
    CHECK(noc_logical_source_macro_frame_at(NULL, 0) == NULL);
    text = noc_logical_source_text(NULL);
    CHECK(text.data == NULL && text.count == 0);
    text = noc_logical_source_token_text(NULL, 0);
    CHECK(text.data == NULL && text.count == 0);
    memset(&preserved, 0x5a, sizeof(preserved));
    CHECK(!noc_logical_source_token_macro_provenance(NULL, 0, &preserved));
    CHECK(((const unsigned char *)&preserved)[0] == 0x5a);

    macro_fixture_init(&fixture, "", "value");
    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    fixture.environment.count,
                                    &fixture.input,
                                    (Noc_Token_Range){0, 0},
                                    noc_macro_expansion_default_limits(),
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(fixture.expansion.count == 0);
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    options,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_is_valid(&source));
    CHECK(noc_logical_source_generation(&source) == 1);
    text = noc_logical_source_text(&source);
    CHECK(text.data != NULL && text.count == 0 && text.data[0] == '\0');
    CHECK(noc_logical_source_token_count(&source) == 0);
    CHECK(noc_logical_source_file_count(&source) == 0);
    CHECK(noc_logical_source_macro_frame_count(&source) == 0);
    CHECK(noc_logical_source_token_at(&source, 0) == NULL);
    CHECK(noc_logical_source_file_at(&source, 0) == NULL);
    CHECK(noc_logical_source_macro_frame_at(&source, 0) == NULL);

    noc_logical_source_free(&source);
    CHECK(!noc_logical_source_is_valid(&source));
    CHECK(source.impl == NULL && source.generation == 1);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
}

static void test_limits_cancellation_and_transactionality(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source exact = {0};
    Noc_Logical_Source exhausted = {0};
    Noc_Logical_Source_Options defaults = noc_logical_source_default_options();
    Noc_Logical_Source_Options options;
    Noc_Logical_Source_Impl *implementation;
    const char *text;
    size_t generation;
    Cancel_State cancel = {0, 1};

    macro_fixture_init(&fixture,
                       "#define INNER(x) x+x\n#define OUTER(x) INNER(x)\n",
                       "OUTER(a)");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    defaults,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_OK);
    CHECK(slice_equals(noc_logical_source_text(&source), "a + a"));
    implementation = source.impl;
    generation = source.generation;
    text = noc_logical_source_text(&source).data;

    options = defaults;
    options.max_source_bytes = strlen("a + a");
    options.max_input_bytes_examined = 3;
    options.max_tokens = 5;
    options.max_macro_frames = 2;
    options.max_source_files = 2;
    options.max_path_bytes = strlen("macro-definitions.h") + 1 +
                             strlen("macro-input.c") + 1;
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    options,
                                                    &exact) ==
          NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_is_valid(&exact));
    CHECK(slice_equals(noc_logical_source_text(&exact), "a + a"));
    CHECK(noc_logical_source_token_count(&exact) == 5);
    CHECK(noc_logical_source_macro_frame_count(&exact) == 2);
    CHECK(noc_logical_source_file_count(&exact) == 2);

#define CHECK_FAILURE_PRESERVES(expected_status)                                \
    do {                                                                        \
        CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,      \
                                                        options,                 \
                                                        &source) ==              \
              (expected_status));                                               \
        check_preserved(&source, implementation, generation, text);             \
    } while (0)

    options = defaults;
    options.max_source_bytes = 4;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);
    options = defaults;
    options.max_input_bytes_examined = 1;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);
    options = defaults;
    options.max_tokens = 2;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);
    options = defaults;
    options.max_macro_frames = 1;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);
    options = defaults;
    options.max_source_files = 1;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);
    options = defaults;
    options.max_path_bytes = 1;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);
    fixture.expansion.environment_generation += 1;
    options = defaults;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_STALE);
    fixture.expansion.environment_generation -= 1;
    options = defaults;
    options.should_cancel = cancel_logical_source;
    options.cancel_user_data = &cancel;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_CANCELLED);
    CHECK(cancel.calls == 1);

    options = defaults;
    options.max_source_bytes = 0;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    options = defaults;
    options.max_source_bytes = SIZE_MAX;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    options = defaults;
    options.max_input_bytes_examined = 0;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    options = defaults;
    options.max_tokens = 0;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    options = defaults;
    options.max_macro_frames = 0;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    options = defaults;
    options.max_source_files = 0;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    options = defaults;
    options.max_path_bytes = 0;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    CHECK(noc_logical_source_build_macro_expansion(NULL, defaults, &source) ==
          NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    check_preserved(&source, implementation, generation, text);
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    defaults,
                                                    NULL) ==
          NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    check_preserved(&source, implementation, generation, text);

    exhausted.generation = SIZE_MAX;
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    defaults,
                                                    &exhausted) ==
          NOC_LOGICAL_SOURCE_GENERATION_EXHAUSTED);
    CHECK(exhausted.impl == NULL && exhausted.generation == SIZE_MAX);

    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    defaults,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_OK);
    CHECK(source.impl != implementation);
    CHECK(source.generation == generation + 1);
    CHECK(slice_equals(noc_logical_source_text(&source), "a + a"));

#undef CHECK_FAILURE_PRESERVES
    noc_logical_source_free(&exact);
    noc_logical_source_free(&exhausted);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
}

static void test_mid_spelling_cancellation(void)
{
    enum { IDENTIFIER_COUNT = 9000 };
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source_Options options = noc_logical_source_default_options();
    Cancel_State cancel = {0, 6};
    char *input = (char *)malloc(IDENTIFIER_COUNT + 1);

    CHECK(input != NULL);
    if (!input) return;
    memset(input, 'a', IDENTIFIER_COUNT);
    input[IDENTIFIER_COUNT] = '\0';
    macro_fixture_init(&fixture, "", input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    options.should_cancel = cancel_logical_source;
    options.cancel_user_data = &cancel;
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    options,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_CANCELLED);
    CHECK(cancel.calls == 6);
    CHECK(source.impl == NULL && source.generation == 0);

    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
    free(input);
}

static void test_validation_cancellation_preserves_seeded_output(void)
{
    enum { IDENTIFIER_COUNT = 700 };
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source_Options options = noc_logical_source_default_options();
    Noc_Logical_Source_Impl *implementation;
    const char *text;
    size_t generation;
    size_t index;
    Cancel_State cancel = {0, 3};
    char *input = (char *)malloc(IDENTIFIER_COUNT * 2);

    CHECK(input != NULL);
    if (!input) return;
    for (index = 0; index < IDENTIFIER_COUNT; ++index) {
        input[index * 2] = 'a';
        if (index + 1 < IDENTIFIER_COUNT) input[index * 2 + 1] = '+';
    }
    input[IDENTIFIER_COUNT * 2 - 1] = '\0';
    macro_fixture_init(&fixture, "", input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(fixture.expansion.count > 512);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    implementation = source.impl;
    generation = source.generation;
    text = noc_logical_source_text(&source).data;
    options.should_cancel = cancel_logical_source;
    options.cancel_user_data = &cancel;
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    options,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_CANCELLED);
    CHECK(cancel.calls == 3);
    check_preserved(&source, implementation, generation, text);

    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
    free(input);
}

static void test_spliced_anchor_budget_and_cancellation(void)
{
    enum { SPLICE_COUNT = 3000 };
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source_Options options = noc_logical_source_default_options();
    Noc_Logical_Source_Impl *implementation;
    const char *text;
    size_t generation;
    size_t position = 0;
    size_t index;
    size_t definitions_count = strlen("#define CAT(a,b) a#") +
                               SPLICE_COUNT * 2 + strlen("#b\n");
    char *definitions = (char *)malloc(definitions_count + 1);
    Cancel_State cancel = {0, 4};

    CHECK(definitions != NULL);
    if (!definitions) return;
    memcpy(definitions + position,
           "#define CAT(a,b) a#",
           strlen("#define CAT(a,b) a#"));
    position += strlen("#define CAT(a,b) a#");
    for (index = 0; index < SPLICE_COUNT; ++index) {
        definitions[position++] = '\\';
        definitions[position++] = '\n';
    }
    memcpy(definitions + position, "#b\n", strlen("#b\n") + 1);
    CHECK(position + strlen("#b\n") == definitions_count);

    macro_fixture_init(&fixture, definitions, "CAT(left,right)");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(slice_equals(noc_logical_source_text(&source), "leftright"));
    implementation = source.impl;
    generation = source.generation;
    text = noc_logical_source_text(&source).data;

    options.max_input_bytes_examined = SPLICE_COUNT * 2;
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    options,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);
    check_preserved(&source, implementation, generation, text);

    options = noc_logical_source_default_options();
    options.should_cancel = cancel_logical_source;
    options.cancel_user_data = &cancel;
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    options,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_CANCELLED);
    CHECK(cancel.calls == cancel.cancel_at);
    check_preserved(&source, implementation, generation, text);

    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
    free(definitions);
}

static void test_line_map_cancellation_preserves_seeded_output(void)
{
    enum { CRLF_COUNT = 5000 };
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source_Options options = noc_logical_source_default_options();
    Noc_Logical_Source_Impl *implementation;
    const char *text;
    size_t generation;
    size_t position = 0;
    size_t index;
    char *input = (char *)malloc(3 + CRLF_COUNT * 2 + 3);
    Cancel_State polling = {0, SIZE_MAX};
    Cancel_State cancel = {0, 9};

    CHECK(input != NULL);
    if (!input) return;
    memcpy(input + position, "/*x", 3);
    position += 3;
    for (index = 0; index < CRLF_COUNT; ++index) {
        input[position++] = '\r';
        input[position++] = '\n';
    }
    memcpy(input + position, "*/", 3);
    macro_fixture_init(&fixture, "", input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    implementation = source.impl;
    generation = source.generation;
    text = noc_logical_source_text(&source).data;

    options.should_cancel = cancel_logical_source;
    options.cancel_user_data = &polling;
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    options,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_OK);
    /* Both line-map scans must continue polling even though CRLF processing
       advances over the LF byte and skips exact interval multiples. */
    CHECK(polling.calls >= 14);
    implementation = source.impl;
    generation = source.generation;
    text = noc_logical_source_text(&source).data;

    options = noc_logical_source_default_options();
    options.should_cancel = cancel_logical_source;
    options.cancel_user_data = &cancel;
    CHECK(noc_logical_source_build_macro_expansion(&fixture.expansion,
                                                    options,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_CANCELLED);
    CHECK(cancel.calls == cancel.cancel_at);
    check_preserved(&source, implementation, generation, text);

    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
    free(input);
}

int main(void)
{
    check_names_defaults_and_empty_expansion();
    test_limits_cancellation_and_transactionality();
    test_mid_spelling_cancellation();
    test_validation_cancellation_preserves_seeded_output();
    test_spliced_anchor_budget_and_cancellation();
    test_line_map_cancellation_preserves_seeded_output();
    return finish_suite("logical-source-lifecycle");
}
