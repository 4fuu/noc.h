#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_MACRO_EXPANSION_IMPLEMENTATION_INCLUDED
#define NOC_MACRO_EXPANSION_IMPLEMENTATION_INCLUDED

#define NOC__MACRO_EXPANSION_HARD_MAX_DEPTH 256u
#define NOC__MACRO_TOKEN_PLACEMARKER 1u
#define NOC__MACRO_TOKEN_PASTE_OPERATOR 2u
#define NOC__MACRO_BUILTIN_BIT(kind) (UINT32_C(1) << (unsigned)(kind))
#define NOC__MACRO_BUILTIN_ALWAYS_MASK                                      \
    (NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_FILE) |                       \
     NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_LINE) |                       \
     NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_STDC) |                       \
     NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_STDC_VERSION))
#define NOC__MACRO_BUILTIN_SUPPORTED_MASK                                   \
    (NOC__MACRO_BUILTIN_ALWAYS_MASK |                                       \
     NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_STDC_HOSTED) |                \
     NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_DATE) |                       \
     NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_TIME))

typedef struct {
    Noc_Macro_Expansion_Token *items;
    size_t *hide_sets;
    unsigned char *flags;
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
    Noc_Macro_Expansion_Options options;
    uint32_t available_builtin_mask;
    Noc_Macro_Expansion *output;
    Noc__Macro_Hide_Set *hide_sets;
    size_t hide_set_count;
    size_t hide_set_capacity;
    bool preserve_defined_operands;
} Noc__Macro_Expansion_Builder;

typedef struct {
    Noc__Macro_Token_Sequence tokens;
    Noc_Token_Range source_tokens;
    size_t expanded_use_count;
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
    case NOC_MACRO_EXPANSION_INVALID_PASTE: return "invalid-paste";
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
    case NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION: return "stringification";
    case NOC_MACRO_EXPANSION_TOKEN_PASTE: return "paste";
    case NOC_MACRO_EXPANSION_TOKEN_BUILTIN: return "builtin";
    }
    return "unknown";
}

NOCDEF const char *noc_macro_builtin_kind_name(Noc_Macro_Builtin_Kind kind)
{
    switch (kind) {
    case NOC_MACRO_BUILTIN_NONE: return "none";
    case NOC_MACRO_BUILTIN_FILE: return "file";
    case NOC_MACRO_BUILTIN_LINE: return "line";
    case NOC_MACRO_BUILTIN_STDC: return "stdc";
    case NOC_MACRO_BUILTIN_STDC_VERSION: return "stdc-version";
    case NOC_MACRO_BUILTIN_STDC_HOSTED: return "stdc-hosted";
    case NOC_MACRO_BUILTIN_DATE: return "date";
    case NOC_MACRO_BUILTIN_TIME: return "time";
    }
    return "unknown";
}

NOCDEF Noc_Macro_Builtin_Kind noc_macro_builtin_kind_from_name(Noc_Slice name)
{
    if (noc__slices_logically_equal(name, noc_slice_from_cstr("__FILE__"))) {
        return NOC_MACRO_BUILTIN_FILE;
    }
    if (noc__slices_logically_equal(name, noc_slice_from_cstr("__LINE__"))) {
        return NOC_MACRO_BUILTIN_LINE;
    }
    if (noc__slices_logically_equal(name, noc_slice_from_cstr("__STDC__"))) {
        return NOC_MACRO_BUILTIN_STDC;
    }
    if (noc__slices_logically_equal(name,
                                    noc_slice_from_cstr("__STDC_VERSION__"))) {
        return NOC_MACRO_BUILTIN_STDC_VERSION;
    }
    if (noc__slices_logically_equal(name,
                                    noc_slice_from_cstr("__STDC_HOSTED__"))) {
        return NOC_MACRO_BUILTIN_STDC_HOSTED;
    }
    if (noc__slices_logically_equal(name, noc_slice_from_cstr("__DATE__"))) {
        return NOC_MACRO_BUILTIN_DATE;
    }
    if (noc__slices_logically_equal(name, noc_slice_from_cstr("__TIME__"))) {
        return NOC_MACRO_BUILTIN_TIME;
    }
    return NOC_MACRO_BUILTIN_NONE;
}

NOCDEF Noc_Macro_Expansion_Limits noc_macro_expansion_default_limits(void)
{
    Noc_Macro_Expansion_Limits limits;
    limits.max_depth = 128;
    limits.max_output_tokens = 1024u * 1024u;
    limits.max_expansions = 64u * 1024u;
    return limits;
}

NOCDEF Noc_Macro_Expansion_Options noc_macro_expansion_default_options(void)
{
    Noc_Macro_Expansion_Options options;
    memset(&options, 0, sizeof(options));
    options.limits = noc_macro_expansion_default_limits();
    options.execution_environment = NOC_EXECUTION_ENVIRONMENT_UNSPECIFIED;
    return options;
}

NOC__PRIVATE bool noc__macro_expansion_limits_are_valid(
    Noc_Macro_Expansion_Limits limits)
{
    return limits.max_depth > 0 &&
           limits.max_depth <= NOC__MACRO_EXPANSION_HARD_MAX_DEPTH &&
           limits.max_output_tokens > 0 && limits.max_expansions > 0;
}

