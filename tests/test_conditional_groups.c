#include "test_support.h"

typedef struct {
    Noc_Context context;
    Noc_Workspace workspace;
    Noc_Document_Snapshot definitions_snapshot;
    Noc_Document_Snapshot input_snapshot;
    Noc_Preprocessor_Unit definitions;
    Noc_Preprocessor_Unit input;
    Noc_Macro_Environment initial_environment;
    Noc_Preprocessor_Conditional_Groups groups;
    Diagnostic_State diagnostics;
} Conditional_Fixture;

static void conditional_fixture_init(Conditional_Fixture *fixture,
                                     const char *definitions,
                                     const char *source,
                                     Noc_Source_Class source_class,
                                     Noc_Macro_Policy macro_policy)
{
    size_t index;
    memset(fixture, 0, sizeof(*fixture));
    noc_context_init(&fixture->context);
    noc_context_set_diagnostic(&fixture->context,
                               count_diagnostics,
                               &fixture->diagnostics);
    noc_workspace_init(&fixture->workspace);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      "conditional-definitions.h",
                                      definitions,
                                      strlen(definitions),
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &fixture->definitions_snapshot) ==
          NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      "conditional-input.c",
                                      source,
                                      strlen(source),
                                      source_class,
                                      &fixture->input_snapshot) ==
          NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&fixture->context,
                                      &fixture->definitions_snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture->definitions));
    CHECK(noc_preprocessor_unit_build(&fixture->context,
                                      &fixture->input_snapshot,
                                      macro_policy,
                                      &fixture->input));
    for (index = 0;
         index < fixture->definitions.macro_directive_count;
         ++index) {
        CHECK(noc_macro_environment_apply(&fixture->initial_environment,
                                          &fixture->definitions,
                                          index) == NOC_MACRO_ENVIRONMENT_OK);
    }
}

static void conditional_fixture_build(Conditional_Fixture *fixture,
                                      size_t initial_entry_limit)
{
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture->initial_environment,
              initial_entry_limit,
              &fixture->input,
              noc_macro_expansion_default_limits(),
              &fixture->groups) == NOC_CONDITIONAL_GROUPS_OK);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture->groups));
}

static void conditional_fixture_deinit(Conditional_Fixture *fixture)
{
    noc_preprocessor_conditional_groups_free(&fixture->groups);
    noc_macro_environment_free(&fixture->initial_environment);
    noc_preprocessor_unit_free(&fixture->input);
    noc_preprocessor_unit_free(&fixture->definitions);
    noc_document_snapshot_free(&fixture->input_snapshot);
    noc_document_snapshot_free(&fixture->definitions_snapshot);
    noc_workspace_deinit(&fixture->workspace);
    noc_context_deinit(&fixture->context);
}

