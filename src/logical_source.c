#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_LOGICAL_SOURCE_IMPLEMENTATION_INCLUDED
#define NOC_LOGICAL_SOURCE_IMPLEMENTATION_INCLUDED

enum {
    NOC__LOGICAL_SOURCE_DEFAULT_MAX_BYTES = 16 * 1024 * 1024,
    NOC__LOGICAL_SOURCE_DEFAULT_MAX_INPUT_BYTES = 64 * 1024 * 1024,
    /* A separator may follow every significant expansion token except the last. */
    NOC__LOGICAL_SOURCE_DEFAULT_MAX_TOKENS = 2 * 1024 * 1024,
    NOC__LOGICAL_SOURCE_DEFAULT_MAX_FRAMES = 1024 * 1024,
    NOC__LOGICAL_SOURCE_DEFAULT_MAX_FILES = 4096,
    NOC__LOGICAL_SOURCE_DEFAULT_MAX_PATH_BYTES = 16 * 1024 * 1024,
    NOC__LOGICAL_SOURCE_VALIDATION_INTERVAL = 256,
    NOC__LOGICAL_SOURCE_CANCEL_INTERVAL = 4096,
    /* __STDC_VERSION__ is the longest anchor spelling classified here. */
    NOC__LOGICAL_SOURCE_MAX_ANCHOR_LENGTH = 16,
};

struct Noc_Logical_Source_Impl {
    char *text;
    size_t text_count;
    size_t *line_starts;
    size_t line_count;
    Noc_Logical_Token *tokens;
    Noc_Logical_Token_Macro_Provenance *provenances;
    size_t token_count;
    size_t token_capacity;
    Noc_Logical_Source_File *files;
    size_t file_count;
    size_t file_capacity;
    Noc_Logical_Macro_Frame *frames;
    size_t frame_count;
    size_t generation;
};

/* Unit pointers are temporary hash keys used only while copying durable source
   identities and paths. They are discarded before the result is published. */
typedef struct {
    Noc_Logical_Source_Impl *implementation;
    const Noc_Preprocessor_Unit **unit_keys;
    size_t *hash_slots;
    size_t hash_slot_count;
    size_t path_bytes;
    size_t input_bytes_examined;
    const Noc_Logical_Source_Options *options;
} Noc__Logical_Source_Builder;

static void noc__logical_source_impl_free(
    Noc_Logical_Source_Impl *implementation)
{
    size_t index;
    if (!implementation) return;
    for (index = 0; index < implementation->file_count; ++index) {
        free((void *)implementation->files[index].path.data);
    }
    free(implementation->frames);
    free(implementation->files);
    free(implementation->provenances);
    free(implementation->tokens);
    free(implementation->line_starts);
    free(implementation->text);
    free(implementation);
}

static void noc__logical_source_builder_deinit(
    Noc__Logical_Source_Builder *builder)
{
    if (!builder) return;
    free(builder->hash_slots);
    free(builder->unit_keys);
    memset(builder, 0, sizeof(*builder));
}

static bool noc__logical_source_origin_is_valid(
    Noc_Macro_Expansion_Token_Origin origin)
{
    return origin >= NOC_MACRO_EXPANSION_TOKEN_INPUT &&
           origin <= NOC_MACRO_EXPANSION_TOKEN_BUILTIN;
}

static bool noc__logical_source_builtin_is_valid(Noc_Macro_Builtin_Kind kind)
{
    return kind >= NOC_MACRO_BUILTIN_NONE &&
           kind <= NOC_MACRO_BUILTIN_TIME;
}

static bool noc__logical_source_site_is_valid(
    const Noc_Logical_Source_Impl *implementation,
    Noc_Logical_Physical_Site site)
{
    return site.file_index < implementation->file_count &&
           site.bytes.begin <= site.bytes.end && site.line != 0 &&
           site.byte_column != 0;
}

static bool noc__logical_source_provenance_is_valid(
    const Noc_Logical_Source_Impl *implementation,
    const Noc_Logical_Token_Macro_Provenance *provenance)
{
    if (!noc__logical_source_site_is_valid(implementation,
                                           provenance->anchor) ||
        !noc__logical_source_origin_is_valid(provenance->macro_origin) ||
        !noc__logical_source_builtin_is_valid(provenance->builtin_kind) ||
        (provenance->macro_origin == NOC_MACRO_EXPANSION_TOKEN_BUILTIN) !=
            (provenance->builtin_kind != NOC_MACRO_BUILTIN_NONE)) {
        return false;
    }
    switch (provenance->macro_origin) {
    case NOC_MACRO_EXPANSION_TOKEN_INPUT:
        return provenance->macro_frame_index == NOC_TOKEN_INDEX_NONE;
    case NOC_MACRO_EXPANSION_TOKEN_ARGUMENT:
    case NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT:
    case NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION:
    case NOC_MACRO_EXPANSION_TOKEN_PASTE:
        return provenance->macro_frame_index < implementation->frame_count;
    case NOC_MACRO_EXPANSION_TOKEN_BUILTIN:
        return provenance->macro_frame_index == NOC_TOKEN_INDEX_NONE ||
               provenance->macro_frame_index < implementation->frame_count;
    }
    return false;
}

