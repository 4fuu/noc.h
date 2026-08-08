#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_MACRO_EXPANSION_IMPLEMENTATION_INCLUDED
#define NOC_MACRO_EXPANSION_IMPLEMENTATION_INCLUDED

#define NOC__MACRO_EXPANSION_HARD_MAX_DEPTH 256u

typedef struct {
    Noc_Macro_Expansion_Token *items;
    size_t *hide_sets;
    size_t count;
    size_t capacity;
} Noc__Macro_Token_Sequence;

typedef struct {
    size_t environment_entry_index;
    size_t parent_index;
} Noc__Macro_Hide_Set;

typedef struct {
    const Noc_Macro_Environment *environment;
    size_t entry_limit;
    Noc_Macro_Expansion_Limits limits;
    Noc_Macro_Expansion *output;
    Noc__Macro_Hide_Set *hide_sets;
    size_t hide_set_count;
    size_t hide_set_capacity;
} Noc__Macro_Expansion_Builder;

typedef struct {
    Noc__Macro_Token_Sequence tokens;
    Noc_Token_Range source_tokens;
    size_t use_count;
} Noc__Macro_Expanded_Argument;

NOCDEF const char *noc_macro_expansion_status_name(
    Noc_Macro_Expansion_Status status)
{
    switch (status) {
    case NOC_MACRO_EXPANSION_OK: return "ok";
    case NOC_MACRO_EXPANSION_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_MACRO_EXPANSION_STALE: return "stale";
    case NOC_MACRO_EXPANSION_INCOMPLETE_INVOCATION:
        return "incomplete-invocation";
    case NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH:
        return "argument-count-mismatch";
    case NOC_MACRO_EXPANSION_INVALID_DEFINITION: return "invalid-definition";
    case NOC_MACRO_EXPANSION_DEPTH_LIMIT: return "depth-limit";
    case NOC_MACRO_EXPANSION_OUTPUT_LIMIT: return "output-limit";
    case NOC_MACRO_EXPANSION_COUNT_LIMIT: return "count-limit";
    case NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR:
        return "unsupported-operator";
    case NOC_MACRO_EXPANSION_UNSUPPORTED_VARIADIC:
        return "unsupported-variadic";
    case NOC_MACRO_EXPANSION_GENERATION_EXHAUSTED:
        return "generation-exhausted";
    case NOC_MACRO_EXPANSION_OUT_OF_MEMORY: return "out-of-memory";
    }
    return "unknown";
}

NOCDEF const char *noc_macro_expansion_token_origin_name(
    Noc_Macro_Expansion_Token_Origin origin)
{
    switch (origin) {
    case NOC_MACRO_EXPANSION_TOKEN_INPUT: return "input";
    case NOC_MACRO_EXPANSION_TOKEN_ARGUMENT: return "argument";
    case NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT: return "replacement";
    }
    return "unknown";
}

NOCDEF Noc_Macro_Expansion_Limits noc_macro_expansion_default_limits(void)
{
    Noc_Macro_Expansion_Limits limits;
    limits.max_depth = 128;
    limits.max_output_tokens = 1024u * 1024u;
    limits.max_expansions = 64u * 1024u;
    return limits;
}

NOCDEF void noc_macro_expansion_free(Noc_Macro_Expansion *expansion)
{
    size_t generation;
    if (!expansion) return;
    generation = expansion->generation;
    free(expansion->frames);
    free(expansion->items);
    memset(expansion, 0, sizeof(*expansion));
    expansion->generation = generation;
}