static bool noc__macro_translation_date_is_valid(Noc_Slice date)
{
    static const char *const months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    size_t month;
    unsigned day;
    size_t index;
    if (date.count == 0) return true;
    if (!date.data || date.count != 11 || date.data[3] != ' ' ||
        date.data[6] != ' ') {
        return false;
    }
    for (month = 0; month < sizeof(months) / sizeof(months[0]); ++month) {
        if (memcmp(date.data, months[month], 3) == 0) break;
    }
    if (month == sizeof(months) / sizeof(months[0]) ||
        date.data[5] < '0' || date.data[5] > '9') {
        return false;
    }
    if (date.data[4] == ' ') {
        day = (unsigned)(date.data[5] - '0');
        if (day == 0) return false;
    } else {
        if (date.data[4] < '1' || date.data[4] > '3') return false;
        day = (unsigned)(date.data[4] - '0') * 10u +
              (unsigned)(date.data[5] - '0');
        if (day < 10u || day > 31u) return false;
    }
    for (index = 7; index < 11; ++index) {
        if (date.data[index] < '0' || date.data[index] > '9') return false;
    }
    return true;
}

static bool noc__macro_translation_time_is_valid(Noc_Slice time)
{
    unsigned hour;
    unsigned minute;
    unsigned second;
    size_t index;
    if (time.count == 0) return true;
    if (!time.data || time.count != 8 || time.data[2] != ':' ||
        time.data[5] != ':') {
        return false;
    }
    for (index = 0; index < 8; ++index) {
        if (index == 2 || index == 5) continue;
        if (time.data[index] < '0' || time.data[index] > '9') return false;
    }
    hour = (unsigned)(time.data[0] - '0') * 10u +
           (unsigned)(time.data[1] - '0');
    minute = (unsigned)(time.data[3] - '0') * 10u +
             (unsigned)(time.data[4] - '0');
    second = (unsigned)(time.data[6] - '0') * 10u +
             (unsigned)(time.data[7] - '0');
    return hour <= 23u && minute <= 59u && second <= 60u;
}

NOC__PRIVATE bool noc__macro_expansion_options_are_valid(
    Noc_Macro_Expansion_Options options)
{
    if (!noc__macro_expansion_limits_are_valid(options.limits)) return false;
    switch (options.execution_environment) {
    case NOC_EXECUTION_ENVIRONMENT_UNSPECIFIED:
    case NOC_EXECUTION_ENVIRONMENT_FREESTANDING:
    case NOC_EXECUTION_ENVIRONMENT_HOSTED:
        break;
    default:
        return false;
    }
    return noc__macro_translation_date_is_valid(options.translation_date) &&
           noc__macro_translation_time_is_valid(options.translation_time);
}

NOC__PRIVATE uint32_t noc__macro_builtin_mask_from_options(
    Noc_Macro_Expansion_Options options)
{
    uint32_t mask = NOC__MACRO_BUILTIN_ALWAYS_MASK;
    if (options.execution_environment !=
        NOC_EXECUTION_ENVIRONMENT_UNSPECIFIED) {
        mask |= NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_STDC_HOSTED);
    }
    if (options.translation_date.count != 0) {
        mask |= NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_DATE);
    }
    if (options.translation_time.count != 0) {
        mask |= NOC__MACRO_BUILTIN_BIT(NOC_MACRO_BUILTIN_TIME);
    }
    return mask;
}

NOC__PRIVATE bool noc__macro_builtin_mask_contains(
    uint32_t mask,
    Noc_Macro_Builtin_Kind kind)
{
    if (kind < NOC_MACRO_BUILTIN_FILE || kind > NOC_MACRO_BUILTIN_TIME) {
        return false;
    }
    return (mask & NOC__MACRO_BUILTIN_BIT(kind)) != 0;
}