/* Deep validation follows only memory owned by the result. */
static bool noc__logical_source_owned_is_valid(
    const Noc_Logical_Source *source)
{
    const Noc_Logical_Source_Impl *implementation;
    size_t index;
    size_t previous_end = 0;
    if (!source || !source->impl || source->generation == 0) return false;
    implementation = source->impl;
    if (implementation->generation != source->generation ||
        !implementation->text ||
        implementation->text[implementation->text_count] != '\0' ||
        !implementation->line_starts || implementation->line_count == 0 ||
        implementation->line_starts[0] != 0 ||
        implementation->token_count > implementation->token_capacity ||
        (implementation->token_capacity == 0) !=
            (implementation->tokens == NULL) ||
        (implementation->token_capacity == 0) !=
            (implementation->provenances == NULL) ||
        implementation->file_count > implementation->file_capacity ||
        (implementation->file_capacity == 0) !=
            (implementation->files == NULL) ||
        (implementation->frame_count == 0) !=
            (implementation->frames == NULL)) {
        return false;
    }
    for (index = 1; index < implementation->line_count; ++index) {
        size_t line_start = implementation->line_starts[index];
        if (line_start <= implementation->line_starts[index - 1] ||
            line_start > implementation->text_count ||
            (implementation->text[line_start - 1] != '\n' &&
             implementation->text[line_start - 1] != '\r')) {
            return false;
        }
    }
    for (index = 0; index < implementation->file_count; ++index) {
        const Noc_Logical_Source_File *file = &implementation->files[index];
        if (file->file_id == NOC_FILE_ID_NONE ||
            file->document_generation == 0 ||
            !noc__source_class_is_valid(file->source_class) || !file->path.data ||
            file->path.data[file->path.count] != '\0') {
            return false;
        }
    }
    for (index = 0; index < implementation->frame_count; ++index) {
        const Noc_Logical_Macro_Frame *frame = &implementation->frames[index];
        if (!noc__logical_source_site_is_valid(implementation,
                                                frame->definition) ||
            !noc__logical_source_site_is_valid(implementation,
                                                frame->invocation) ||
            (frame->parent_macro_frame_index != NOC_TOKEN_INDEX_NONE &&
             frame->parent_macro_frame_index >= index)) {
            return false;
        }
    }
    for (index = 0; index < implementation->token_count; ++index) {
        const Noc_Logical_Token *token = &implementation->tokens[index];
        const Noc_Logical_Token_Macro_Provenance *provenance =
            &implementation->provenances[index];
        if (token->kind < NOC_TOKEN_EOF || token->kind > NOC_TOKEN_INVALID ||
            token->generation != source->generation ||
            token->bytes.begin != previous_end ||
            token->bytes.begin > token->bytes.end ||
            token->bytes.end > implementation->text_count ||
            (token->flags & ~NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) != 0) {
            return false;
        }
        if ((token->flags & NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) != 0) {
            if (token->flags != NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR ||
                token->kind != NOC_TOKEN_WHITESPACE ||
                token->bytes.end - token->bytes.begin != 1 ||
                implementation->text[token->bytes.begin] != ' ') {
                return false;
            }
        } else if (!noc__logical_source_provenance_is_valid(implementation,
                                                            provenance)) {
            return false;
        }
        previous_end = token->bytes.end;
    }
    return previous_end == implementation->text_count;
}

static bool noc__logical_source_handle_is_current(
    const Noc_Logical_Source *source)
{
    return source && source->impl && source->generation != 0 &&
           source->impl->generation == source->generation;
}

NOCDEF Noc_Logical_Source_Options noc_logical_source_default_options(void)
{
    Noc_Logical_Source_Options options;
    options.max_source_bytes = NOC__LOGICAL_SOURCE_DEFAULT_MAX_BYTES;
    options.max_input_bytes_examined =
        NOC__LOGICAL_SOURCE_DEFAULT_MAX_INPUT_BYTES;
    options.max_tokens = NOC__LOGICAL_SOURCE_DEFAULT_MAX_TOKENS;
    options.max_macro_frames = NOC__LOGICAL_SOURCE_DEFAULT_MAX_FRAMES;
    options.max_source_files = NOC__LOGICAL_SOURCE_DEFAULT_MAX_FILES;
    options.max_path_bytes = NOC__LOGICAL_SOURCE_DEFAULT_MAX_PATH_BYTES;
    options.should_cancel = NULL;
    options.cancel_user_data = NULL;
    return options;
}

NOCDEF const char *noc_logical_source_status_name(
    Noc_Logical_Source_Status status)
{
    switch (status) {
    case NOC_LOGICAL_SOURCE_OK: return "ok";
    case NOC_LOGICAL_SOURCE_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_LOGICAL_SOURCE_STALE: return "stale";
    case NOC_LOGICAL_SOURCE_CANCELLED: return "cancelled";
    case NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED: return "limit-exceeded";
    case NOC_LOGICAL_SOURCE_GENERATION_EXHAUSTED:
        return "generation-exhausted";
    case NOC_LOGICAL_SOURCE_OUT_OF_MEMORY: return "out-of-memory";
    }
    return "unknown";
}

NOCDEF void noc_logical_source_free(Noc_Logical_Source *source)
{
    Noc_Logical_Source_Impl *implementation;
    size_t generation;
    if (!source) return;
    implementation = source->impl;
    generation = source->generation;
    memset(source, 0, sizeof(*source));
    source->generation = generation;
    noc__logical_source_impl_free(implementation);
}

NOCDEF bool noc_logical_source_is_valid(const Noc_Logical_Source *source)
{
    return noc__logical_source_owned_is_valid(source);
}

NOCDEF size_t noc_logical_source_generation(
    const Noc_Logical_Source *source)
{
    return noc_logical_source_is_valid(source) ? source->generation : 0;
}

static bool noc__logical_source_should_cancel(
    const Noc_Logical_Source_Options *options)
{
    return options->should_cancel &&
           options->should_cancel(options->cancel_user_data);
}

static Noc_Logical_Source_Status noc__logical_source_charge_input_bytes(
    const Noc_Logical_Source_Options *options,
    size_t *input_bytes_examined,
    size_t count)
{
    if (*input_bytes_examined > options->max_input_bytes_examined ||
        count > options->max_input_bytes_examined - *input_bytes_examined) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    *input_bytes_examined += count;
    return NOC_LOGICAL_SOURCE_OK;
}

/* Generated tokens point at a physical operator or builtin name. Classify that
   anchor once, charging every physical byte (including phase-2 splices) to the
   same budget later used for rendered spellings. The fixed buffer bounds all
   comparisons after the cancellable physical scan. */
