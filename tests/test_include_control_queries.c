#include "include_control_test_support.h"

static void test_status_names(void)
{
    static const Noc_Pragma_Once_Status pragma_statuses[] = {
        NOC_PRAGMA_ONCE_VALID,
        NOC_PRAGMA_ONCE_OTHER,
        NOC_PRAGMA_ONCE_MISSING,
        NOC_PRAGMA_ONCE_MALFORMED,
        NOC_PRAGMA_ONCE_INCOMPLETE,
    };
    static const char *const pragma_names[] = {
        "valid", "other", "missing", "malformed", "incomplete",
    };
    static const Noc_Include_Guard_Status guard_statuses[] = {
        NOC_INCLUDE_GUARD_NONE,
        NOC_INCLUDE_GUARD_CANONICAL,
        NOC_INCLUDE_GUARD_INCOMPLETE,
        NOC_INCLUDE_GUARD_MALFORMED,
        NOC_INCLUDE_GUARD_MISSING_DEFINE,
        NOC_INCLUDE_GUARD_NAME_MISMATCH,
        NOC_INCLUDE_GUARD_HAS_PEER_BRANCH,
        NOC_INCLUDE_GUARD_NOT_FILE_ENCLOSING,
    };
    static const char *const guard_names[] = {
        "none", "canonical", "incomplete", "malformed", "missing-define",
        "name-mismatch", "has-peer-branch", "not-file-enclosing",
    };
    static const Noc_Include_Control_Build_Status build_statuses[] = {
        NOC_INCLUDE_CONTROL_BUILD_OK,
        NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT,
        NOC_INCLUDE_CONTROL_BUILD_STALE,
        NOC_INCLUDE_CONTROL_BUILD_GENERATION_EXHAUSTED,
    };
    static const char *const build_names[] = {
        "ok", "invalid-argument", "stale", "generation-exhausted",
    };
    size_t index;

    for (index = 0; index < sizeof(pragma_statuses) / sizeof(pragma_statuses[0]);
         ++index) {
        CHECK(strcmp(noc_pragma_once_status_name(pragma_statuses[index]),
                     pragma_names[index]) == 0);
    }
    for (index = 0; index < sizeof(guard_statuses) / sizeof(guard_statuses[0]);
         ++index) {
        CHECK(strcmp(noc_include_guard_status_name(guard_statuses[index]),
                     guard_names[index]) == 0);
    }
    for (index = 0; index < sizeof(build_statuses) / sizeof(build_statuses[0]);
         ++index) {
        CHECK(strcmp(noc_include_control_build_status_name(build_statuses[index]),
                     build_names[index]) == 0);
    }
    CHECK(strcmp(noc_pragma_once_status_name((Noc_Pragma_Once_Status)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_include_guard_status_name((Noc_Include_Guard_Status)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_include_control_build_status_name(
                     (Noc_Include_Control_Build_Status)99),
                 "unknown") == 0);
}

static void test_transactionality_invalid_arguments_and_staleness(void)
{
    static const char source[] =
        "#pragma once\n"
        "#ifndef GUARD\n"
        "#define GUARD\n"
        "#endif\n";
    Include_Control_Fixture fixture;
    Include_Control_Fixture other;
    Noc_Pragma_Once pragma_once = {0};
    Noc_Pragma_Once pragma_copy;
    Noc_Include_Guard guard = {0};
    Noc_Include_Guard guard_copy;
    size_t unit_generation;
    size_t pragma_directive;

    include_control_fixture_init(&fixture,
                                 "queries.h",
                                 source,
                                 NOC_MACROS_FULL);
    pragma_directive = include_control_fixture_directive(
        &fixture, NOC_PREPROCESSOR_DIRECTIVE_PRAGMA, 0);
    CHECK(slice_equals(fixture.unit.items[pragma_directive].keyword, "pragma"));
    CHECK(noc_pragma_once_build(&fixture.unit,
                                pragma_directive,
                                &pragma_once) == NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(noc_include_guard_build(&fixture.unit,
                                  &fixture.groups,
                                  &guard) == NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(noc_pragma_once_is_valid(&pragma_once));
    CHECK(noc_include_guard_is_valid(&guard));
    pragma_copy = pragma_once;
    guard_copy = guard;

    CHECK(noc_pragma_once_build(NULL, pragma_directive, &pragma_once) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(noc_pragma_once_build(&fixture.unit, pragma_directive, NULL) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(noc_pragma_once_build(&fixture.unit, fixture.unit.count, &pragma_once) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(noc_pragma_once_build(&fixture.unit, 1, &pragma_once) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(memcmp(&pragma_once, &pragma_copy, sizeof(pragma_once)) == 0);

    CHECK(noc_include_guard_build(NULL, &fixture.groups, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(noc_include_guard_build(&fixture.unit, NULL, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(noc_include_guard_build(&fixture.unit, &fixture.groups, NULL) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(memcmp(&guard, &guard_copy, sizeof(guard)) == 0);

    include_control_fixture_init(&other,
                                 "other.h",
                                 "#ifndef OTHER\n#define OTHER\n#endif\n",
                                 NOC_MACROS_FULL);
    CHECK(noc_include_guard_build(&fixture.unit, &other.groups, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT);
    CHECK(memcmp(&guard, &guard_copy, sizeof(guard)) == 0);
    include_control_fixture_deinit(&other);

    unit_generation = fixture.unit.stream.generation;
    fixture.unit.stream.generation += 1;
    CHECK(!noc_pragma_once_is_valid(&pragma_copy));
    CHECK(!noc_include_guard_is_valid(&guard_copy));
    CHECK(noc_pragma_once_build(&fixture.unit,
                                pragma_directive,
                                &pragma_once) == NOC_INCLUDE_CONTROL_BUILD_STALE);
    CHECK(noc_include_guard_build(&fixture.unit, &fixture.groups, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_STALE);
    CHECK(memcmp(&pragma_once, &pragma_copy, sizeof(pragma_once)) == 0);
    CHECK(memcmp(&guard, &guard_copy, sizeof(guard)) == 0);
    fixture.unit.stream.generation = unit_generation;
    CHECK(noc_pragma_once_is_valid(&pragma_copy));
    CHECK(noc_include_guard_is_valid(&guard_copy));

    include_control_fixture_rebuild_groups(&fixture);
    CHECK(noc_pragma_once_is_valid(&pragma_copy));
    CHECK(!noc_include_guard_is_valid(&guard_copy));

    pragma_once.generation = SIZE_MAX;
    pragma_copy = pragma_once;
    CHECK(noc_pragma_once_build(&fixture.unit,
                                pragma_directive,
                                &pragma_once) ==
          NOC_INCLUDE_CONTROL_BUILD_GENERATION_EXHAUSTED);
    CHECK(memcmp(&pragma_once, &pragma_copy, sizeof(pragma_once)) == 0);
    guard.generation = SIZE_MAX;
    guard_copy = guard;
    CHECK(noc_include_guard_build(&fixture.unit, &fixture.groups, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_GENERATION_EXHAUSTED);
    CHECK(memcmp(&guard, &guard_copy, sizeof(guard)) == 0);

    CHECK(!noc_pragma_once_is_valid(NULL));
    CHECK(!noc_include_guard_is_valid(NULL));
    include_control_fixture_deinit(&fixture);
    CHECK(!noc_pragma_once_is_valid(&pragma_copy));
    CHECK(!noc_include_guard_is_valid(&guard_copy));
}

static void test_invalid_conditional_links_preserve_output(void)
{
    static const char source[] =
        "#ifndef GUARD\n"
        "#define GUARD\n"
        "#else\n"
        "#endif\n";
    Include_Control_Fixture fixture;
    Noc_Include_Guard guard = {0};
    Noc_Include_Guard preserved;
    size_t next_branch;

    include_control_fixture_init(&fixture,
                                 "invalid-links.h",
                                 source,
                                 NOC_MACROS_FULL);
    CHECK(fixture.groups.branch_count == 2);
    CHECK(noc_include_guard_build(&fixture.unit,
                                  &fixture.groups,
                                  &guard) == NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(guard.status == NOC_INCLUDE_GUARD_HAS_PEER_BRANCH);
    preserved = guard;
    next_branch = fixture.groups.branches[0].next_branch_index;

    fixture.groups.branches[0].next_branch_index = NOC_TOKEN_INDEX_NONE;
    CHECK(!noc_preprocessor_conditional_groups_is_valid(&fixture.groups));
    CHECK(noc_include_guard_build(&fixture.unit, &fixture.groups, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_STALE);
    CHECK(memcmp(&guard, &preserved, sizeof(guard)) == 0);

    fixture.groups.branches[0].next_branch_index =
        fixture.groups.branch_count;
    CHECK(!noc_preprocessor_conditional_groups_is_valid(&fixture.groups));
    CHECK(noc_include_guard_build(&fixture.unit, &fixture.groups, &guard) ==
          NOC_INCLUDE_CONTROL_BUILD_STALE);
    CHECK(memcmp(&guard, &preserved, sizeof(guard)) == 0);

    fixture.groups.branches[0].next_branch_index = next_branch;
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture.groups));
    include_control_fixture_deinit(&fixture);
}

int main(void)
{
    test_status_names();
    test_transactionality_invalid_arguments_and_staleness();
    test_invalid_conditional_links_preserve_output();
    return finish_suite("include-control-queries");
}
