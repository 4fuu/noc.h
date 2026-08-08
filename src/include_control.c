#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_INCLUDE_CONTROL_IMPLEMENTATION_INCLUDED
#define NOC_INCLUDE_CONTROL_IMPLEMENTATION_INCLUDED

NOCDEF const char *noc_pragma_once_status_name(Noc_Pragma_Once_Status status)
{
    switch (status) {
    case NOC_PRAGMA_ONCE_VALID: return "valid";
    case NOC_PRAGMA_ONCE_OTHER: return "other";
    case NOC_PRAGMA_ONCE_MISSING: return "missing";
    case NOC_PRAGMA_ONCE_MALFORMED: return "malformed";
    case NOC_PRAGMA_ONCE_INCOMPLETE: return "incomplete";
    }
    return "unknown";
}

NOCDEF const char *noc_include_guard_status_name(Noc_Include_Guard_Status status)
{
    switch (status) {
    case NOC_INCLUDE_GUARD_NONE: return "none";
    case NOC_INCLUDE_GUARD_CANONICAL: return "canonical";
    case NOC_INCLUDE_GUARD_INCOMPLETE: return "incomplete";
    case NOC_INCLUDE_GUARD_MALFORMED: return "malformed";
    case NOC_INCLUDE_GUARD_MISSING_DEFINE: return "missing-define";
    case NOC_INCLUDE_GUARD_NAME_MISMATCH: return "name-mismatch";
    case NOC_INCLUDE_GUARD_HAS_PEER_BRANCH: return "has-peer-branch";
    case NOC_INCLUDE_GUARD_NOT_FILE_ENCLOSING: return "not-file-enclosing";
    }
    return "unknown";
}

NOCDEF const char *noc_include_control_build_status_name(
    Noc_Include_Control_Build_Status status)
{
    switch (status) {
    case NOC_INCLUDE_CONTROL_BUILD_OK: return "ok";
    case NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_INCLUDE_CONTROL_BUILD_STALE: return "stale";
    case NOC_INCLUDE_CONTROL_BUILD_GENERATION_EXHAUSTED:
        return "generation-exhausted";
    }
    return "unknown";
}