static Noc_Logical_Source_Status noc__logical_source_classify_anchor(
    Noc_Slice spelling,
    const Noc_Logical_Source_Options *options,
    size_t *input_bytes_examined,
    bool *is_stringification_operator,
    bool *is_paste_operator,
    Noc_Macro_Builtin_Kind *builtin_kind)
{
    char logical[NOC__LOGICAL_SOURCE_MAX_ANCHOR_LENGTH + 1];
    size_t position = 0;
    size_t logical_count = 0;
    size_t next_cancel_poll = 0;
    Noc_Logical_Source_Status status;
    *is_stringification_operator = false;
    *is_paste_operator = false;
    *builtin_kind = NOC_MACRO_BUILTIN_NONE;
    if (!spelling.data && spelling.count != 0) {
        return NOC_LOGICAL_SOURCE_STALE;
    }
    status = noc__logical_source_charge_input_bytes(options,
                                                     input_bytes_examined,
                                                     spelling.count);
    if (status != NOC_LOGICAL_SOURCE_OK) return status;
    while (position < spelling.count) {
        size_t splice;
        if (position >= next_cancel_poll) {
            if (noc__logical_source_should_cancel(options)) {
                return NOC_LOGICAL_SOURCE_CANCELLED;
            }
            if (position > SIZE_MAX - NOC__LOGICAL_SOURCE_CANCEL_INTERVAL) {
                next_cancel_poll = SIZE_MAX;
            } else {
                next_cancel_poll =
                    position + NOC__LOGICAL_SOURCE_CANCEL_INTERVAL;
            }
        }
        splice = noc__splice_length(spelling.data, spelling.count, position);
        if (splice != 0) {
            position += splice;
            continue;
        }
        if (logical_count == NOC__LOGICAL_SOURCE_MAX_ANCHOR_LENGTH) {
            return NOC_LOGICAL_SOURCE_OK;
        }
        logical[logical_count++] = spelling.data[position++];
    }
    logical[logical_count] = '\0';
    *is_stringification_operator =
        strcmp(logical, "#") == 0 || strcmp(logical, "%:") == 0;
    *is_paste_operator =
        strcmp(logical, "##") == 0 || strcmp(logical, "%:%:") == 0;
    *builtin_kind = noc_macro_builtin_kind_from_name(
        (Noc_Slice){logical, logical_count});
    return NOC_LOGICAL_SOURCE_OK;
}

static Noc_Logical_Source_Status noc__logical_source_reserve_token(
    Noc_Logical_Source_Impl *implementation,
    size_t max_tokens)
{
    Noc_Logical_Token *tokens;
    Noc_Logical_Token_Macro_Provenance *provenances;
    size_t capacity;
    if (implementation->token_count >= max_tokens) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    if (implementation->token_count < implementation->token_capacity) {
        return NOC_LOGICAL_SOURCE_OK;
    }
    if (implementation->token_capacity == 0) {
        capacity = max_tokens < 256 ? max_tokens : 256;
    } else if (implementation->token_capacity > max_tokens / 2) {
        capacity = max_tokens;
    } else {
        capacity = implementation->token_capacity * 2;
    }
    if (capacity <= implementation->token_capacity ||
        capacity > SIZE_MAX / sizeof(*tokens) ||
        capacity > SIZE_MAX / sizeof(*provenances)) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    tokens = (Noc_Logical_Token *)malloc(capacity * sizeof(*tokens));
    provenances = (Noc_Logical_Token_Macro_Provenance *)malloc(
        capacity * sizeof(*provenances));
    if (!tokens || !provenances) {
        free(provenances);
        free(tokens);
        return NOC_LOGICAL_SOURCE_OUT_OF_MEMORY;
    }
    if (implementation->token_count != 0) {
        memcpy(tokens,
               implementation->tokens,
               implementation->token_count * sizeof(*tokens));
        memcpy(provenances,
               implementation->provenances,
               implementation->token_count * sizeof(*provenances));
    }
    free(implementation->provenances);
    free(implementation->tokens);
    implementation->tokens = tokens;
    implementation->provenances = provenances;
    implementation->token_capacity = capacity;
    return NOC_LOGICAL_SOURCE_OK;
}

static size_t noc__logical_source_hash_unit(
    const Noc_Preprocessor_Unit *unit)
{
    uintptr_t value = (uintptr_t)unit;
    value ^= value >> 4;
    value *= (uintptr_t)2654435761u;
    value ^= value >> 9;
    return (size_t)value;
}

static Noc_Logical_Source_Status noc__logical_source_rehash_units(
    Noc__Logical_Source_Builder *builder,
    size_t slot_count)
{
    size_t *slots;
    size_t index;
    if (slot_count == 0 || slot_count > SIZE_MAX / sizeof(*slots)) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    slots = (size_t *)malloc(slot_count * sizeof(*slots));
    if (!slots) return NOC_LOGICAL_SOURCE_OUT_OF_MEMORY;
    for (index = 0; index < slot_count; ++index) {
        slots[index] = NOC_TOKEN_INDEX_NONE;
    }
    for (index = 0; index < builder->implementation->file_count; ++index) {
        size_t slot = noc__logical_source_hash_unit(builder->unit_keys[index]) &
                      (slot_count - 1);
        while (slots[slot] != NOC_TOKEN_INDEX_NONE) {
            slot = (slot + 1) & (slot_count - 1);
        }
        slots[slot] = index;
    }
    free(builder->hash_slots);
    builder->hash_slots = slots;
    builder->hash_slot_count = slot_count;
    return NOC_LOGICAL_SOURCE_OK;
}

static Noc_Logical_Source_Status noc__logical_source_prepare_unit_slot(
    Noc__Logical_Source_Builder *builder)
{
    size_t slot_count = builder->hash_slot_count;
    if (slot_count != 0 &&
        builder->implementation->file_count + 1 <= slot_count / 2) {
        return NOC_LOGICAL_SOURCE_OK;
    }
    if (slot_count == 0) {
        slot_count = 16;
    } else {
        if (slot_count > SIZE_MAX / 2) {
            return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
        }
        slot_count *= 2;
    }
    return noc__logical_source_rehash_units(builder, slot_count);
}

static Noc_Logical_Source_Status noc__logical_source_reserve_file(
    Noc__Logical_Source_Builder *builder)
{
    Noc_Logical_Source_Impl *implementation = builder->implementation;
    Noc_Logical_Source_File *files;
    const Noc_Preprocessor_Unit **keys;
    size_t capacity;
    if (implementation->file_count >= builder->options->max_source_files) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    if (implementation->file_count < implementation->file_capacity) {
        return NOC_LOGICAL_SOURCE_OK;
    }
    if (implementation->file_capacity == 0) {
        capacity = builder->options->max_source_files < 16
                       ? builder->options->max_source_files
                       : 16;
    } else if (implementation->file_capacity >
               builder->options->max_source_files / 2) {
        capacity = builder->options->max_source_files;
    } else {
        capacity = implementation->file_capacity * 2;
    }
    if (capacity <= implementation->file_capacity ||
        capacity > SIZE_MAX / sizeof(*files) ||
        capacity > SIZE_MAX / sizeof(*keys)) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    files = (Noc_Logical_Source_File *)malloc(capacity * sizeof(*files));
    keys = (const Noc_Preprocessor_Unit **)malloc(capacity * sizeof(*keys));
    if (!files || !keys) {
        free(keys);
        free(files);
        return NOC_LOGICAL_SOURCE_OUT_OF_MEMORY;
    }
    if (implementation->file_count != 0) {
        memcpy(files,
               implementation->files,
               implementation->file_count * sizeof(*files));
        memcpy(keys,
               builder->unit_keys,
               implementation->file_count * sizeof(*keys));
    }
    free(builder->unit_keys);
    free(implementation->files);
    builder->unit_keys = keys;
    implementation->files = files;
    implementation->file_capacity = capacity;
    return NOC_LOGICAL_SOURCE_OK;
}