static size_t preprocessing_token_on_line(const Noc_Preprocessor_Unit *unit,
                                          size_t line,
                                          const char *spelling)
{
    size_t index;
    for (index = 0; index < unit->preprocessing_token_count; ++index) {
        Noc_Token token = unit->preprocessing_tokens[index].token;
        if (token.location.line == line &&
            noc_slice_equal_cstr(token.text, spelling)) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static size_t issue_count(const Noc_Preprocessor_Conditional_Groups *groups,
                          Noc_Conditional_Issue_Kind kind)
{
    size_t count = 0;
    size_t index;
    for (index = 0; index < groups->issue_count; ++index) {
        if (groups->issues[index].kind == kind) count += 1;
    }
    return count;
}

static void test_names_basic_selection_and_accessors(void)
{
    static const Noc_Conditional_Group_Status group_statuses[] = {
        NOC_CONDITIONAL_GROUP_COMPLETE,
        NOC_CONDITIONAL_GROUP_INCOMPLETE,
        NOC_CONDITIONAL_GROUP_MALFORMED,
        NOC_CONDITIONAL_GROUP_MALFORMED_INCOMPLETE,
    };
    static const char *const group_names[] = {
        "complete", "incomplete", "malformed", "malformed-incomplete",
    };
    static const Noc_Conditional_Condition_Status condition_statuses[] = {
        NOC_CONDITIONAL_CONDITION_NOT_APPLICABLE,
        NOC_CONDITIONAL_CONDITION_EVALUATED,
        NOC_CONDITIONAL_CONDITION_NOT_EVALUATED,
        NOC_CONDITIONAL_CONDITION_MALFORMED,
        NOC_CONDITIONAL_CONDITION_EXPANSION_FAILED,
        NOC_CONDITIONAL_CONDITION_EVALUATION_FAILED,
    };
    static const char *const condition_names[] = {
        "not-applicable", "evaluated", "not-evaluated", "malformed",
        "expansion-failed", "evaluation-failed",
    };
    static const Noc_Conditional_Issue_Kind issue_kinds[] = {
        NOC_CONDITIONAL_ISSUE_UNMATCHED_ELIF,
        NOC_CONDITIONAL_ISSUE_UNMATCHED_ELSE,
        NOC_CONDITIONAL_ISSUE_UNMATCHED_ENDIF,
        NOC_CONDITIONAL_ISSUE_ELIF_AFTER_ELSE,
        NOC_CONDITIONAL_ISSUE_DUPLICATE_ELSE,
        NOC_CONDITIONAL_ISSUE_MISSING_ENDIF,
        NOC_CONDITIONAL_ISSUE_UNEXPECTED_TOKENS,
        NOC_CONDITIONAL_ISSUE_UNRESOLVED_CONDITION,
        NOC_CONDITIONAL_ISSUE_UNSUPPORTED_DIRECTIVE,
    };
    static const char *const issue_names[] = {
        "unmatched-elif", "unmatched-else", "unmatched-endif",
        "elif-after-else", "duplicate-else", "missing-endif",
        "unexpected-tokens", "unresolved-condition", "unsupported-directive",
    };
    static const Noc_Conditional_Groups_Build_Status build_statuses[] = {
        NOC_CONDITIONAL_GROUPS_OK,
        NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT,
        NOC_CONDITIONAL_GROUPS_STALE,
        NOC_CONDITIONAL_GROUPS_GENERATION_EXHAUSTED,
        NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY,
    };
    static const char *const build_names[] = {
        "ok", "invalid-argument", "stale", "generation-exhausted",
        "out-of-memory",
    };
    static const char source[] =
        "#define A 1\n"
        "#if A\n"
        "active\n"
        "#elif 1 / 0\n"
        "skipped\n"
        "#else\n"
        "also_skipped\n"
        "#endif\n";
    Conditional_Fixture fixture;
    const Noc_Preprocessor_Conditional_Group *group;
    const Noc_Preprocessor_Conditional_Branch *branch;
    const Noc_Macro_Environment *environment;
    size_t active_token;
    size_t skipped_token;
    size_t define_token;
    size_t index;

    for (index = 0; index < sizeof(group_statuses) / sizeof(group_statuses[0]);
         ++index) {
        CHECK(strcmp(noc_conditional_group_status_name(group_statuses[index]),
                     group_names[index]) == 0);
    }
    for (index = 0;
         index < sizeof(condition_statuses) / sizeof(condition_statuses[0]);
         ++index) {
        CHECK(strcmp(noc_conditional_condition_status_name(
                         condition_statuses[index]),
                     condition_names[index]) == 0);
    }
    for (index = 0; index < sizeof(issue_kinds) / sizeof(issue_kinds[0]);
         ++index) {
        CHECK(strcmp(noc_conditional_issue_kind_name(issue_kinds[index]),
                     issue_names[index]) == 0);
    }
    for (index = 0; index < sizeof(build_statuses) / sizeof(build_statuses[0]);
         ++index) {
        CHECK(strcmp(noc_conditional_groups_build_status_name(
                         build_statuses[index]),
                     build_names[index]) == 0);
    }
    CHECK(strcmp(noc_conditional_group_status_name(
                     (Noc_Conditional_Group_Status)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_conditional_condition_status_name(
                     (Noc_Conditional_Condition_Status)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_conditional_issue_kind_name(
                     (Noc_Conditional_Issue_Kind)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_conditional_groups_build_status_name(
                     (Noc_Conditional_Groups_Build_Status)99),
                 "unknown") == 0);

    conditional_fixture_init(&fixture,
                             "",
                             source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(fixture.groups.group_count == 1);
    CHECK(fixture.groups.branch_count == 3);
    CHECK(fixture.groups.issue_count == 0);
    CHECK(fixture.groups.macro_state_complete);
    CHECK(slice_equals(fixture.input.items[0].keyword, "define"));
    CHECK(noc_preprocessor_conditional_groups_is_fully_resolved(
        &fixture.groups));

    group = noc_preprocessor_conditional_group_at(&fixture.groups, 0);
    CHECK(group && group->status == NOC_CONDITIONAL_GROUP_COMPLETE);
    CHECK(group && group->parent_branch_index == NOC_TOKEN_INDEX_NONE);
    CHECK(group && group->opener_directive_index == 1);
    CHECK(group && group->closer_directive_index == 4);
    CHECK(group && group->first_branch_index == 0);
    CHECK(group && group->last_branch_index == 2);
    branch = noc_preprocessor_conditional_branch_at(&fixture.groups, 0);
    CHECK(branch &&
          branch->condition_status == NOC_CONDITIONAL_CONDITION_EVALUATED);
    CHECK(branch &&
          branch->condition_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(branch && branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(branch && branch->condition_environment_entry_limit == 1);
    CHECK(branch && branch->previous_branch_index == NOC_TOKEN_INDEX_NONE);
    CHECK(branch && branch->next_branch_index == 1);
    branch = noc_preprocessor_conditional_branch_at(&fixture.groups, 1);
    CHECK(branch && branch->condition_status ==
                        NOC_CONDITIONAL_CONDITION_NOT_EVALUATED);
    CHECK(branch && branch->content_activity ==
                        NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(branch &&
          branch->condition_environment_entry_limit == NOC_TOKEN_INDEX_NONE);
    branch = noc_preprocessor_conditional_branch_at(&fixture.groups, 2);
    CHECK(branch && branch->condition_status ==
                        NOC_CONDITIONAL_CONDITION_NOT_APPLICABLE);
    CHECK(branch && branch->content_activity ==
                        NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(branch && branch->next_branch_index == NOC_TOKEN_INDEX_NONE);

    CHECK(noc_preprocessor_conditional_owned_group(&fixture.groups, 1) == 0);
    CHECK(noc_preprocessor_conditional_owned_group(&fixture.groups, 2) == 0);
    CHECK(noc_preprocessor_conditional_owned_group(&fixture.groups, 4) == 0);
    CHECK(noc_preprocessor_conditional_introduced_branch(&fixture.groups, 1) == 0);
    CHECK(noc_preprocessor_conditional_introduced_branch(&fixture.groups, 2) == 1);
    CHECK(noc_preprocessor_conditional_introduced_branch(&fixture.groups, 3) == 2);
    CHECK(noc_preprocessor_conditional_introduced_branch(&fixture.groups, 4) ==
          NOC_TOKEN_INDEX_NONE);

    define_token = preprocessing_token_on_line(&fixture.input, 1, "define");
    active_token = preprocessing_token_on_line(&fixture.input, 3, "active");
    skipped_token = preprocessing_token_on_line(&fixture.input, 5, "skipped");
    CHECK(define_token != NOC_TOKEN_INDEX_NONE);
    CHECK(active_token != NOC_TOKEN_INDEX_NONE);
    CHECK(skipped_token != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_conditional_token_macro_entry_limit(&fixture.groups,
                                                               define_token) == 0);
    CHECK(noc_preprocessor_conditional_token_macro_entry_limit(&fixture.groups,
                                                               active_token) == 1);
    CHECK(noc_preprocessor_conditional_token_macro_entry_limit(
              &fixture.groups,
              fixture.input.preprocessing_token_count - 1) == 1);
    CHECK(noc_preprocessor_conditional_token_activity(&fixture.groups,
                                                      active_token) ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(noc_preprocessor_conditional_token_activity(&fixture.groups,
                                                      skipped_token) ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    environment = noc_preprocessor_conditional_environment(&fixture.groups);
    CHECK(environment && environment->count == 1);

    CHECK(noc_preprocessor_conditional_group_at(&fixture.groups, 1) == NULL);
    CHECK(noc_preprocessor_conditional_branch_at(&fixture.groups, 3) == NULL);
    CHECK(noc_preprocessor_conditional_issue_at(&fixture.groups, 0) == NULL);
    CHECK(noc_preprocessor_conditional_owned_group(NULL, 0) ==
          NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_conditional_token_activity(NULL, 0) ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(noc_preprocessor_conditional_environment(NULL) == NULL);

    conditional_fixture_deinit(&fixture);
}

static void test_nested_hierarchy_ranges_and_inactive_parent(void)
{
    static const char source[] =
        "#if 0\n"
        "#if 1\n"
        "#endif\n"
        "#else\n"
        "#endif\n";
    Conditional_Fixture fixture;
    const Noc_Preprocessor_Conditional_Group *outer;
    const Noc_Preprocessor_Conditional_Group *inner;
    const Noc_Preprocessor_Conditional_Branch *outer_first;
    const Noc_Preprocessor_Conditional_Branch *inner_branch;
    const Noc_Preprocessor_Conditional_Branch *outer_else;
    size_t inner_if_token;

    conditional_fixture_init(&fixture,
                             "",
                             source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(fixture.groups.group_count == 2);
    CHECK(fixture.groups.branch_count == 3);
    CHECK(fixture.groups.issue_count == 0);
    CHECK(noc_preprocessor_conditional_groups_is_fully_resolved(
        &fixture.groups));

    outer = &fixture.groups.groups[0];
    inner = &fixture.groups.groups[1];
    outer_first = &fixture.groups.branches[0];
    inner_branch = &fixture.groups.branches[1];
    outer_else = &fixture.groups.branches[2];
    CHECK(inner->parent_branch_index == 0);
    CHECK(inner->preprocessing_tokens.begin >= outer_first->content_tokens.begin);
    CHECK(inner->preprocessing_tokens.end <= outer_first->content_tokens.end);
    CHECK(inner_branch->condition_status ==
          NOC_CONDITIONAL_CONDITION_NOT_EVALUATED);
    CHECK(inner_branch->condition_environment_entry_limit ==
          NOC_TOKEN_INDEX_NONE);
    CHECK(inner_branch->content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(inner_branch->content_tokens.begin == inner_branch->content_tokens.end);
    CHECK(outer_else->content_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(outer_else->content_tokens.begin == outer_else->content_tokens.end);
    CHECK(outer->preprocessing_tokens.begin ==
          fixture.input.items[0].preprocessing_tokens.begin);
    CHECK(outer->preprocessing_tokens.end ==
          fixture.input.items[4].preprocessing_tokens.end);

    inner_if_token = preprocessing_token_on_line(&fixture.input, 2, "if");
    CHECK(inner_if_token != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_conditional_token_activity(&fixture.groups,
                                                      inner_if_token) ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);

    conditional_fixture_deinit(&fixture);
}

static void test_elif_selection_and_unknown_availability(void)
{
    static const char source[] =
        "#if 0\n"
        "first\n"
        "#elif 1\n"
        "second\n"
        "#else\n"
        "third\n"
        "#endif\n"
        "#if 'A'\n"
        "unknown_first\n"
        "#elif 1\n"
        "unknown_second\n"
        "#endif\n";
    Conditional_Fixture fixture;
    const Noc_Preprocessor_Conditional_Branch *branch;

    conditional_fixture_init(&fixture,
                             "",
                             source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(fixture.groups.group_count == 2);
    CHECK(fixture.groups.branch_count == 5);
    branch = &fixture.groups.branches[1];
    CHECK(branch->condition_status == NOC_CONDITIONAL_CONDITION_EVALUATED);
    CHECK(branch->condition_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    branch = &fixture.groups.branches[2];
    CHECK(branch->condition_status ==
          NOC_CONDITIONAL_CONDITION_NOT_APPLICABLE);
    CHECK(branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    branch = &fixture.groups.branches[4];
    CHECK(branch->condition_status ==
          NOC_CONDITIONAL_CONDITION_NOT_EVALUATED);
    CHECK(branch->condition_environment_entry_limit == NOC_TOKEN_INDEX_NONE);
    CHECK(branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_UNRESOLVED_CONDITION) == 1);

    conditional_fixture_deinit(&fixture);
}

static void test_ifdef_prefix_builtins_and_malformed_operands(void)
{
    static const char definitions[] =
        "#define A 1\n"
        "#define LATE 1\n";
    static const char source[] =
        "#ifdef /* comment */ A\n"
        "#endif\n"
        "#ifndef LATE\n"
        "#endif\n"
        "#ifdef __STDC__\n"
        "#endif\n"
        "#ifdef SPLI\\\nCE\n"
        "#endif\n"
        "#ifdef A extra\n"
        "#endif\n"
        "#ifndef\n"
        "#endif\n";
    Conditional_Fixture fixture;
    const Noc_Preprocessor_Conditional_Branch *branch;

    conditional_fixture_init(&fixture,
                             definitions,
                             source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 1);
    CHECK(fixture.groups.group_count == 6);
    CHECK(fixture.groups.issue_count == 2);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_UNRESOLVED_CONDITION) == 2);
    branch = &fixture.groups.branches[0];
    CHECK(branch->condition_status == NOC_CONDITIONAL_CONDITION_EVALUATED);
    CHECK(branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    branch = &fixture.groups.branches[1];
    CHECK(branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    branch = &fixture.groups.branches[2];
    CHECK(branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    branch = &fixture.groups.branches[3];
    CHECK(branch->condition_status == NOC_CONDITIONAL_CONDITION_EVALUATED);
    CHECK(branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    branch = &fixture.groups.branches[4];
    CHECK(branch->condition_status == NOC_CONDITIONAL_CONDITION_MALFORMED);
    CHECK(branch->problem_unit == &fixture.input);
    branch = &fixture.groups.branches[5];
    CHECK(branch->condition_status == NOC_CONDITIONAL_CONDITION_MALFORMED);
    CHECK(branch->problem_token_index == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_conditional_environment(&fixture.groups)->count == 1);

    /* The cloned prefix retains definition units, not the source environment
       object itself. */
    noc_macro_environment_free(&fixture.initial_environment);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture.groups));

    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "#define A 1\n#undef A\n#define A 2\n",
                             "#ifdef A\n#endif\n",
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 2);
    CHECK(fixture.groups.environment.count == 2);
    CHECK(fixture.groups.environment.items[0].previous_entry_index ==
          NOC_TOKEN_INDEX_NONE);
    CHECK(fixture.groups.environment.items[1].previous_entry_index == 0);
    CHECK(fixture.groups.branches[0].condition_status ==
          NOC_CONDITIONAL_CONDITION_EVALUATED);
    CHECK(fixture.groups.branches[0].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    noc_macro_environment_free(&fixture.initial_environment);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture.groups));
    CHECK(noc_macro_environment_lookup(&fixture.groups.environment,
                                       noc_slice_from_cstr("A")) == NULL);
    conditional_fixture_deinit(&fixture);
}

static void test_macro_state_poisoning_and_active_only_updates(void)
{
    static const char poisoned_source[] =
        "#define BASE 0\n"
        "#if 'A'\n"
        "#undef BASE\n"
        "#endif\n"
        "#if BASE == 0\n"
        "after_unknown\n"
        "#endif\n"
        "#define DEFINITE 1\n"
        "#if DEFINITE\n"
        "later\n"
        "#endif\n";
    static const char unpoisoned_source[] =
        "#define X 1\n"
        "#if 'A'\n"
        "no_macro_event\n"
        "#endif\n"
        "#if X\n"
        "known_after_unknown\n"
        "#endif\n";
    static const char inactive_source[] =
        "#if 0\n"
        "#define HIDDEN 1\n"
        "#endif\n"
        "#if defined(HIDDEN)\n"
        "hidden\n"
        "#endif\n";
    static const char blocked_source[] =
        "#if 'A'\n"
        "#define BLOCKED 1\n"
        "#endif\n"
        "#if defined(BLOCKED)\n"
        "blocked\n"
        "#endif\n";
    static const char nested_source[] =
        "#if 'A'\n"
        "#if 1\n"
        "#define MAYBE 1\n"
        "#endif\n"
        "#endif\n"
        "#if defined(MAYBE)\n"
        "later\n"
        "#endif\n";
    Conditional_Fixture fixture;
    const Noc_Preprocessor_Conditional_Branch *branch;
    size_t token;

    conditional_fixture_init(&fixture,
                             "",
                             poisoned_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(!fixture.groups.macro_state_complete);
    CHECK(!noc_preprocessor_conditional_groups_is_fully_resolved(
        &fixture.groups));
    CHECK(noc_preprocessor_conditional_environment(&fixture.groups)->count == 2);
    branch = &fixture.groups.branches[1];
    CHECK(branch->condition_status ==
          NOC_CONDITIONAL_CONDITION_NOT_EVALUATED);
    CHECK(branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    branch = &fixture.groups.branches[2];
    CHECK(branch->condition_status ==
          NOC_CONDITIONAL_CONDITION_NOT_EVALUATED);
    token = preprocessing_token_on_line(&fixture.input, 6, "after_unknown");
    CHECK(token != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_conditional_token_macro_entry_limit(&fixture.groups,
                                                               token) ==
          NOC_TOKEN_INDEX_NONE);
    token = preprocessing_token_on_line(&fixture.input, 3, "undef");
    CHECK(token != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_conditional_token_macro_entry_limit(&fixture.groups,
                                                               token) == 1);
    token = preprocessing_token_on_line(&fixture.input, 4, "endif");
    CHECK(token != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_conditional_token_macro_entry_limit(&fixture.groups,
                                                               token) ==
          NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_conditional_token_macro_entry_limit(
              &fixture.groups,
              fixture.input.preprocessing_token_count - 1) ==
          NOC_TOKEN_INDEX_NONE);
    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "",
                             unpoisoned_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(fixture.groups.macro_state_complete);
    branch = &fixture.groups.branches[1];
    CHECK(branch->condition_status == NOC_CONDITIONAL_CONDITION_EVALUATED);
    CHECK(branch->content_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_UNRESOLVED_CONDITION) == 1);
    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "",
                             inactive_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(fixture.groups.macro_state_complete);
    CHECK(noc_preprocessor_conditional_environment(&fixture.groups)->count == 0);
    CHECK(fixture.groups.branches[1].condition_status ==
          NOC_CONDITIONAL_CONDITION_EVALUATED);
    CHECK(fixture.groups.branches[1].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "",
                             blocked_source,
                             NOC_SOURCE_CLASS_PROJECT,
                             NOC_MACROS_TRUSTED_ONLY);
    conditional_fixture_build(&fixture, 0);
    CHECK(fixture.groups.macro_state_complete);
    CHECK(noc_preprocessor_conditional_environment(&fixture.groups)->count == 0);
    CHECK(fixture.groups.branches[1].condition_status ==
          NOC_CONDITIONAL_CONDITION_EVALUATED);
    CHECK(fixture.groups.branches[1].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "",
                             nested_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(!fixture.groups.macro_state_complete);
    CHECK(fixture.groups.group_count == 3);
    CHECK(fixture.groups.groups[1].parent_branch_index == 0);
    CHECK(fixture.groups.branches[1].condition_status ==
          NOC_CONDITIONAL_CONDITION_NOT_EVALUATED);
    CHECK(fixture.groups.branches[1].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(fixture.groups.branches[2].condition_status ==
          NOC_CONDITIONAL_CONDITION_NOT_EVALUATED);
    conditional_fixture_deinit(&fixture);
}

static void test_malformed_recovery_and_incomplete_eof(void)
{
    static const char recovery_source[] =
        "#elif 1\n"
        "#else\n"
        "#endif\n"
        "#if 1\n"
        "#else garbage\n"
        "#if 1\n"
        "nested\n"
        "#endif\n"
        "#elif 1\n"
        "#else\n"
        "#endif trailing\n"
        "after\n";
    static const char incomplete_source[] =
        "#if 1\n"
        "#if 0\n"
        "tail\n";
    static const char malformed_child_source[] =
        "#if 1\n"
        "#if 0\n"
        "#else garbage\n"
        "#endif\n"
        "#endif\n";
    static const char malformed_incomplete_source[] =
        "#if 1\n"
        "#else garbage\n";
    static const char unsupported_source[] =
        "#if 0\n"
        "#elifdef FEATURE\n"
        "extension\n"
        "#endif\n";
    Conditional_Fixture fixture;
    size_t after_token;
    size_t eof_index;

    conditional_fixture_init(&fixture,
                             "",
                             recovery_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_UNMATCHED_ELIF) == 1);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_UNMATCHED_ELSE) == 1);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_UNMATCHED_ENDIF) == 1);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_ELIF_AFTER_ELSE) == 1);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_DUPLICATE_ELSE) == 1);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_UNEXPECTED_TOKENS) == 2);
    CHECK(fixture.groups.group_count == 2);
    CHECK(fixture.groups.groups[0].status == NOC_CONDITIONAL_GROUP_MALFORMED);
    CHECK(fixture.groups.groups[1].status == NOC_CONDITIONAL_GROUP_COMPLETE);
    CHECK(fixture.groups.groups[1].parent_branch_index == 1);
    after_token = preprocessing_token_on_line(&fixture.input, 12, "after");
    CHECK(after_token != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_conditional_token_activity(&fixture.groups,
                                                      after_token) ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "",
                             incomplete_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    eof_index = fixture.input.preprocessing_token_count - 1;
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_MISSING_ENDIF) == 2);
    CHECK(fixture.groups.groups[0].status == NOC_CONDITIONAL_GROUP_INCOMPLETE);
    CHECK(fixture.groups.groups[1].status == NOC_CONDITIONAL_GROUP_INCOMPLETE);
    CHECK(fixture.groups.groups[0].preprocessing_tokens.end == eof_index);
    CHECK(fixture.groups.groups[1].preprocessing_tokens.end == eof_index);
    CHECK(fixture.groups.branches[0].content_tokens.end == eof_index);
    CHECK(fixture.groups.branches[1].content_tokens.end == eof_index);
    CHECK(noc_preprocessor_conditional_token_activity(&fixture.groups,
                                                      eof_index) ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "",
                             malformed_child_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(fixture.groups.groups[0].status == NOC_CONDITIONAL_GROUP_COMPLETE);
    CHECK(fixture.groups.groups[1].status == NOC_CONDITIONAL_GROUP_MALFORMED);
    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "",
                             malformed_incomplete_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    eof_index = fixture.input.preprocessing_token_count - 1;
    CHECK(fixture.groups.groups[0].status ==
          NOC_CONDITIONAL_GROUP_MALFORMED_INCOMPLETE);
    CHECK(fixture.groups.groups[0].preprocessing_tokens.end == eof_index);
    CHECK(fixture.groups.branches[1].content_tokens.end == eof_index);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_UNEXPECTED_TOKENS) == 1);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_MISSING_ENDIF) == 1);
    CHECK(noc_preprocessor_conditional_token_activity(&fixture.groups,
                                                      eof_index) ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "",
                             unsupported_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    CHECK(issue_count(&fixture.groups,
                      NOC_CONDITIONAL_ISSUE_UNSUPPORTED_DIRECTIVE) == 1);
    CHECK(fixture.groups.branches[1].condition_status ==
          NOC_CONDITIONAL_CONDITION_MALFORMED);
    CHECK(fixture.groups.branches[1].content_activity ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    conditional_fixture_deinit(&fixture);
}

static void test_expression_failure_and_definition_provenance(void)
{
    static const char definitions[] =
        "#define BAD (1 / 0)\n"
        "#define F(x) x\n";
    static const char source[] =
        "#if BAD\n"
        "#endif\n"
        "#if (1 + )\n"
        "#endif\n"
        "#if F(1, 2)\n"
        "#endif\n";
    Conditional_Fixture fixture;
    const Noc_Preprocessor_Conditional_Branch *branch;

    conditional_fixture_init(&fixture,
                             definitions,
                             source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, fixture.initial_environment.count);
    CHECK(fixture.groups.issue_count == 3);
    branch = &fixture.groups.branches[0];
    CHECK(branch->condition_status ==
          NOC_CONDITIONAL_CONDITION_EVALUATION_FAILED);
    CHECK(branch->expression_status ==
          NOC_PREPROCESSOR_EXPRESSION_DIVISION_BY_ZERO);
    CHECK(branch->problem_unit == &fixture.definitions);
    CHECK(branch->problem_token_index != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_token_is_punct(
        fixture.definitions.preprocessing_tokens[
            branch->problem_token_index].token,
        "/"));
    branch = &fixture.groups.branches[1];
    CHECK(branch->condition_status ==
          NOC_CONDITIONAL_CONDITION_EVALUATION_FAILED);
    CHECK(branch->problem_unit == &fixture.input);
    branch = &fixture.groups.branches[2];
    CHECK(branch->condition_status ==
          NOC_CONDITIONAL_CONDITION_EXPANSION_FAILED);
    CHECK(branch->expansion_status ==
          NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH);
    CHECK(branch->problem_unit == &fixture.input);

    conditional_fixture_deinit(&fixture);
}

static void test_lifetimes_epochs_and_transactional_failure(void)
{
    static const char source[] = "#if 1\n#endif\n";
    static const char late_failure_source[] =
        "#define LOCAL 1\n"
        "#if LOCAL\n"
        "#endif\n";
    Conditional_Fixture fixture;
    Noc_Macro_Expansion old_expansion = {0};
    Noc_Macro_Expansion freed_expansion = {0};
    Noc_Preprocessor_Conditional_Group *preserved_groups;
    Noc_Preprocessor_Conditional_Branch *preserved_branches;
    size_t preserved_generation;
    size_t input_generation;
    Noc_Macro_Expansion_Limits bad_limits =
        noc_macro_expansion_default_limits();
    Noc_Token_Range condition;

    conditional_fixture_init(&fixture,
                             "",
                             source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    preserved_groups = fixture.groups.groups;
    preserved_branches = fixture.groups.branches;
    preserved_generation = fixture.groups.generation;
    bad_limits.max_depth = 0;
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture.initial_environment,
              0,
              &fixture.input,
              bad_limits,
              &fixture.groups) == NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT);
    CHECK(fixture.groups.groups == preserved_groups);
    CHECK(fixture.groups.branches == preserved_branches);
    CHECK(fixture.groups.generation == preserved_generation);
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture.initial_environment,
              fixture.initial_environment.count + 1,
              &fixture.input,
              noc_macro_expansion_default_limits(),
              &fixture.groups) == NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT);
    CHECK(fixture.groups.groups == preserved_groups);
    input_generation = fixture.input.stream.generation;
    fixture.input.stream.generation += 1;
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture.initial_environment,
              0,
              &fixture.input,
              noc_macro_expansion_default_limits(),
              &fixture.groups) == NOC_CONDITIONAL_GROUPS_STALE);
    CHECK(fixture.groups.groups == preserved_groups);
    fixture.input.stream.generation = input_generation;
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture.groups));

    condition = fixture.groups.branches[0].condition_tokens;
    CHECK(noc_macro_expansion_build_condition(
              &fixture.groups.environment,
              fixture.groups.branches[0].condition_environment_entry_limit,
              &fixture.input,
              condition,
              noc_macro_expansion_default_limits(),
              &old_expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_is_valid(&old_expansion));
    conditional_fixture_build(&fixture, 0);
    CHECK(!noc_macro_expansion_is_valid(&old_expansion));

    /* Reusing the inline environment as the next initial prefix is safe because
       the prefix is cloned before the old result is released. */
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture.groups.environment,
              fixture.groups.environment.count,
              &fixture.input,
              noc_macro_expansion_default_limits(),
              &fixture.groups) == NOC_CONDITIONAL_GROUPS_OK);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture.groups));

    fixture.groups.generation = SIZE_MAX;
    fixture.groups.environment.generation = SIZE_MAX;
    fixture.groups.published_environment_generation = SIZE_MAX;
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture.groups));
    CHECK(noc_macro_expansion_build_condition(
              &fixture.groups.environment,
              fixture.groups.branches[0].condition_environment_entry_limit,
              &fixture.input,
              fixture.groups.branches[0].condition_tokens,
              noc_macro_expansion_default_limits(),
              &freed_expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_is_valid(&freed_expansion));
    /* Rebuild exhaustion is reported without changing the published result. */
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture.initial_environment,
              0,
              &fixture.input,
              noc_macro_expansion_default_limits(),
              &fixture.groups) ==
          NOC_CONDITIONAL_GROUPS_GENERATION_EXHAUSTED);
    CHECK(fixture.groups.generation == SIZE_MAX);
    noc_preprocessor_conditional_groups_free(&fixture.groups);
    CHECK(!noc_macro_expansion_is_valid(&freed_expansion));

    noc_macro_expansion_free(&freed_expansion);
    noc_macro_expansion_free(&old_expansion);
    conditional_fixture_deinit(&fixture);

    conditional_fixture_init(&fixture,
                             "",
                             late_failure_source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 0);
    fixture.groups.generation = SIZE_MAX - 1;
    fixture.groups.environment.generation = SIZE_MAX - 1;
    fixture.groups.published_environment_generation = SIZE_MAX - 1;
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture.groups));
    preserved_groups = fixture.groups.groups;
    preserved_branches = fixture.groups.branches;
    preserved_generation = fixture.groups.generation;
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture.initial_environment,
              0,
              &fixture.input,
              noc_macro_expansion_default_limits(),
              &fixture.groups) ==
          NOC_CONDITIONAL_GROUPS_GENERATION_EXHAUSTED);
    CHECK(fixture.groups.groups == preserved_groups);
    CHECK(fixture.groups.branches == preserved_branches);
    CHECK(fixture.groups.generation == preserved_generation);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture.groups));
    conditional_fixture_deinit(&fixture);
}

