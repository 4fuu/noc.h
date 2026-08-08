#include "include_control_test_support.h"

static Noc_Include_Guard build_guard(Include_Control_Fixture *fixture)
{
    Noc_Include_Guard guard = {0};
    CHECK(noc_include_guard_build(&fixture->unit,
                                  &fixture->groups,
                                  &guard) == NOC_INCLUDE_CONTROL_BUILD_OK);
    CHECK(noc_include_guard_is_valid(&guard));
    return guard;
}

static void check_guard_status(const char *source,
                               Noc_Macro_Policy macro_policy,
                               Noc_Include_Guard_Status expected)
{
    Include_Control_Fixture fixture;
    Noc_Include_Guard guard;
    include_control_fixture_init(&fixture,
                                 "guard.h",
                                 source,
                                 macro_policy);
    guard = build_guard(&fixture);
    CHECK(guard.status == expected);
    include_control_fixture_deinit(&fixture);
}

static void test_canonical_structure_ranges_and_policy(void)
{
    static const char source[] =
        "/* lead */\n"
        "#ifndef PROJECT_GUARD\n"
        "/* between */ #define PROJECT_GUARD 1\n"
        "#if 1\n"
        "int value;\n"
        "#endif\n"
        "#endif /* PROJECT_GUARD */\n"
        "/* tail */\n";
    Include_Control_Fixture fixture;
    Noc_Include_Guard guard;
    const Noc_Preprocessor_Conditional_Group *group;
    Noc_Buffer logical_name = {0};

    include_control_fixture_init(&fixture,
                                 "canonical.h",
                                 source,
                                 NOC_MACROS_DISABLED);
    guard = build_guard(&fixture);
    CHECK(guard.status == NOC_INCLUDE_GUARD_CANONICAL);
    CHECK(!guard.definition_allowed);
    CHECK(guard.group_index == 0);
    CHECK(guard.branch_index == 0);
    CHECK(guard.opener_directive_index == 0);
    CHECK(guard.define_directive_index == 1);
    CHECK(guard.closer_directive_index == 4);
    CHECK(guard.guard_name_token_index != NOC_TOKEN_INDEX_NONE);
    CHECK(slice_equals(guard.guard_name, "PROJECT_GUARD"));
    CHECK(guard.problem_directive_index == NOC_TOKEN_INDEX_NONE);
    CHECK(guard.problem_token_index == NOC_TOKEN_INDEX_NONE);
    group = noc_preprocessor_conditional_group_at(&fixture.groups,
                                                   guard.group_index);
    CHECK(group != NULL);
    CHECK(group && guard.preprocessing_tokens.begin ==
                       group->preprocessing_tokens.begin);
    CHECK(group && guard.preprocessing_tokens.end ==
                       group->preprocessing_tokens.end);
    CHECK(guard.preprocessing_tokens.begin < guard.guard_name_token_index);
    CHECK(guard.guard_name_token_index < guard.preprocessing_tokens.end);
    CHECK(noc_token_logical_text(
        fixture.unit.preprocessing_tokens[guard.guard_name_token_index].token,
        &logical_name));
    CHECK(slice_equals((Noc_Slice){logical_name.items, logical_name.count},
                       "PROJECT_GUARD"));
    noc_buffer_free(&logical_name);
    include_control_fixture_deinit(&fixture);

    include_control_fixture_init(&fixture,
                                 "canonical-policy.h",
                                 source,
                                 NOC_MACROS_FULL);
    guard = build_guard(&fixture);
    CHECK(guard.status == NOC_INCLUDE_GUARD_CANONICAL);
    CHECK(guard.definition_allowed);
    include_control_fixture_deinit(&fixture);
}

static void test_splice_aware_name_and_object_replacement(void)
{
    static const char source[] =
        "#ifndef PROJ\\\nECT_GUARD\n"
        "#define PROJECT_G\\\nUARD value tokens\n"
        "int value;\n"
        "#endif\n";
    Include_Control_Fixture fixture;
    Noc_Include_Guard guard;
    Noc_Buffer name = {0};

    include_control_fixture_init(&fixture,
                                 "spliced-guard.h",
                                 source,
                                 NOC_MACROS_FULL);
    guard = build_guard(&fixture);
    CHECK(guard.status == NOC_INCLUDE_GUARD_CANONICAL);
    CHECK(noc_token_logical_text(
        fixture.unit.preprocessing_tokens[guard.guard_name_token_index].token,
        &name));
    CHECK(slice_equals((Noc_Slice){name.items, name.count}, "PROJECT_GUARD"));
    noc_buffer_free(&name);
    include_control_fixture_deinit(&fixture);
}

static void test_recovery_and_conservative_recognition(void)
{
    check_guard_status("#ifndef GUARD\n#define OTHER\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_NAME_MISMATCH);
    check_guard_status("#ifndef GUARD\nint x;\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_MISSING_DEFINE);
    check_guard_status("#ifndef GUARD\nint x;\n#define GUARD\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_MISSING_DEFINE);
    check_guard_status("#ifndef GUARD\n#define GUARD() 1\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_MALFORMED);
    check_guard_status("#ifndef GUARD\n#define\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_INCOMPLETE);
    check_guard_status("#ifndef GUARD\n#define GUARD \"unfinished\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_INCOMPLETE);
    check_guard_status("#ifndef GUARD\n#define GUARD\n#else\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_HAS_PEER_BRANCH);
    check_guard_status("#ifndef GUARD\n#define GUARD\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_INCOMPLETE);
    check_guard_status("#ifndef\n#define GUARD\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_INCOMPLETE);
    check_guard_status("#ifndef GUARD extra\n#define GUARD\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_MALFORMED);
    check_guard_status("#ifndef GUARD\n#define GUARD\n#endif\nint x;\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_NOT_FILE_ENCLOSING);
    check_guard_status("int before;\n#ifndef GUARD\n#define GUARD\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_NONE);
    check_guard_status("#if !defined(GUARD)\n#define GUARD\n#endif\n",
                       NOC_MACROS_FULL,
                       NOC_INCLUDE_GUARD_NONE);
}

int main(void)
{
    test_canonical_structure_ranges_and_policy();
    test_splice_aware_name_and_object_replacement();
    test_recovery_and_conservative_recognition();
    return finish_suite("include-guard");
}