static Noc_Logical_Source_Status noc__logical_source_intern_unit(
    Noc__Logical_Source_Builder *builder,
    const Noc_Preprocessor_Unit *unit,
    size_t *file_index)
{
    Noc_Logical_Source_Impl *implementation = builder->implementation;
    Noc_Logical_Source_Status status;
    size_t slot;
    size_t path_count;
    size_t remaining_path_bytes;
    size_t stored_path_bytes;
    char *path;
    Noc_Logical_Source_File *file;
    if (!unit || !file_index || !noc_preprocessor_unit_is_valid(unit)) {
        return NOC_LOGICAL_SOURCE_STALE;
    }
    status = noc__logical_source_prepare_unit_slot(builder);
    if (status != NOC_LOGICAL_SOURCE_OK) return status;
    slot = noc__logical_source_hash_unit(unit) &
           (builder->hash_slot_count - 1);
    while (builder->hash_slots[slot] != NOC_TOKEN_INDEX_NONE) {
        size_t index = builder->hash_slots[slot];
        if (builder->unit_keys[index] == unit) {
            *file_index = index;
            return NOC_LOGICAL_SOURCE_OK;
        }
        slot = (slot + 1) & (builder->hash_slot_count - 1);
    }
    status = noc__logical_source_reserve_file(builder);
    if (status != NOC_LOGICAL_SOURCE_OK) return status;
    if (builder->path_bytes >= builder->options->max_path_bytes) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    remaining_path_bytes =
        builder->options->max_path_bytes - builder->path_bytes;
    for (path_count = 0;
         path_count < remaining_path_bytes && unit->stream.path[path_count] != '\0';
         ++path_count) {
        if ((path_count % NOC__LOGICAL_SOURCE_CANCEL_INTERVAL) == 0 &&
            noc__logical_source_should_cancel(builder->options)) {
            return NOC_LOGICAL_SOURCE_CANCELLED;
        }
    }
    if (path_count == remaining_path_bytes) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    stored_path_bytes = path_count + 1;
    path = (char *)malloc(stored_path_bytes);
    if (!path) return NOC_LOGICAL_SOURCE_OUT_OF_MEMORY;
    memcpy(path, unit->stream.path, stored_path_bytes);
    file = &implementation->files[implementation->file_count];
    file->file_id = unit->file_id;
    file->document_generation = unit->document_generation;
    file->source_class = unit->source_class;
    file->path.data = path;
    file->path.count = path_count;
    builder->unit_keys[implementation->file_count] = unit;
    builder->hash_slots[slot] = implementation->file_count;
    *file_index = implementation->file_count;
    implementation->file_count += 1;
    builder->path_bytes += stored_path_bytes;
    return NOC_LOGICAL_SOURCE_OK;
}

static Noc_Logical_Source_Status noc__logical_source_make_site(
    Noc__Logical_Source_Builder *builder,
    const Noc_Preprocessor_Unit *unit,
    size_t preprocessing_token_index,
    Noc_Logical_Physical_Site *site)
{
    const Noc_Token *token;
    size_t file_index;
    Noc_Logical_Source_Status status;
    if (!unit || !site || !noc_preprocessor_unit_is_valid(unit) ||
        preprocessing_token_index >= unit->preprocessing_token_count) {
        return NOC_LOGICAL_SOURCE_STALE;
    }
    token = &unit->preprocessing_tokens[preprocessing_token_index].token;
    if (token->location.offset > unit->stream.source_count ||
        token->text.count >
            unit->stream.source_count - token->location.offset ||
        token->location.line == 0 || token->location.column == 0) {
        return NOC_LOGICAL_SOURCE_STALE;
    }
    status = noc__logical_source_intern_unit(builder, unit, &file_index);
    if (status != NOC_LOGICAL_SOURCE_OK) return status;
    site->file_index = file_index;
    site->bytes.begin = token->location.offset;
    site->bytes.end = token->location.offset + token->text.count;
    site->line = token->location.line;
    site->byte_column = token->location.column;
    return NOC_LOGICAL_SOURCE_OK;
}

static Noc_Logical_Source_Status noc__logical_source_append_bytes(
    Noc_Buffer *text,
    const char *bytes,
    size_t count,
    size_t max_source_bytes)
{
    if (text->count > max_source_bytes ||
        count > max_source_bytes - text->count) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    if (!noc_buffer_append(text, bytes, count)) {
        return NOC_LOGICAL_SOURCE_OUT_OF_MEMORY;
    }
    return NOC_LOGICAL_SOURCE_OK;
}

static Noc_Logical_Source_Status noc__logical_source_append_logical_spelling(
    Noc__Logical_Source_Builder *builder,
    Noc_Buffer *text,
    Noc_Slice spelling)
{
    Noc_Logical_Source_Status status;
    size_t position = 0;
    size_t run_start = 0;
    size_t next_cancel_poll = 0;
    if (!spelling.data && spelling.count != 0) {
        return NOC_LOGICAL_SOURCE_STALE;
    }
    status = noc__logical_source_charge_input_bytes(
        builder->options,
        &builder->input_bytes_examined,
        spelling.count);
    if (status != NOC_LOGICAL_SOURCE_OK) return status;
    while (position < spelling.count) {
        size_t splice;
        if (position >= next_cancel_poll) {
            if (noc__logical_source_should_cancel(builder->options)) {
                return NOC_LOGICAL_SOURCE_CANCELLED;
            }
            if (position > SIZE_MAX - NOC__LOGICAL_SOURCE_CANCEL_INTERVAL) {
                next_cancel_poll = SIZE_MAX;
            } else {
                next_cancel_poll =
                    position + NOC__LOGICAL_SOURCE_CANCEL_INTERVAL;
            }
        }
        splice = noc__splice_length(spelling.data, spelling.count, position);
        if (splice == 0) {
            position += 1;
            continue;
        }
        status = noc__logical_source_append_bytes(
            text,
            spelling.data + run_start,
            position - run_start,
            builder->options->max_source_bytes);
        if (status != NOC_LOGICAL_SOURCE_OK) return status;
        position += splice;
        run_start = position;
    }
    return noc__logical_source_append_bytes(
        text,
        spelling.data ? spelling.data + run_start : NULL,
        position - run_start,
        builder->options->max_source_bytes);
}