NOCDEF bool noc_macro_expansion_is_valid(const Noc_Macro_Expansion *expansion)
{
    size_t index;
    if (!expansion || expansion->generation == 0 ||
        !noc_macro_environment_is_valid(expansion->environment) ||
        expansion->environment_generation != expansion->environment->generation ||
        expansion->environment_entry_count != expansion->environment->count ||
        expansion->environment_entry_limit > expansion->environment_entry_count ||
        !noc_preprocessor_unit_is_valid(expansion->input_unit) ||
        expansion->input_unit_stream_generation !=
            expansion->input_unit->stream.generation ||
        expansion->count > expansion->capacity ||
        ((expansion->capacity == 0) != (expansion->items == NULL)) ||
        expansion->frame_count > expansion->frame_capacity ||
        ((expansion->frame_capacity == 0) != (expansion->frames == NULL))) {
        return false;
    }
    for (index = 0; index < expansion->count; ++index) {
        const Noc_Macro_Expansion_Token *token = &expansion->items[index];
        if (!noc_preprocessor_unit_is_valid(token->unit) ||
            token->unit_stream_generation != token->unit->stream.generation ||
            token->preprocessing_token_index >=
                token->unit->preprocessing_token_count) {
            return false;
        }
        if (token->frame_index == NOC_TOKEN_INDEX_NONE) {
            if (token->origin != NOC_MACRO_EXPANSION_TOKEN_INPUT) return false;
        } else if (token->frame_index >= expansion->frame_count ||
                   (token->origin != NOC_MACRO_EXPANSION_TOKEN_ARGUMENT &&
                    token->origin != NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT)) {
            return false;
        }
    }
    for (index = 0; index < expansion->frame_count; ++index) {
        const Noc_Macro_Expansion_Frame *frame = &expansion->frames[index];
        if (frame->environment_entry_index >= expansion->environment_entry_limit ||
            (frame->parent_frame_index != NOC_TOKEN_INDEX_NONE &&
             frame->parent_frame_index >= index) ||
            !noc_preprocessor_unit_is_valid(frame->invocation_unit) ||
            frame->invocation_unit_stream_generation !=
                frame->invocation_unit->stream.generation ||
            frame->invocation_token_index >=
                frame->invocation_unit->preprocessing_token_count) {
            return false;
        }
    }
    return true;
}

static void noc__macro_token_sequence_free(Noc__Macro_Token_Sequence *sequence)
{
    if (!sequence) return;
    free(sequence->hide_sets);
    free(sequence->items);
    memset(sequence, 0, sizeof(*sequence));
}

