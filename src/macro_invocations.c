#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_MACRO_INVOCATIONS_IMPLEMENTATION_INCLUDED
#define NOC_MACRO_INVOCATIONS_IMPLEMENTATION_INCLUDED

NOCDEF const char *noc_macro_invocation_status_name(
    Noc_Macro_Invocation_Status status)
{
    switch (status) {
    case NOC_MACRO_INVOCATION_NOT_INVOKED: return "not-invoked";
    case NOC_MACRO_INVOCATION_COMPLETE: return "complete";
    case NOC_MACRO_INVOCATION_INCOMPLETE: return "incomplete";
    }
    return "unknown";
}

NOCDEF const char *noc_macro_invocation_build_status_name(
    Noc_Macro_Invocation_Build_Status status)
{
    switch (status) {
    case NOC_MACRO_INVOCATION_BUILD_OK: return "ok";
    case NOC_MACRO_INVOCATION_BUILD_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_MACRO_INVOCATION_BUILD_STALE: return "stale";
    case NOC_MACRO_INVOCATION_BUILD_GENERATION_EXHAUSTED:
        return "generation-exhausted";
    case NOC_MACRO_INVOCATION_BUILD_OUT_OF_MEMORY: return "out-of-memory";
    }
    return "unknown";
}

NOCDEF void noc_macro_invocation_free(Noc_Macro_Invocation *invocation)
{
    size_t generation;
    if (!invocation) return;
    generation = invocation->generation;
    free(invocation->arguments);
    memset(invocation, 0, sizeof(*invocation));
    invocation->generation = generation;
}

static bool noc__macro_invocation_range_has_token(
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range range)
{
    size_t index;
    for (index = range.begin; index < range.end; ++index) {
        if (!noc_token_is_trivia(unit->preprocessing_tokens[index].token)) return true;
    }
    return false;
}

static Noc_Macro_Invocation_Build_Status noc__macro_invocation_argument_append(
    Noc_Macro_Invocation *invocation,
    Noc_Token_Range tokens)
{
    Noc_Macro_Argument *arguments;
    size_t capacity;
    if (invocation->argument_count < invocation->argument_capacity) {
        invocation->arguments[invocation->argument_count++].tokens = tokens;
        return NOC_MACRO_INVOCATION_BUILD_OK;
    }
    if (invocation->argument_capacity == 0) {
        capacity = 8;
    } else {
        if (invocation->argument_capacity > SIZE_MAX / 2) {
            return NOC_MACRO_INVOCATION_BUILD_OUT_OF_MEMORY;
        }
        capacity = invocation->argument_capacity * 2;
    }
    if (capacity > SIZE_MAX / sizeof(*arguments)) {
        return NOC_MACRO_INVOCATION_BUILD_OUT_OF_MEMORY;
    }
    arguments = (Noc_Macro_Argument *)realloc(
        invocation->arguments,
        capacity * sizeof(*arguments));
    if (!arguments) return NOC_MACRO_INVOCATION_BUILD_OUT_OF_MEMORY;
    invocation->arguments = arguments;
    invocation->argument_capacity = capacity;
    invocation->arguments[invocation->argument_count++].tokens = tokens;
    return NOC_MACRO_INVOCATION_BUILD_OK;
}