static Noc_Logical_Source_Status noc__logical_source_publish_token(
    Noc_Logical_Source_Impl *implementation,
    size_t begin,
    size_t end,
    Noc_Token_Kind kind,
    unsigned int flags,
    const Noc_Logical_Token_Macro_Provenance *provenance,
    size_t generation,
    size_t max_tokens)
{
    Noc_Logical_Source_Status status =
        noc__logical_source_reserve_token(implementation, max_tokens);
    Noc_Logical_Token *token;
    Noc_Logical_Token_Macro_Provenance *stored_provenance;
    if (status != NOC_LOGICAL_SOURCE_OK) return status;
    token = &implementation->tokens[implementation->token_count];
    stored_provenance =
        &implementation->provenances[implementation->token_count];
    memset(token, 0, sizeof(*token));
    memset(stored_provenance, 0, sizeof(*stored_provenance));
    token->kind = kind;
    token->bytes.begin = begin;
    token->bytes.end = end;
    token->generation = generation;
    token->flags = flags;
    if (provenance) *stored_provenance = *provenance;
    implementation->token_count += 1;
    return NOC_LOGICAL_SOURCE_OK;
}

static Noc_Logical_Source_Status noc__logical_source_append_separator(
    Noc_Logical_Source_Impl *implementation,
    Noc_Buffer *text,
    size_t generation,
    const Noc_Logical_Source_Options *options)
{
    static const char separator_text[] = " ";
    Noc_Logical_Source_Status status;
    size_t begin = text->count;
    status = noc__logical_source_append_bytes(text,
                                              separator_text,
                                              1,
                                              options->max_source_bytes);
    if (status != NOC_LOGICAL_SOURCE_OK) return status;
    return noc__logical_source_publish_token(
        implementation,
        begin,
        text->count,
        NOC_TOKEN_WHITESPACE,
        NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR,
        NULL,
        generation,
        options->max_tokens);
}

static bool noc__logical_source_is_significant(Noc_Token token)
{
    return token.kind != NOC_TOKEN_EOF && !noc_token_is_trivia(token);
}

static bool noc__logical_source_builtin_mask_is_valid(uint32_t mask)
{
    uint32_t supported = 0;
    uint32_t required = 0;
    Noc_Macro_Builtin_Kind kind;
    for (kind = NOC_MACRO_BUILTIN_FILE;
         kind <= NOC_MACRO_BUILTIN_TIME;
         kind = (Noc_Macro_Builtin_Kind)(kind + 1)) {
        supported |= UINT32_C(1) << (unsigned int)kind;
    }
    required |= UINT32_C(1) << (unsigned int)NOC_MACRO_BUILTIN_FILE;
    required |= UINT32_C(1) << (unsigned int)NOC_MACRO_BUILTIN_LINE;
    required |= UINT32_C(1) << (unsigned int)NOC_MACRO_BUILTIN_STDC;
    required |= UINT32_C(1) << (unsigned int)NOC_MACRO_BUILTIN_STDC_VERSION;
    return (mask & ~supported) == 0 && (mask & required) == required;
}

/* Validate exactly the expansion state consumed by this bridge. Unlike the
   general public validator, this does not walk unrelated environment history or
   unused generated spellings, and every variable-size pass is cancellable and
   already bounded by the logical-source token/frame limits. */
