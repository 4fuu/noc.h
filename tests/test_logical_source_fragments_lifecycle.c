#include "macro_expansion_test_support.h"

typedef struct {
    size_t calls;
    size_t cancel_at;
} Fragment_Cancel;

static bool cancel_fragment_build(void *user_data)
{
    Fragment_Cancel *cancel = (Fragment_Cancel *)user_data;
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

static void test_combined_limits_stale_inputs_and_transactionality(void)
{
    Macro_Expansion_Fixture first;
    Macro_Expansion_Fixture second;
    const Noc_Macro_Expansion *fragments[2];
    const Noc_Macro_Expansion *invalid_fragments[2];
    Noc_Logical_Source source = {0};
    Noc_Logical_Source exhausted = {0};
    Noc_Logical_Source_Options defaults = noc_logical_source_default_options();
    Noc_Logical_Source_Options options;
    Noc_Logical_Source_Impl *implementation;
    const char *text;
    size_t generation;
    size_t raw_token_count;
    Fragment_Cancel cancel = {0, 1};

    macro_fixture_init(&first,
                       "#define INNER(x) x+x\n#define OUTER(x) INNER(x)\n",
                       "OUTER(a)");
    macro_fixture_init(&second,
                       "#define INNER(x) x+x\n#define OUTER(x) INNER(x)\n",
                       "OUTER(b)");
    CHECK(macro_fixture_expand(&first, macro_fixture_full_input(&first)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_expand(&second, macro_fixture_full_input(&second)) ==
          NOC_MACRO_EXPANSION_OK);
    fragments[0] = &first.expansion;
    fragments[1] = &second.expansion;
    CHECK(noc_logical_source_build_macro_expansion(&first.expansion,
                                                    defaults,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_OK);
    implementation = source.impl;
    generation = source.generation;
    text = noc_logical_source_text(&source).data;

#define CHECK_FAILURE_PRESERVES(expected_status)                                \
    do {                                                                        \
        CHECK(noc_logical_source_build_macro_expansions(fragments,              \
                                                         2,                      \
                                                         options,                \
                                                         &source) ==             \
              (expected_status));                                               \
        check_preserved(&source, implementation, generation, text);             \
    } while (0)

    options = defaults;
    options.max_macro_frames = first.expansion.frame_count +
                               second.expansion.frame_count - 1;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);

    options = defaults;
    options.max_fragments = 1;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);

    options = defaults;
    options.max_fragments = 0;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);

    raw_token_count = first.expansion.count + second.expansion.count;
    options = defaults;
    options.max_tokens = raw_token_count;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED);

    options = defaults;
    second.expansion.environment_generation += 1;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_STALE);
    second.expansion.environment_generation -= 1;

    invalid_fragments[0] = &first.expansion;
    invalid_fragments[1] = NULL;
    CHECK(noc_logical_source_build_macro_expansions(invalid_fragments,
                                                    2,
                                                    defaults,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    check_preserved(&source, implementation, generation, text);
    CHECK(noc_logical_source_build_macro_expansions(NULL,
                                                    1,
                                                    defaults,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    check_preserved(&source, implementation, generation, text);
    CHECK(noc_logical_source_build_macro_expansions(fragments,
                                                    2,
                                                    defaults,
                                                    NULL) ==
          NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    check_preserved(&source, implementation, generation, text);

    options = defaults;
    options.should_cancel = cancel_fragment_build;
    options.cancel_user_data = &cancel;
    CHECK_FAILURE_PRESERVES(NOC_LOGICAL_SOURCE_CANCELLED);
    CHECK(cancel.calls == 1);

    exhausted.generation = SIZE_MAX;
    CHECK(noc_logical_source_build_macro_expansions(fragments,
                                                    2,
                                                    defaults,
                                                    &exhausted) ==
          NOC_LOGICAL_SOURCE_GENERATION_EXHAUSTED);
    CHECK(exhausted.impl == NULL && exhausted.generation == SIZE_MAX);

    CHECK(noc_logical_source_build_macro_expansions(fragments,
                                                    2,
                                                    defaults,
                                                    &source) ==
          NOC_LOGICAL_SOURCE_OK);
    CHECK(source.impl != implementation);
    CHECK(source.generation == generation + 1);
    CHECK(slice_equals(noc_logical_source_text(&source), "a + a b + b"));
    CHECK(noc_logical_source_macro_frame_count(&source) == 4);
    CHECK(noc_logical_source_file_count(&source) == 4);

#undef CHECK_FAILURE_PRESERVES
    noc_logical_source_free(&exhausted);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&second);
    macro_fixture_deinit(&first);
}

int main(void)
{
    test_combined_limits_stale_inputs_and_transactionality();
    return finish_suite("logical-source fragment lifecycle");
}