NOCDEF bool noc_macro_invocation_is_valid(
    const Noc_Macro_Invocation *invocation)
{
    const Noc_Preprocessor_Unit *unit;
    size_t argument_begin;
    size_t argument_index = 0;
    size_t content_end;
    size_t cursor;
    size_t depth = 0;
    bool saw_separator = false;
    if (!invocation || invocation->generation == 0 ||
        !noc_preprocessor_unit_is_valid(invocation->unit) ||
        invocation->unit_stream_generation != invocation->unit->stream.generation ||
        invocation->name_token_index >=
            invocation->unit->preprocessing_token_count ||
        invocation->unit->preprocessing_tokens[
            invocation->name_token_index].token.kind != NOC_TOKEN_IDENTIFIER ||
        invocation->tokens.begin != invocation->name_token_index ||
        invocation->tokens.begin >= invocation->tokens.end ||
        invocation->tokens.end > invocation->unit->preprocessing_token_count ||
        invocation->argument_count > invocation->argument_capacity ||
        ((invocation->argument_capacity == 0) !=
         (invocation->arguments == NULL))) {
        return false;
    }
    unit = invocation->unit;
    if (invocation->status == NOC_MACRO_INVOCATION_NOT_INVOKED) {
        return invocation->open_token_index == NOC_TOKEN_INDEX_NONE &&
               invocation->close_token_index == NOC_TOKEN_INDEX_NONE &&
               invocation->problem_token_index == NOC_TOKEN_INDEX_NONE &&
               invocation->argument_count == 0 &&
               invocation->tokens.end == invocation->name_token_index + 1;
    }
    if (invocation->status != NOC_MACRO_INVOCATION_COMPLETE &&
        invocation->status != NOC_MACRO_INVOCATION_INCOMPLETE) {
        return false;
    }
    if (invocation->open_token_index <= invocation->name_token_index ||
        invocation->open_token_index >= invocation->tokens.end ||
        !noc_token_is_punct(
            unit->preprocessing_tokens[invocation->open_token_index].token,
            "(")) {
        return false;
    }
    for (cursor = invocation->name_token_index + 1;
         cursor < invocation->open_token_index;
         ++cursor) {
        if (!noc_token_is_trivia(unit->preprocessing_tokens[cursor].token)) {
            return false;
        }
    }
    if (invocation->status == NOC_MACRO_INVOCATION_COMPLETE) {
        if (invocation->close_token_index <= invocation->open_token_index ||
            invocation->close_token_index >= invocation->tokens.end ||
            invocation->tokens.end != invocation->close_token_index + 1 ||
            invocation->problem_token_index != NOC_TOKEN_INDEX_NONE ||
            !noc_token_is_punct(
                unit->preprocessing_tokens[invocation->close_token_index].token,
                ")")) {
            return false;
        }
        content_end = invocation->close_token_index;
    } else {
        if (invocation->close_token_index != NOC_TOKEN_INDEX_NONE) return false;
        if (invocation->problem_token_index != NOC_TOKEN_INDEX_NONE) {
            Noc_Token problem;
            if (invocation->problem_token_index >=
                unit->preprocessing_token_count) {
                return false;
            }
            problem = unit->preprocessing_tokens[
                invocation->problem_token_index].token;
            if (!((problem.kind == NOC_TOKEN_EOF &&
                   invocation->problem_token_index == invocation->tokens.end) ||
                  (problem.kind == NOC_TOKEN_INVALID &&
                   invocation->problem_token_index + 1 ==
                       invocation->tokens.end))) {
                return false;
            }
        }
        content_end = invocation->tokens.end;
    }

    argument_begin = invocation->open_token_index + 1;
    for (cursor = argument_begin; cursor < content_end; ++cursor) {
        Noc_Token token = unit->preprocessing_tokens[cursor].token;
        if (noc_token_is_punct(token, "(")) {
            depth += 1;
        } else if (noc_token_is_punct(token, ")")) {
            if (depth == 0) return false;
            depth -= 1;
        } else if (depth == 0 && noc_token_is_punct(token, ",")) {
            Noc_Token_Range expected = {argument_begin, cursor};
            if (argument_index >= invocation->argument_count ||
                invocation->arguments[argument_index].tokens.begin !=
                    expected.begin ||
                invocation->arguments[argument_index].tokens.end != expected.end) {
                return false;
            }
            argument_index += 1;
            argument_begin = cursor + 1;
            saw_separator = true;
        }
    }
    if (invocation->status == NOC_MACRO_INVOCATION_COMPLETE && depth != 0) {
        return false;
    }
    if (saw_separator ||
        noc__macro_invocation_range_has_token(
            unit,
            (Noc_Token_Range){argument_begin, content_end})) {
        if (argument_index >= invocation->argument_count ||
            invocation->arguments[argument_index].tokens.begin != argument_begin ||
            invocation->arguments[argument_index].tokens.end != content_end) {
            return false;
        }
        argument_index += 1;
    }
    return argument_index == invocation->argument_count;
}