static Noc_Logical_Source_Status noc__logical_source_validate_expansion(
    const Noc_Macro_Expansion *expansion,
    const Noc_Logical_Source_Options *options,
    size_t *input_bytes_examined)
{
    size_t index;
    if (!expansion || expansion->generation == 0 || !expansion->environment ||
        expansion->environment_generation != expansion->environment->generation ||
        expansion->environment_entry_count != expansion->environment->count ||
        expansion->environment_entry_limit > expansion->environment_entry_count ||
        expansion->environment->count > expansion->environment->capacity ||
        ((expansion->environment->capacity == 0) !=
         (expansion->environment->items == NULL)) ||
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
        !noc__logical_source_builtin_mask_is_valid(
            expansion->available_builtin_mask)) {
        return NOC_LOGICAL_SOURCE_STALE;
    }
    for (index = 0; index < expansion->count; ++index) {
        const Noc_Macro_Expansion_Token *token = &expansion->items[index];
        const Noc_Preprocessing_Token *anchor;
        bool anchor_is_stringification = false;
        bool anchor_is_paste = false;
        Noc_Macro_Builtin_Kind anchor_builtin = NOC_MACRO_BUILTIN_NONE;
        Noc_Logical_Source_Status status;
        if ((index % NOC__LOGICAL_SOURCE_VALIDATION_INTERVAL) == 0 &&
            noc__logical_source_should_cancel(options)) {
            return NOC_LOGICAL_SOURCE_CANCELLED;
        }
        if (!noc_preprocessor_unit_is_valid(token->unit) ||
            token->unit_stream_generation != token->unit->stream.generation ||
            token->preprocessing_token_index >=
                token->unit->preprocessing_token_count ||
            !noc__logical_source_origin_is_valid(token->origin) ||
            token->token.kind < NOC_TOKEN_EOF ||
            token->token.kind > NOC_TOKEN_INVALID ||
            (!token->token.text.data && token->token.text.count != 0)) {
            return NOC_LOGICAL_SOURCE_STALE;
        }
        anchor = &token->unit->preprocessing_tokens[
            token->preprocessing_token_index];
        if (anchor->token.location.offset > token->unit->stream.source_count ||
            anchor->token.text.count > token->unit->stream.source_count -
                                           anchor->token.location.offset ||
            anchor->token.text.data != token->unit->stream.source +
                                           anchor->token.location.offset ||
            anchor->token.location.line == 0 ||
            anchor->token.location.column == 0) {
            return NOC_LOGICAL_SOURCE_STALE;
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
                return NOC_LOGICAL_SOURCE_STALE;
            }
            status = noc__logical_source_classify_anchor(
                anchor->token.text,
                options,
                input_bytes_examined,
                &anchor_is_stringification,
                &anchor_is_paste,
                &anchor_builtin);
            if (status != NOC_LOGICAL_SOURCE_OK) return status;
            if (token->origin == NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION) {
                if (token->token.kind != NOC_TOKEN_STRING ||
                    !anchor_is_stringification) {
                    return NOC_LOGICAL_SOURCE_STALE;
                }
            } else if (token->origin == NOC_MACRO_EXPANSION_TOKEN_PASTE) {
                if (!anchor_is_paste ||
                    token->token.kind == NOC_TOKEN_EOF ||
                    token->token.kind == NOC_TOKEN_PREPROCESSOR ||
                    token->token.kind == NOC_TOKEN_HEADER_NAME ||
                    token->token.kind == NOC_TOKEN_INVALID ||
                    noc_token_is_trivia(token->token)) {
                    return NOC_LOGICAL_SOURCE_STALE;
                }
            } else {
                switch (token->builtin_kind) {
                case NOC_MACRO_BUILTIN_FILE:
                case NOC_MACRO_BUILTIN_DATE:
                case NOC_MACRO_BUILTIN_TIME:
                    if (token->token.kind != NOC_TOKEN_STRING) {
                        return NOC_LOGICAL_SOURCE_STALE;
                    }
                    break;
                case NOC_MACRO_BUILTIN_LINE:
                case NOC_MACRO_BUILTIN_STDC:
                case NOC_MACRO_BUILTIN_STDC_VERSION:
                case NOC_MACRO_BUILTIN_STDC_HOSTED:
                    if (token->token.kind != NOC_TOKEN_NUMBER) {
                        return NOC_LOGICAL_SOURCE_STALE;
                    }
                    break;
                case NOC_MACRO_BUILTIN_NONE:
                default:
                    return NOC_LOGICAL_SOURCE_STALE;
                }
                if (!noc__macro_builtin_mask_contains(
                        expansion->available_builtin_mask,
                        token->builtin_kind) ||
                    (token->frame_index == NOC_TOKEN_INDEX_NONE &&
                     token->unit != expansion->input_unit) ||
                    (anchor_builtin != token->builtin_kind &&
                     !anchor_is_paste)) {
                    return NOC_LOGICAL_SOURCE_STALE;
                }
            }
            spelling = &expansion->generated_spellings[
                token->generated_spelling_index];
            if (!spelling->data || spelling->count == 0 ||
                token->token.text.data != spelling->data ||
                token->token.text.count != spelling->count) {
                return NOC_LOGICAL_SOURCE_STALE;
            }
        } else if (token->generated_spelling_index != NOC_TOKEN_INDEX_NONE ||
                   token->builtin_kind != NOC_MACRO_BUILTIN_NONE ||
                   token->token.text.data != anchor->token.text.data ||
                   token->token.text.count != anchor->token.text.count) {
            return NOC_LOGICAL_SOURCE_STALE;
        } else if (token->frame_index == NOC_TOKEN_INDEX_NONE) {
            if (token->origin != NOC_MACRO_EXPANSION_TOKEN_INPUT) {
                return NOC_LOGICAL_SOURCE_STALE;
            }
        } else if (token->frame_index >= expansion->frame_count ||
                   (token->origin != NOC_MACRO_EXPANSION_TOKEN_ARGUMENT &&
                    token->origin != NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT)) {
            return NOC_LOGICAL_SOURCE_STALE;
        }
    }
    for (index = 0; index < expansion->frame_count; ++index) {
        const Noc_Macro_Expansion_Frame *frame = &expansion->frames[index];
        const Noc_Macro_Environment_Entry *entry;
        const Noc_Macro_Directive *directive;
        if ((index % NOC__LOGICAL_SOURCE_VALIDATION_INTERVAL) == 0 &&
            noc__logical_source_should_cancel(options)) {
            return NOC_LOGICAL_SOURCE_CANCELLED;
        }
        if (frame->environment_entry_index >=
                expansion->environment_entry_limit ||
            (frame->parent_frame_index != NOC_TOKEN_INDEX_NONE &&
             frame->parent_frame_index >= index) ||
            !noc_preprocessor_unit_is_valid(frame->invocation_unit) ||
            frame->invocation_unit_stream_generation !=
                frame->invocation_unit->stream.generation ||
            frame->invocation_token_index >=
                frame->invocation_unit->preprocessing_token_count) {
            return NOC_LOGICAL_SOURCE_STALE;
        }
        entry = &expansion->environment->items[
            frame->environment_entry_index];
        if (!noc_preprocessor_unit_is_valid(entry->unit) ||
            entry->unit_stream_generation != entry->unit->stream.generation ||
            entry->macro_directive_index >= entry->unit->macro_directive_count ||
            (entry->previous_entry_index != NOC_TOKEN_INDEX_NONE &&
             entry->previous_entry_index >= frame->environment_entry_index)) {
            return NOC_LOGICAL_SOURCE_STALE;
        }
        directive = &entry->unit->macro_directives[
            entry->macro_directive_index];
        if (directive->status != NOC_MACRO_DIRECTIVE_STATUS_VALID ||
            directive->name_token_index >=
                entry->unit->preprocessing_token_count) {
            return NOC_LOGICAL_SOURCE_STALE;
        }
    }
    return NOC_LOGICAL_SOURCE_OK;
}

