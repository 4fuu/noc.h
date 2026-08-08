#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_CONDITIONAL_GROUPS_IMPLEMENTATION_INCLUDED
#define NOC_CONDITIONAL_GROUPS_IMPLEMENTATION_INCLUDED

typedef struct {
    size_t group_index;
    size_t branch_index;
    Noc_Preprocessor_Activity parent_activity;
    Noc_Preprocessor_Activity prior_taken;
    bool saw_else;
    bool malformed;
} Noc__Conditional_Frame;

static bool noc__conditional_activity_is_valid(
    Noc_Preprocessor_Activity activity)
{
    return activity == NOC_PREPROCESSOR_ACTIVITY_UNKNOWN ||
           activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE ||
           activity == NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
}

static Noc_Preprocessor_Activity noc__conditional_and(
    Noc_Preprocessor_Activity left,
    Noc_Preprocessor_Activity right)
{
    if (left == NOC_PREPROCESSOR_ACTIVITY_INACTIVE ||
        right == NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    }
    if (left == NOC_PREPROCESSOR_ACTIVITY_ACTIVE &&
        right == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

static Noc_Preprocessor_Activity noc__conditional_or(
    Noc_Preprocessor_Activity left,
    Noc_Preprocessor_Activity right)
{
    if (left == NOC_PREPROCESSOR_ACTIVITY_ACTIVE ||
        right == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    }
    if (left == NOC_PREPROCESSOR_ACTIVITY_INACTIVE &&
        right == NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

static Noc_Preprocessor_Activity noc__conditional_not(
    Noc_Preprocessor_Activity activity)
{
    if (activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    }
    if (activity == NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

static bool noc__conditional_is_opener(
    Noc_Preprocessor_Directive_Kind kind)
{
    return kind == NOC_PREPROCESSOR_DIRECTIVE_IF ||
           kind == NOC_PREPROCESSOR_DIRECTIVE_IFDEF ||
           kind == NOC_PREPROCESSOR_DIRECTIVE_IFNDEF;
}

static bool noc__conditional_is_peer(Noc_Preprocessor_Directive_Kind kind)
{
    return kind == NOC_PREPROCESSOR_DIRECTIVE_ELIF ||
           kind == NOC_PREPROCESSOR_DIRECTIVE_ELIFDEF ||
           kind == NOC_PREPROCESSOR_DIRECTIVE_ELIFNDEF ||
           kind == NOC_PREPROCESSOR_DIRECTIVE_ELSE;
}

static bool noc__conditional_group_status_is_valid(
    Noc_Conditional_Group_Status status)
{
    return status == NOC_CONDITIONAL_GROUP_COMPLETE ||
           status == NOC_CONDITIONAL_GROUP_INCOMPLETE ||
           status == NOC_CONDITIONAL_GROUP_MALFORMED ||
           status == NOC_CONDITIONAL_GROUP_MALFORMED_INCOMPLETE;
}

static bool noc__conditional_condition_status_is_valid(
    Noc_Conditional_Condition_Status status)
{
    return status == NOC_CONDITIONAL_CONDITION_NOT_APPLICABLE ||
           status == NOC_CONDITIONAL_CONDITION_EVALUATED ||
           status == NOC_CONDITIONAL_CONDITION_NOT_EVALUATED ||
           status == NOC_CONDITIONAL_CONDITION_MALFORMED ||
           status == NOC_CONDITIONAL_CONDITION_EXPANSION_FAILED ||
           status == NOC_CONDITIONAL_CONDITION_EVALUATION_FAILED;
}

static bool noc__conditional_issue_kind_is_valid(
    Noc_Conditional_Issue_Kind kind)
{
    return kind >= NOC_CONDITIONAL_ISSUE_UNMATCHED_ELIF &&
           kind <= NOC_CONDITIONAL_ISSUE_UNSUPPORTED_DIRECTIVE;
}

static bool noc__conditional_range_is_valid(
    size_t token_count,
    Noc_Token_Range range,
    bool allow_absent)
{
    if (allow_absent && range.begin == NOC_TOKEN_INDEX_NONE &&
        range.end == NOC_TOKEN_INDEX_NONE) {
        return true;
    }
    return range.begin <= range.end && range.end <= token_count;
}

static bool noc__conditional_grow(void **items,
                                  size_t *capacity,
                                  size_t item_size)
{
    size_t grown_capacity;
    void *grown;
    if (*capacity == 0) {
        grown_capacity = 8;
    } else {
        if (*capacity > SIZE_MAX / 2) return false;
        grown_capacity = *capacity * 2;
    }
    if (grown_capacity > SIZE_MAX / item_size) return false;
    grown = realloc(*items, grown_capacity * item_size);
    if (!grown) return false;
    *items = grown;
    *capacity = grown_capacity;
    return true;
}

static bool noc__conditional_append_group(
    Noc_Preprocessor_Conditional_Groups *result,
    Noc_Preprocessor_Conditional_Group group,
    size_t *index)
{
    if (result->group_count == result->group_capacity &&
        !noc__conditional_grow((void **)&result->groups,
                               &result->group_capacity,
                               sizeof(*result->groups))) {
        return false;
    }
    *index = result->group_count;
    result->groups[result->group_count++] = group;
    return true;
}

static bool noc__conditional_append_branch(
    Noc_Preprocessor_Conditional_Groups *result,
    Noc_Preprocessor_Conditional_Branch branch,
    size_t *index)
{
    if (result->branch_count == result->branch_capacity &&
        !noc__conditional_grow((void **)&result->branches,
                               &result->branch_capacity,
                               sizeof(*result->branches))) {
        return false;
    }
    *index = result->branch_count;
    result->branches[result->branch_count++] = branch;
    return true;
}

static bool noc__conditional_append_issue(
    Noc_Preprocessor_Conditional_Groups *result,
    Noc_Conditional_Issue_Kind kind,
    size_t directive_index,
    size_t group_index,
    size_t branch_index,
    size_t problem_token_index)
{
    Noc_Preprocessor_Conditional_Issue issue;
    if (result->issue_count == result->issue_capacity &&
        !noc__conditional_grow((void **)&result->issues,
                               &result->issue_capacity,
                               sizeof(*result->issues))) {
        return false;
    }
    issue.kind = kind;
    issue.directive_index = directive_index;
    issue.group_index = group_index;
    issue.branch_index = branch_index;
    issue.problem_token_index = problem_token_index;
    result->issues[result->issue_count++] = issue;
    return true;
}

static void noc__conditional_fill_tokens(
    Noc_Preprocessor_Conditional_Groups *result,
    size_t begin,
    size_t end,
    Noc_Preprocessor_Activity activity,
    bool macro_state_complete)
{
    size_t macro_entry_limit = macro_state_complete ?
                                   result->environment.count :
                                   NOC_TOKEN_INDEX_NONE;
    size_t index;
    for (index = begin; index < end; ++index) {
        result->token_activities[index] = activity;
        result->token_macro_entry_limits[index] = macro_entry_limit;
    }
}

static Noc_Token_Range noc__conditional_absent_range(void)
{
    Noc_Token_Range result = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    return result;
}

static size_t noc__conditional_first_body_token(
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index)
{
    Noc_Token_Range body =
        noc_preprocessor_directive_body_tokens(unit, directive_index);
    return body.begin;
}

static void noc__conditional_branch_init(
    Noc_Preprocessor_Conditional_Branch *branch,
    size_t group_index,
    size_t previous_branch_index,
    size_t directive_index,
    Noc_Preprocessor_Directive_Kind directive_kind,
    Noc_Token_Range condition_tokens,
    size_t content_begin)
{
    memset(branch, 0, sizeof(*branch));
    branch->group_index = group_index;
    branch->previous_branch_index = previous_branch_index;
    branch->next_branch_index = NOC_TOKEN_INDEX_NONE;
    branch->directive_index = directive_index;
    branch->directive_kind = directive_kind;
    branch->condition_tokens = condition_tokens;
    branch->content_tokens.begin = content_begin;
    branch->content_tokens.end = content_begin;
    branch->condition_environment_entry_limit = NOC_TOKEN_INDEX_NONE;
    branch->condition_activity = NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
    branch->content_activity = NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
    branch->condition_status = NOC_CONDITIONAL_CONDITION_NOT_EVALUATED;
    branch->expansion_status = NOC_MACRO_EXPANSION_OK;
    branch->expression_status = NOC_PREPROCESSOR_EXPRESSION_OK;
    branch->problem_token_index = NOC_TOKEN_INDEX_NONE;
}

static void noc__conditional_set_direct_problem(
    Noc_Preprocessor_Conditional_Branch *branch,
    const Noc_Preprocessor_Unit *unit,
    size_t token_index)
{
    if (token_index == NOC_TOKEN_INDEX_NONE) return;
    branch->problem_unit = unit;
    branch->problem_unit_stream_generation = unit->stream.generation;
    branch->problem_token_index = token_index;
}

static void noc__conditional_set_expansion_problem(
    Noc_Preprocessor_Conditional_Branch *branch,
    const Noc_Macro_Expansion *expansion,
    size_t expansion_token_index)
{
    const Noc_Macro_Expansion_Token *problem;
    if (expansion_token_index == NOC_TOKEN_INDEX_NONE ||
        expansion_token_index >= expansion->count) {
        return;
    }
    problem = &expansion->items[expansion_token_index];
    branch->problem_unit = problem->unit;
    branch->problem_unit_stream_generation =
        problem->unit_stream_generation;
    branch->problem_token_index = problem->preprocessing_token_index;
}

static Noc_Conditional_Groups_Build_Status noc__conditional_evaluate_expression(
    Noc_Preprocessor_Conditional_Groups *result,
    Noc_Preprocessor_Conditional_Branch *branch,
    Noc_Macro_Expansion_Options options)
{
    Noc_Macro_Expansion expansion = {0};
    Noc_Macro_Expansion_Status expansion_status;
    Noc_Preprocessor_Expression_Status expression_status;
    bool value = false;
    size_t problem_token_index = NOC_TOKEN_INDEX_NONE;

    expansion_status = noc_macro_expansion_build_condition_with_options(
        &result->environment,
        branch->condition_environment_entry_limit,
        result->unit,
        branch->condition_tokens,
        options,
        &expansion);
    branch->expansion_status = expansion_status;
    if (expansion_status != NOC_MACRO_EXPANSION_OK) {
        if (expansion_status == NOC_MACRO_EXPANSION_INVALID_ARGUMENT) {
            return NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT;
        }
        if (expansion_status == NOC_MACRO_EXPANSION_STALE) {
            return NOC_CONDITIONAL_GROUPS_STALE;
        }
        if (expansion_status == NOC_MACRO_EXPANSION_GENERATION_EXHAUSTED) {
            return NOC_CONDITIONAL_GROUPS_GENERATION_EXHAUSTED;
        }
        if (expansion_status == NOC_MACRO_EXPANSION_OUT_OF_MEMORY) {
            return NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
        }
        branch->condition_status =
            NOC_CONDITIONAL_CONDITION_EXPANSION_FAILED;
        noc__conditional_set_direct_problem(branch,
                                            result->unit,
                                            branch->condition_tokens.begin);
        return NOC_CONDITIONAL_GROUPS_OK;
    }

    expression_status = noc_preprocessor_expression_evaluate(
        &expansion,
        &value,
        &problem_token_index);
    branch->expression_status = expression_status;
    if (expression_status == NOC_PREPROCESSOR_EXPRESSION_INVALID_ARGUMENT) {
        noc_macro_expansion_free(&expansion);
        return NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT;
    }
    if (expression_status == NOC_PREPROCESSOR_EXPRESSION_STALE) {
        noc_macro_expansion_free(&expansion);
        return NOC_CONDITIONAL_GROUPS_STALE;
    }
    if (expression_status == NOC_PREPROCESSOR_EXPRESSION_OUT_OF_MEMORY) {
        noc_macro_expansion_free(&expansion);
        return NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
    }
    if (expression_status == NOC_PREPROCESSOR_EXPRESSION_OK) {
        branch->condition_status = NOC_CONDITIONAL_CONDITION_EVALUATED;
        branch->condition_activity = value ?
                                         NOC_PREPROCESSOR_ACTIVITY_ACTIVE :
                                         NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    } else {
        branch->condition_status =
            NOC_CONDITIONAL_CONDITION_EVALUATION_FAILED;
        noc__conditional_set_expansion_problem(branch,
                                               &expansion,
                                               problem_token_index);
    }
    noc_macro_expansion_free(&expansion);
    return NOC_CONDITIONAL_GROUPS_OK;
}

static void noc__conditional_evaluate_defined(
    Noc_Preprocessor_Conditional_Groups *result,
    Noc_Preprocessor_Conditional_Branch *branch,
    uint32_t available_builtin_mask,
    bool negate)
{
    Noc_Token_Range body = branch->condition_tokens;
    size_t identifier_index = NOC_TOKEN_INDEX_NONE;
    size_t significant_count = 0;
    size_t index;
    bool defined;

    if (body.begin == NOC_TOKEN_INDEX_NONE) {
        branch->condition_status = NOC_CONDITIONAL_CONDITION_MALFORMED;
        return;
    }
    for (index = body.begin; index < body.end; ++index) {
        Noc_Token token = result->unit->preprocessing_tokens[index].token;
        if (noc_token_is_trivia(token)) continue;
        if (significant_count == 0) identifier_index = index;
        significant_count += 1;
    }
    if (significant_count != 1 ||
        result->unit->preprocessing_tokens[identifier_index].token.kind !=
            NOC_TOKEN_IDENTIFIER) {
        branch->condition_status = NOC_CONDITIONAL_CONDITION_MALFORMED;
        noc__conditional_set_direct_problem(branch,
                                            result->unit,
                                            identifier_index);
        return;
    }

    defined = noc_macro_environment_lookup_before(
                  &result->environment,
                  result->unit->preprocessing_tokens[identifier_index].token.text,
                  branch->condition_environment_entry_limit) != NULL ||
              noc__macro_builtin_mask_contains(
                  available_builtin_mask,
                  noc_macro_builtin_kind_from_name(
                      result->unit->preprocessing_tokens[
                          identifier_index].token.text));
    branch->condition_status = NOC_CONDITIONAL_CONDITION_EVALUATED;
    branch->condition_activity = defined != negate ?
                                     NOC_PREPROCESSOR_ACTIVITY_ACTIVE :
                                     NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
}

static bool noc__conditional_record_unresolved(
    Noc_Preprocessor_Conditional_Groups *result,
    const Noc_Preprocessor_Conditional_Branch *branch,
    size_t branch_index)
{
    size_t problem_token_index = NOC_TOKEN_INDEX_NONE;
    if (branch->problem_unit == result->unit) {
        problem_token_index = branch->problem_token_index;
    }
    return noc__conditional_append_issue(
        result,
        NOC_CONDITIONAL_ISSUE_UNRESOLVED_CONDITION,
        branch->directive_index,
        branch->group_index,
        branch_index,
        problem_token_index);
}

static bool noc__conditional_branch_is_unresolved(
    const Noc_Preprocessor_Conditional_Branch *branch)
{
    return branch->condition_status == NOC_CONDITIONAL_CONDITION_MALFORMED ||
           branch->condition_status ==
               NOC_CONDITIONAL_CONDITION_EXPANSION_FAILED ||
           branch->condition_status ==
               NOC_CONDITIONAL_CONDITION_EVALUATION_FAILED;
}

NOCDEF const char *noc_conditional_group_status_name(
    Noc_Conditional_Group_Status status)
{
    switch (status) {
    case NOC_CONDITIONAL_GROUP_COMPLETE: return "complete";
    case NOC_CONDITIONAL_GROUP_INCOMPLETE: return "incomplete";
    case NOC_CONDITIONAL_GROUP_MALFORMED: return "malformed";
    case NOC_CONDITIONAL_GROUP_MALFORMED_INCOMPLETE:
        return "malformed-incomplete";
    }
    return "unknown";
}

NOCDEF const char *noc_conditional_condition_status_name(
    Noc_Conditional_Condition_Status status)
{
    switch (status) {
    case NOC_CONDITIONAL_CONDITION_NOT_APPLICABLE: return "not-applicable";
    case NOC_CONDITIONAL_CONDITION_EVALUATED: return "evaluated";
    case NOC_CONDITIONAL_CONDITION_NOT_EVALUATED: return "not-evaluated";
    case NOC_CONDITIONAL_CONDITION_MALFORMED: return "malformed";
    case NOC_CONDITIONAL_CONDITION_EXPANSION_FAILED:
        return "expansion-failed";
    case NOC_CONDITIONAL_CONDITION_EVALUATION_FAILED:
        return "evaluation-failed";
    }
    return "unknown";
}

NOCDEF const char *noc_conditional_issue_kind_name(
    Noc_Conditional_Issue_Kind kind)
{
    switch (kind) {
    case NOC_CONDITIONAL_ISSUE_UNMATCHED_ELIF: return "unmatched-elif";
    case NOC_CONDITIONAL_ISSUE_UNMATCHED_ELSE: return "unmatched-else";
    case NOC_CONDITIONAL_ISSUE_UNMATCHED_ENDIF: return "unmatched-endif";
    case NOC_CONDITIONAL_ISSUE_ELIF_AFTER_ELSE: return "elif-after-else";
    case NOC_CONDITIONAL_ISSUE_DUPLICATE_ELSE: return "duplicate-else";
    case NOC_CONDITIONAL_ISSUE_MISSING_ENDIF: return "missing-endif";
    case NOC_CONDITIONAL_ISSUE_UNEXPECTED_TOKENS:
        return "unexpected-tokens";
    case NOC_CONDITIONAL_ISSUE_UNRESOLVED_CONDITION:
        return "unresolved-condition";
    case NOC_CONDITIONAL_ISSUE_UNSUPPORTED_DIRECTIVE:
        return "unsupported-directive";
    }
    return "unknown";
}

NOCDEF const char *noc_conditional_groups_build_status_name(
    Noc_Conditional_Groups_Build_Status status)
{
    switch (status) {
    case NOC_CONDITIONAL_GROUPS_OK: return "ok";
    case NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_CONDITIONAL_GROUPS_STALE: return "stale";
    case NOC_CONDITIONAL_GROUPS_GENERATION_EXHAUSTED:
        return "generation-exhausted";
    case NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY: return "out-of-memory";
    }
    return "unknown";
}

NOCDEF void noc_preprocessor_conditional_groups_free(
    Noc_Preprocessor_Conditional_Groups *groups)
{
    size_t epoch;
    if (!groups) return;
    epoch = groups->generation > groups->environment.generation ?
                groups->generation :
                groups->environment.generation;
    free(groups->groups);
    free(groups->branches);
    free(groups->issues);
    free(groups->directive_owned_groups);
    free(groups->directive_introduced_branches);
    free(groups->token_activities);
    free(groups->token_macro_entry_limits);
    free(groups->environment.items);
    memset(groups, 0, sizeof(*groups));
    if (epoch == 0) return;
    groups->generation = epoch == SIZE_MAX ? SIZE_MAX : epoch + 1;
    groups->environment.generation = groups->generation;
    if (epoch == SIZE_MAX) {
        /* No distinct generation remains. Keep the inline environment invalid
           so an expansion borrowing the final published epoch cannot revive. */
        groups->environment.count = 1;
    }
}

NOCDEF bool noc_preprocessor_conditional_groups_is_valid(
    const Noc_Preprocessor_Conditional_Groups *groups)
{
    size_t index;
    if (!groups || groups->generation == 0 ||
        !noc_preprocessor_unit_is_valid(groups->unit) ||
        groups->unit_stream_generation != groups->unit->stream.generation ||
        groups->directive_count != groups->unit->count ||
        groups->preprocessing_token_count !=
            groups->unit->preprocessing_token_count ||
        groups->group_count > groups->group_capacity ||
        ((groups->group_capacity == 0) != (groups->groups == NULL)) ||
        groups->branch_count > groups->branch_capacity ||
        ((groups->branch_capacity == 0) != (groups->branches == NULL)) ||
        groups->issue_count > groups->issue_capacity ||
        ((groups->issue_capacity == 0) != (groups->issues == NULL)) ||
        (groups->directive_count > 0 &&
         (!groups->directive_owned_groups ||
          !groups->directive_introduced_branches)) ||
        !groups->token_activities || !groups->token_macro_entry_limits ||
        !noc_macro_environment_is_valid(&groups->environment) ||
        groups->published_environment_generation !=
            groups->environment.generation ||
        groups->published_environment_count != groups->environment.count ||
        groups->generation != groups->environment.generation) {
        return false;
    }
    for (index = 0; index < groups->group_count; ++index) {
        const Noc_Preprocessor_Conditional_Group *group =
            &groups->groups[index];
        if ((group->parent_branch_index != NOC_TOKEN_INDEX_NONE &&
             group->parent_branch_index >= groups->branch_count) ||
            group->opener_directive_index >= groups->directive_count ||
            (group->closer_directive_index != NOC_TOKEN_INDEX_NONE &&
             group->closer_directive_index >= groups->directive_count) ||
            group->first_branch_index >= groups->branch_count ||
            group->last_branch_index >= groups->branch_count ||
            !noc__conditional_range_is_valid(
                groups->preprocessing_token_count,
                group->preprocessing_tokens,
                false) ||
            !noc__conditional_group_status_is_valid(group->status)) {
            return false;
        }
    }
    for (index = 0; index < groups->branch_count; ++index) {
        const Noc_Preprocessor_Conditional_Branch *branch =
            &groups->branches[index];
        if (branch->group_index >= groups->group_count ||
            (branch->previous_branch_index != NOC_TOKEN_INDEX_NONE &&
             branch->previous_branch_index >= index) ||
            (branch->next_branch_index != NOC_TOKEN_INDEX_NONE &&
             branch->next_branch_index <= index) ||
            branch->directive_index >= groups->directive_count ||
            !noc__conditional_range_is_valid(
                groups->preprocessing_token_count,
                branch->condition_tokens,
                true) ||
            !noc__conditional_range_is_valid(
                groups->preprocessing_token_count,
                branch->content_tokens,
                false) ||
            (branch->condition_environment_entry_limit != NOC_TOKEN_INDEX_NONE &&
             branch->condition_environment_entry_limit >
                 groups->environment.count) ||
            !noc__conditional_activity_is_valid(branch->condition_activity) ||
            !noc__conditional_activity_is_valid(branch->content_activity) ||
            !noc__conditional_condition_status_is_valid(
                branch->condition_status)) {
            return false;
        }
        if (branch->problem_unit) {
            if (!noc_preprocessor_unit_is_valid(branch->problem_unit) ||
                branch->problem_unit_stream_generation !=
                    branch->problem_unit->stream.generation ||
                branch->problem_token_index >=
                    branch->problem_unit->preprocessing_token_count) {
                return false;
            }
        } else if (branch->problem_unit_stream_generation != 0 ||
                   branch->problem_token_index != NOC_TOKEN_INDEX_NONE) {
            return false;
        }
    }
    for (index = 0; index < groups->issue_count; ++index) {
        const Noc_Preprocessor_Conditional_Issue *issue = &groups->issues[index];
        if (!noc__conditional_issue_kind_is_valid(issue->kind) ||
            issue->directive_index >= groups->directive_count ||
            (issue->group_index != NOC_TOKEN_INDEX_NONE &&
             issue->group_index >= groups->group_count) ||
            (issue->branch_index != NOC_TOKEN_INDEX_NONE &&
             issue->branch_index >= groups->branch_count) ||
            (issue->problem_token_index != NOC_TOKEN_INDEX_NONE &&
             issue->problem_token_index >= groups->preprocessing_token_count)) {
            return false;
        }
    }
    for (index = 0; index < groups->directive_count; ++index) {
        if ((groups->directive_owned_groups[index] != NOC_TOKEN_INDEX_NONE &&
             groups->directive_owned_groups[index] >= groups->group_count) ||
            (groups->directive_introduced_branches[index] !=
                 NOC_TOKEN_INDEX_NONE &&
             groups->directive_introduced_branches[index] >=
                 groups->branch_count)) {
            return false;
        }
    }
    for (index = 0; index < groups->preprocessing_token_count; ++index) {
        if (!noc__conditional_activity_is_valid(groups->token_activities[index]) ||
            (groups->token_macro_entry_limits[index] != NOC_TOKEN_INDEX_NONE &&
             groups->token_macro_entry_limits[index] >
                 groups->environment.count)) {
            return false;
        }
    }
    return true;
}

NOCDEF bool noc_preprocessor_conditional_groups_is_fully_resolved(
    const Noc_Preprocessor_Conditional_Groups *groups)
{
    size_t index;
    if (!noc_preprocessor_conditional_groups_is_valid(groups) ||
        !groups->macro_state_complete || groups->issue_count != 0) {
        return false;
    }
    for (index = 0; index < groups->group_count; ++index) {
        if (groups->groups[index].status != NOC_CONDITIONAL_GROUP_COMPLETE) {
            return false;
        }
    }
    return true;
}

NOCDEF Noc_Conditional_Groups_Build_Status
noc_preprocessor_conditional_groups_build(
    const Noc_Macro_Environment *initial_environment,
    size_t initial_entry_limit,
    const Noc_Preprocessor_Unit *unit,
    Noc_Macro_Expansion_Limits limits,
    Noc_Preprocessor_Conditional_Groups *output)
{
    Noc_Macro_Expansion_Options options = noc_macro_expansion_default_options();
    options.limits = limits;
    return noc_preprocessor_conditional_groups_build_with_options(
        initial_environment,
        initial_entry_limit,
        unit,
        options,
        output);
}

NOCDEF Noc_Conditional_Groups_Build_Status
noc_preprocessor_conditional_groups_build_with_options(
    const Noc_Macro_Environment *initial_environment,
    size_t initial_entry_limit,
    const Noc_Preprocessor_Unit *unit,
    Noc_Macro_Expansion_Options options,
    Noc_Preprocessor_Conditional_Groups *output)
{
    Noc_Preprocessor_Conditional_Groups parsed = {0};
    Noc__Conditional_Frame *stack = NULL;
    size_t stack_count = 0;
    size_t stack_capacity = 0;
    size_t token_cursor = 0;
    size_t epoch;
    size_t directive_index;
    Noc_Preprocessor_Activity current_activity =
        NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    bool macro_state_complete = true;
    Noc_Macro_Environment_Status clone_status;
    Noc_Conditional_Groups_Build_Status status = NOC_CONDITIONAL_GROUPS_OK;
    uint32_t available_builtin_mask;

    if (!initial_environment || !unit || !output ||
        initial_entry_limit > initial_environment->count ||
        !noc__macro_expansion_options_are_valid(options)) {
        return NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT;
    }
    if (!noc_macro_environment_is_valid(initial_environment) ||
        !noc_preprocessor_unit_is_valid(unit)) {
        return NOC_CONDITIONAL_GROUPS_STALE;
    }
    available_builtin_mask = noc__macro_builtin_mask_from_options(options);
    epoch = output->generation > output->environment.generation ?
                output->generation :
                output->environment.generation;
    if (epoch == SIZE_MAX) {
        return NOC_CONDITIONAL_GROUPS_GENERATION_EXHAUSTED;
    }
    clone_status = noc__macro_environment_clone_prefix(initial_environment,
                                                        initial_entry_limit,
                                                        epoch + 1,
                                                        &parsed.environment);
    if (clone_status == NOC_MACRO_ENVIRONMENT_STALE) {
        return NOC_CONDITIONAL_GROUPS_STALE;
    }
    if (clone_status == NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT) {
        return NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT;
    }
    if (clone_status != NOC_MACRO_ENVIRONMENT_OK) {
        return NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
    }

    parsed.unit = unit;
    parsed.unit_stream_generation = unit->stream.generation;
    parsed.directive_count = unit->count;
    parsed.preprocessing_token_count = unit->preprocessing_token_count;
    if (parsed.directive_count > SIZE_MAX / sizeof(size_t) ||
        parsed.preprocessing_token_count > SIZE_MAX / sizeof(size_t) ||
        parsed.preprocessing_token_count >
            SIZE_MAX / sizeof(*parsed.token_activities)) {
        status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
        goto failed;
    }
    if (parsed.directive_count > 0) {
        parsed.directive_owned_groups = (size_t *)malloc(
            parsed.directive_count * sizeof(*parsed.directive_owned_groups));
        parsed.directive_introduced_branches = (size_t *)malloc(
            parsed.directive_count *
            sizeof(*parsed.directive_introduced_branches));
        if (!parsed.directive_owned_groups ||
            !parsed.directive_introduced_branches) {
            status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
            goto failed;
        }
    }
    parsed.token_activities = (Noc_Preprocessor_Activity *)malloc(
        parsed.preprocessing_token_count * sizeof(*parsed.token_activities));
    parsed.token_macro_entry_limits = (size_t *)malloc(
        parsed.preprocessing_token_count *
        sizeof(*parsed.token_macro_entry_limits));
    if (!parsed.token_activities || !parsed.token_macro_entry_limits) {
        status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
        goto failed;
    }
    for (directive_index = 0;
         directive_index < parsed.directive_count;
         ++directive_index) {
        parsed.directive_owned_groups[directive_index] = NOC_TOKEN_INDEX_NONE;
        parsed.directive_introduced_branches[directive_index] =
            NOC_TOKEN_INDEX_NONE;
    }

    for (directive_index = 0;
         directive_index < unit->count;
         ++directive_index) {
        const Noc_Preprocessor_Directive *directive =
            &unit->items[directive_index];
        Noc_Preprocessor_Activity directive_activity = current_activity;

        if ((noc__conditional_is_peer(directive->kind) ||
             directive->kind == NOC_PREPROCESSOR_DIRECTIVE_ENDIF) &&
            stack_count > 0) {
            directive_activity = stack[stack_count - 1].parent_activity;
        }
        noc__conditional_fill_tokens(&parsed,
                                     token_cursor,
                                     directive->preprocessing_tokens.begin,
                                     current_activity,
                                     macro_state_complete);
        noc__conditional_fill_tokens(&parsed,
                                     directive->preprocessing_tokens.begin,
                                     directive->preprocessing_tokens.end,
                                     directive_activity,
                                     macro_state_complete);
        token_cursor = directive->preprocessing_tokens.end;

        if (noc__conditional_is_opener(directive->kind)) {
            Noc_Preprocessor_Conditional_Group group;
            Noc_Preprocessor_Conditional_Branch branch;
            Noc__Conditional_Frame frame;
            size_t group_index;
            size_t branch_index;

            memset(&group, 0, sizeof(group));
            group.parent_branch_index = stack_count > 0 ?
                                            stack[stack_count - 1].branch_index :
                                            NOC_TOKEN_INDEX_NONE;
            group.opener_directive_index = directive_index;
            group.closer_directive_index = NOC_TOKEN_INDEX_NONE;
            group.first_branch_index = NOC_TOKEN_INDEX_NONE;
            group.last_branch_index = NOC_TOKEN_INDEX_NONE;
            group.preprocessing_tokens.begin =
                directive->preprocessing_tokens.begin;
            group.preprocessing_tokens.end =
                directive->preprocessing_tokens.end;
            group.status = NOC_CONDITIONAL_GROUP_INCOMPLETE;
            if (!noc__conditional_append_group(&parsed, group, &group_index)) {
                status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                goto failed;
            }

            noc__conditional_branch_init(
                &branch,
                group_index,
                NOC_TOKEN_INDEX_NONE,
                directive_index,
                directive->kind,
                noc_preprocessor_directive_body_tokens(unit, directive_index),
                directive->preprocessing_tokens.end);
            if (current_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE &&
                macro_state_complete) {
                branch.condition_environment_entry_limit =
                    parsed.environment.count;
                if (directive->kind == NOC_PREPROCESSOR_DIRECTIVE_IF) {
                    if (branch.condition_tokens.begin == NOC_TOKEN_INDEX_NONE) {
                        branch.condition_status =
                            NOC_CONDITIONAL_CONDITION_MALFORMED;
                    } else {
                        status = noc__conditional_evaluate_expression(&parsed,
                                                                      &branch,
                                                                      options);
                        if (status != NOC_CONDITIONAL_GROUPS_OK) goto failed;
                    }
                } else {
                    noc__conditional_evaluate_defined(
                        &parsed,
                        &branch,
                        available_builtin_mask,
                        directive->kind == NOC_PREPROCESSOR_DIRECTIVE_IFNDEF);
                }
            }
            branch.content_activity = noc__conditional_and(
                current_activity,
                branch.condition_activity);
            if (!noc__conditional_append_branch(&parsed,
                                                branch,
                                                &branch_index)) {
                status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                goto failed;
            }
            parsed.groups[group_index].first_branch_index = branch_index;
            parsed.groups[group_index].last_branch_index = branch_index;
            parsed.directive_owned_groups[directive_index] = group_index;
            parsed.directive_introduced_branches[directive_index] = branch_index;

            if (stack_count == stack_capacity &&
                !noc__conditional_grow((void **)&stack,
                                       &stack_capacity,
                                       sizeof(*stack))) {
                status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                goto failed;
            }
            frame.group_index = group_index;
            frame.branch_index = branch_index;
            frame.parent_activity = current_activity;
            frame.prior_taken = branch.condition_activity;
            frame.saw_else = false;
            frame.malformed = false;
            stack[stack_count++] = frame;
            current_activity = branch.content_activity;
            if (noc__conditional_branch_is_unresolved(&branch) &&
                !noc__conditional_record_unresolved(&parsed,
                                                    &branch,
                                                    branch_index)) {
                status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                goto failed;
            }
            continue;
        }

        if (noc__conditional_is_peer(directive->kind)) {
            Noc__Conditional_Frame *frame;
            Noc_Preprocessor_Conditional_Branch branch;
            Noc_Preprocessor_Activity available;
            size_t branch_index;
            bool is_else =
                directive->kind == NOC_PREPROCESSOR_DIRECTIVE_ELSE;
            bool specific_issue_recorded = false;

            if (stack_count == 0) {
                if (!noc__conditional_append_issue(
                        &parsed,
                        is_else ? NOC_CONDITIONAL_ISSUE_UNMATCHED_ELSE :
                                  NOC_CONDITIONAL_ISSUE_UNMATCHED_ELIF,
                        directive_index,
                        NOC_TOKEN_INDEX_NONE,
                        NOC_TOKEN_INDEX_NONE,
                        NOC_TOKEN_INDEX_NONE)) {
                    status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                    goto failed;
                }
                continue;
            }

            frame = &stack[stack_count - 1];
            parsed.branches[frame->branch_index].content_tokens.end =
                directive->preprocessing_tokens.begin;
            noc__conditional_branch_init(
                &branch,
                frame->group_index,
                frame->branch_index,
                directive_index,
                directive->kind,
                is_else ? noc__conditional_absent_range() :
                          noc_preprocessor_directive_body_tokens(unit,
                                                                 directive_index),
                directive->preprocessing_tokens.end);
            available = noc__conditional_not(frame->prior_taken);

            if (frame->saw_else) {
                frame->malformed = true;
                branch.condition_status = NOC_CONDITIONAL_CONDITION_MALFORMED;
                if (!noc__conditional_append_issue(
                        &parsed,
                        is_else ? NOC_CONDITIONAL_ISSUE_DUPLICATE_ELSE :
                                  NOC_CONDITIONAL_ISSUE_ELIF_AFTER_ELSE,
                        directive_index,
                        frame->group_index,
                        parsed.branch_count,
                        NOC_TOKEN_INDEX_NONE)) {
                    status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                    goto failed;
                }
                specific_issue_recorded = true;
            } else if (is_else) {
                frame->saw_else = true;
                if (noc__conditional_first_body_token(unit, directive_index) !=
                    NOC_TOKEN_INDEX_NONE) {
                    size_t problem =
                        noc__conditional_first_body_token(unit, directive_index);
                    frame->malformed = true;
                    branch.condition_status =
                        NOC_CONDITIONAL_CONDITION_MALFORMED;
                    noc__conditional_set_direct_problem(&branch, unit, problem);
                    if (!noc__conditional_append_issue(
                            &parsed,
                            NOC_CONDITIONAL_ISSUE_UNEXPECTED_TOKENS,
                            directive_index,
                            frame->group_index,
                            parsed.branch_count,
                            problem)) {
                        status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                        goto failed;
                    }
                    specific_issue_recorded = true;
                } else {
                    branch.condition_status =
                        NOC_CONDITIONAL_CONDITION_NOT_APPLICABLE;
                    branch.condition_activity =
                        NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
                }
            } else if (directive->kind != NOC_PREPROCESSOR_DIRECTIVE_ELIF) {
                branch.condition_status = NOC_CONDITIONAL_CONDITION_MALFORMED;
                if (!noc__conditional_append_issue(
                        &parsed,
                        NOC_CONDITIONAL_ISSUE_UNSUPPORTED_DIRECTIVE,
                        directive_index,
                        frame->group_index,
                        parsed.branch_count,
                        noc__conditional_first_body_token(unit,
                                                          directive_index))) {
                    status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                    goto failed;
                }
                specific_issue_recorded = true;
            } else if (frame->parent_activity ==
                           NOC_PREPROCESSOR_ACTIVITY_ACTIVE &&
                       available == NOC_PREPROCESSOR_ACTIVITY_ACTIVE &&
                       macro_state_complete) {
                branch.condition_environment_entry_limit =
                    parsed.environment.count;
                if (branch.condition_tokens.begin == NOC_TOKEN_INDEX_NONE) {
                    branch.condition_status =
                        NOC_CONDITIONAL_CONDITION_MALFORMED;
                } else {
                    status = noc__conditional_evaluate_expression(&parsed,
                                                                  &branch,
                                                                  options);
                    if (status != NOC_CONDITIONAL_GROUPS_OK) goto failed;
                }
            }

            branch.content_activity = noc__conditional_and(
                frame->parent_activity,
                noc__conditional_and(available,
                                     branch.condition_activity));
            if (frame->malformed) {
                branch.content_activity = noc__conditional_and(
                    frame->parent_activity,
                    NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
            }
            if (!noc__conditional_append_branch(&parsed,
                                                branch,
                                                &branch_index)) {
                status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                goto failed;
            }
            parsed.branches[frame->branch_index].next_branch_index = branch_index;
            parsed.groups[frame->group_index].last_branch_index = branch_index;
            parsed.directive_owned_groups[directive_index] = frame->group_index;
            parsed.directive_introduced_branches[directive_index] = branch_index;
            frame->branch_index = branch_index;
            frame->prior_taken = frame->malformed ?
                                     NOC_PREPROCESSOR_ACTIVITY_UNKNOWN :
                                     noc__conditional_or(frame->prior_taken,
                                                         branch.condition_activity);
            current_activity = branch.content_activity;
            if (!specific_issue_recorded &&
                noc__conditional_branch_is_unresolved(&branch) &&
                !noc__conditional_record_unresolved(&parsed,
                                                    &branch,
                                                    branch_index)) {
                status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                goto failed;
            }
            continue;
        }

        if (directive->kind == NOC_PREPROCESSOR_DIRECTIVE_ENDIF) {
            Noc__Conditional_Frame frame;
            size_t problem;
            if (stack_count == 0) {
                if (!noc__conditional_append_issue(
                        &parsed,
                        NOC_CONDITIONAL_ISSUE_UNMATCHED_ENDIF,
                        directive_index,
                        NOC_TOKEN_INDEX_NONE,
                        NOC_TOKEN_INDEX_NONE,
                        NOC_TOKEN_INDEX_NONE)) {
                    status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                    goto failed;
                }
                continue;
            }

            frame = stack[--stack_count];
            parsed.branches[frame.branch_index].content_tokens.end =
                directive->preprocessing_tokens.begin;
            problem = noc__conditional_first_body_token(unit, directive_index);
            if (problem != NOC_TOKEN_INDEX_NONE) {
                frame.malformed = true;
                if (!noc__conditional_append_issue(
                        &parsed,
                        NOC_CONDITIONAL_ISSUE_UNEXPECTED_TOKENS,
                        directive_index,
                        frame.group_index,
                        frame.branch_index,
                        problem)) {
                    status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                    goto failed;
                }
            }
            parsed.groups[frame.group_index].closer_directive_index =
                directive_index;
            parsed.groups[frame.group_index].preprocessing_tokens.end =
                directive->preprocessing_tokens.end;
            parsed.groups[frame.group_index].status = frame.malformed ?
                NOC_CONDITIONAL_GROUP_MALFORMED :
                NOC_CONDITIONAL_GROUP_COMPLETE;
            parsed.directive_owned_groups[directive_index] = frame.group_index;
            current_activity = frame.parent_activity;
            continue;
        }

        if (directive->macro_directive_index != NOC_TOKEN_INDEX_NONE) {
            const Noc_Macro_Directive *macro =
                &unit->macro_directives[directive->macro_directive_index];
            if (macro->status == NOC_MACRO_DIRECTIVE_STATUS_VALID &&
                directive->macro_definition_allowed) {
                if (current_activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
                    Noc_Macro_Environment_Status apply_status =
                        noc_macro_environment_apply(
                            &parsed.environment,
                            unit,
                            directive->macro_directive_index);
                    if (apply_status ==
                        NOC_MACRO_ENVIRONMENT_GENERATION_EXHAUSTED) {
                        status = NOC_CONDITIONAL_GROUPS_GENERATION_EXHAUSTED;
                        goto failed;
                    }
                    if (apply_status == NOC_MACRO_ENVIRONMENT_OUT_OF_MEMORY) {
                        status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
                        goto failed;
                    }
                    if (apply_status == NOC_MACRO_ENVIRONMENT_STALE) {
                        status = NOC_CONDITIONAL_GROUPS_STALE;
                        goto failed;
                    }
                    if (apply_status != NOC_MACRO_ENVIRONMENT_OK) {
                        status = NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT;
                        goto failed;
                    }
                } else if (current_activity ==
                           NOC_PREPROCESSOR_ACTIVITY_UNKNOWN) {
                    macro_state_complete = false;
                }
            }
        }
    }

    /* Preserve the active editor prefix at EOF before structurally closing
       incomplete frames for recovery metadata. */
    noc__conditional_fill_tokens(&parsed,
                                 token_cursor,
                                 parsed.preprocessing_token_count,
                                 current_activity,
                                 macro_state_complete);
    while (stack_count > 0) {
        Noc__Conditional_Frame frame = stack[--stack_count];
        size_t eof_index = parsed.preprocessing_token_count - 1;
        parsed.branches[frame.branch_index].content_tokens.end = eof_index;
        parsed.groups[frame.group_index].preprocessing_tokens.end = eof_index;
        parsed.groups[frame.group_index].status = frame.malformed ?
            NOC_CONDITIONAL_GROUP_MALFORMED_INCOMPLETE :
            NOC_CONDITIONAL_GROUP_INCOMPLETE;
        if (!noc__conditional_append_issue(
                &parsed,
                NOC_CONDITIONAL_ISSUE_MISSING_ENDIF,
                parsed.groups[frame.group_index].opener_directive_index,
                frame.group_index,
                frame.branch_index,
                NOC_TOKEN_INDEX_NONE)) {
            status = NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY;
            goto failed;
        }
    }

    free(stack);
    parsed.macro_state_complete = macro_state_complete;
    parsed.generation = parsed.environment.generation;
    parsed.published_environment_generation = parsed.environment.generation;
    parsed.published_environment_count = parsed.environment.count;
    noc_preprocessor_conditional_groups_free(output);
    *output = parsed;
    return NOC_CONDITIONAL_GROUPS_OK;

failed:
    free(stack);
    noc_preprocessor_conditional_groups_free(&parsed);
    return status;
}

NOCDEF const Noc_Preprocessor_Conditional_Group *
noc_preprocessor_conditional_group_at(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t index)
{
    if (!noc_preprocessor_conditional_groups_is_valid(groups) ||
        index >= groups->group_count) {
        return NULL;
    }
    return &groups->groups[index];
}

NOCDEF const Noc_Preprocessor_Conditional_Branch *
noc_preprocessor_conditional_branch_at(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t index)
{
    if (!noc_preprocessor_conditional_groups_is_valid(groups) ||
        index >= groups->branch_count) {
        return NULL;
    }
    return &groups->branches[index];
}

NOCDEF const Noc_Preprocessor_Conditional_Issue *
noc_preprocessor_conditional_issue_at(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t index)
{
    if (!noc_preprocessor_conditional_groups_is_valid(groups) ||
        index >= groups->issue_count) {
        return NULL;
    }
    return &groups->issues[index];
}

NOCDEF size_t noc_preprocessor_conditional_owned_group(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t directive_index)
{
    if (!noc_preprocessor_conditional_groups_is_valid(groups) ||
        directive_index >= groups->directive_count) {
        return NOC_TOKEN_INDEX_NONE;
    }
    return groups->directive_owned_groups[directive_index];
}

NOCDEF size_t noc_preprocessor_conditional_introduced_branch(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t directive_index)
{
    if (!noc_preprocessor_conditional_groups_is_valid(groups) ||
        directive_index >= groups->directive_count) {
        return NOC_TOKEN_INDEX_NONE;
    }
    return groups->directive_introduced_branches[directive_index];
}

NOCDEF Noc_Preprocessor_Activity noc_preprocessor_conditional_token_activity(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t preprocessing_token_index)
{
    if (!noc_preprocessor_conditional_groups_is_valid(groups) ||
        preprocessing_token_index >= groups->preprocessing_token_count) {
        return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
    }
    return groups->token_activities[preprocessing_token_index];
}

NOCDEF size_t noc_preprocessor_conditional_token_macro_entry_limit(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t preprocessing_token_index)
{
    if (!noc_preprocessor_conditional_groups_is_valid(groups) ||
        preprocessing_token_index >= groups->preprocessing_token_count) {
        return NOC_TOKEN_INDEX_NONE;
    }
    return groups->token_macro_entry_limits[preprocessing_token_index];
}

NOCDEF const Noc_Macro_Environment *noc_preprocessor_conditional_environment(
    const Noc_Preprocessor_Conditional_Groups *groups)
{
    if (!noc_preprocessor_conditional_groups_is_valid(groups)) return NULL;
    return &groups->environment;
}

#endif /* NOC_CONDITIONAL_GROUPS_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