NOCDEF Noc_Macro_Invocation_Build_Status noc_macro_invocation_parse(
    const Noc_Preprocessor_Unit *unit,
    size_t name_token_index,
    size_t token_limit,
    Noc_Macro_Invocation *output)
{
    Noc_Macro_Invocation parsed;
    Noc_Macro_Invocation_Build_Status build_status;
    size_t cursor;
    size_t argument_begin;
    size_t depth = 0;
    size_t generation;
    bool saw_separator = false;
    if (!unit || !output) {
        return NOC_MACRO_INVOCATION_BUILD_INVALID_ARGUMENT;
    }
    if (!noc_preprocessor_unit_is_valid(unit)) {
        return NOC_MACRO_INVOCATION_BUILD_STALE;
    }
    if (name_token_index >= token_limit ||
        token_limit > unit->preprocessing_token_count) {
        return NOC_MACRO_INVOCATION_BUILD_INVALID_ARGUMENT;
    }
    if (unit->preprocessing_tokens[name_token_index].token.kind !=
        NOC_TOKEN_IDENTIFIER) {
        return NOC_MACRO_INVOCATION_BUILD_INVALID_ARGUMENT;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_MACRO_INVOCATION_BUILD_GENERATION_EXHAUSTED;
    }
    memset(&parsed, 0, sizeof(parsed));
    parsed.unit = unit;
    parsed.unit_stream_generation = unit->stream.generation;
    parsed.name_token_index = name_token_index;
    parsed.open_token_index = NOC_TOKEN_INDEX_NONE;
    parsed.close_token_index = NOC_TOKEN_INDEX_NONE;
    parsed.problem_token_index = NOC_TOKEN_INDEX_NONE;
    parsed.tokens.begin = name_token_index;
    parsed.tokens.end = name_token_index + 1;
    cursor = name_token_index + 1;
    while (cursor < token_limit &&
           noc_token_is_trivia(unit->preprocessing_tokens[cursor].token)) {
        cursor += 1;
    }
    if (cursor >= token_limit ||
        !noc_token_is_punct(unit->preprocessing_tokens[cursor].token, "(")) {
        parsed.status = NOC_MACRO_INVOCATION_NOT_INVOKED;
        goto publish;
    }
    parsed.open_token_index = cursor;
    argument_begin = ++cursor;
    while (cursor < token_limit) {
        Noc_Token token = unit->preprocessing_tokens[cursor].token;
        if (token.kind == NOC_TOKEN_EOF) {
            parsed.status = NOC_MACRO_INVOCATION_INCOMPLETE;
            parsed.tokens.end = cursor;
            parsed.problem_token_index = cursor;
            goto append_partial;
        }
        if (token.kind == NOC_TOKEN_INVALID) {
            parsed.status = NOC_MACRO_INVOCATION_INCOMPLETE;
            parsed.tokens.end = cursor + 1;
            parsed.problem_token_index = cursor;
            goto append_partial;
        }
        if (noc_token_is_punct(token, "(")) {
            depth += 1;
        } else if (noc_token_is_punct(token, ")")) {
            if (depth != 0) {
                depth -= 1;
            } else {
                if (saw_separator ||
                    noc__macro_invocation_range_has_token(
                        unit,
                        (Noc_Token_Range){argument_begin, cursor})) {
                    build_status = noc__macro_invocation_argument_append(
                        &parsed,
                        (Noc_Token_Range){argument_begin, cursor});
                    if (build_status != NOC_MACRO_INVOCATION_BUILD_OK) goto fail;
                }
                parsed.status = NOC_MACRO_INVOCATION_COMPLETE;
                parsed.close_token_index = cursor;
                parsed.tokens.end = cursor + 1;
                goto publish;
            }
        } else if (depth == 0 && noc_token_is_punct(token, ",")) {
            build_status = noc__macro_invocation_argument_append(
                &parsed,
                (Noc_Token_Range){argument_begin, cursor});
            if (build_status != NOC_MACRO_INVOCATION_BUILD_OK) goto fail;
            saw_separator = true;
            argument_begin = cursor + 1;
        }
        cursor += 1;
    }
    parsed.status = NOC_MACRO_INVOCATION_INCOMPLETE;
    parsed.tokens.end = token_limit;

append_partial:
    if (saw_separator ||
        noc__macro_invocation_range_has_token(
            unit,
            (Noc_Token_Range){argument_begin, parsed.tokens.end})) {
        build_status = noc__macro_invocation_argument_append(
            &parsed,
            (Noc_Token_Range){argument_begin, parsed.tokens.end});
        if (build_status != NOC_MACRO_INVOCATION_BUILD_OK) goto fail;
    }

publish:
    generation = output->generation + 1;
    noc_macro_invocation_free(output);
    *output = parsed;
    output->generation = generation;
    return NOC_MACRO_INVOCATION_BUILD_OK;

fail:
    noc_macro_invocation_free(&parsed);
    return build_status;
}

NOCDEF const Noc_Macro_Argument *noc_macro_invocation_argument_at(
    const Noc_Macro_Invocation *invocation,
    size_t index)
{
    if (!noc_macro_invocation_is_valid(invocation) ||
        index >= invocation->argument_count) {
        return NULL;
    }
    return &invocation->arguments[index];
}

#endif /* NOC_MACRO_INVOCATIONS_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