NOCDEF Noc_Logical_Source_Status noc_logical_source_build_macro_expansion(
    const Noc_Macro_Expansion *expansion,
    Noc_Logical_Source_Options options,
    Noc_Logical_Source *output)
{
    Noc_Logical_Source_Impl *built = NULL;
    Noc_Logical_Source_Impl *previous;
    Noc__Logical_Source_Builder builder;
    Noc_Buffer text = {0};
    Noc_Logical_Source_Status status = NOC_LOGICAL_SOURCE_OK;
    Noc__Line_Map_Status line_status;
    size_t generation;
    size_t index;
    size_t validation_input_bytes_examined = 0;
    bool saw_significant = false;
    bool saw_nonempty_trivia = false;

    memset(&builder, 0, sizeof(builder));
    if (!expansion || !output || options.max_source_bytes == 0 ||
        options.max_source_bytes == SIZE_MAX ||
        options.max_input_bytes_examined == 0 || options.max_tokens == 0 ||
        options.max_macro_frames == 0 || options.max_source_files == 0 ||
        options.max_path_bytes == 0 ||
        (output->impl && !noc__logical_source_handle_is_current(output))) {
        return NOC_LOGICAL_SOURCE_INVALID_ARGUMENT;
    }
    if (noc__logical_source_should_cancel(&options)) {
        return NOC_LOGICAL_SOURCE_CANCELLED;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_LOGICAL_SOURCE_GENERATION_EXHAUSTED;
    }
    if (expansion->count > options.max_tokens ||
        expansion->frame_count > options.max_macro_frames) {
        return NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
    }
    status = noc__logical_source_validate_expansion(
        expansion,
        &options,
        &validation_input_bytes_examined);
    if (status != NOC_LOGICAL_SOURCE_OK) return status;

    generation = output->generation + 1;
    built = (Noc_Logical_Source_Impl *)calloc(1, sizeof(*built));
    if (!built) return NOC_LOGICAL_SOURCE_OUT_OF_MEMORY;
    built->generation = generation;
    builder.implementation = built;
    builder.options = &options;
    builder.input_bytes_examined = validation_input_bytes_examined;

    if (expansion->frame_count != 0) {
        if (expansion->frame_count > SIZE_MAX / sizeof(*built->frames)) {
            status = NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
            goto failed;
        }
        built->frames = (Noc_Logical_Macro_Frame *)malloc(
            expansion->frame_count * sizeof(*built->frames));
        if (!built->frames) {
            status = NOC_LOGICAL_SOURCE_OUT_OF_MEMORY;
            goto failed;
        }
        built->frame_count = expansion->frame_count;
    }
    for (index = 0; index < expansion->frame_count; ++index) {
        const Noc_Macro_Expansion_Frame *source_frame = &expansion->frames[index];
        const Noc_Macro_Environment_Entry *definition =
            &expansion->environment->items[
                source_frame->environment_entry_index];
        const Noc_Macro_Directive *directive;
        Noc_Logical_Macro_Frame *frame = &built->frames[index];
        if (noc__logical_source_should_cancel(&options)) {
            status = NOC_LOGICAL_SOURCE_CANCELLED;
            goto failed;
        }
        if (definition->macro_directive_index >=
            definition->unit->macro_directive_count) {
            status = NOC_LOGICAL_SOURCE_STALE;
            goto failed;
        }
        directive = &definition->unit->macro_directives[
            definition->macro_directive_index];
        status = noc__logical_source_make_site(&builder,
                                                definition->unit,
                                                directive->name_token_index,
                                                &frame->definition);
        if (status != NOC_LOGICAL_SOURCE_OK) goto failed;
        status = noc__logical_source_make_site(
            &builder,
            source_frame->invocation_unit,
            source_frame->invocation_token_index,
            &frame->invocation);
        if (status != NOC_LOGICAL_SOURCE_OK) goto failed;
        frame->parent_macro_frame_index = source_frame->parent_frame_index;
    }

    for (index = 0; index < expansion->count; ++index) {
        const Noc_Macro_Expansion_Token *source_token = &expansion->items[index];
        Noc_Logical_Token_Macro_Provenance provenance;
        size_t begin;
        bool significant;
        if (noc__logical_source_should_cancel(&options)) {
            status = NOC_LOGICAL_SOURCE_CANCELLED;
            goto failed;
        }
        significant = noc__logical_source_is_significant(source_token->token);
        if (significant && saw_significant && !saw_nonempty_trivia) {
            status = noc__logical_source_append_separator(built,
                                                          &text,
                                                          generation,
                                                          &options);
            if (status != NOC_LOGICAL_SOURCE_OK) goto failed;
        }
        memset(&provenance, 0, sizeof(provenance));
        status = noc__logical_source_make_site(
            &builder,
            source_token->unit,
            source_token->preprocessing_token_index,
            &provenance.anchor);
        if (status != NOC_LOGICAL_SOURCE_OK) goto failed;
        provenance.macro_frame_index = source_token->frame_index;
        provenance.macro_origin = source_token->origin;
        provenance.builtin_kind = source_token->builtin_kind;
        begin = text.count;
        status = noc__logical_source_append_logical_spelling(
            &builder,
            &text,
            source_token->token.text);
        if (status != NOC_LOGICAL_SOURCE_OK) goto failed;
        status = noc__logical_source_publish_token(built,
                                                   begin,
                                                   text.count,
                                                   source_token->token.kind,
                                                   0,
                                                   &provenance,
                                                   generation,
                                                   options.max_tokens);
        if (status != NOC_LOGICAL_SOURCE_OK) goto failed;
        if (significant) {
            saw_significant = true;
            saw_nonempty_trivia = false;
        } else if (noc_token_is_trivia(source_token->token) &&
                   begin != text.count) {
            saw_nonempty_trivia = true;
        }
    }
    if (noc__logical_source_should_cancel(&options)) {
        status = NOC_LOGICAL_SOURCE_CANCELLED;
        goto failed;
    }
    if (!noc_buffer_terminate(&text)) {
        status = NOC_LOGICAL_SOURCE_OUT_OF_MEMORY;
        goto failed;
    }
    built->text = text.items;
    built->text_count = text.count;
    memset(&text, 0, sizeof(text));
    line_status = noc__line_starts_build(built->text,
                                         built->text_count,
                                         options.should_cancel,
                                         options.cancel_user_data,
                                         &built->line_starts,
                                         &built->line_count);
    if (line_status != NOC__LINE_MAP_OK) {
        status = line_status == NOC__LINE_MAP_CANCELLED
                     ? NOC_LOGICAL_SOURCE_CANCELLED
                     : line_status == NOC__LINE_MAP_OUT_OF_MEMORY
                           ? NOC_LOGICAL_SOURCE_OUT_OF_MEMORY
                           : NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED;
        goto failed;
    }
    if (noc__logical_source_should_cancel(&options)) {
        status = NOC_LOGICAL_SOURCE_CANCELLED;
        goto failed;
    }

    previous = output->impl;
    output->impl = built;
    output->generation = generation;
    noc__logical_source_builder_deinit(&builder);
    noc__logical_source_impl_free(previous);
    return NOC_LOGICAL_SOURCE_OK;

failed:
    noc_buffer_free(&text);
    noc__logical_source_builder_deinit(&builder);
    noc__logical_source_impl_free(built);
    return status;
}