NOCDEF void noc_macro_expansion_free(Noc_Macro_Expansion *expansion)
{
    size_t generation;
    size_t index;
    if (!expansion) return;
    generation = expansion->generation;
    for (index = 0; index < expansion->generated_spelling_count; ++index) {
        free((void *)expansion->generated_spellings[index].data);
    }
    free(expansion->generated_spellings);
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
        ((expansion->frame_capacity == 0) != (expansion->frames == NULL)) ||
        expansion->generated_spelling_count >
            expansion->generated_spelling_capacity ||
        ((expansion->generated_spelling_capacity == 0) !=
         (expansion->generated_spellings == NULL)) ||
        (expansion->available_builtin_mask &
         ~NOC__MACRO_BUILTIN_SUPPORTED_MASK) != 0 ||
        (expansion->available_builtin_mask & NOC__MACRO_BUILTIN_ALWAYS_MASK) !=
            NOC__MACRO_BUILTIN_ALWAYS_MASK) {
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
        if (token->origin == NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION ||
            token->origin == NOC_MACRO_EXPANSION_TOKEN_PASTE ||
            token->origin == NOC_MACRO_EXPANSION_TOKEN_BUILTIN) {
            const Noc_Slice *spelling;
            if ((token->frame_index != NOC_TOKEN_INDEX_NONE &&
                 token->frame_index >= expansion->frame_count) ||
                (token->origin != NOC_MACRO_EXPANSION_TOKEN_BUILTIN &&
                 token->frame_index == NOC_TOKEN_INDEX_NONE) ||
                (token->origin != NOC_MACRO_EXPANSION_TOKEN_BUILTIN &&
                 token->builtin_kind != NOC_MACRO_BUILTIN_NONE) ||
                token->generated_spelling_index >=
                    expansion->generated_spelling_count) {
                return false;
            }
            if (token->origin == NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION) {
                Noc_Token operator_token = token->unit->preprocessing_tokens[
                    token->preprocessing_token_index].token;
                if (token->token.kind != NOC_TOKEN_STRING ||
                    (!noc_token_is_punct(operator_token, "#") &&
                     !noc_token_is_punct(operator_token, "%:"))) {
                    return false;
                }
            } else if (token->origin == NOC_MACRO_EXPANSION_TOKEN_PASTE) {
                Noc_Token operator_token = token->unit->preprocessing_tokens[
                    token->preprocessing_token_index].token;
                if ((!noc_token_is_punct(operator_token, "##") &&
                     !noc_token_is_punct(operator_token, "%:%:")) ||
                    token->token.kind == NOC_TOKEN_EOF ||
                    token->token.kind == NOC_TOKEN_PREPROCESSOR ||
                    token->token.kind == NOC_TOKEN_HEADER_NAME ||
                    token->token.kind == NOC_TOKEN_INVALID ||
                    noc_token_is_trivia(token->token)) {
                    return false;
                }
            } else {
                Noc_Token source_token = token->unit->preprocessing_tokens[
                    token->preprocessing_token_index].token;
                Noc_Macro_Builtin_Kind source_kind =
                    noc_macro_builtin_kind_from_name(source_token.text);
                switch (token->builtin_kind) {
                case NOC_MACRO_BUILTIN_FILE:
                case NOC_MACRO_BUILTIN_DATE:
                case NOC_MACRO_BUILTIN_TIME:
                    if (token->token.kind != NOC_TOKEN_STRING) return false;
                    break;
                case NOC_MACRO_BUILTIN_LINE:
                case NOC_MACRO_BUILTIN_STDC:
                case NOC_MACRO_BUILTIN_STDC_VERSION:
                case NOC_MACRO_BUILTIN_STDC_HOSTED:
                    if (token->token.kind != NOC_TOKEN_NUMBER) return false;
                    break;
                case NOC_MACRO_BUILTIN_NONE:
                default:
                    return false;
                }
                if (!noc__macro_builtin_mask_contains(
                        expansion->available_builtin_mask,
                        token->builtin_kind) ||
                    (token->frame_index == NOC_TOKEN_INDEX_NONE &&
                     token->unit != expansion->input_unit)) {
                    return false;
                }
                if (source_kind != token->builtin_kind &&
                    !noc_token_is_punct(source_token, "##") &&
                    !noc_token_is_punct(source_token, "%:%:")) {
                    return false;
                }
            }
            spelling = &expansion->generated_spellings[
                token->generated_spelling_index];
            if (!spelling->data || spelling->count == 0 ||
                token->token.text.data != spelling->data ||
                token->token.text.count != spelling->count) {
                return false;
            }
        } else if (token->generated_spelling_index != NOC_TOKEN_INDEX_NONE ||
                   token->builtin_kind != NOC_MACRO_BUILTIN_NONE) {
            return false;
        } else if (token->frame_index == NOC_TOKEN_INDEX_NONE) {
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

NOCDEF bool noc_macro_expansion_builtin_is_available(
    const Noc_Macro_Expansion *expansion,
    Noc_Macro_Builtin_Kind kind)
{
    return noc_macro_expansion_is_valid(expansion) &&
           noc__macro_builtin_mask_contains(expansion->available_builtin_mask,
                                            kind);
}

static void noc__macro_token_sequence_free(Noc__Macro_Token_Sequence *sequence)
{
    if (!sequence) return;
    free(sequence->flags);
    free(sequence->hide_sets);
    free(sequence->items);
    memset(sequence, 0, sizeof(*sequence));
}

static Noc_Macro_Expansion_Status noc__macro_token_sequence_reserve(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence,
    size_t needed)
{
    unsigned char *flags;
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
    flags = (unsigned char *)realloc(sequence->flags,
                                    capacity * sizeof(*flags));
    if (!flags) return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    sequence->flags = flags;
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
    sequence->flags[sequence->count] = 0;
    sequence->count += 1;
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Status noc__macro_token_sequence_append_flagged(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence,
    Noc_Macro_Expansion_Token token,
    size_t hide_set,
    unsigned char flags)
{
    Noc_Macro_Expansion_Status status = noc__macro_token_sequence_append(
        builder,
        sequence,
        token,
        hide_set);
    if (status == NOC_MACRO_EXPANSION_OK) {
        sequence->flags[sequence->count - 1] = flags;
    }
    return status;
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
        memcpy(sequence->flags + sequence->count,
               suffix->flags,
               suffix->count * sizeof(*suffix->flags));
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
        memmove(sequence->flags + removed.begin + replacement->count,
                sequence->flags + removed.end,
                suffix_count * sizeof(*sequence->flags));
    }
    if (replacement->count != 0) {
        memcpy(sequence->items + removed.begin,
               replacement->items,
               replacement->count * sizeof(*replacement->items));
        memcpy(sequence->hide_sets + removed.begin,
               replacement->hide_sets,
               replacement->count * sizeof(*replacement->hide_sets));
        memcpy(sequence->flags + removed.begin,
               replacement->flags,
               replacement->count * sizeof(*replacement->flags));
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
    result.generated_spelling_index = NOC_TOKEN_INDEX_NONE;
    result.builtin_kind = NOC_MACRO_BUILTIN_NONE;
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

static Noc_Macro_Expansion_Status noc__macro_generated_spelling_append(
    Noc__Macro_Expansion_Builder *builder,
    Noc_Buffer *spelling,
    size_t *spelling_index)
{
    Noc_Slice *items;
    size_t capacity;
    if (!noc_buffer_terminate(spelling)) {
        return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    }
    if (builder->output->generated_spelling_count ==
        builder->output->generated_spelling_capacity) {
        if (builder->output->generated_spelling_capacity == 0) {
            capacity = 16;
        } else {
            if (builder->output->generated_spelling_capacity > SIZE_MAX / 2) {
                return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
            }
            capacity = builder->output->generated_spelling_capacity * 2;
        }
        if (capacity > SIZE_MAX / sizeof(*items)) {
            return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
        }
        items = (Noc_Slice *)realloc(builder->output->generated_spellings,
                                    capacity * sizeof(*items));
        if (!items) return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
        builder->output->generated_spellings = items;
        builder->output->generated_spelling_capacity = capacity;
    }
    *spelling_index = builder->output->generated_spelling_count++;
    builder->output->generated_spellings[*spelling_index].data = spelling->items;
    builder->output->generated_spellings[*spelling_index].count = spelling->count;
    memset(spelling, 0, sizeof(*spelling));
    return NOC_MACRO_EXPANSION_OK;
}

static void noc__macro_builtin_context(
    const Noc_Macro_Expansion *output,
    const Noc_Macro_Expansion_Token *token,
    const Noc_Preprocessor_Unit **unit,
    size_t *token_index)
{
    size_t frame_index = token->frame_index;
    *unit = token->unit;
    *token_index = token->preprocessing_token_index;
    if (token->unit == output->input_unit) return;
    while (frame_index != NOC_TOKEN_INDEX_NONE) {
        const Noc_Macro_Expansion_Frame *frame = &output->frames[frame_index];
        if (frame->invocation_unit == output->input_unit) {
            *unit = frame->invocation_unit;
            *token_index = frame->invocation_token_index;
            return;
        }
        frame_index = frame->parent_frame_index;
    }
}

static Noc_Macro_Expansion_Status noc__macro_expand_builtin(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *sequence,
    size_t cursor,
    Noc_Macro_Builtin_Kind kind)
{
    const Noc_Preprocessor_Unit *context_unit;
    size_t context_token_index;
    Noc_Buffer spelling = {0};
    Noc_Macro_Expansion_Token token = sequence->items[cursor];
    Noc_Macro_Expansion_Status status = NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    size_t generated_spelling_index;
    noc__macro_builtin_context(builder->output,
                               &token,
                               &context_unit,
                               &context_token_index);
    switch (kind) {
    case NOC_MACRO_BUILTIN_FILE:
        if (!noc__buffer_append_c_string(
                &spelling,
                context_unit->stream.path,
                strlen(context_unit->stream.path))) {
            goto done;
        }
        token.token.kind = NOC_TOKEN_STRING;
        break;
    case NOC_MACRO_BUILTIN_LINE:
        if (!noc_buffer_appendf(
                &spelling,
                "%zu",
                context_unit->preprocessing_tokens[
                    context_token_index].token.location.line)) {
            goto done;
        }
        token.token.kind = NOC_TOKEN_NUMBER;
        break;
    case NOC_MACRO_BUILTIN_STDC:
        if (!noc_buffer_append_cstr(&spelling, "1")) goto done;
        token.token.kind = NOC_TOKEN_NUMBER;
        break;
    case NOC_MACRO_BUILTIN_STDC_VERSION:
        if (!noc_buffer_append_cstr(&spelling, "201112L")) goto done;
        token.token.kind = NOC_TOKEN_NUMBER;
        break;
    case NOC_MACRO_BUILTIN_STDC_HOSTED:
        if (!noc_buffer_append_cstr(
                &spelling,
                builder->options.execution_environment ==
                        NOC_EXECUTION_ENVIRONMENT_HOSTED ?
                    "1" :
                    "0")) {
            goto done;
        }
        token.token.kind = NOC_TOKEN_NUMBER;
        break;
    case NOC_MACRO_BUILTIN_DATE:
        if (!noc__buffer_append_c_string(
                &spelling,
                builder->options.translation_date.data,
                builder->options.translation_date.count)) {
            goto done;
        }
        token.token.kind = NOC_TOKEN_STRING;
        break;
    case NOC_MACRO_BUILTIN_TIME:
        if (!noc__buffer_append_c_string(
                &spelling,
                builder->options.translation_time.data,
                builder->options.translation_time.count)) {
            goto done;
        }
        token.token.kind = NOC_TOKEN_STRING;
        break;
    case NOC_MACRO_BUILTIN_NONE:
        status = NOC_MACRO_EXPANSION_INVALID_ARGUMENT;
        goto done;
    }
    status = noc__macro_generated_spelling_append(builder,
                                                  &spelling,
                                                  &generated_spelling_index);
    if (status != NOC_MACRO_EXPANSION_OK) goto done;
    token.token.text = builder->output->generated_spellings[
        generated_spelling_index];
    token.generated_spelling_index = generated_spelling_index;
    token.builtin_kind = kind;
    token.origin = NOC_MACRO_EXPANSION_TOKEN_BUILTIN;
    sequence->items[cursor] = token;

done:
    noc_buffer_free(&spelling);
    return status;
}

static bool noc__macro_token_is_paste_operator(Noc_Token token)
{
    return noc_token_is_punct(token, "##") ||
           noc_token_is_punct(token, "%:%:");
}

static size_t noc__macro_replacement_previous_significant(
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range replacement,
    size_t index)
{
    while (index > replacement.begin) {
        index -= 1;
        if (!noc_token_is_trivia(unit->preprocessing_tokens[index].token)) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static size_t noc__macro_replacement_next_significant(
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range replacement,
    size_t index)
{
    for (index += 1; index < replacement.end; ++index) {
        if (!noc_token_is_trivia(unit->preprocessing_tokens[index].token)) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static bool noc__macro_replacement_pastes_are_valid(
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range replacement)
{
    size_t index;
    for (index = replacement.begin; index < replacement.end; ++index) {
        size_t left;
        size_t right;
        if (!noc__macro_token_is_paste_operator(
                unit->preprocessing_tokens[index].token)) {
            continue;
        }
        left = noc__macro_replacement_previous_significant(unit,
                                                           replacement,
                                                           index);
        right = noc__macro_replacement_next_significant(unit,
                                                        replacement,
                                                        index);
        if (left == NOC_TOKEN_INDEX_NONE || right == NOC_TOKEN_INDEX_NONE ||
            noc__macro_token_is_paste_operator(
                unit->preprocessing_tokens[left].token) ||
            noc__macro_token_is_paste_operator(
                unit->preprocessing_tokens[right].token)) {
            return false;
        }
    }
    return true;
}

static bool noc__macro_replacement_parameter_is_pasted(
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range replacement,
    size_t index)
{
    size_t adjacent = noc__macro_replacement_previous_significant(unit,
                                                                  replacement,
                                                                  index);
    if (adjacent != NOC_TOKEN_INDEX_NONE &&
        noc__macro_token_is_paste_operator(
            unit->preprocessing_tokens[adjacent].token)) {
        return true;
    }
    adjacent = noc__macro_replacement_next_significant(unit,
                                                       replacement,
                                                       index);
    return adjacent != NOC_TOKEN_INDEX_NONE &&
           noc__macro_token_is_paste_operator(
               unit->preprocessing_tokens[adjacent].token);
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

static size_t noc__macro_stringification_operand(
    const Noc_Preprocessor_Unit *unit,
    const Noc_Macro_Directive *directive,
    size_t operator_index,
    size_t *parameter_index)
{
    size_t operand_index = operator_index + 1;
    while (operand_index < directive->replacement_tokens.end &&
           noc_token_is_trivia(
               unit->preprocessing_tokens[operand_index].token)) {
        operand_index += 1;
    }
    if (operand_index >= directive->replacement_tokens.end) {
        return NOC_TOKEN_INDEX_NONE;
    }
    *parameter_index = noc__macro_replacement_parameter_index(
        unit,
        directive,
        unit->preprocessing_tokens[operand_index].token);
    return *parameter_index == NOC_TOKEN_INDEX_NONE
               ? NOC_TOKEN_INDEX_NONE
               : operand_index;
}

static Noc_Macro_Expansion_Status noc__macro_stringify_argument(
    Noc__Macro_Expansion_Builder *builder,
    const Noc__Macro_Token_Sequence *sequence,
    Noc_Token_Range source,
    const Noc_Macro_Environment_Entry *entry,
    size_t operator_index,
    size_t frame_index,
    size_t hide_set,
    Noc__Macro_Token_Sequence *replacement)
{
    Noc_Buffer spelling = {0};
    Noc_Buffer logical = {0};
    Noc_Macro_Expansion_Token token;
    Noc_Macro_Expansion_Status status = NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    size_t generated_spelling_index;
    size_t index;
    bool emitted_token = false;
    bool pending_space = false;
    if (!noc_buffer_append_cstr(&spelling, "\"")) goto done;
    for (index = source.begin; index < source.end; ++index) {
        Noc_Token source_token = sequence->items[index].token;
        size_t position;
        if (noc_token_is_trivia(source_token)) {
            bool separates = source_token.kind == NOC_TOKEN_LINE_COMMENT ||
                             source_token.kind == NOC_TOKEN_BLOCK_COMMENT;
            if (!separates) {
                if (!noc_token_logical_text(source_token, &logical)) goto done;
                separates = logical.count != 0;
            }
            if (separates && emitted_token) pending_space = true;
            continue;
        }
        if (pending_space && !noc_buffer_append_cstr(&spelling, " ")) goto done;
        pending_space = false;
        if (!noc_token_logical_text(source_token, &logical)) goto done;
        for (position = 0; position < logical.count; ++position) {
            char character = logical.items[position];
            if ((source_token.kind == NOC_TOKEN_STRING ||
                 source_token.kind == NOC_TOKEN_CHARACTER) &&
                (character == '\\' || character == '"') &&
                !noc_buffer_append_cstr(&spelling, "\\")) {
                goto done;
            }
            if (!noc_buffer_append(&spelling, &character, 1)) goto done;
        }
        emitted_token = true;
    }
    if (!noc_buffer_append_cstr(&spelling, "\"")) goto done;
    status = noc__macro_generated_spelling_append(builder,
                                                  &spelling,
                                                  &generated_spelling_index);
    if (status != NOC_MACRO_EXPANSION_OK) goto done;
    token = noc__macro_expansion_token(entry->unit,
                                       operator_index,
                                       frame_index,
                                       NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION);
    token.token.kind = NOC_TOKEN_STRING;
    token.token.text = builder->output->generated_spellings[
        generated_spelling_index];
    token.generated_spelling_index = generated_spelling_index;
    status = noc__macro_token_sequence_append(builder,
                                              replacement,
                                              token,
                                              hide_set);

done:
    noc_buffer_free(&logical);
    noc_buffer_free(&spelling);
    return status;
}

static Noc_Macro_Expansion_Status noc__macro_append_raw_paste_argument(
    Noc__Macro_Expansion_Builder *builder,
    const Noc__Macro_Token_Sequence *sequence,
    Noc_Token_Range source,
    const Noc_Macro_Environment_Entry *entry,
    size_t parameter_token_index,
    size_t frame_index,
    size_t replacement_hide_set,
    Noc__Macro_Token_Sequence *replacement)
{
    Noc_Macro_Expansion_Status status;
    size_t index;
    while (source.begin < source.end &&
           noc_token_is_trivia(sequence->items[source.begin].token)) {
        source.begin += 1;
    }
    while (source.end > source.begin &&
           noc_token_is_trivia(sequence->items[source.end - 1].token)) {
        source.end -= 1;
    }
    if (source.begin == source.end) {
        return noc__macro_token_sequence_append_flagged(
            builder,
            replacement,
            noc__macro_expansion_token(entry->unit,
                                       parameter_token_index,
                                       frame_index,
                                       NOC_MACRO_EXPANSION_TOKEN_ARGUMENT),
            replacement_hide_set,
            NOC__MACRO_TOKEN_PLACEMARKER);
    }
    for (index = source.begin; index < source.end; ++index) {
        Noc_Macro_Expansion_Token token = sequence->items[index];
        size_t hide_set;
        token.frame_index = frame_index;
        if (token.generated_spelling_index == NOC_TOKEN_INDEX_NONE) {
            token.origin = NOC_MACRO_EXPANSION_TOKEN_ARGUMENT;
        }
        status = noc__macro_hide_set_union(builder,
                                           sequence->hide_sets[index],
                                           replacement_hide_set,
                                           &hide_set);
        if (status != NOC_MACRO_EXPANSION_OK) return status;
        status = noc__macro_token_sequence_append(builder,
                                                  replacement,
                                                  token,
                                                  hide_set);
        if (status != NOC_MACRO_EXPANSION_OK) return status;
    }
    return NOC_MACRO_EXPANSION_OK;
}

static Noc_Macro_Expansion_Status noc__macro_paste_tokens(
    Noc__Macro_Expansion_Builder *builder,
    const Noc_Macro_Expansion_Token *left,
    const Noc_Macro_Expansion_Token *right,
    const Noc_Macro_Expansion_Token *operator_token,
    size_t hide_set,
    Noc__Macro_Token_Sequence *result)
{
    Noc_Buffer spelling = {0};
    Noc_Buffer logical = {0};
    Noc_Lexer lexer;
    Noc_Token lexed;
    Noc_Token tail;
    Noc_Macro_Expansion_Token token;
    Noc_Macro_Expansion_Status status = NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    size_t generated_spelling_index;
    if (!noc_token_logical_text(left->token, &logical) ||
        !noc_buffer_append_slice(
            &spelling,
            (Noc_Slice){logical.items, logical.count}) ||
        !noc_token_logical_text(right->token, &logical) ||
        !noc_buffer_append_slice(
            &spelling,
            (Noc_Slice){logical.items, logical.count})) {
        goto done;
    }
    noc_lexer_init(&lexer, "<macro-paste>", spelling.items, spelling.count);
    lexer.beginning_of_line = false;
    lexed = noc_lexer_next(&lexer);
    tail = noc_lexer_next(&lexer);
    if (lexed.kind == NOC_TOKEN_EOF ||
        lexed.kind == NOC_TOKEN_PREPROCESSOR ||
        lexed.kind == NOC_TOKEN_HEADER_NAME ||
        lexed.kind == NOC_TOKEN_INVALID ||
        noc_token_is_trivia(lexed) ||
        lexed.text.data != spelling.items ||
        lexed.text.count != spelling.count ||
        tail.kind != NOC_TOKEN_EOF) {
        status = NOC_MACRO_EXPANSION_INVALID_PASTE;
        goto done;
    }
    status = noc__macro_generated_spelling_append(builder,
                                                  &spelling,
                                                  &generated_spelling_index);
    if (status != NOC_MACRO_EXPANSION_OK) goto done;
    token = *operator_token;
    token.token.kind = lexed.kind;
    token.token.text = builder->output->generated_spellings[
        generated_spelling_index];
    token.generated_spelling_index = generated_spelling_index;
    token.origin = NOC_MACRO_EXPANSION_TOKEN_PASTE;
    status = noc__macro_token_sequence_append(builder,
                                              result,
                                              token,
                                              hide_set);

done:
    noc_buffer_free(&logical);
    noc_buffer_free(&spelling);
    return status;
}

static Noc_Macro_Expansion_Status noc__macro_resolve_pastes(
    Noc__Macro_Expansion_Builder *builder,
    Noc__Macro_Token_Sequence *replacement)
{
    Noc_Macro_Expansion_Status status;
    size_t operator_index;
    for (;;) {
        Noc__Macro_Token_Sequence result = {0};
        size_t left;
        size_t right;
        size_t hide_set;
        for (operator_index = 0;
             operator_index < replacement->count &&
             !(replacement->flags[operator_index] &
               NOC__MACRO_TOKEN_PASTE_OPERATOR);
             ++operator_index) {
        }
        if (operator_index == replacement->count) break;
        left = operator_index;
        while (left > 0) {
            left -= 1;
            if (!noc_token_is_trivia(replacement->items[left].token)) break;
        }
        right = operator_index + 1;
        while (right < replacement->count &&
               noc_token_is_trivia(replacement->items[right].token)) {
            right += 1;
        }
        if (left == operator_index || right >= replacement->count ||
            (replacement->flags[left] & NOC__MACRO_TOKEN_PASTE_OPERATOR) ||
            (replacement->flags[right] & NOC__MACRO_TOKEN_PASTE_OPERATOR)) {
            return NOC_MACRO_EXPANSION_INVALID_DEFINITION;
        }
        if ((replacement->flags[left] & NOC__MACRO_TOKEN_PLACEMARKER) &&
            (replacement->flags[right] & NOC__MACRO_TOKEN_PLACEMARKER)) {
            status = noc__macro_hide_set_intersection(
                builder,
                replacement->hide_sets[left],
                replacement->hide_sets[right],
                &hide_set);
            if (status == NOC_MACRO_EXPANSION_OK) {
                status = noc__macro_token_sequence_append_flagged(
                    builder,
                    &result,
                    replacement->items[left],
                    hide_set,
                    NOC__MACRO_TOKEN_PLACEMARKER);
            }
        } else if (replacement->flags[left] &
                   NOC__MACRO_TOKEN_PLACEMARKER) {
            status = noc__macro_token_sequence_append(
                builder,
                &result,
                replacement->items[right],
                replacement->hide_sets[right]);
        } else if (replacement->flags[right] &
                   NOC__MACRO_TOKEN_PLACEMARKER) {
            status = noc__macro_token_sequence_append(
                builder,
                &result,
                replacement->items[left],
                replacement->hide_sets[left]);
        } else {
            status = noc__macro_hide_set_intersection(
                builder,
                replacement->hide_sets[left],
                replacement->hide_sets[right],
                &hide_set);
            if (status == NOC_MACRO_EXPANSION_OK) {
                status = noc__macro_paste_tokens(
                    builder,
                    &replacement->items[left],
                    &replacement->items[right],
                    &replacement->items[operator_index],
                    hide_set,
                    &result);
            }
        }
        if (status == NOC_MACRO_EXPANSION_OK) {
            status = noc__macro_token_sequence_splice(
                builder,
                replacement,
                (Noc_Token_Range){left, right + 1},
                &result);
        }
        noc__macro_token_sequence_free(&result);
        if (status != NOC_MACRO_EXPANSION_OK) return status;
    }
    {
        size_t read;
        size_t write = 0;
        for (read = 0; read < replacement->count; ++read) {
            if (replacement->flags[read] & NOC__MACRO_TOKEN_PLACEMARKER) {
                continue;
            }
            if (replacement->flags[read] != 0) {
                return NOC_MACRO_EXPANSION_INVALID_DEFINITION;
            }
            if (write != read) {
                replacement->items[write] = replacement->items[read];
                replacement->hide_sets[write] = replacement->hide_sets[read];
                replacement->flags[write] = 0;
            }
            write += 1;
        }
        replacement->count = write;
    }
    return NOC_MACRO_EXPANSION_OK;
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
    if (!noc__macro_replacement_pastes_are_valid(
            entry->unit,
            directive->replacement_tokens)) {
        return NOC_MACRO_EXPANSION_INVALID_DEFINITION;
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
        Noc_Macro_Expansion_Token token = noc__macro_expansion_token(
            entry->unit,
            index,
            frame_index,
            NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT);
        if (noc__macro_token_is_paste_operator(token.token)) {
            status = noc__macro_token_sequence_append_flagged(
                builder,
                &replacement,
                token,
                replacement_hide_set,
                NOC__MACRO_TOKEN_PASTE_OPERATOR);
        } else {
            status = noc__macro_token_sequence_append(builder,
                                                      &replacement,
                                                      token,
                                                      replacement_hide_set);
        }
        if (status != NOC_MACRO_EXPANSION_OK) goto done;
    }
    status = noc__macro_resolve_pastes(builder, &replacement);
    if (status != NOC_MACRO_EXPANSION_OK) goto done;
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
    if (!noc__macro_replacement_pastes_are_valid(
            entry->unit,
            directive->replacement_tokens)) {
        return NOC_MACRO_EXPANSION_INVALID_DEFINITION;
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
        Noc_Token replacement_token =
            entry->unit->preprocessing_tokens[index].token;
        if (noc__macro_token_is_paste_operator(replacement_token)) {
            continue;
        }
        if (noc_token_is_punct(replacement_token, "#") ||
            noc_token_is_punct(replacement_token, "%:")) {
            size_t parameter_index;
            size_t operand_index = noc__macro_stringification_operand(
                entry->unit,
                directive,
                index,
                &parameter_index);
            if (operand_index == NOC_TOKEN_INDEX_NONE) {
                status = NOC_MACRO_EXPANSION_INVALID_DEFINITION;
                goto done;
            }
            index = operand_index;
            continue;
        }
        size_t parameter_index = noc__macro_replacement_parameter_index(
            entry->unit,
            directive,
            replacement_token);
        if (parameter_index != NOC_TOKEN_INDEX_NONE &&
            !noc__macro_replacement_parameter_is_pasted(
                entry->unit,
                directive->replacement_tokens,
                index)) {
            arguments[parameter_index].expanded_use_count += 1;
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
        if (arguments[index].expanded_use_count == 0) continue;
        for (token_index = source.begin; token_index < source.end; ++token_index) {
            Noc_Macro_Expansion_Token token = sequence->items[token_index];
            token.frame_index = frame_index;
            if (token.generated_spelling_index == NOC_TOKEN_INDEX_NONE) {
                token.origin = NOC_MACRO_EXPANSION_TOKEN_ARGUMENT;
            }
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
        if (arguments[index].expanded_use_count != 0 &&
            arguments[index].tokens.count >
                builder->limits.max_output_tokens /
                    arguments[index].expanded_use_count) {
            status = NOC_MACRO_EXPANSION_OUTPUT_LIMIT;
            goto done;
        }
    }
    for (index = directive->replacement_tokens.begin;
         index < directive->replacement_tokens.end;
         ++index) {
        Noc_Token replacement_token =
            entry->unit->preprocessing_tokens[index].token;
        if (noc__macro_token_is_paste_operator(replacement_token)) {
            status = noc__macro_token_sequence_append_flagged(
                builder,
                &replacement,
                noc__macro_expansion_token(
                    entry->unit,
                    index,
                    frame_index,
                    NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT),
                replacement_hide_set,
                NOC__MACRO_TOKEN_PASTE_OPERATOR);
            if (status != NOC_MACRO_EXPANSION_OK) goto done;
            continue;
        }
        if (noc_token_is_punct(replacement_token, "#") ||
            noc_token_is_punct(replacement_token, "%:")) {
            size_t parameter_index;
            size_t operand_index = noc__macro_stringification_operand(
                entry->unit,
                directive,
                index,
                &parameter_index);
            if (operand_index == NOC_TOKEN_INDEX_NONE) {
                status = NOC_MACRO_EXPANSION_INVALID_DEFINITION;
                goto done;
            }
            status = noc__macro_stringify_argument(
                builder,
                sequence,
                arguments[parameter_index].source_tokens,
                entry,
                index,
                frame_index,
                replacement_hide_set,
                &replacement);
            index = operand_index;
            if (status != NOC_MACRO_EXPANSION_OK) goto done;
            continue;
        }
        size_t parameter_index = noc__macro_replacement_parameter_index(
            entry->unit,
            directive,
            replacement_token);
        if (parameter_index != NOC_TOKEN_INDEX_NONE) {
            if (noc__macro_replacement_parameter_is_pasted(
                    entry->unit,
                    directive->replacement_tokens,
                    index)) {
                status = noc__macro_append_raw_paste_argument(
                    builder,
                    sequence,
                    arguments[parameter_index].source_tokens,
                    entry,
                    index,
                    frame_index,
                    replacement_hide_set,
                    &replacement);
            } else {
                status = noc__macro_token_sequence_append_sequence(
                    builder,
                    &replacement,
                    &arguments[parameter_index].tokens);
            }
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
    status = noc__macro_resolve_pastes(builder, &replacement);
    if (status != NOC_MACRO_EXPANSION_OK) goto done;
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
        Noc_Macro_Builtin_Kind builtin_kind = NOC_MACRO_BUILTIN_NONE;
        size_t environment_entry_index = NOC_TOKEN_INDEX_NONE;
        Noc_Macro_Expansion_Status status;
        if (builder->preserve_defined_operands &&
            noc_token_is_identifier(token->token, "defined")) {
            size_t operand = cursor + 1;
            while (operand < sequence->count &&
                   noc_token_is_trivia(sequence->items[operand].token)) {
                operand += 1;
            }
            if (operand < sequence->count &&
                noc_token_is_punct(sequence->items[operand].token, "(")) {
                operand += 1;
                while (operand < sequence->count &&
                       noc_token_is_trivia(sequence->items[operand].token)) {
                    operand += 1;
                }
            }
            cursor = operand < sequence->count ? operand + 1 : cursor + 1;
            continue;
        }
        if (token->token.kind == NOC_TOKEN_IDENTIFIER) {
            entry = noc_macro_environment_lookup_before(builder->environment,
                                                        token->token.text,
                                                        builder->entry_limit);
        }
        if (entry) {
            environment_entry_index =
                (size_t)(entry - builder->environment->items);
            directive = noc__macro_environment_entry_directive(entry);
        } else if (token->token.kind == NOC_TOKEN_IDENTIFIER) {
            builtin_kind = noc_macro_builtin_kind_from_name(token->token.text);
            if (!noc__macro_builtin_mask_contains(
                    builder->available_builtin_mask,
                    builtin_kind)) {
                builtin_kind = NOC_MACRO_BUILTIN_NONE;
            }
        }
        if (!directive) {
            if (builtin_kind != NOC_MACRO_BUILTIN_NONE) {
                status = noc__macro_expand_builtin(builder,
                                                   sequence,
                                                   cursor,
                                                   builtin_kind);
                if (status != NOC_MACRO_EXPANSION_OK) return status;
            }
            cursor += 1;
            continue;
        }
        if (noc__macro_hide_set_contains(builder,
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

NOC__PRIVATE Noc_Macro_Expansion_Status noc__macro_expansion_build(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Options options,
    bool preserve_defined_operands,
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
        !noc__macro_expansion_options_are_valid(options)) {
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
    parsed.available_builtin_mask =
        noc__macro_builtin_mask_from_options(options);
    memset(&builder, 0, sizeof(builder));
    builder.environment = environment;
    builder.entry_limit = entry_limit;
    builder.limits = options.limits;
    builder.options = options;
    builder.available_builtin_mask = parsed.available_builtin_mask;
    builder.output = &parsed;
    builder.preserve_defined_operands = preserve_defined_operands;
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
    free(sequence.flags);
    sequence.flags = NULL;
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

NOCDEF Noc_Macro_Expansion_Status noc_macro_expansion_build(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Limits limits,
    Noc_Macro_Expansion *output)
{
    Noc_Macro_Expansion_Options options = noc_macro_expansion_default_options();
    options.limits = limits;
    return noc_macro_expansion_build_with_options(environment,
                                                  entry_limit,
                                                  input_unit,
                                                  input_tokens,
                                                  options,
                                                  output);
}

NOCDEF Noc_Macro_Expansion_Status noc_macro_expansion_build_with_options(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Options options,
    Noc_Macro_Expansion *output)
{
    return noc__macro_expansion_build(environment,
                                      entry_limit,
                                      input_unit,
                                      input_tokens,
                                      options,
                                      false,
                                      output);
}

NOCDEF Noc_Macro_Expansion_Status noc_macro_expansion_build_condition(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Limits limits,
    Noc_Macro_Expansion *output)
{
    Noc_Macro_Expansion_Options options = noc_macro_expansion_default_options();
    options.limits = limits;
    return noc_macro_expansion_build_condition_with_options(environment,
                                                            entry_limit,
                                                            input_unit,
                                                            input_tokens,
                                                            options,
                                                            output);
}

NOCDEF Noc_Macro_Expansion_Status
noc_macro_expansion_build_condition_with_options(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Options options,
    Noc_Macro_Expansion *output)
{
    return noc__macro_expansion_build(environment,
                                      entry_limit,
                                      input_unit,
                                      input_tokens,
                                      options,
                                      true,
                                      output);
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
#undef NOC__MACRO_TOKEN_PLACEMARKER
#undef NOC__MACRO_TOKEN_PASTE_OPERATOR
#undef NOC__MACRO_BUILTIN_SUPPORTED_MASK
#undef NOC__MACRO_BUILTIN_ALWAYS_MASK
#undef NOC__MACRO_BUILTIN_BIT

#endif /* NOC_MACRO_EXPANSION_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
