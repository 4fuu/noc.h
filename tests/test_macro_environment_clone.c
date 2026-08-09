#include "macro_expansion_test_support.h"

static void test_prefix_history_and_independent_mutation(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Environment prefix = {0};
    const Noc_Macro_Environment_Entry *entry;

    macro_fixture_init(&fixture,
                       "#define A 1\n"
                       "#define B 2\n"
                       "#undef A\n"
                       "#define C 3\n",
                       "A B C\n");
    CHECK(fixture.environment.count == 4);
    CHECK(noc_macro_environment_clone_prefix(&fixture.environment,
                                             2,
                                             &prefix) ==
          NOC_MACRO_ENVIRONMENT_OK);
    CHECK(noc_macro_environment_is_valid(&prefix));
    CHECK(prefix.count == 2);
    CHECK(prefix.capacity == 2);
    CHECK(prefix.generation == 1);
    CHECK(prefix.items != fixture.environment.items);
    CHECK(prefix.items[0].unit == &fixture.definitions);
    CHECK(prefix.items[0].previous_entry_index == NOC_TOKEN_INDEX_NONE);
    CHECK(prefix.items[1].previous_entry_index == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_macro_environment_lookup(&prefix,
                                       noc_slice_from_cstr("A")) ==
          &prefix.items[0]);
    CHECK(noc_macro_environment_lookup(&prefix,
                                       noc_slice_from_cstr("B")) ==
          &prefix.items[1]);
    CHECK(noc_macro_environment_lookup(&prefix,
                                       noc_slice_from_cstr("C")) == NULL);

    CHECK(noc_macro_environment_apply(&prefix,
                                      &fixture.definitions,
                                      2) == NOC_MACRO_ENVIRONMENT_OK);
    CHECK(prefix.count == 3 && prefix.generation == 2);
    CHECK(noc_macro_environment_lookup(&prefix,
                                       noc_slice_from_cstr("A")) == NULL);
    entry = noc_macro_environment_lookup(&fixture.environment,
                                         noc_slice_from_cstr("A"));
    CHECK(entry == NULL);
    CHECK(fixture.environment.count == 4);
    CHECK(fixture.environment.generation == 4);

    noc_macro_environment_free(&prefix);
    macro_fixture_deinit(&fixture);
}

static void test_zero_prefix_replacement_and_in_place_truncation(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Environment output = {0};
    Noc_Macro_Environment_Entry *old_items;

    macro_fixture_init(&fixture,
                       "#define A 1\n"
                       "#define B 2\n"
                       "#undef A\n",
                       "A B\n");
    CHECK(noc_macro_environment_apply(&output,
                                      &fixture.definitions,
                                      0) == NOC_MACRO_ENVIRONMENT_OK);
    old_items = output.items;
    CHECK(noc_macro_environment_clone_prefix(&fixture.environment,
                                             0,
                                             &output) ==
          NOC_MACRO_ENVIRONMENT_OK);
    CHECK(output.items == NULL);
    CHECK(output.count == 0 && output.capacity == 0);
    CHECK(output.generation == 2);
    CHECK(old_items != output.items);

    CHECK(noc_macro_environment_clone_prefix(&fixture.environment,
                                             fixture.environment.count,
                                             &fixture.environment) ==
          NOC_MACRO_ENVIRONMENT_OK);
    CHECK(fixture.environment.count == 3);
    CHECK(fixture.environment.generation == 4);
    CHECK(noc_macro_environment_lookup(&fixture.environment,
                                       noc_slice_from_cstr("A")) == NULL);
    CHECK(noc_macro_environment_clone_prefix(&fixture.environment,
                                             2,
                                             &fixture.environment) ==
          NOC_MACRO_ENVIRONMENT_OK);
    CHECK(fixture.environment.count == 2);
    CHECK(fixture.environment.generation == 5);
    CHECK(noc_macro_environment_lookup(&fixture.environment,
                                       noc_slice_from_cstr("A")) ==
          &fixture.environment.items[0]);
    CHECK(noc_macro_environment_lookup(&fixture.environment,
                                       noc_slice_from_cstr("B")) ==
          &fixture.environment.items[1]);

    noc_macro_environment_free(&output);
    macro_fixture_deinit(&fixture);
}

static void test_failure_preserves_output_and_stale_dependencies(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Environment output = {0};
    Noc_Macro_Environment preserved;
    Noc_Macro_Environment stale_source;
    size_t stream_generation;

    macro_fixture_init(&fixture,
                       "#define A 1\n#define B 2\n",
                       "A B\n");
    CHECK(noc_macro_environment_apply(&output,
                                      &fixture.definitions,
                                      1) == NOC_MACRO_ENVIRONMENT_OK);
    preserved = output;
    CHECK(noc_macro_environment_clone_prefix(NULL, 0, &output) ==
          NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT);
    CHECK(noc_macro_environment_clone_prefix(&fixture.environment,
                                             0,
                                             NULL) ==
          NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT);
    CHECK(noc_macro_environment_clone_prefix(&fixture.environment,
                                             fixture.environment.count + 1,
                                             &output) ==
          NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT);
    CHECK(memcmp(&output, &preserved, sizeof(output)) == 0);

    output.generation = SIZE_MAX;
    preserved = output;
    CHECK(noc_macro_environment_clone_prefix(&fixture.environment,
                                             1,
                                             &output) ==
          NOC_MACRO_ENVIRONMENT_GENERATION_EXHAUSTED);
    CHECK(memcmp(&output, &preserved, sizeof(output)) == 0);
    output.generation = 1;

    stale_source = fixture.environment;
    stream_generation = fixture.definitions.stream.generation;
    fixture.definitions.stream.generation += 1;
    preserved = output;
    CHECK(noc_macro_environment_clone_prefix(&stale_source,
                                             stale_source.count,
                                             &output) ==
          NOC_MACRO_ENVIRONMENT_STALE);
    CHECK(memcmp(&output, &preserved, sizeof(output)) == 0);
    fixture.definitions.stream.generation = stream_generation;

    output.capacity = 0;
    preserved = output;
    CHECK(noc_macro_environment_clone_prefix(&fixture.environment,
                                             1,
                                             &output) ==
          NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT);
    CHECK(memcmp(&output, &preserved, sizeof(output)) == 0);
    output.capacity = 16;

    noc_macro_environment_free(&output);
    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_prefix_history_and_independent_mutation();
    test_zero_prefix_replacement_and_in_place_truncation();
    test_failure_preserves_output_and_stale_dependencies();
    return finish_suite("macro-environment-clone");
}