NOCDEF Noc_Slice noc_logical_source_text(const Noc_Logical_Source *source)
{
    Noc_Slice result = {0};
    if (!noc__logical_source_handle_is_current(source)) return result;
    result.data = source->impl->text;
    result.count = source->impl->text_count;
    return result;
}

NOCDEF size_t noc_logical_source_token_count(
    const Noc_Logical_Source *source)
{
    return noc__logical_source_handle_is_current(source)
               ? source->impl->token_count
               : 0;
}

NOCDEF const Noc_Logical_Token *noc_logical_source_token_at(
    const Noc_Logical_Source *source,
    size_t token_index)
{
    if (!noc__logical_source_handle_is_current(source) ||
        token_index >= source->impl->token_count) {
        return NULL;
    }
    return &source->impl->tokens[token_index];
}

NOCDEF Noc_Slice noc_logical_source_token_text(
    const Noc_Logical_Source *source,
    size_t token_index)
{
    Noc_Slice result = {0};
    const Noc_Logical_Token *token =
        noc_logical_source_token_at(source, token_index);
    if (!token) return result;
    result.data = source->impl->text + token->bytes.begin;
    result.count = token->bytes.end - token->bytes.begin;
    return result;
}

NOCDEF size_t noc_logical_source_line_count(
    const Noc_Logical_Source *source)
{
    return noc__logical_source_handle_is_current(source)
               ? source->impl->line_count
               : 0;
}

NOCDEF bool noc_logical_source_location(
    const Noc_Logical_Source *source,
    size_t offset,
    Noc_Logical_Location *output)
{
    const Noc_Logical_Source_Impl *implementation;
    Noc_Logical_Location location;
    size_t lower;
    size_t upper;
    if (!noc__logical_source_handle_is_current(source) || !output) return false;
    implementation = source->impl;
    if (offset > implementation->text_count) return false;
    lower = 0;
    upper = implementation->line_count;
    while (lower + 1 < upper) {
        size_t middle = lower + (upper - lower) / 2;
        if (implementation->line_starts[middle] <= offset) {
            lower = middle;
        } else {
            upper = middle;
        }
    }
    location.offset = offset;
    location.line = lower + 1;
    location.byte_column = offset - implementation->line_starts[lower] + 1;
    *output = location;
    return true;
}

NOCDEF bool noc_logical_source_offset(
    const Noc_Logical_Source *source,
    size_t line,
    size_t byte_column,
    size_t *output)
{
    const Noc_Logical_Source_Impl *implementation;
    size_t start;
    size_t end;
    size_t column_offset;
    if (!noc__logical_source_handle_is_current(source) || !output || line == 0 ||
        byte_column == 0) {
        return false;
    }
    implementation = source->impl;
    if (line > implementation->line_count) return false;
    start = implementation->line_starts[line - 1];
    end = line < implementation->line_count
              ? implementation->line_starts[line] - 1
              : implementation->text_count;
    column_offset = byte_column - 1;
    if (column_offset > end - start) return false;
    *output = start + column_offset;
    return true;
}

NOCDEF bool noc_logical_source_token_range_for_bytes(
    const Noc_Logical_Source *source,
    Noc_Logical_Byte_Range bytes,
    Noc_Logical_Token_Range *output)
{
    const Noc_Logical_Source_Impl *implementation;
    Noc_Logical_Token_Range result;
    size_t lower;
    size_t upper;
    if (!noc__logical_source_handle_is_current(source) || !output) return false;
    implementation = source->impl;
    if (bytes.begin > bytes.end || bytes.end > implementation->text_count) {
        return false;
    }
    lower = 0;
    upper = implementation->token_count;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2;
        if (implementation->tokens[middle].bytes.end <= bytes.begin) {
            lower = middle + 1;
        } else {
            upper = middle;
        }
    }
    result.begin = lower;
    if (bytes.begin == bytes.end) {
        result.end = lower;
        *output = result;
        return true;
    }
    upper = implementation->token_count;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2;
        if (implementation->tokens[middle].bytes.begin < bytes.end) {
            lower = middle + 1;
        } else {
            upper = middle;
        }
    }
    result.end = lower;
    *output = result;
    return true;
}

NOCDEF bool noc_logical_source_token_macro_provenance(
    const Noc_Logical_Source *source,
    size_t token_index,
    Noc_Logical_Token_Macro_Provenance *output)
{
    const Noc_Logical_Token *token =
        noc_logical_source_token_at(source, token_index);
    if (!token || !output ||
        (token->flags & NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) != 0) {
        return false;
    }
    *output = source->impl->provenances[token_index];
    return true;
}

NOCDEF size_t noc_logical_source_file_count(
    const Noc_Logical_Source *source)
{
    return noc__logical_source_handle_is_current(source)
               ? source->impl->file_count
               : 0;
}

NOCDEF const Noc_Logical_Source_File *noc_logical_source_file_at(
    const Noc_Logical_Source *source,
    size_t file_index)
{
    if (!noc__logical_source_handle_is_current(source) ||
        file_index >= source->impl->file_count) {
        return NULL;
    }
    return &source->impl->files[file_index];
}

NOCDEF size_t noc_logical_source_macro_frame_count(
    const Noc_Logical_Source *source)
{
    return noc__logical_source_handle_is_current(source)
               ? source->impl->frame_count
               : 0;
}

NOCDEF const Noc_Logical_Macro_Frame *noc_logical_source_macro_frame_at(
    const Noc_Logical_Source *source,
    size_t frame_index)
{
    if (!noc__logical_source_handle_is_current(source) ||
        frame_index >= source->impl->frame_count) {
        return NULL;
    }
    return &source->impl->frames[frame_index];
}

#endif /* NOC_LOGICAL_SOURCE_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