static void test_stale_owners_and_external_environment_mutation(void)
{
    static const char definitions[] = "#define EXTERNAL 1\n";
    static const char source[] = "#if EXTERNAL\n#endif\n";
    Conditional_Fixture fixture;
    Noc_Macro_Expansion expansion = {0};
    Noc_Token_Range condition;
    size_t saved_generation;

    conditional_fixture_init(&fixture,
                             definitions,
                             source,
                             NOC_SOURCE_CLASS_TRUSTED,
                             NOC_MACROS_FULL);
    conditional_fixture_build(&fixture, 1);
    saved_generation = fixture.definitions.stream.generation;
    fixture.definitions.stream.generation += 1;
    CHECK(!noc_preprocessor_conditional_groups_is_valid(&fixture.groups));
    fixture.definitions.stream.generation = saved_generation;
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture.groups));

    condition = fixture.groups.branches[0].condition_tokens;
    noc_macro_environment_free(&fixture.groups.environment);
    CHECK(noc_macro_environment_apply(&fixture.groups.environment,
                                      &fixture.definitions,
                                      0) == NOC_MACRO_ENVIRONMENT_OK);
    CHECK(!noc_preprocessor_conditional_groups_is_valid(&fixture.groups));
    CHECK(fixture.groups.environment.count == 1);
    CHECK(noc_macro_expansion_build_condition(
              &fixture.groups.environment,
              fixture.groups.environment.count,
              &fixture.input,
              condition,
              noc_macro_expansion_default_limits(),
              &expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_is_valid(&expansion));
    conditional_fixture_build(&fixture, 1);
    CHECK(fixture.groups.environment.count == 1);
    CHECK(!noc_macro_expansion_is_valid(&expansion));

    noc_macro_expansion_free(&expansion);
    conditional_fixture_deinit(&fixture);
}

int main(void)
{
    test_names_basic_selection_and_accessors();
    test_nested_hierarchy_ranges_and_inactive_parent();
    test_elif_selection_and_unknown_availability();
    test_ifdef_prefix_builtins_and_malformed_operands();
    test_macro_state_poisoning_and_active_only_updates();
    test_malformed_recovery_and_incomplete_eof();
    test_expression_failure_and_definition_provenance();
    test_lifetimes_epochs_and_transactional_failure();
    test_stale_owners_and_external_environment_mutation();
    return finish_suite("conditional-groups");
}
