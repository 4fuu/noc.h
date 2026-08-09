#include "include_control_test_support.h"

static Noc_Include_Guard build_structural_guard(
    Include_Control_Fixture *fixture)
{
    Noc_Include_Guard guard = {0};
    CHECK(noc_include_guard_build_structural(&fixture->unit, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(noc_include_guard_is_valid(&guard));
    return guard;
}

static void check_status_parity(const char *source,
                                Noc_Include_Guard_Status expected)
{
    Include_Control_Fixture fixture;
    Noc_Include_Guard structural;
    Noc_Include_Guard evaluated = {0};

    include_control_fixture_init(&fixture,
                                 "structural-status.h",
                                 source,
                                 NOC_MACROS_FULL);
    structural = build_structural_guard(&fixture);
    CHECK(noc_include_guard_build(&fixture.unit,
                                  &fixture.groups,
                                  &evaluated) ==
          NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(noc_include_guard_is_valid(&evaluated));
    CHECK(structural.status == expected);
    CHECK(structural.status == evaluated.status);
    CHECK(structural.opener_directive_index ==
          evaluated.opener_directive_index);
    CHECK(structural.define_directive_index ==
          evaluated.define_directive_index);
    CHECK(structural.closer_directive_index ==
          evaluated.closer_directive_index);
    CHECK(structural.guard_name_token_index ==
          evaluated.guard_name_token_index);
    CHECK(structural.problem_directive_index ==
          evaluated.problem_directive_index);
    CHECK(structural.problem_token_index == evaluated.problem_token_index);
    /* Incomplete semantic groups publish their partial recovery range, while
       the structural view has no balanced file-enclosing range to promise. */
    if (structural.closer_directive_index != NOC_TOKEN_INDEX_NONE) {
        CHECK(structural.preprocessing_tokens.begin ==
              evaluated.preprocessing_tokens.begin);
        CHECK(structural.preprocessing_tokens.end ==
              evaluated.preprocessing_tokens.end);
    }
    CHECK(structural.guard_name.data == evaluated.guard_name.data);
    CHECK(structural.guard_name.count == evaluated.guard_name.count);
    CHECK(structural.definition_allowed == evaluated.definition_allowed);
    include_control_fixture_deinit(&fixture);
}

static void test_canonical_metadata_without_condition_evaluation(void)
{
    static const char source[] =
        "/* lead */\n"
        "#ifndef PROJECT_GUARD\n"
        "#define PROJECT_GUARD 1\n"
        "#if UNKNOWN_EXPRESSION + 1\n"
        "int active_if_selected;\n"
        "#else\n"
        "int active_else_selected;\n"
        "#endif\n"
        "#endif /* PROJECT_GUARD */\n"
        "/* tail */\n";
    Include_Control_Fixture fixture;
    Noc_Include_Guard guard;
    size_t opener;
    size_t define;
    size_t closer;

    include_control_fixture_init(&fixture,
                                 "structural-canonical.h",
                                 source,
                                 NOC_MACROS_DISABLED);
    guard = build_structural_guard(&fixture);
    opener = include_control_fixture_directive(
        &fixture, NOC_PREPROCESSOR_DIRECTIVE_IFNDEF, 0);
    define = include_control_fixture_directive(
        &fixture, NOC_PREPROCESSOR_DIRECTIVE_DEFINE, 0);
    closer = include_control_fixture_directive(
        &fixture, NOC_PREPROCESSOR_DIRECTIVE_ENDIF, 1);

    CHECK(guard.status == NOC_INCLUDE_GUARD_CANONICAL);
    CHECK(guard.groups == NULL);
    CHECK(guard.groups_generation == 0);
    CHECK(guard.group_index == NOC_TOKEN_INDEX_NONE);
    CHECK(guard.branch_index == NOC_TOKEN_INDEX_NONE);
    CHECK(guard.opener_directive_index == opener);
    CHECK(guard.define_directive_index == define);
    CHECK(guard.closer_directive_index == closer);
    CHECK(slice_equals(guard.guard_name, "PROJECT_GUARD"));
    CHECK(!guard.definition_allowed);
    CHECK(guard.preprocessing_tokens.begin ==
          fixture.unit.items[opener].preprocessing_tokens.begin);
    CHECK(guard.preprocessing_tokens.end ==
          fixture.unit.items[closer].preprocessing_tokens.end);

    /* Rebuilding unrelated semantic analysis cannot invalidate a result that
       intentionally borrows only the physical preprocessing unit. */
    include_control_fixture_rebuild_groups(&fixture);
    CHECK(noc_include_guard_is_valid(&guard));
    include_control_fixture_deinit(&fixture);
    CHECK(!noc_include_guard_is_valid(&guard));
}

static void test_recovery_status_parity(void)
{
    check_status_parity("#ifndef GUARD\n#define GUARD\n#endif\n",
                        NOC_INCLUDE_GUARD_CANONICAL);
    check_status_parity("#ifndef GUARD\n#define OTHER\n#endif\n",
                        NOC_INCLUDE_GUARD_NAME_MISMATCH);
    check_status_parity("#ifndef GUARD\nint x;\n#endif\n",
                        NOC_INCLUDE_GUARD_MISSING_DEFINE);
    check_status_parity("#ifndef GUARD\n#define GUARD() 1\n#endif\n",
                        NOC_INCLUDE_GUARD_MALFORMED);
    check_status_parity("#ifndef GUARD\n#define\n#endif\n",
                        NOC_INCLUDE_GUARD_INCOMPLETE);
    check_status_parity("#ifndef GUARD\n#define GUARD\n#else\n#endif\n",
                        NOC_INCLUDE_GUARD_HAS_PEER_BRANCH);
    check_status_parity("#ifndef GUARD\n#define GUARD\n",
                        NOC_INCLUDE_GUARD_INCOMPLETE);
    check_status_parity("#ifndef GUARD extra\n#define GUARD\n#endif\n",
                        NOC_INCLUDE_GUARD_MALFORMED);
    check_status_parity("#ifndef GUARD\n#define GUARD\n#endif\nint x;\n",
                        NOC_INCLUDE_GUARD_NOT_FILE_ENCLOSING);
    check_status_parity("int before;\n#ifndef GUARD\n#define GUARD\n#endif\n",
                        NOC_INCLUDE_GUARD_NONE);
    check_status_parity("#if !defined(GUARD)\n#define GUARD\n#endif\n",
                        NOC_INCLUDE_GUARD_NONE);
}

static void check_structural_status(const char *source,
                                    Noc_Include_Guard_Status expected)
{
    Include_Control_Fixture fixture;
    Noc_Include_Guard guard;

    include_control_fixture_init(&fixture,
                                 "structural-only-status.h",
                                 source,
                                 NOC_MACROS_FULL);
    guard = build_structural_guard(&fixture);
    CHECK(guard.status == expected);
    include_control_fixture_deinit(&fixture);
}

static void test_nested_balancing_and_malformed_outer_structure(void)
{
    check_structural_status(
        "#ifndef GUARD\n"
        "#define GUARD\n"
        "#if 1\n"
        "#ifdef INNER\n"
        "#else\n"
        "#endif\n"
        "#elif 0\n"
        "#endif\n"
        "#endif\n",
        NOC_INCLUDE_GUARD_CANONICAL);
    check_structural_status(
        "#ifndef GUARD\n"
        "#define GUARD\n"
        "#if 1\n"
        "#endif\n",
        NOC_INCLUDE_GUARD_INCOMPLETE);
    check_structural_status(
        "#ifndef GUARD\n"
        "#define GUARD\n"
        "#if 1\n"
        "#else\n"
        "#endif\n"
        "#else\n"
        "#endif\n",
        NOC_INCLUDE_GUARD_HAS_PEER_BRANCH);
    check_structural_status(
        "#ifndef GUARD\n#define GUARD\n#else garbage\n#endif\n",
        NOC_INCLUDE_GUARD_HAS_PEER_BRANCH);
    check_structural_status(
        "#ifndef GUARD\n#define GUARD\n#else\n#else\n#endif\n",
        NOC_INCLUDE_GUARD_HAS_PEER_BRANCH);
    check_structural_status(
        "#ifndef GUARD\n#define GUARD\n#else\n#elif 1\n#endif\n",
        NOC_INCLUDE_GUARD_HAS_PEER_BRANCH);
    check_structural_status(
        "#ifndef GUARD\n#define GUARD\n#endif trailing\n",
        NOC_INCLUDE_GUARD_NOT_FILE_ENCLOSING);
    check_structural_status(
        "#ifndef GUARD\n#define GUARD\n#elifdef OTHER\n#endif\n",
        NOC_INCLUDE_GUARD_HAS_PEER_BRANCH);
}

static void test_null_group_validity_and_cross_builder_reuse(void)
{
    static const char *const sources[] = {
        "int value;\n",
        "#ifndef GUARD\n#define GUARD\n",
        "#ifndef GUARD\n#define GUARD\n#else\n#endif\n",
        "#ifndef GUARD\n#define GUARD\n#endif\nint value;\n",
    };
    static const Noc_Include_Guard_Status statuses[] = {
        NOC_INCLUDE_GUARD_NONE,
        NOC_INCLUDE_GUARD_INCOMPLETE,
        NOC_INCLUDE_GUARD_HAS_PEER_BRANCH,
        NOC_INCLUDE_GUARD_NOT_FILE_ENCLOSING,
    };
    size_t index;

    for (index = 0; index < sizeof(sources) / sizeof(sources[0]); ++index) {
        Include_Control_Fixture fixture;
        Noc_Include_Guard guard;
        Noc_Include_Guard corrupted;

        include_control_fixture_init(&fixture,
                                     "structural-validity.h",
                                     sources[index],
                                     NOC_MACROS_FULL);
        guard = build_structural_guard(&fixture);
        CHECK(guard.status == statuses[index]);
        corrupted = guard;
        corrupted.groups_generation = 1;
        CHECK(!noc_include_guard_is_valid(&corrupted));
        corrupted = guard;
        corrupted.group_index = 0;
        CHECK(!noc_include_guard_is_valid(&corrupted));
        corrupted = guard;
        corrupted.branch_index = 0;
        CHECK(!noc_include_guard_is_valid(&corrupted));
        include_control_fixture_deinit(&fixture);
    }

    {
        Include_Control_Fixture fixture;
        Noc_Include_Guard guard = {0};

        include_control_fixture_init(
            &fixture,
            "cross-builder.h",
            "#ifndef GUARD\n#define GUARD\n#endif\n",
            NOC_MACROS_FULL);
        CHECK(noc_include_guard_build(&fixture.unit,
                                      &fixture.groups,
                                      &guard) == NOC_INCLUDE_CONTROL_BUILD_OK);
        CHECK(guard.generation == 1 && guard.groups == &fixture.groups);
        CHECK(guard.group_index != NOC_TOKEN_INDEX_NONE);
        CHECK(noc_include_guard_build_structural(&fixture.unit, &guard) ==
              NOC_INCLUDE_CONTROL_BUILD_OK);
        CHECK(guard.generation == 2 && guard.groups == NULL);
        CHECK(guard.groups_generation == 0);
        CHECK(guard.group_index == NOC_TOKEN_INDEX_NONE);
        CHECK(guard.branch_index == NOC_TOKEN_INDEX_NONE);
        CHECK(noc_include_guard_build(&fixture.unit,
                                      &fixture.groups,
                                      &guard) == NOC_INCLUDE_CONTROL_BUILD_OK);
        CHECK(guard.generation == 3 && guard.groups == &fixture.groups);
        CHECK(guard.group_index != NOC_TOKEN_INDEX_NONE);
        CHECK(noc_include_guard_is_valid(&guard));
        include_control_fixture_deinit(&fixture);
    }
}

static void test_standalone_unit_and_real_rebuild(void)
{
    static const char source[] =
        "#ifndef STANDALONE_GUARD\n"
        "#define STANDALONE_GUARD\n"
        "int value;\n"
        "#endif\n";
    Noc_Context context = {0};
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Noc_Include_Guard guard = {0};
    Noc_Include_Guard stale;

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "standalone.h",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(noc_include_guard_build_structural(&unit, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(guard.status == NOC_INCLUDE_GUARD_CANONICAL);
    CHECK(slice_equals(guard.guard_name, "STANDALONE_GUARD"));
    CHECK(guard.groups == NULL);
    stale = guard;

    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(!noc_include_guard_is_valid(&stale));
    CHECK(noc_include_guard_build_structural(&unit, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(guard.generation == 2);
    CHECK(noc_include_guard_is_valid(&guard));
    CHECK(slice_equals(guard.guard_name, "STANDALONE_GUARD"));

    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_transactionality_generation_and_staleness(void)
{
    Include_Control_Fixture fixture;
    Noc_Include_Guard guard = {0};
    Noc_Include_Guard preserved;
    size_t stream_generation;

    include_control_fixture_init(&fixture,
                                 "structural-lifecycle.h",
                                 "#ifndef GUARD\n#define GUARD\n#endif\n",
                                 NOC_MACROS_FULL);
    guard = build_structural_guard(&fixture);
    CHECK(guard.generation == 1);
    CHECK(noc_include_guard_build_structural(&fixture.unit, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(guard.generation == 2);
    CHECK(noc_include_guard_is_valid(&guard));
    preserved = guard;

    CHECK(noc_include_guard_build_structural(NULL, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(noc_include_guard_build_structural(&fixture.unit, NULL) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(memcmp(&guard, &preserved, sizeof(guard)) == 0);

    stream_generation = fixture.unit.stream.generation;
    fixture.unit.stream.generation += 1;
    CHECK(!noc_include_guard_is_valid(&guard));
    CHECK(noc_include_guard_build_structural(&fixture.unit, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_STALE);
    CHECK(memcmp(&guard, &preserved, sizeof(guard)) == 0);
    fixture.unit.stream.generation = stream_generation;
    CHECK(noc_include_guard_is_valid(&guard));

    guard.generation = SIZE_MAX;
    preserved = guard;
    CHECK(noc_include_guard_build_structural(&fixture.unit, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_GENERATION_EXHAUSTED);
    CHECK(memcmp(&guard, &preserved, sizeof(guard)) == 0);
    include_control_fixture_deinit(&fixture);
}

int main(void)
{
    test_canonical_metadata_without_condition_evaluation();
    test_recovery_status_parity();
    test_nested_balancing_and_malformed_outer_structure();
    test_null_group_validity_and_cross_builder_reuse();
    test_standalone_unit_and_real_rebuild();
    test_transactionality_generation_and_staleness();
    return finish_suite("include-guard-structural");
}
