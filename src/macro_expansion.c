#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_MACRO_EXPANSION_IMPLEMENTATION_INCLUDED
#define NOC_MACRO_EXPANSION_IMPLEMENTATION_INCLUDED

#define NOC__MACRO_EXPANSION_HARD_MAX_DEPTH 256u

typedef struct {
    const Noc_Macro_Environment *environment;
    size_t entry_limit;
    Noc_Macro_Expansion_Limits limits;
    size_t stack[NOC__MACRO_EXPANSION_HARD_MAX_DEPTH];
    size_t stack_count;
    Noc_Macro_Expansion *output;
} Noc__Macro_Expansion_Builder;

NOCDEF const char *noc_macro_expansion_status_name(
    Noc_Macro_Expansion_Status status)
{
    switch (status) {
    case NOC_MACRO_EXPANSION_OK: return "ok";
    case NOC_MACRO_EXPANSION_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_MACRO_EXPANSION_STALE: return "stale";
    case NOC_MACRO_EXPANSION_DEPTH_LIMIT: return "depth-limit";
    case NOC_MACRO_EXPANSION_OUTPUT_LIMIT: return "output-limit";
    case NOC_MACRO_EXPANSION_COUNT_LIMIT: return "count-limit";
    case NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR:
        return "unsupported-operator";
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
                   token->origin != NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT) {
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

static Noc_Macro_Expansion_Status noc__macro_expansion_token_append(
    Noc__Macro_Expansion_Builder *builder,
    const Noc_Preprocessor_Unit *unit,
    size_t token_index,
    size_t frame_index,
    Noc_Macro_Expansion_Token_Origin origin)
{
    Noc_Macro_Expansion_Token *items;
    Noc_Macro_Expansion_Token *item;
    size_t capacity;
    if (builder->output->count >= builder->limits.max_output_tokens) {
        return NOC_MACRO_EXPANSION_OUTPUT_LIMIT;
    }
    if (builder->output->count == builder->output->capacity) {
        if (builder->output->capacity == 0) {
            capacity = 32;
        } else {
            if (builder->output->capacity > SIZE_MAX / 2) {
                return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
            }
            capacity = builder->output->capacity * 2;
        }
        if (capacity > builder->limits.max_output_tokens) {
            capacity = builder->limits.max_output_tokens;
        }
        if (capacity == 0 || capacity > SIZE_MAX / sizeof(*items)) {
            return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
        }
        items = (Noc_Macro_Expansion_Token *)realloc(
            builder->output->items,
            capacity * sizeof(*items));
        if (!items) return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
        builder->output->items = items;
        builder->output->capacity = capacity;
    }
    item = &builder->output->items[builder->output->count++];
    item->token = unit->preprocessing_tokens[token_index].token;
    item->unit = unit;
    item->unit_stream_generation = unit->stream.generation;
    item->preprocessing_token_index = token_index;
    item->frame_index = frame_index;
    item->origin = origin;
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Status noc__macro_expansion_frame_append(
    Noc__Macro_Expansion_Builder *builder,
    size_t environment_entry_index,
    const Noc_Preprocessor_Unit *invocation_unit,
    size_t invocation_token_index,
    size_t parent_frame_index,
    size_t *frame_index)
{
    Noc_Macro_Expansion_Frame *frames;
    Noc_Macro_Expansion_Frame *frame;
    size_t capacity;
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
        if (capacity == 0 || capacity > SIZE_MAX / sizeof(*frames)) {
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
    frame->parent_frame_index = parent_frame_index;
    frame->invocation_unit = invocation_unit;
    frame->invocation_unit_stream_generation = invocation_unit->stream.generation;
    frame->invocation_token_index = invocation_token_index;
    return NOC_MACRO_EXPANSION_OK;
}

static bool noc__macro_expansion_stack_contains(
    const Noc__Macro_Expansion_Builder *builder,
    size_t environment_entry_index)
{
    size_t index;
    for (index = 0; index < builder->stack_count; ++index) {
        if (builder->stack[index] == environment_entry_index) return true;
    }
    return false;
}

static bool noc__macro_expansion_has_paste(
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range replacement)
{
    size_t index;
    for (index = replacement.begin; index < replacement.end; ++index) {
        Noc_Token token = unit->preprocessing_tokens[index].token;
        if (noc_token_is_punct(token, "##") ||
            noc_token_is_punct(token, "%:%:")) {
            return true;
        }
    }
    return false;
}

static Noc_Macro_Expansion_Status noc__macro_expand_range(
    Noc__Macro_Expansion_Builder *builder,
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range range,
    size_t parent_frame_index,
    Noc_Macro_Expansion_Token_Origin origin)
{
    size_t token_index;
    for (token_index = range.begin; token_index < range.end; ++token_index) {
        Noc_Token token = unit->preprocessing_tokens[token_index].token;
        const Noc_Macro_Environment_Entry *entry = NULL;
        const Noc_Macro_Directive *directive = NULL;
        size_t entry_index = NOC_TOKEN_INDEX_NONE;
        size_t frame_index;
        Noc_Macro_Expansion_Status status;
        if (token.kind == NOC_TOKEN_IDENTIFIER) {
            entry = noc_macro_environment_lookup_before(builder->environment,
                                                        token.text,
                                                        builder->entry_limit);
        }
        if (entry) {
            entry_index = (size_t)(entry - builder->environment->items);
            directive = noc__macro_environment_entry_directive(entry);
        }
        if (!directive ||
            directive->kind != NOC_MACRO_DIRECTIVE_DEFINE_OBJECT ||
            noc__macro_expansion_stack_contains(builder, entry_index)) {
            status = noc__macro_expansion_token_append(builder,
                                                       unit,
                                                       token_index,
                                                       parent_frame_index,
                                                       origin);
            if (status != NOC_MACRO_EXPANSION_OK) return status;
            continue;
        }
        if (noc__macro_expansion_has_paste(entry->unit,
                                           directive->replacement_tokens)) {
            return NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR;
        }
        if (builder->stack_count >= builder->limits.max_depth) {
            return NOC_MACRO_EXPANSION_DEPTH_LIMIT;
        }
        status = noc__macro_expansion_frame_append(builder,
                                                   entry_index,
                                                   unit,
                                                   token_index,
                                                   parent_frame_index,
                                                   &frame_index);
        if (status != NOC_MACRO_EXPANSION_OK) return status;
        builder->stack[builder->stack_count++] = entry_index;
        status = noc__macro_expand_range(builder,
                                         entry->unit,
                                         directive->replacement_tokens,
                                         frame_index,
                                         NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT);
        builder->stack_count -= 1;
        if (status != NOC_MACRO_EXPANSION_OK) return status;
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
    Noc_Macro_Expansion_Status status;
    size_t generation;
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
    status = noc__macro_expand_range(&builder,
                                     input_unit,
                                     input_tokens,
                                     NOC_TOKEN_INDEX_NONE,
                                     NOC_MACRO_EXPANSION_TOKEN_INPUT);
    if (status != NOC_MACRO_EXPANSION_OK) {
        noc_macro_expansion_free(&parsed);
        return status;
    }
    generation = output->generation + 1;
    noc_macro_expansion_free(output);
    *output = parsed;
    output->generation = generation;
    return NOC_MACRO_EXPANSION_OK;
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