static Noc_Macro_Expansion_Status noc__macro_token_sequence_reserve(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence,
    size_t needed)
{
    size_t *hide_sets;
    Noc_Macro_Expansion_Token *items;
    size_t capacity;
    if (needed > builder->limits.max_output_tokens) {
        return NOC_MACRO_EXPANSION_OUTPUT_LIMIT;
    }
    if (needed <= sequence->capacity) return NOC_MACRO_EXPANSION_OK;
    capacity = sequence->capacity == 0 ? 32 : sequence->capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    if (capacity > builder->limits.max_output_tokens) {
        capacity = builder->limits.max_output_tokens;
    }
    if (capacity < needed || capacity > SIZE_MAX / sizeof(*items)) {
        return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    }
    items = (Noc_Macro_Expansion_Token *)realloc(
        sequence->items,
        capacity * sizeof(*items));
    if (!items) return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    sequence->items = items;
    hide_sets = (size_t *)realloc(sequence->hide_sets,
                                 capacity * sizeof(*hide_sets));
    if (!hide_sets) return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    sequence->hide_sets = hide_sets;
    sequence->capacity = capacity;
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Status noc__macro_token_sequence_append(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence,
    Noc_Macro_Expansion_Token token,
    size_t hide_set)
{
    Noc_Macro_Expansion_Status status;
    if (sequence->count == SIZE_MAX) {
        return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    }
    status = noc__macro_token_sequence_reserve(builder,
                                               sequence,
                                               sequence->count + 1);
    if (status != NOC_MACRO_EXPANSION_OK) return status;
    sequence->items[sequence->count] = token;
    sequence->hide_sets[sequence->count] = hide_set;
    sequence->count += 1;
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Status noc__macro_token_sequence_append_sequence(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence,
    const Noc__Macro_Token_Sequence *suffix)
{
    Noc_Macro_Expansion_Status status;
    size_t needed;
    if (suffix->count > SIZE_MAX - sequence->count) {
        return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    }
    needed = sequence->count + suffix->count;
    status = noc__macro_token_sequence_reserve(builder, sequence, needed);
    if (status != NOC_MACRO_EXPANSION_OK) return status;
    if (suffix->count != 0) {
        memcpy(sequence->items + sequence->count,
               suffix->items,
               suffix->count * sizeof(*suffix->items));
        memcpy(sequence->hide_sets + sequence->count,
               suffix->hide_sets,
               suffix->count * sizeof(*suffix->hide_sets));
    }
    sequence->count = needed;
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Status noc__macro_token_sequence_splice(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence,
    Noc_Token_Range removed,
    const Noc__Macro_Token_Sequence *replacement)
{
    Noc_Macro_Expansion_Status status;
    size_t kept;
    size_t needed;
    size_t suffix_count;
    if (removed.begin > removed.end || removed.end > sequence->count) {
        return NOC_MACRO_EXPANSION_INVALID_ARGUMENT;
    }
    kept = sequence->count - (removed.end - removed.begin);
    if (replacement->count > SIZE_MAX - kept) {
        return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    }
    needed = kept + replacement->count;
    status = noc__macro_token_sequence_reserve(builder, sequence, needed);
    if (status != NOC_MACRO_EXPANSION_OK) return status;
    suffix_count = sequence->count - removed.end;
    if (suffix_count != 0 && replacement->count != removed.end - removed.begin) {
        memmove(sequence->items + removed.begin + replacement->count,
                sequence->items + removed.end,
                suffix_count * sizeof(*sequence->items));
        memmove(sequence->hide_sets + removed.begin + replacement->count,
                sequence->hide_sets + removed.end,
                suffix_count * sizeof(*sequence->hide_sets));
    }
    if (replacement->count != 0) {
        memcpy(sequence->items + removed.begin,
               replacement->items,
               replacement->count * sizeof(*replacement->items));
        memcpy(sequence->hide_sets + removed.begin,
               replacement->hide_sets,
               replacement->count * sizeof(*replacement->hide_sets));
    }
    sequence->count = needed;
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Token noc__macro_expansion_token(
    const Noc_Preprocessor_Unit *unit,
    size_t token_index,
    size_t frame_index,
    Noc_Macro_Expansion_Token_Origin origin)
{
    Noc_Macro_Expansion_Token result;
    result.token = unit->preprocessing_tokens[token_index].token;
    result.unit = unit;
    result.unit_stream_generation = unit->stream.generation;
    result.preprocessing_token_index = token_index;
    result.frame_index = frame_index;
    result.origin = origin;
    return result;
}

static Noc_Token noc__macro_logical_token_at(const void *tokens, size_t index)
{
    const Noc__Macro_Token_Sequence *sequence =
        (const Noc__Macro_Token_Sequence *)tokens;
    return sequence->items[index].token;
}

static size_t noc__macro_expansion_frame_depth(
    const Noc_Macro_Expansion *output,
    size_t frame_index)
{
    size_t depth = 0;
    while (frame_index != NOC_TOKEN_INDEX_NONE) {
        depth += 1;
        frame_index = output->frames[frame_index].parent_frame_index;
    }
    return depth;
}

static bool noc__macro_hide_set_contains(
    const Noc__Macro_Expansion_Builder *builder,
    size_t hide_set,
    size_t environment_entry_index)
{
    while (hide_set != NOC_TOKEN_INDEX_NONE) {
        const Noc__Macro_Hide_Set *item = &builder->hide_sets[hide_set];
        if (item->environment_entry_index == environment_entry_index) return true;
        hide_set = item->parent_index;
    }
    return false;
}

static Noc_Macro_Expansion_Status noc__macro_hide_set_add(
    Noc__Macro_Expansion_Builder *builder,
    size_t hide_set,
    size_t environment_entry_index,
    size_t *result)
{
    Noc__Macro_Hide_Set *items;
    size_t capacity;
    if (noc__macro_hide_set_contains(builder,
                                     hide_set,
                                     environment_entry_index)) {
        *result = hide_set;
        return NOC_MACRO_EXPANSION_OK;
    }
    if (builder->hide_set_count == builder->hide_set_capacity) {
        if (builder->hide_set_capacity == 0) {
            capacity = 32;
        } else {
            if (builder->hide_set_capacity > SIZE_MAX / 2) {
                return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
            }
            capacity = builder->hide_set_capacity * 2;
        }
        if (capacity <= builder->hide_set_capacity ||
            capacity > SIZE_MAX / sizeof(*items)) {
            return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
        }
        items = (Noc__Macro_Hide_Set *)realloc(
            builder->hide_sets,
            capacity * sizeof(*items));
        if (!items) return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
        builder->hide_sets = items;
        builder->hide_set_capacity = capacity;
    }
    *result = builder->hide_set_count++;
    builder->hide_sets[*result].environment_entry_index =
        environment_entry_index;
    builder->hide_sets[*result].parent_index = hide_set;
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Status noc__macro_hide_set_union(
    Noc__Macro_Expansion_Builder *builder,
    size_t left,
    size_t right,
    size_t *result)
{
    Noc_Macro_Expansion_Status status;
    *result = left;
    while (right != NOC_TOKEN_INDEX_NONE) {
        const Noc__Macro_Hide_Set *item = &builder->hide_sets[right];
        size_t environment_entry_index = item->environment_entry_index;
        size_t parent_index = item->parent_index;
        status = noc__macro_hide_set_add(builder,
                                         *result,
                                         environment_entry_index,
                                         result);
        if (status != NOC_MACRO_EXPANSION_OK) return status;
        right = parent_index;
    }
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Status noc__macro_hide_set_intersection(
    Noc__Macro_Expansion_Builder *builder,
    size_t left,
    size_t right,
    size_t *result)
{
    Noc_Macro_Expansion_Status status;
    *result = NOC_TOKEN_INDEX_NONE;
    while (left != NOC_TOKEN_INDEX_NONE) {
        const Noc__Macro_Hide_Set *item = &builder->hide_sets[left];
        size_t environment_entry_index = item->environment_entry_index;
        size_t parent_index = item->parent_index;
        if (noc__macro_hide_set_contains(builder,
                                         right,
                                         environment_entry_index)) {
            status = noc__macro_hide_set_add(builder,
                                             *result,
                                             environment_entry_index,
                                             result);
            if (status != NOC_MACRO_EXPANSION_OK) return status;
        }
        left = parent_index;
    }
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Status noc__macro_expansion_frame_append(
    Noc__Macro_Expansion_Builder *builder,
    size_t environment_entry_index,
    const Noc_Macro_Expansion_Token *invocation,
    size_t *frame_index)
{
    Noc_Macro_Expansion_Frame *frames;
    Noc_Macro_Expansion_Frame *frame;
    size_t capacity;
    if (noc__macro_expansion_frame_depth(builder->output,
                                         invocation->frame_index) >=
        builder->limits.max_depth) {
        return NOC_MACRO_EXPANSION_DEPTH_LIMIT;
    }
    if (builder->output->frame_count >= builder->limits.max_expansions) {
        return NOC_MACRO_EXPANSION_COUNT_LIMIT;
    }
    if (builder->output->frame_count == builder->output->frame_capacity) {
        if (builder->output->frame_capacity == 0) {
            capacity = 16;
        } else {
            if (builder->output->frame_capacity > SIZE_MAX / 2) {
                return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
            }
            capacity = builder->output->frame_capacity * 2;
        }
        if (capacity > builder->limits.max_expansions) {
            capacity = builder->limits.max_expansions;
        }
        if (capacity <= builder->output->frame_capacity ||
            capacity > SIZE_MAX / sizeof(*frames)) {
            return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
        }
        frames = (Noc_Macro_Expansion_Frame *)realloc(
            builder->output->frames,
            capacity * sizeof(*frames));
        if (!frames) return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
        builder->output->frames = frames;
        builder->output->frame_capacity = capacity;
    }
    *frame_index = builder->output->frame_count++;
    frame = &builder->output->frames[*frame_index];
    frame->environment_entry_index = environment_entry_index;
    frame->parent_frame_index = invocation->frame_index;
    frame->invocation_unit = invocation->unit;
    frame->invocation_unit_stream_generation = invocation->unit_stream_generation;
    frame->invocation_token_index = invocation->preprocessing_token_index;
    return NOC_MACRO_EXPANSION_OK;
}

static bool noc__macro_replacement_has_operator(
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range replacement,
    bool include_stringify)
{
    size_t index;
    for (index = replacement.begin; index < replacement.end; ++index) {
        Noc_Token token = unit->preprocessing_tokens[index].token;
        if (noc_token_is_punct(token, "##") ||
            noc_token_is_punct(token, "%:%:") ||
            (include_stringify &&
             (noc_token_is_punct(token, "#") ||
              noc_token_is_punct(token, "%:")))) {
            return true;
        }
    }
    return false;
}

static size_t noc__macro_replacement_parameter_index(
    const Noc_Preprocessor_Unit *unit,
    const Noc_Macro_Directive *directive,
    Noc_Token token)
{
    size_t index;
    if (token.kind != NOC_TOKEN_IDENTIFIER) return NOC_TOKEN_INDEX_NONE;
    for (index = 0; index < directive->parameter_count; ++index) {
        const Noc_Macro_Parameter *parameter =
            &unit->macro_parameters[directive->parameter_begin + index];
        Noc_Token parameter_token =
            unit->preprocessing_tokens[parameter->token_index].token;
        if ((parameter->variadic &&
             noc_token_is_identifier(token, "__VA_ARGS__")) ||
            (!parameter->variadic &&
             noc__slices_logically_equal(token.text, parameter_token.text))) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static bool noc__macro_replacement_uses_va_args(
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range replacement)
{
    size_t index;
    for (index = replacement.begin; index < replacement.end; ++index) {
        if (noc_token_is_identifier(unit->preprocessing_tokens[index].token,
                                    "__VA_ARGS__")) {
            return true;
        }
    }
    return false;
}

static bool noc__macro_directive_has_reserved_va_args_name(
    const Noc_Preprocessor_Unit *unit,
    const Noc_Macro_Directive *directive)
{
    return noc_token_is_identifier(
        unit->preprocessing_tokens[directive->name_token_index].token,
        "__VA_ARGS__");
}

static bool noc__macro_function_parameters_are_valid(
    const Noc_Preprocessor_Unit *unit,
    const Noc_Macro_Directive *directive)
{
    size_t left;
    bool found_variadic = false;
    for (left = 0; left < directive->parameter_count; ++left) {
        const Noc_Macro_Parameter *left_parameter =
            &unit->macro_parameters[directive->parameter_begin + left];
        size_t right;
        if (left_parameter->variadic) {
            if (found_variadic || left + 1 != directive->parameter_count) {
                return false;
            }
            found_variadic = true;
            continue;
        }
        if (noc_token_is_identifier(
                unit->preprocessing_tokens[
                    left_parameter->token_index].token,
                "__VA_ARGS__")) {
            return false;
        }
        for (right = left + 1; right < directive->parameter_count; ++right) {
            const Noc_Macro_Parameter *right_parameter =
                &unit->macro_parameters[directive->parameter_begin + right];
            if (!right_parameter->variadic &&
                noc__slices_logically_equal(
                    unit->preprocessing_tokens[
                        left_parameter->token_index].token.text,
                    unit->preprocessing_tokens[
                        right_parameter->token_index].token.text)) {
                return false;
            }
        }
    }
    return found_variadic == directive->variadic;
}

static Noc_Macro_Expansion_Status noc__macro_expand_sequence(
    Noc__Macro_Expansion_Builder *, Noc__Macro_Token_Sequence *);

static Noc_Macro_Expansion_Status noc__macro_expand_object(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence,
    size_t cursor,
    size_t environment_entry_index,
    const Noc_Macro_Environment_Entry *entry,
    const Noc_Macro_Directive *directive)
{
    Noc__Macro_Token_Sequence replacement = {0};
    Noc_Macro_Expansion_Status status;
    size_t frame_index;
    size_t replacement_hide_set;
    size_t index;
    if (noc__macro_directive_has_reserved_va_args_name(entry->unit, directive) ||
        noc__macro_replacement_uses_va_args(entry->unit,
                                            directive->replacement_tokens)) {
        return NOC_MACRO_EXPANSION_INVALID_DEFINITION;
    }
    if (noc__macro_replacement_has_operator(entry->unit,
                                            directive->replacement_tokens,
                                            false)) {
        return NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR;
    }
    status = noc__macro_expansion_frame_append(builder,
                                               environment_entry_index,
                                               &sequence->items[cursor],
                                               &frame_index);
    if (status != NOC_MACRO_EXPANSION_OK) return status;
    status = noc__macro_hide_set_add(builder,
                                     sequence->hide_sets[cursor],
                                     environment_entry_index,
                                     &replacement_hide_set);
    if (status != NOC_MACRO_EXPANSION_OK) return status;
    for (index = directive->replacement_tokens.begin;
         index < directive->replacement_tokens.end;
         ++index) {
        status = noc__macro_token_sequence_append(
            builder,
            &replacement,
            noc__macro_expansion_token(entry->unit,
                                       index,
                                       frame_index,
                                       NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT),
            replacement_hide_set);
        if (status != NOC_MACRO_EXPANSION_OK) goto done;
    }
    status = noc__macro_token_sequence_splice(
        builder,
        sequence,
        (Noc_Token_Range){cursor, cursor + 1},
        &replacement);

done:
    noc__macro_token_sequence_free(&replacement);
    return status;
}

static Noc_Macro_Expansion_Status noc__macro_expand_function(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence,
    size_t cursor,
    size_t environment_entry_index,
    const Noc_Macro_Environment_Entry *entry,
    const Noc_Macro_Directive *directive,
    const Noc__Macro_Invocation_Collection *invocation)
{
    Noc__Macro_Expanded_Argument *arguments = NULL;
    Noc__Macro_Token_Sequence replacement = {0};
    Noc_Macro_Expansion_Status status = NOC_MACRO_EXPANSION_OK;
    size_t invocation_hide_set;
    size_t frame_index;
    size_t replacement_hide_set;
    size_t supplied_argument_count;
    size_t index;
    if (noc__macro_directive_has_reserved_va_args_name(entry->unit, directive) ||
        !noc__macro_function_parameters_are_valid(entry->unit, directive) ||
        (!directive->variadic &&
         noc__macro_replacement_uses_va_args(entry->unit,
                                             directive->replacement_tokens))) {
        return NOC_MACRO_EXPANSION_INVALID_DEFINITION;
    }
    if (noc__macro_replacement_has_operator(entry->unit,
                                            directive->replacement_tokens,
                                            true)) {
        return NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR;
    }
    supplied_argument_count = invocation->argument_count;
    if (supplied_argument_count == 0 && directive->parameter_count != 0) {
        supplied_argument_count = 1;
    }
    if ((directive->variadic &&
         supplied_argument_count < directive->parameter_count) ||
        (!directive->variadic &&
         supplied_argument_count != directive->parameter_count)) {
        return NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH;
    }
    if (directive->parameter_count > SIZE_MAX / sizeof(*arguments)) {
        return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    }
    if (directive->parameter_count != 0) {
        arguments = (Noc__Macro_Expanded_Argument *)calloc(
            directive->parameter_count,
            sizeof(*arguments));
        if (!arguments) return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    }
    for (index = 0; index < directive->parameter_count; ++index) {
        const Noc_Macro_Parameter *parameter =
            &entry->unit->macro_parameters[directive->parameter_begin + index];
        if (parameter->variadic && invocation->argument_count != 0) {
            arguments[index].source_tokens.begin =
                invocation->arguments[index].tokens.begin;
            arguments[index].source_tokens.end =
                invocation->arguments[
                    invocation->argument_count - 1].tokens.end;
        } else if (invocation->argument_count == 0) {
            arguments[index].source_tokens.begin = invocation->open_token_index + 1;
            arguments[index].source_tokens.end = invocation->open_token_index + 1;
        } else {
            arguments[index].source_tokens = invocation->arguments[index].tokens;
        }
    }
    for (index = directive->replacement_tokens.begin;
         index < directive->replacement_tokens.end;
         ++index) {
        size_t parameter_index = noc__macro_replacement_parameter_index(
            entry->unit,
            directive,
            entry->unit->preprocessing_tokens[index].token);
        if (parameter_index != NOC_TOKEN_INDEX_NONE) {
            arguments[parameter_index].use_count += 1;
        }
    }
    status = noc__macro_expansion_frame_append(builder,
                                               environment_entry_index,
                                               &sequence->items[cursor],
                                               &frame_index);
    if (status != NOC_MACRO_EXPANSION_OK) goto done;
    status = noc__macro_hide_set_intersection(
        builder,
        sequence->hide_sets[cursor],
        sequence->hide_sets[invocation->close_token_index],
        &invocation_hide_set);
    if (status != NOC_MACRO_EXPANSION_OK) goto done;
    status = noc__macro_hide_set_add(builder,
                                     invocation_hide_set,
                                     environment_entry_index,
                                     &replacement_hide_set);
    if (status != NOC_MACRO_EXPANSION_OK) goto done;
    for (index = 0; index < directive->parameter_count; ++index) {
        Noc_Token_Range source = arguments[index].source_tokens;
        size_t token_index;
        if (arguments[index].use_count == 0) continue;
        for (token_index = source.begin; token_index < source.end; ++token_index) {
            Noc_Macro_Expansion_Token token = sequence->items[token_index];
            token.frame_index = frame_index;
            token.origin = NOC_MACRO_EXPANSION_TOKEN_ARGUMENT;
            status = noc__macro_token_sequence_append(builder,
                                                      &arguments[index].tokens,
                                                      token,
                                                      sequence->hide_sets[
                                                          token_index]);
            if (status != NOC_MACRO_EXPANSION_OK) goto done;
        }
        status = noc__macro_expand_sequence(builder, &arguments[index].tokens);
        if (status != NOC_MACRO_EXPANSION_OK) goto done;
        for (token_index = 0;
             token_index < arguments[index].tokens.count;
             ++token_index) {
            status = noc__macro_hide_set_union(
                builder,
                arguments[index].tokens.hide_sets[token_index],
                replacement_hide_set,
                &arguments[index].tokens.hide_sets[token_index]);
            if (status != NOC_MACRO_EXPANSION_OK) goto done;
        }
        if (arguments[index].use_count != 0 &&
            arguments[index].tokens.count >
                builder->limits.max_output_tokens /
                    arguments[index].use_count) {
            status = NOC_MACRO_EXPANSION_OUTPUT_LIMIT;
            goto done;
        }
    }
    for (index = directive->replacement_tokens.begin;
         index < directive->replacement_tokens.end;
         ++index) {
        size_t parameter_index = noc__macro_replacement_parameter_index(
            entry->unit,
            directive,
            entry->unit->preprocessing_tokens[index].token);
        if (parameter_index != NOC_TOKEN_INDEX_NONE) {
            status = noc__macro_token_sequence_append_sequence(
                builder,
                &replacement,
                &arguments[parameter_index].tokens);
        } else {
            status = noc__macro_token_sequence_append(
                builder,
                &replacement,
                noc__macro_expansion_token(
                    entry->unit,
                    index,
                    frame_index,
                    NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT),
                replacement_hide_set);
        }
        if (status != NOC_MACRO_EXPANSION_OK) goto done;
    }
    status = noc__macro_token_sequence_splice(builder,
                                              sequence,
                                              invocation->tokens,
                                              &replacement);

done:
    for (index = 0; index < directive->parameter_count; ++index) {
        noc__macro_token_sequence_free(&arguments[index].tokens);
    }
    free(arguments);
    noc__macro_token_sequence_free(&replacement);
    return status;
}

static Noc_Macro_Expansion_Status noc__macro_expand_sequence(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence)
{
    size_t cursor = 0;
    while (cursor < sequence->count) {
        Noc_Macro_Expansion_Token *token = &sequence->items[cursor];
        const Noc_Macro_Environment_Entry *entry = NULL;
        const Noc_Macro_Directive *directive = NULL;
        size_t environment_entry_index = NOC_TOKEN_INDEX_NONE;
        Noc_Macro_Expansion_Status status;
        if (token->token.kind == NOC_TOKEN_IDENTIFIER) {
            entry = noc_macro_environment_lookup_before(builder->environment,
                                                        token->token.text,
                                                        builder->entry_limit);
        }
        if (entry) {
            environment_entry_index =
                (size_t)(entry - builder->environment->items);
            directive = noc__macro_environment_entry_directive(entry);
        }
        if (!directive ||
            noc__macro_hide_set_contains(builder,
                                         sequence->hide_sets[cursor],
                                         environment_entry_index)) {
            cursor += 1;
            continue;
        }
        if (directive->kind == NOC_MACRO_DIRECTIVE_DEFINE_OBJECT) {
            status = noc__macro_expand_object(builder,
                                              sequence,
                                              cursor,
                                              environment_entry_index,
                                              entry,
                                              directive);
            if (status != NOC_MACRO_EXPANSION_OK) return status;
            continue;
        }
        if (directive->kind == NOC_MACRO_DIRECTIVE_DEFINE_FUNCTION) {
            Noc__Macro_Invocation_Collection invocation = {0};
            Noc_Macro_Invocation_Build_Status collect_status =
                noc__macro_invocation_collect(sequence,
                                              noc__macro_logical_token_at,
                                              cursor,
                                              sequence->count,
                                              &invocation);
            if (collect_status == NOC_MACRO_INVOCATION_BUILD_OUT_OF_MEMORY) {
                return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
            }
            if (collect_status != NOC_MACRO_INVOCATION_BUILD_OK) {
                return NOC_MACRO_EXPANSION_INVALID_ARGUMENT;
            }
            if (invocation.status == NOC_MACRO_INVOCATION_NOT_INVOKED) {
                noc__macro_invocation_collection_free(&invocation);
                cursor += 1;
                continue;
            }
            if (invocation.status == NOC_MACRO_INVOCATION_INCOMPLETE) {
                noc__macro_invocation_collection_free(&invocation);
                return NOC_MACRO_EXPANSION_INCOMPLETE_INVOCATION;
            }
            status = noc__macro_expand_function(builder,
                                                sequence,
                                                cursor,
                                                environment_entry_index,
                                                entry,
                                                directive,
                                                &invocation);
            noc__macro_invocation_collection_free(&invocation);
            if (status != NOC_MACRO_EXPANSION_OK) return status;
            continue;
        }
        cursor += 1;
    }
    return NOC_MACRO_EXPANSION_OK;
}

NOCDEF Noc_Macro_Expansion_Status noc_macro_expansion_build(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Limits limits,
    Noc_Macro_Expansion *output)
{
    Noc_Macro_Expansion parsed;
    Noc__Macro_Expansion_Builder builder;
    Noc__Macro_Token_Sequence sequence = {0};
    Noc_Macro_Expansion_Status status = NOC_MACRO_EXPANSION_OK;
    size_t generation;
    size_t token_index;
    if (!environment || !input_unit || !output ||
        entry_limit > environment->count ||
        input_tokens.begin > input_tokens.end ||
        limits.max_depth == 0 ||
        limits.max_depth > NOC__MACRO_EXPANSION_HARD_MAX_DEPTH ||
        limits.max_output_tokens == 0 || limits.max_expansions == 0) {
        return NOC_MACRO_EXPANSION_INVALID_ARGUMENT;
    }
    if (!noc_macro_environment_is_valid(environment) ||
        !noc_preprocessor_unit_is_valid(input_unit)) {
        return NOC_MACRO_EXPANSION_STALE;
    }
    if (input_tokens.end > input_unit->preprocessing_token_count) {
        return NOC_MACRO_EXPANSION_INVALID_ARGUMENT;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_MACRO_EXPANSION_GENERATION_EXHAUSTED;
    }
    memset(&parsed, 0, sizeof(parsed));
    parsed.environment = environment;
    parsed.environment_generation = environment->generation;
    parsed.environment_entry_count = environment->count;
    parsed.environment_entry_limit = entry_limit;
    parsed.input_unit = input_unit;
    parsed.input_unit_stream_generation = input_unit->stream.generation;
    memset(&builder, 0, sizeof(builder));
    builder.environment = environment;
    builder.entry_limit = entry_limit;
    builder.limits = limits;
    builder.output = &parsed;
    for (token_index = input_tokens.begin;
         token_index < input_tokens.end;
         ++token_index) {
        status = noc__macro_token_sequence_append(
            &builder,
            &sequence,
            noc__macro_expansion_token(input_unit,
                                       token_index,
                                       NOC_TOKEN_INDEX_NONE,
                                       NOC_MACRO_EXPANSION_TOKEN_INPUT),
            NOC_TOKEN_INDEX_NONE);
        if (status != NOC_MACRO_EXPANSION_OK) goto fail;
    }
    status = noc__macro_expand_sequence(&builder, &sequence);
    if (status != NOC_MACRO_EXPANSION_OK) goto fail;
    parsed.items = sequence.items;
    parsed.count = sequence.count;
    parsed.capacity = sequence.capacity;
    free(sequence.hide_sets);
    sequence.hide_sets = NULL;
    memset(&sequence, 0, sizeof(sequence));
    free(builder.hide_sets);
    builder.hide_sets = NULL;
    generation = output->generation + 1;
    noc_macro_expansion_free(output);
    *output = parsed;
    output->generation = generation;
    return NOC_MACRO_EXPANSION_OK;

fail:
    noc__macro_token_sequence_free(&sequence);
    free(builder.hide_sets);
    noc_macro_expansion_free(&parsed);
    return status;
}

NOCDEF const Noc_Macro_Expansion_Token *noc_macro_expansion_token_at(
    const Noc_Macro_Expansion *expansion,
    size_t index)
{
    if (!noc_macro_expansion_is_valid(expansion) || index >= expansion->count) {
        return NULL;
    }
    return &expansion->items[index];
}

NOCDEF const Noc_Macro_Expansion_Frame *noc_macro_expansion_frame_at(
    const Noc_Macro_Expansion *expansion,
    size_t index)
{
    if (!noc_macro_expansion_is_valid(expansion) ||
        index >= expansion->frame_count) {
        return NULL;
    }
    return &expansion->frames[index];
}

NOCDEF bool noc_macro_expansion_render(const Noc_Macro_Expansion *expansion,
                                       Noc_Buffer *output)
{
    Noc_Buffer rendered = {0};
    size_t index;
    if (!noc_macro_expansion_is_valid(expansion) || !output) return false;
    for (index = 0; index < expansion->count; ++index) {
        if (!noc_buffer_append_slice(&rendered, expansion->items[index].token.text)) {
            noc_buffer_free(&rendered);
            return false;
        }
    }
    if (!noc_buffer_terminate(&rendered)) {
        noc_buffer_free(&rendered);
        return false;
    }
    noc_buffer_free(output);
    *output = rendered;
    return true;
}

#undef NOC__MACRO_EXPANSION_HARD_MAX_DEPTH

#endif /* NOC_MACRO_EXPANSION_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