static size_t noc__include_control_next_significant(
    const Noc_Preprocessor_Unit *unit,
    size_t begin,
    size_t end)
{
    size_t index;
    for (index = begin; index < end; ++index) {
        Noc_Token token = unit->preprocessing_tokens[index].token;
        if (!noc_token_is_trivia(token) && token.kind != NOC_TOKEN_EOF) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static size_t noc__include_control_last_significant(
    const Noc_Preprocessor_Unit *unit,
    size_t begin,
    size_t end)
{
    size_t index = end;
    while (index > begin) {
        Noc_Token token = unit->preprocessing_tokens[--index].token;
        if (!noc_token_is_trivia(token) && token.kind != NOC_TOKEN_EOF) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static bool noc__include_control_index_is_valid(size_t index, size_t count)
{
    return index == NOC_TOKEN_INDEX_NONE || index < count;
}

NOCDEF bool noc_pragma_once_is_valid(const Noc_Pragma_Once *pragma_once)
{
    Noc_Token_Range body;
    size_t first;
    size_t next;
    size_t invalid = NOC_TOKEN_INDEX_NONE;
    size_t index;
    if (!pragma_once || pragma_once->generation == 0 ||
        !noc_preprocessor_unit_is_valid(pragma_once->unit) ||
        pragma_once->unit_stream_generation !=
            pragma_once->unit->stream.generation ||
        pragma_once->directive_index >= pragma_once->unit->count ||
        pragma_once->unit->items[pragma_once->directive_index].kind !=
            NOC_PREPROCESSOR_DIRECTIVE_PRAGMA ||
        pragma_once->status < NOC_PRAGMA_ONCE_VALID ||
        pragma_once->status > NOC_PRAGMA_ONCE_INCOMPLETE ||
        !noc__include_control_index_is_valid(
            pragma_once->once_token_index,
            pragma_once->unit->preprocessing_token_count) ||
        !noc__include_control_index_is_valid(
            pragma_once->problem_token_index,
            pragma_once->unit->preprocessing_token_count)) {
        return false;
    }
    body = noc_preprocessor_directive_body_tokens(pragma_once->unit,
                                                  pragma_once->directive_index);
    if (body.begin != pragma_once->body_tokens.begin ||
        body.end != pragma_once->body_tokens.end) {
        return false;
    }
    if (pragma_once->once_token_index != NOC_TOKEN_INDEX_NONE &&
        (body.begin == NOC_TOKEN_INDEX_NONE ||
         pragma_once->once_token_index < body.begin ||
         pragma_once->once_token_index >= body.end)) {
        return false;
    }
    if (pragma_once->problem_token_index != NOC_TOKEN_INDEX_NONE &&
        (body.begin == NOC_TOKEN_INDEX_NONE ||
         pragma_once->problem_token_index < body.begin ||
         pragma_once->problem_token_index >= body.end)) {
        return false;
    }
    first = body.begin == NOC_TOKEN_INDEX_NONE ? NOC_TOKEN_INDEX_NONE :
        noc__include_control_next_significant(pragma_once->unit,
                                              body.begin,
                                              body.end);
    if (body.begin != NOC_TOKEN_INDEX_NONE) {
        for (index = body.begin; index < body.end; ++index) {
            if (pragma_once->unit->preprocessing_tokens[index].token.kind ==
                NOC_TOKEN_INVALID) {
                invalid = index;
                break;
            }
        }
    }
    if (pragma_once->status == NOC_PRAGMA_ONCE_VALID) {
        return first != NOC_TOKEN_INDEX_NONE &&
               invalid == NOC_TOKEN_INDEX_NONE &&
               pragma_once->once_token_index == first &&
               noc_token_is_identifier(
                   pragma_once->unit->preprocessing_tokens[first].token,
                   "once") &&
               noc__include_control_next_significant(pragma_once->unit,
                                                      first + 1,
                                                      body.end) ==
                   NOC_TOKEN_INDEX_NONE &&
               pragma_once->problem_token_index == NOC_TOKEN_INDEX_NONE;
    }
    if (pragma_once->status == NOC_PRAGMA_ONCE_MISSING) {
        return first == NOC_TOKEN_INDEX_NONE &&
               pragma_once->once_token_index == NOC_TOKEN_INDEX_NONE &&
               pragma_once->problem_token_index == NOC_TOKEN_INDEX_NONE;
    }
    if (pragma_once->status == NOC_PRAGMA_ONCE_OTHER) {
        return first != NOC_TOKEN_INDEX_NONE &&
               invalid == NOC_TOKEN_INDEX_NONE &&
               !noc_token_is_identifier(
                   pragma_once->unit->preprocessing_tokens[first].token,
                   "once") &&
               pragma_once->once_token_index == NOC_TOKEN_INDEX_NONE &&
               pragma_once->problem_token_index == NOC_TOKEN_INDEX_NONE;
    }
    if (pragma_once->status == NOC_PRAGMA_ONCE_INCOMPLETE) {
        return invalid != NOC_TOKEN_INDEX_NONE &&
               pragma_once->problem_token_index == invalid &&
               pragma_once->once_token_index == NOC_TOKEN_INDEX_NONE &&
               first != NOC_TOKEN_INDEX_NONE;
    }
    if (first == NOC_TOKEN_INDEX_NONE ||
        !noc_token_is_identifier(
            pragma_once->unit->preprocessing_tokens[first].token,
            "once")) {
        return false;
    }
    next = noc__include_control_next_significant(pragma_once->unit,
                                                 first + 1,
                                                 body.end);
    return invalid == NOC_TOKEN_INDEX_NONE &&
           pragma_once->once_token_index == first &&
           next != NOC_TOKEN_INDEX_NONE &&
           pragma_once->problem_token_index == next;
}

NOCDEF Noc_Include_Control_Build_Status noc_pragma_once_build(
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index,
    Noc_Pragma_Once *output)
{
    Noc_Pragma_Once parsed;
    Noc_Token_Range body;
    size_t first;
    size_t next;
    size_t invalid;
    size_t generation;
    if (!unit || !output) return NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT;
    if (!noc_preprocessor_unit_is_valid(unit)) {
        return NOC_INCLUDE_CONTROL_BUILD_STALE;
    }
    if (directive_index >= unit->count ||
        unit->items[directive_index].kind != NOC_PREPROCESSOR_DIRECTIVE_PRAGMA) {
        return NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_INCLUDE_CONTROL_BUILD_GENERATION_EXHAUSTED;
    }
    memset(&parsed, 0, sizeof(parsed));
    parsed.unit = unit;
    parsed.unit_stream_generation = unit->stream.generation;
    parsed.directive_index = directive_index;
    parsed.once_token_index = NOC_TOKEN_INDEX_NONE;
    parsed.problem_token_index = NOC_TOKEN_INDEX_NONE;
    body = noc_preprocessor_directive_body_tokens(unit, directive_index);
    parsed.body_tokens = body;
    if (body.begin == NOC_TOKEN_INDEX_NONE) {
        parsed.status = NOC_PRAGMA_ONCE_MISSING;
    } else {
        first = noc__include_control_next_significant(unit,
                                                      body.begin,
                                                      body.end);
        if (first == NOC_TOKEN_INDEX_NONE) {
            parsed.status = NOC_PRAGMA_ONCE_MISSING;
        } else {
            invalid = first;
            while (invalid < body.end &&
                   unit->preprocessing_tokens[invalid].token.kind !=
                       NOC_TOKEN_INVALID) {
                invalid += 1;
            }
            if (invalid < body.end) {
                parsed.status = NOC_PRAGMA_ONCE_INCOMPLETE;
                parsed.problem_token_index = invalid;
            } else if (!noc_token_is_identifier(
                           unit->preprocessing_tokens[first].token,
                           "once")) {
                parsed.status = NOC_PRAGMA_ONCE_OTHER;
            } else {
                parsed.once_token_index = first;
                next = noc__include_control_next_significant(unit,
                                                             first + 1,
                                                             body.end);
                if (next == NOC_TOKEN_INDEX_NONE) {
                    parsed.status = NOC_PRAGMA_ONCE_VALID;
                } else {
                    parsed.status = NOC_PRAGMA_ONCE_MALFORMED;
                    parsed.problem_token_index = next;
                }
            }
        }
    }
    generation = output->generation + 1;
    parsed.generation = generation;
    *output = parsed;
    return NOC_INCLUDE_CONTROL_BUILD_OK;
}

static void noc__include_guard_init(Noc_Include_Guard *guard,
                                    const Noc_Preprocessor_Unit *unit,
                                    const Noc_Preprocessor_Conditional_Groups *groups)
{
    memset(guard, 0, sizeof(*guard));
    guard->unit = unit;
    guard->unit_stream_generation = unit->stream.generation;
    guard->groups = groups;
    guard->groups_generation = groups->generation;
    guard->group_index = NOC_TOKEN_INDEX_NONE;
    guard->branch_index = NOC_TOKEN_INDEX_NONE;
    guard->opener_directive_index = NOC_TOKEN_INDEX_NONE;
    guard->define_directive_index = NOC_TOKEN_INDEX_NONE;
    guard->closer_directive_index = NOC_TOKEN_INDEX_NONE;
    guard->guard_name_token_index = NOC_TOKEN_INDEX_NONE;
    guard->problem_directive_index = NOC_TOKEN_INDEX_NONE;
    guard->problem_token_index = NOC_TOKEN_INDEX_NONE;
    guard->preprocessing_tokens.begin = NOC_TOKEN_INDEX_NONE;
    guard->preprocessing_tokens.end = NOC_TOKEN_INDEX_NONE;
}

NOCDEF bool noc_include_guard_is_valid(const Noc_Include_Guard *guard)
{
    const Noc_Preprocessor_Conditional_Group *group;
    const Noc_Preprocessor_Conditional_Branch *branch;
    const Noc_Preprocessor_Directive *opener;
    size_t token_count;
    size_t directive_count;
    if (!guard || guard->generation == 0 ||
        !noc_preprocessor_unit_is_valid(guard->unit) ||
        !noc_preprocessor_conditional_groups_is_valid(guard->groups) ||
        guard->groups->unit != guard->unit ||
        guard->unit_stream_generation != guard->unit->stream.generation ||
        guard->groups_generation != guard->groups->generation ||
        guard->status < NOC_INCLUDE_GUARD_NONE ||
        guard->status > NOC_INCLUDE_GUARD_NOT_FILE_ENCLOSING) {
        return false;
    }
    token_count = guard->unit->preprocessing_token_count;
    directive_count = guard->unit->count;
    if (!noc__include_control_index_is_valid(guard->group_index,
                                             guard->groups->group_count) ||
        !noc__include_control_index_is_valid(guard->branch_index,
                                             guard->groups->branch_count) ||
        !noc__include_control_index_is_valid(guard->opener_directive_index,
                                             directive_count) ||
        !noc__include_control_index_is_valid(guard->define_directive_index,
                                             directive_count) ||
        !noc__include_control_index_is_valid(guard->closer_directive_index,
                                             directive_count) ||
        !noc__include_control_index_is_valid(guard->problem_directive_index,
                                             directive_count) ||
        !noc__include_control_index_is_valid(guard->guard_name_token_index,
                                             token_count) ||
        !noc__include_control_index_is_valid(guard->problem_token_index,
                                             token_count)) {
        return false;
    }
    if (guard->status == NOC_INCLUDE_GUARD_NONE) {
        return guard->opener_directive_index == NOC_TOKEN_INDEX_NONE &&
               guard->group_index == NOC_TOKEN_INDEX_NONE &&
               guard->branch_index == NOC_TOKEN_INDEX_NONE &&
               guard->define_directive_index == NOC_TOKEN_INDEX_NONE &&
               guard->closer_directive_index == NOC_TOKEN_INDEX_NONE &&
               guard->guard_name_token_index == NOC_TOKEN_INDEX_NONE &&
               guard->problem_directive_index == NOC_TOKEN_INDEX_NONE &&
               guard->problem_token_index == NOC_TOKEN_INDEX_NONE &&
               guard->preprocessing_tokens.begin == NOC_TOKEN_INDEX_NONE &&
               guard->preprocessing_tokens.end == NOC_TOKEN_INDEX_NONE &&
               guard->guard_name.data == NULL && guard->guard_name.count == 0;
    }
    if (guard->opener_directive_index == NOC_TOKEN_INDEX_NONE) return false;
    opener = &guard->unit->items[guard->opener_directive_index];
    if (opener->kind != NOC_PREPROCESSOR_DIRECTIVE_IFNDEF) return false;
    if (guard->guard_name_token_index != NOC_TOKEN_INDEX_NONE &&
        (guard->unit->preprocessing_tokens[
             guard->guard_name_token_index].token.kind != NOC_TOKEN_IDENTIFIER ||
         guard->guard_name.data != guard->unit->preprocessing_tokens[
             guard->guard_name_token_index].token.text.data ||
         guard->guard_name.count != guard->unit->preprocessing_tokens[
             guard->guard_name_token_index].token.text.count)) {
        return false;
    }
    if (guard->group_index != NOC_TOKEN_INDEX_NONE) {
        group = &guard->groups->groups[guard->group_index];
        if (group->opener_directive_index != guard->opener_directive_index ||
            group->first_branch_index != guard->branch_index ||
            group->closer_directive_index != guard->closer_directive_index ||
            group->preprocessing_tokens.begin !=
                guard->preprocessing_tokens.begin ||
            group->preprocessing_tokens.end != guard->preprocessing_tokens.end) {
            return false;
        }
    }
    if (guard->status == NOC_INCLUDE_GUARD_CANONICAL) {
        const Noc_Preprocessor_Directive *define;
        const Noc_Macro_Directive *macro;
        if (guard->group_index == NOC_TOKEN_INDEX_NONE ||
            guard->branch_index == NOC_TOKEN_INDEX_NONE ||
            guard->define_directive_index == NOC_TOKEN_INDEX_NONE ||
            guard->closer_directive_index == NOC_TOKEN_INDEX_NONE ||
            guard->guard_name_token_index == NOC_TOKEN_INDEX_NONE ||
            guard->guard_name.data == NULL || guard->guard_name.count == 0 ||
            guard->problem_directive_index != NOC_TOKEN_INDEX_NONE ||
            guard->problem_token_index != NOC_TOKEN_INDEX_NONE) {
            return false;
        }
        group = &guard->groups->groups[guard->group_index];
        branch = &guard->groups->branches[guard->branch_index];
        define = &guard->unit->items[guard->define_directive_index];
        if (group->parent_branch_index != NOC_TOKEN_INDEX_NONE ||
            group->first_branch_index != group->last_branch_index ||
            group->status != NOC_CONDITIONAL_GROUP_COMPLETE ||
            branch->directive_kind != NOC_PREPROCESSOR_DIRECTIVE_IFNDEF ||
            define->kind != NOC_PREPROCESSOR_DIRECTIVE_DEFINE ||
            define->macro_directive_index == NOC_TOKEN_INDEX_NONE ||
            define->macro_directive_index >=
                guard->unit->macro_directive_count ||
            define->macro_definition_allowed != guard->definition_allowed) {
            return false;
        }
        macro = &guard->unit->macro_directives[
            define->macro_directive_index];
        return macro->status == NOC_MACRO_DIRECTIVE_STATUS_VALID &&
               macro->kind == NOC_MACRO_DIRECTIVE_DEFINE_OBJECT &&
               macro->name_token_index < token_count &&
               noc__slices_logically_equal(
                   guard->guard_name,
                   guard->unit->preprocessing_tokens[
                       macro->name_token_index].token.text);
    }
    return true;
}

static void noc__include_guard_set_problem(Noc_Include_Guard *guard,
                                           Noc_Include_Guard_Status status,
                                           size_t directive_index,
                                           size_t token_index)
{
    guard->status = status;
    guard->problem_directive_index = directive_index;
    guard->problem_token_index = token_index;
}

NOCDEF Noc_Include_Control_Build_Status noc_include_guard_build(
    const Noc_Preprocessor_Unit *unit,
    const Noc_Preprocessor_Conditional_Groups *groups,
    Noc_Include_Guard *output)
{
    Noc_Include_Guard parsed;
    const Noc_Preprocessor_Directive *opener;
    const Noc_Preprocessor_Conditional_Group *group;
    const Noc_Preprocessor_Conditional_Branch *branch;
    const Noc_Preprocessor_Directive *define;
    const Noc_Macro_Directive *macro;
    Noc_Token_Range body;
    size_t first;
    size_t second;
    size_t last;
    size_t group_index;
    size_t content_first;
    size_t generation;
    if (!unit || !groups || !output) {
        return NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT;
    }
    if (!noc_preprocessor_unit_is_valid(unit) ||
        !noc_preprocessor_conditional_groups_is_valid(groups)) {
        return NOC_INCLUDE_CONTROL_BUILD_STALE;
    }
    if (groups->unit != unit ||
        groups->unit_stream_generation != unit->stream.generation) {
        return NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_INCLUDE_CONTROL_BUILD_GENERATION_EXHAUSTED;
    }
    noc__include_guard_init(&parsed, unit, groups);
    first = noc__include_control_next_significant(
        unit, 0, unit->preprocessing_token_count);
    if (first == NOC_TOKEN_INDEX_NONE ||
        unit->preprocessing_tokens[first].directive_index ==
            NOC_TOKEN_INDEX_NONE ||
        unit->items[unit->preprocessing_tokens[first].directive_index].kind !=
            NOC_PREPROCESSOR_DIRECTIVE_IFNDEF) {
        parsed.status = NOC_INCLUDE_GUARD_NONE;
        goto publish;
    }
    parsed.opener_directive_index =
        unit->preprocessing_tokens[first].directive_index;
    opener = &unit->items[parsed.opener_directive_index];
    body = noc_preprocessor_directive_body_tokens(
        unit, parsed.opener_directive_index);
    if (body.begin == NOC_TOKEN_INDEX_NONE) {
        noc__include_guard_set_problem(&parsed,
                                       NOC_INCLUDE_GUARD_INCOMPLETE,
                                       parsed.opener_directive_index,
                                       opener->preprocessing_tokens.end - 1);
        goto publish;
    }
    first = noc__include_control_next_significant(unit, body.begin, body.end);
    if (first == NOC_TOKEN_INDEX_NONE ||
        unit->preprocessing_tokens[first].token.kind == NOC_TOKEN_INVALID) {
        noc__include_guard_set_problem(&parsed,
                                       NOC_INCLUDE_GUARD_INCOMPLETE,
                                       parsed.opener_directive_index,
                                       first == NOC_TOKEN_INDEX_NONE ?
                                           body.begin : first);
        goto publish;
    }
    if (unit->preprocessing_tokens[first].token.kind != NOC_TOKEN_IDENTIFIER) {
        noc__include_guard_set_problem(&parsed,
                                       NOC_INCLUDE_GUARD_MALFORMED,
                                       parsed.opener_directive_index,
                                       first);
        goto publish;
    }
    parsed.guard_name_token_index = first;
    parsed.guard_name = unit->preprocessing_tokens[first].token.text;
    second = noc__include_control_next_significant(unit, first + 1, body.end);
    if (second != NOC_TOKEN_INDEX_NONE) {
        noc__include_guard_set_problem(
            &parsed,
            unit->preprocessing_tokens[second].token.kind == NOC_TOKEN_INVALID
                ? NOC_INCLUDE_GUARD_INCOMPLETE
                : NOC_INCLUDE_GUARD_MALFORMED,
            parsed.opener_directive_index,
            second);
        goto publish;
    }
    group_index = noc_preprocessor_conditional_owned_group(
        groups, parsed.opener_directive_index);
    if (group_index == NOC_TOKEN_INDEX_NONE) {
        noc__include_guard_set_problem(&parsed,
                                       NOC_INCLUDE_GUARD_MALFORMED,
                                       parsed.opener_directive_index,
                                       parsed.guard_name_token_index);
        goto publish;
    }
    parsed.group_index = group_index;
    group = &groups->groups[group_index];
    parsed.branch_index = group->first_branch_index;
    parsed.closer_directive_index = group->closer_directive_index;
    parsed.preprocessing_tokens = group->preprocessing_tokens;
    branch = &groups->branches[group->first_branch_index];
    if (group->closer_directive_index == NOC_TOKEN_INDEX_NONE ||
        group->status == NOC_CONDITIONAL_GROUP_INCOMPLETE ||
        group->status == NOC_CONDITIONAL_GROUP_MALFORMED_INCOMPLETE) {
        noc__include_guard_set_problem(&parsed,
                                       NOC_INCLUDE_GUARD_INCOMPLETE,
                                       parsed.opener_directive_index,
                                       parsed.guard_name_token_index);
        goto publish;
    }
    if (group->status == NOC_CONDITIONAL_GROUP_MALFORMED ||
        group->parent_branch_index != NOC_TOKEN_INDEX_NONE ||
        branch->directive_kind != NOC_PREPROCESSOR_DIRECTIVE_IFNDEF) {
        noc__include_guard_set_problem(&parsed,
                                       NOC_INCLUDE_GUARD_MALFORMED,
                                       parsed.opener_directive_index,
                                       parsed.guard_name_token_index);
        goto publish;
    }
    if (group->first_branch_index != group->last_branch_index) {
        noc__include_guard_set_problem(&parsed,
                                       NOC_INCLUDE_GUARD_HAS_PEER_BRANCH,
                                       groups->branches[
                                           groups->branches[group->first_branch_index]
                                               .next_branch_index]
                                           .directive_index,
                                       NOC_TOKEN_INDEX_NONE);
        goto publish;
    }
    last = noc__include_control_last_significant(
        unit, 0, unit->preprocessing_token_count);
    body = noc_preprocessor_directive_body_tokens(
        unit, group->closer_directive_index);
    if (last == NOC_TOKEN_INDEX_NONE ||
        unit->preprocessing_tokens[last].directive_index !=
            group->closer_directive_index ||
        body.begin != NOC_TOKEN_INDEX_NONE) {
        noc__include_guard_set_problem(
            &parsed,
            NOC_INCLUDE_GUARD_NOT_FILE_ENCLOSING,
            last == NOC_TOKEN_INDEX_NONE ? NOC_TOKEN_INDEX_NONE :
                unit->preprocessing_tokens[last].directive_index,
            last);
        goto publish;
    }
    content_first = noc__include_control_next_significant(
        unit,
        opener->preprocessing_tokens.end,
        unit->items[group->closer_directive_index].preprocessing_tokens.begin);
    if (content_first == NOC_TOKEN_INDEX_NONE ||
        unit->preprocessing_tokens[content_first].directive_index ==
            NOC_TOKEN_INDEX_NONE ||
        unit->items[unit->preprocessing_tokens[content_first].directive_index].kind !=
            NOC_PREPROCESSOR_DIRECTIVE_DEFINE) {
        noc__include_guard_set_problem(
            &parsed,
            NOC_INCLUDE_GUARD_MISSING_DEFINE,
            content_first == NOC_TOKEN_INDEX_NONE ?
                group->closer_directive_index :
                unit->preprocessing_tokens[content_first].directive_index,
            content_first);
        goto publish;
    }
    parsed.define_directive_index =
        unit->preprocessing_tokens[content_first].directive_index;
    define = &unit->items[parsed.define_directive_index];
    parsed.definition_allowed = define->macro_definition_allowed;
    if (define->macro_directive_index == NOC_TOKEN_INDEX_NONE ||
        define->macro_directive_index >= unit->macro_directive_count) {
        noc__include_guard_set_problem(&parsed,
                                       NOC_INCLUDE_GUARD_MALFORMED,
                                       parsed.define_directive_index,
                                       content_first);
        goto publish;
    }
    macro = &unit->macro_directives[define->macro_directive_index];
    if (macro->status != NOC_MACRO_DIRECTIVE_STATUS_VALID ||
        macro->kind != NOC_MACRO_DIRECTIVE_DEFINE_OBJECT ||
        macro->name_token_index >= unit->preprocessing_token_count) {
        noc__include_guard_set_problem(
            &parsed,
            macro->status == NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE ?
                NOC_INCLUDE_GUARD_INCOMPLETE :
                NOC_INCLUDE_GUARD_MALFORMED,
            parsed.define_directive_index,
            macro->problem_token_index != NOC_TOKEN_INDEX_NONE ?
                macro->problem_token_index : content_first);
        goto publish;
    }
    if (!noc__slices_logically_equal(
            parsed.guard_name,
            unit->preprocessing_tokens[macro->name_token_index].token.text)) {
        noc__include_guard_set_problem(&parsed,
                                       NOC_INCLUDE_GUARD_NAME_MISMATCH,
                                       parsed.define_directive_index,
                                       macro->name_token_index);
        goto publish;
    }
    parsed.status = NOC_INCLUDE_GUARD_CANONICAL;

publish:
    generation = output->generation + 1;
    parsed.generation = generation;
    *output = parsed;
    return NOC_INCLUDE_CONTROL_BUILD_OK;
}

#endif /* NOC_INCLUDE_CONTROL_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
