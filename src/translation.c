#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_TRANSLATION_IMPLEMENTATION_INCLUDED
#define NOC_TRANSLATION_IMPLEMENTATION_INCLUDED

typedef struct {
    Noc_Document_Snapshot snapshot;
    Noc_Preprocessor_Unit unit;
    Noc_Include_Guard guard;
    bool once_seen;
} Noc__Translation_File;

typedef struct {
    const Noc_Preprocessor_Unit *unit;
    Noc_Token_Range tokens;
    size_t macro_entry_limit;
} Noc__Translation_Fragment;

typedef struct {
    size_t opener_directive_index;
    bool parent_active;
    bool active;
    bool taken;
    bool saw_else;
} Noc__Translation_Conditional;

typedef struct {
    Noc__Translation_File *file;
    size_t next_directive_index;
    size_t source_cursor;
    Noc__Translation_Conditional *conditions;
    size_t condition_count;
    size_t condition_capacity;
} Noc__Translation_Frame;

struct Noc_Preprocessor_Translation_Impl {
    Noc__Translation_File **files;
    size_t file_count;
    size_t file_capacity;
    Noc_Macro_Environment environment;
    Noc_Logical_Source logical_source;
    size_t generation;
};

/* Mutable state belongs to one build. File records remain immutable after they
   enter the cache; frames describe individual include occurrences. */
typedef struct {
    Noc_Context *context;
    Noc_Include_Resolver resolver;
    Noc_Preprocessor_Translation_Options options;
    Noc_Preprocessor_Translation_Impl *implementation;
    Noc__Translation_Frame *frames;
    size_t frame_count;
    size_t frame_capacity;
    Noc__Translation_Fragment *fragments;
    size_t fragment_count;
    size_t fragment_capacity;
    size_t include_occurrence_count;
    size_t directive_count;
    size_t input_bytes;
    size_t path_bytes;
    uint32_t builtin_mask;
    Noc_Preprocessor_Translation_Result result;
} Noc__Translation_Builder;

static Noc_Preprocessor_Translation_Result noc__translation_result(
    Noc_Preprocessor_Translation_Status status)
{
    Noc_Preprocessor_Translation_Result result;
    memset(&result, 0, sizeof(result));
    result.status = status;
    result.problem_file_id = NOC_FILE_ID_NONE;
    result.problem_directive_index = NOC_TOKEN_INDEX_NONE;
    result.problem_token_index = NOC_TOKEN_INDEX_NONE;
    return result;
}

static bool noc__translation_options_are_valid(
    Noc_Preprocessor_Translation_Options options)
{
    return options.macro_policy >= NOC_MACROS_DISABLED &&
           options.macro_policy <= NOC_MACROS_FULL &&
           noc__macro_expansion_options_are_valid(options.macro_expansion) &&
           noc__logical_source_options_are_valid(options.logical_source) &&
           options.max_include_depth != 0 && options.max_files != 0 &&
           options.max_include_occurrences != 0 &&
           options.max_directives != 0;
}

NOCDEF Noc_Preprocessor_Translation_Options
noc_preprocessor_translation_default_options(void)
{
    Noc_Preprocessor_Translation_Options options;
    memset(&options, 0, sizeof(options));
    options.macro_policy = NOC_MACROS_TRUSTED_ONLY;
    options.macro_expansion = noc_macro_expansion_default_options();
    options.logical_source = noc_logical_source_default_options();
    options.max_include_depth = 256;
    options.max_files = 4096;
    options.max_include_occurrences = 16384;
    options.max_directives = 1000000;
    return options;
}

NOCDEF const char *noc_preprocessor_translation_status_name(
    Noc_Preprocessor_Translation_Status status)
{
    switch (status) {
    case NOC_PREPROCESSOR_TRANSLATION_OK: return "ok";
    case NOC_PREPROCESSOR_TRANSLATION_INVALID_ARGUMENT:
        return "invalid-argument";
    case NOC_PREPROCESSOR_TRANSLATION_STALE: return "stale";
    case NOC_PREPROCESSOR_TRANSLATION_CANCELLED: return "cancelled";
    case NOC_PREPROCESSOR_TRANSLATION_LIMIT_EXCEEDED:
        return "limit-exceeded";
    case NOC_PREPROCESSOR_TRANSLATION_CYCLE: return "cycle";
    case NOC_PREPROCESSOR_TRANSLATION_MALFORMED_CONDITIONAL:
        return "malformed-conditional";
    case NOC_PREPROCESSOR_TRANSLATION_UNSUPPORTED_DIRECTIVE:
        return "unsupported-directive";
    case NOC_PREPROCESSOR_TRANSLATION_MACRO_FAILED: return "macro-failed";
    case NOC_PREPROCESSOR_TRANSLATION_CONDITION_FAILED:
        return "condition-failed";
    case NOC_PREPROCESSOR_TRANSLATION_INCLUDE_FAILED: return "include-failed";
    case NOC_PREPROCESSOR_TRANSLATION_LOGICAL_SOURCE_FAILED:
        return "logical-source-failed";
    case NOC_PREPROCESSOR_TRANSLATION_GENERATION_EXHAUSTED:
        return "generation-exhausted";
    case NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY: return "out-of-memory";
    }
    return "unknown";
}

static void noc__translation_file_destroy(Noc__Translation_File *file)
{
    if (!file) return;
    noc_preprocessor_unit_free(&file->unit);
    noc_document_snapshot_free(&file->snapshot);
    free(file);
}

static void noc__translation_impl_destroy(
    Noc_Preprocessor_Translation_Impl *implementation)
{
    size_t index;
    if (!implementation) return;
    noc_logical_source_free(&implementation->logical_source);
    noc_macro_environment_free(&implementation->environment);
    for (index = 0; index < implementation->file_count; ++index) {
        noc__translation_file_destroy(implementation->files[index]);
    }
    free(implementation->files);
    free(implementation);
}

NOCDEF void noc_preprocessor_translation_free(
    Noc_Preprocessor_Translation *translation)
{
    if (!translation) return;
    noc__translation_impl_destroy(translation->impl);
    translation->impl = NULL;
}

NOCDEF bool noc_preprocessor_translation_is_valid(
    const Noc_Preprocessor_Translation *translation)
{
    size_t index;
    if (!translation || !translation->impl ||
        translation->generation != translation->impl->generation ||
        !noc_macro_environment_is_valid(&translation->impl->environment) ||
        !noc_logical_source_is_valid(&translation->impl->logical_source) ||
        translation->impl->file_count == 0) {
        return false;
    }
    for (index = 0; index < translation->impl->file_count; ++index) {
        const Noc__Translation_File *file = translation->impl->files[index];
        if (!file || !noc_document_snapshot_is_valid(&file->snapshot) ||
            !noc_preprocessor_unit_is_valid(&file->unit)) {
            return false;
        }
    }
    return true;
}

NOCDEF const Noc_Logical_Source *noc_preprocessor_translation_logical_source(
    const Noc_Preprocessor_Translation *translation)
{
    return noc_preprocessor_translation_is_valid(translation)
               ? &translation->impl->logical_source
               : NULL;
}

NOCDEF const Noc_Macro_Environment *
noc_preprocessor_translation_environment(
    const Noc_Preprocessor_Translation *translation)
{
    return noc_preprocessor_translation_is_valid(translation)
               ? &translation->impl->environment
               : NULL;
}

NOCDEF size_t noc_preprocessor_translation_file_count(
    const Noc_Preprocessor_Translation *translation)
{
    return noc_preprocessor_translation_is_valid(translation)
               ? translation->impl->file_count
               : 0;
}

NOCDEF const Noc_Document_Snapshot *noc_preprocessor_translation_snapshot_at(
    const Noc_Preprocessor_Translation *translation,
    size_t index)
{
    if (!noc_preprocessor_translation_is_valid(translation) ||
        index >= translation->impl->file_count) {
        return NULL;
    }
    return &translation->impl->files[index]->snapshot;
}

NOCDEF const Noc_Preprocessor_Unit *noc_preprocessor_translation_unit_at(
    const Noc_Preprocessor_Translation *translation,
    size_t index)
{
    if (!noc_preprocessor_translation_is_valid(translation) ||
        index >= translation->impl->file_count) {
        return NULL;
    }
    return &translation->impl->files[index]->unit;
}

static bool noc__translation_append(void **items,
                                    size_t *count,
                                    size_t *capacity,
                                    size_t item_size,
                                    const void *item)
{
    size_t new_capacity;
    void *resized;
    if (*count == *capacity) {
        new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        if (new_capacity < *capacity ||
            new_capacity > SIZE_MAX / item_size) {
            return false;
        }
        resized = realloc(*items, new_capacity * item_size);
        if (!resized) return false;
        *items = resized;
        *capacity = new_capacity;
    }
    memcpy((char *)*items + *count * item_size, item, item_size);
    *count += 1;
    return true;
}

static bool noc__translation_frame_is_active(
    const Noc__Translation_Frame *frame)
{
    return frame->condition_count == 0 ||
           frame->conditions[frame->condition_count - 1].active;
}

static void noc__translation_fail_at(
    Noc__Translation_Builder *builder,
    Noc_Preprocessor_Translation_Status status,
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index)
{
    builder->result.status = status;
    if (unit) {
        builder->result.problem_file_id = unit->file_id;
        builder->result.problem_document_generation =
            unit->document_generation;
    }
    builder->result.problem_directive_index = directive_index;
}

static void noc__translation_builder_destroy(
    Noc__Translation_Builder *builder)
{
    size_t index;
    if (!builder) return;
    for (index = 0; index < builder->frame_count; ++index) {
        free(builder->frames[index].conditions);
    }
    free(builder->frames);
    free(builder->fragments);
    noc__translation_impl_destroy(builder->implementation);
    memset(builder, 0, sizeof(*builder));
}

static bool noc__translation_builder_initialize(
    Noc__Translation_Builder *builder,
    Noc_Context *context,
    const Noc_Macro_Environment *initial_environment,
    size_t initial_entry_limit,
    Noc_Include_Resolver resolver,
    Noc_Preprocessor_Translation_Options options)
{
    Noc_Macro_Environment empty_environment = {0};
    const Noc_Macro_Environment *source_environment = initial_environment;
    memset(builder, 0, sizeof(*builder));
    builder->context = context;
    builder->resolver = resolver;
    builder->options = options;
    builder->builtin_mask =
        noc__macro_builtin_mask_from_options(options.macro_expansion);
    builder->result =
        noc__translation_result(NOC_PREPROCESSOR_TRANSLATION_OK);
    builder->implementation =
        (Noc_Preprocessor_Translation_Impl *)calloc(
            1, sizeof(*builder->implementation));
    if (!builder->implementation) {
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY;
        return false;
    }
    if (!source_environment) source_environment = &empty_environment;
    builder->result.macro_environment_status =
        noc_macro_environment_clone_prefix(
            source_environment,
            initial_entry_limit,
            &builder->implementation->environment);
    switch (builder->result.macro_environment_status) {
    case NOC_MACRO_ENVIRONMENT_OK:
        return true;
    case NOC_MACRO_ENVIRONMENT_STALE:
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_STALE;
        return false;
    case NOC_MACRO_ENVIRONMENT_GENERATION_EXHAUSTED:
        builder->result.status =
            NOC_PREPROCESSOR_TRANSLATION_GENERATION_EXHAUSTED;
        return false;
    case NOC_MACRO_ENVIRONMENT_OUT_OF_MEMORY:
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY;
        return false;
    default:
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_MACRO_FAILED;
        return false;
    }
}

static bool noc__translation_environment_borrows_output(
    const Noc_Macro_Environment *environment,
    const Noc_Preprocessor_Translation *output)
{
    size_t entry_index;
    size_t file_index;
    if (!output->impl) return false;
    for (entry_index = 0; entry_index < environment->count; ++entry_index) {
        for (file_index = 0;
             file_index < output->impl->file_count;
             ++file_index) {
            if (environment->items[entry_index].unit ==
                &output->impl->files[file_index]->unit) {
                return true;
            }
        }
    }
    return false;
}

static Noc__Translation_File *noc__translation_find_file(
    const Noc__Translation_Builder *builder,
    const Noc_Document_Snapshot *snapshot)
{
    size_t index;
    for (index = 0; index < builder->implementation->file_count; ++index) {
        Noc__Translation_File *file = builder->implementation->files[index];
        if (file->snapshot.impl == snapshot->impl) return file;
    }
    return NULL;
}

/* On success this moves SNAPSHOT into the immutable file cache. */
static bool noc__translation_cache_file(
    Noc__Translation_Builder *builder,
    Noc_Document_Snapshot *snapshot,
    Noc__Translation_File **output)
{
    Noc__Translation_File *file;
    Noc_Slice source = noc_document_snapshot_source(snapshot);
    const char *path = noc_document_snapshot_path(snapshot);
    size_t path_bytes = path ? strlen(path) + 1 : 1;
    if (builder->implementation->file_count >= builder->options.max_files ||
        source.count >
            builder->options.logical_source.max_input_bytes_examined -
                builder->input_bytes ||
        path_bytes > builder->options.logical_source.max_path_bytes -
                         builder->path_bytes) {
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_LIMIT_EXCEEDED;
        return false;
    }
    file = (Noc__Translation_File *)calloc(1, sizeof(*file));
    if (!file) {
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY;
        return false;
    }
    file->snapshot = *snapshot;
    snapshot->impl = NULL;
    if (!noc_preprocessor_unit_build(builder->context,
                                     &file->snapshot,
                                     builder->options.macro_policy,
                                     &file->unit) ||
        noc_include_guard_build_structural(&file->unit, &file->guard) !=
            NOC_INCLUDE_CONTROL_BUILD_OK) {
        noc__translation_file_destroy(file);
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY;
        return false;
    }
    if (!noc__translation_append(
            (void **)&builder->implementation->files,
            &builder->implementation->file_count,
            &builder->implementation->file_capacity,
            sizeof(file),
            &file)) {
        noc__translation_file_destroy(file);
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY;
        return false;
    }
    builder->input_bytes += source.count;
    builder->path_bytes += path_bytes;
    *output = file;
    return true;
}

static bool noc__translation_push_frame(
    Noc__Translation_Builder *builder,
    Noc__Translation_File *file)
{
    Noc__Translation_Frame frame;
    if (builder->frame_count >= builder->options.max_include_depth ||
        builder->include_occurrence_count >=
            builder->options.max_include_occurrences) {
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_LIMIT_EXCEEDED;
        return false;
    }
    memset(&frame, 0, sizeof(frame));
    frame.file = file;
    if (!noc__translation_append((void **)&builder->frames,
                                 &builder->frame_count,
                                 &builder->frame_capacity,
                                 sizeof(frame),
                                 &frame)) {
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY;
        return false;
    }
    builder->include_occurrence_count += 1;
    return true;
}

static bool noc__translation_append_fragment(
    Noc__Translation_Builder *builder,
    const Noc_Preprocessor_Unit *unit,
    size_t begin,
    size_t end)
{
    Noc__Translation_Fragment fragment;
    if (begin >= end) return true;
    if (builder->fragment_count >=
        builder->options.logical_source.max_fragments) {
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_LIMIT_EXCEEDED;
        return false;
    }
    fragment.unit = unit;
    fragment.tokens.begin = begin;
    fragment.tokens.end = end;
    fragment.macro_entry_limit = builder->implementation->environment.count;
    if (!noc__translation_append((void **)&builder->fragments,
                                 &builder->fragment_count,
                                 &builder->fragment_capacity,
                                 sizeof(fragment),
                                 &fragment)) {
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY;
        return false;
    }
    return true;
}

static bool noc__translation_finish_frame(
    Noc__Translation_Builder *builder)
{
    Noc__Translation_Frame *frame =
        &builder->frames[builder->frame_count - 1];
    const Noc_Preprocessor_Unit *unit = &frame->file->unit;
    if (noc__translation_frame_is_active(frame) &&
        !noc__translation_append_fragment(
            builder,
            unit,
            frame->source_cursor,
            unit->preprocessing_token_count - 1)) {
        return false;
    }
    if (frame->condition_count != 0) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_MALFORMED_CONDITIONAL,
            unit,
            frame->conditions[frame->condition_count - 1]
                .opener_directive_index);
        return false;
    }
    free(frame->conditions);
    memset(frame, 0, sizeof(*frame));
    builder->frame_count -= 1;
    return true;
}

static bool noc__translation_macro_is_defined(
    const Noc__Translation_Builder *builder,
    Noc_Slice name)
{
    return noc_macro_environment_lookup(
               &builder->implementation->environment, name) != NULL ||
           noc__macro_builtin_mask_contains(
               builder->builtin_mask,
               noc_macro_builtin_kind_from_name(name));
}

static bool noc__translation_one_identifier(
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index,
    size_t *token_index)
{
    Noc_Token_Range body =
        noc_preprocessor_directive_body_tokens(unit, directive_index);
    size_t index;
    size_t count = 0;
    if (body.begin == NOC_TOKEN_INDEX_NONE) return false;
    for (index = body.begin; index < body.end; ++index) {
        if (!noc_token_is_trivia(unit->preprocessing_tokens[index].token)) {
            *token_index = index;
            count += 1;
        }
    }
    return count == 1 &&
           unit->preprocessing_tokens[*token_index].token.kind ==
               NOC_TOKEN_IDENTIFIER;
}

static bool noc__translation_evaluate_condition(
    Noc__Translation_Builder *builder,
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index,
    Noc_Preprocessor_Directive_Kind kind,
    bool *value)
{
    Noc_Macro_Expansion expansion = {0};
    Noc_Token_Range body;
    size_t problem_token_index = NOC_TOKEN_INDEX_NONE;
    size_t name_token_index;
    if (kind != NOC_PREPROCESSOR_DIRECTIVE_IF &&
        kind != NOC_PREPROCESSOR_DIRECTIVE_ELIF) {
        if (!noc__translation_one_identifier(
                unit, directive_index, &name_token_index)) {
            noc__translation_fail_at(
                builder,
                NOC_PREPROCESSOR_TRANSLATION_MALFORMED_CONDITIONAL,
                unit,
                directive_index);
            return false;
        }
        *value = noc__translation_macro_is_defined(
            builder,
            unit->preprocessing_tokens[name_token_index].token.text);
        if (kind == NOC_PREPROCESSOR_DIRECTIVE_IFNDEF ||
            kind == NOC_PREPROCESSOR_DIRECTIVE_ELIFNDEF) {
            *value = !*value;
        }
        return true;
    }
    body = noc_preprocessor_directive_body_tokens(unit, directive_index);
    builder->result.macro_expansion_status =
        noc_macro_expansion_build_condition_with_options(
            &builder->implementation->environment,
            builder->implementation->environment.count,
            unit,
            body,
            builder->options.macro_expansion,
            &expansion);
    if (builder->result.macro_expansion_status != NOC_MACRO_EXPANSION_OK) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_CONDITION_FAILED,
            unit,
            directive_index);
        return false;
    }
    builder->result.expression_status =
        noc_preprocessor_expression_evaluate(
            &expansion, value, &problem_token_index);
    noc_macro_expansion_free(&expansion);
    if (builder->result.expression_status != NOC_PREPROCESSOR_EXPRESSION_OK) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_CONDITION_FAILED,
            unit,
            directive_index);
        builder->result.problem_token_index = problem_token_index;
        return false;
    }
    return true;
}

static bool noc__translation_open_conditional(
    Noc__Translation_Builder *builder,
    Noc__Translation_Frame *frame,
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index,
    Noc_Preprocessor_Directive_Kind kind,
    bool parent_active)
{
    Noc__Translation_Conditional conditional;
    bool value = false;
    memset(&conditional, 0, sizeof(conditional));
    conditional.opener_directive_index = directive_index;
    conditional.parent_active = parent_active;
    if (parent_active &&
        !noc__translation_evaluate_condition(
            builder, unit, directive_index, kind, &value)) {
        return false;
    }
    conditional.active = parent_active && value;
    conditional.taken = conditional.active;
    if (!noc__translation_append((void **)&frame->conditions,
                                 &frame->condition_count,
                                 &frame->condition_capacity,
                                 sizeof(conditional),
                                 &conditional)) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY,
            unit,
            directive_index);
        return false;
    }
    return true;
}

static bool noc__translation_continue_conditional(
    Noc__Translation_Builder *builder,
    Noc__Translation_Frame *frame,
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index,
    Noc_Preprocessor_Directive_Kind kind)
{
    Noc__Translation_Conditional *conditional;
    bool value = false;
    if (frame->condition_count == 0) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_MALFORMED_CONDITIONAL,
            unit,
            directive_index);
        return false;
    }
    conditional = &frame->conditions[frame->condition_count - 1];
    if (kind == NOC_PREPROCESSOR_DIRECTIVE_ENDIF) {
        if (noc_preprocessor_directive_body_tokens(unit, directive_index).begin !=
            NOC_TOKEN_INDEX_NONE) {
            noc__translation_fail_at(
                builder,
                NOC_PREPROCESSOR_TRANSLATION_MALFORMED_CONDITIONAL,
                unit,
                directive_index);
            return false;
        }
        frame->condition_count -= 1;
        return true;
    }
    if (conditional->saw_else) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_MALFORMED_CONDITIONAL,
            unit,
            directive_index);
        return false;
    }
    if (kind == NOC_PREPROCESSOR_DIRECTIVE_ELSE) {
        if (noc_preprocessor_directive_body_tokens(unit, directive_index).begin !=
            NOC_TOKEN_INDEX_NONE) {
            noc__translation_fail_at(
                builder,
                NOC_PREPROCESSOR_TRANSLATION_MALFORMED_CONDITIONAL,
                unit,
                directive_index);
            return false;
        }
        conditional->saw_else = true;
        value = !conditional->taken;
    } else if (conditional->parent_active && !conditional->taken &&
               !noc__translation_evaluate_condition(
                   builder, unit, directive_index, kind, &value)) {
        return false;
    }
    conditional->active = conditional->parent_active &&
                          !conditional->taken && value;
    conditional->taken = conditional->taken || conditional->active;
    return true;
}

static bool noc__translation_apply_macro(
    Noc__Translation_Builder *builder,
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index,
    const Noc_Preprocessor_Directive *directive)
{
    if (directive->macro_directive_index == NOC_TOKEN_INDEX_NONE) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_MACRO_FAILED,
            unit,
            directive_index);
        return false;
    }
    builder->result.macro_environment_status = noc_macro_environment_apply(
        &builder->implementation->environment,
        unit,
        directive->macro_directive_index);
    if (builder->result.macro_environment_status !=
        NOC_MACRO_ENVIRONMENT_OK) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_MACRO_FAILED,
            unit,
            directive_index);
        return false;
    }
    return true;
}

static bool noc__translation_apply_pragma_once(
    Noc__Translation_Builder *builder,
    Noc__Translation_Frame *frame,
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index)
{
    Noc_Pragma_Once pragma_once = {0};
    if (noc_pragma_once_build(unit, directive_index, &pragma_once) !=
            NOC_INCLUDE_CONTROL_BUILD_OK ||
        pragma_once.status != NOC_PRAGMA_ONCE_VALID) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_UNSUPPORTED_DIRECTIVE,
            unit,
            directive_index);
        return false;
    }
    frame->file->once_seen = true;
    return true;
}

static bool noc__translation_file_is_active_ancestor(
    const Noc__Translation_Builder *builder,
    const Noc__Translation_File *file)
{
    size_t index;
    for (index = 0; index < builder->frame_count; ++index) {
        if (builder->frames[index].file == file) return true;
    }
    return false;
}

static bool noc__translation_file_is_suppressed(
    const Noc__Translation_Builder *builder,
    const Noc__Translation_File *file)
{
    return file->once_seen ||
           (file->guard.status == NOC_INCLUDE_GUARD_CANONICAL &&
            file->guard.definition_allowed &&
            noc__translation_macro_is_defined(builder,
                                               file->guard.guard_name));
}

static bool noc__translation_include(
    Noc__Translation_Builder *builder,
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index)
{
    Noc_Include_Operand operand = {0};
    Noc_Include_Expansion expansion = {0};
    Noc_Document_Snapshot child_snapshot = {0};
    Noc__Translation_File *child_file = NULL;
    Noc_Include_Resolve_Status resolve_status = NOC_INCLUDE_RESOLVE_FAILED;
    bool success = false;
    builder->result.include_operand_build_status =
        noc_include_operand_build(unit, directive_index, &operand);
    if (builder->result.include_operand_build_status !=
        NOC_INCLUDE_OPERAND_BUILD_OK) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_INCLUDE_FAILED,
            unit,
            directive_index);
        goto done;
    }
    builder->result.include_operand_status = operand.status;
    if (operand.status == NOC_INCLUDE_OPERAND_DIRECT) {
        resolve_status = noc_include_resolve(
            builder->resolver, &operand, &child_snapshot);
    } else if (operand.status == NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED) {
        builder->result.macro_expansion_status =
            noc_include_expansion_build_with_options(
                &builder->implementation->environment,
                builder->implementation->environment.count,
                &operand,
                builder->options.macro_expansion,
                &expansion);
        if (builder->result.macro_expansion_status != NOC_MACRO_EXPANSION_OK ||
            expansion.status != NOC_INCLUDE_OPERAND_DIRECT) {
            builder->result.include_operand_status = expansion.status;
            noc__translation_fail_at(
                builder,
                NOC_PREPROCESSOR_TRANSLATION_INCLUDE_FAILED,
                unit,
                directive_index);
            goto done;
        }
        resolve_status = noc_include_expansion_resolve(
            builder->resolver, &expansion, &child_snapshot);
    } else {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_INCLUDE_FAILED,
            unit,
            directive_index);
        goto done;
    }
    builder->result.include_resolve_status = resolve_status;
    if (resolve_status != NOC_INCLUDE_RESOLVE_FOUND) {
        noc__translation_fail_at(
            builder,
            resolve_status == NOC_INCLUDE_RESOLVE_CANCELLED
                ? NOC_PREPROCESSOR_TRANSLATION_CANCELLED
                : NOC_PREPROCESSOR_TRANSLATION_INCLUDE_FAILED,
            unit,
            directive_index);
        goto done;
    }
    child_file = noc__translation_find_file(builder, &child_snapshot);
    if (!child_file &&
        !noc__translation_cache_file(
            builder, &child_snapshot, &child_file)) {
        builder->result.problem_file_id = unit->file_id;
        builder->result.problem_document_generation =
            unit->document_generation;
        builder->result.problem_directive_index = directive_index;
        goto done;
    }
    if (noc__translation_file_is_suppressed(builder, child_file)) {
        success = true;
        goto done;
    }
    if (noc__translation_file_is_active_ancestor(builder, child_file)) {
        noc__translation_fail_at(builder,
                                 NOC_PREPROCESSOR_TRANSLATION_CYCLE,
                                 unit,
                                 directive_index);
        goto done;
    }
    if (!noc__translation_push_frame(builder, child_file)) {
        builder->result.problem_file_id = unit->file_id;
        builder->result.problem_document_generation =
            unit->document_generation;
        builder->result.problem_directive_index = directive_index;
        goto done;
    }
    success = true;

done:
    noc_document_snapshot_free(&child_snapshot);
    noc_include_expansion_free(&expansion);
    noc_include_operand_free(&operand);
    return success;
}

static bool noc__translation_process_directive(
    Noc__Translation_Builder *builder)
{
    Noc__Translation_Frame *frame =
        &builder->frames[builder->frame_count - 1];
    const Noc_Preprocessor_Unit *unit = &frame->file->unit;
    size_t directive_index = frame->next_directive_index++;
    const Noc_Preprocessor_Directive *directive =
        &unit->items[directive_index];
    bool active = noc__translation_frame_is_active(frame);
    builder->directive_count += 1;
    if (builder->directive_count > builder->options.max_directives) {
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_LIMIT_EXCEEDED,
            unit,
            directive_index);
        return false;
    }
    if (active &&
        !noc__translation_append_fragment(
            builder,
            unit,
            frame->source_cursor,
            directive->preprocessing_tokens.begin)) {
        builder->result.problem_file_id = unit->file_id;
        builder->result.problem_document_generation =
            unit->document_generation;
        builder->result.problem_directive_index = directive_index;
        return false;
    }
    frame->source_cursor = directive->preprocessing_tokens.end;
    switch (directive->kind) {
    case NOC_PREPROCESSOR_DIRECTIVE_IF:
    case NOC_PREPROCESSOR_DIRECTIVE_IFDEF:
    case NOC_PREPROCESSOR_DIRECTIVE_IFNDEF:
        return noc__translation_open_conditional(
            builder,
            frame,
            unit,
            directive_index,
            directive->kind,
            active);
    case NOC_PREPROCESSOR_DIRECTIVE_ELIF:
    case NOC_PREPROCESSOR_DIRECTIVE_ELIFDEF:
    case NOC_PREPROCESSOR_DIRECTIVE_ELIFNDEF:
    case NOC_PREPROCESSOR_DIRECTIVE_ELSE:
    case NOC_PREPROCESSOR_DIRECTIVE_ENDIF:
        return noc__translation_continue_conditional(
            builder, frame, unit, directive_index, directive->kind);
    default:
        break;
    }
    if (!active || directive->kind == NOC_PREPROCESSOR_DIRECTIVE_NULL) {
        return true;
    }
    switch (directive->kind) {
    case NOC_PREPROCESSOR_DIRECTIVE_DEFINE:
    case NOC_PREPROCESSOR_DIRECTIVE_UNDEF:
        return noc__translation_apply_macro(
            builder, unit, directive_index, directive);
    case NOC_PREPROCESSOR_DIRECTIVE_PRAGMA:
        return noc__translation_apply_pragma_once(
            builder, frame, unit, directive_index);
    case NOC_PREPROCESSOR_DIRECTIVE_INCLUDE:
        return noc__translation_include(builder, unit, directive_index);
    default:
        noc__translation_fail_at(
            builder,
            NOC_PREPROCESSOR_TRANSLATION_UNSUPPORTED_DIRECTIVE,
            unit,
            directive_index);
        return false;
    }
}

static bool noc__translation_compose_logical_source(
    Noc__Translation_Builder *builder)
{
    Noc_Macro_Expansion *expansions = NULL;
    const Noc_Macro_Expansion **views = NULL;
    Noc_Logical_Source_Options logical_options =
        builder->options.logical_source;
    size_t expanded_token_count = 0;
    size_t expanded_frame_count = 0;
    size_t index;
    bool success = false;
    if (builder->fragment_count != 0) {
        expansions = (Noc_Macro_Expansion *)calloc(
            builder->fragment_count, sizeof(*expansions));
        views = (const Noc_Macro_Expansion **)malloc(
            builder->fragment_count * sizeof(*views));
        if (!expansions || !views) {
            builder->result.status =
                NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY;
            goto done;
        }
    }
    for (index = 0; index < builder->fragment_count; ++index) {
        const Noc__Translation_Fragment *fragment =
            &builder->fragments[index];
        builder->result.macro_expansion_status =
            noc_macro_expansion_build_with_options(
                &builder->implementation->environment,
                fragment->macro_entry_limit,
                fragment->unit,
                fragment->tokens,
                builder->options.macro_expansion,
                &expansions[index]);
        if (builder->result.macro_expansion_status != NOC_MACRO_EXPANSION_OK) {
            builder->result.status =
                NOC_PREPROCESSOR_TRANSLATION_MACRO_FAILED;
            goto done;
        }
        if (expansions[index].count >
                logical_options.max_tokens - expanded_token_count ||
            expansions[index].frame_count >
                logical_options.max_macro_frames - expanded_frame_count) {
            builder->result.status =
                NOC_PREPROCESSOR_TRANSLATION_LIMIT_EXCEEDED;
            goto done;
        }
        expanded_token_count += expansions[index].count;
        expanded_frame_count += expansions[index].frame_count;
        views[index] = &expansions[index];
    }
    if (builder->input_bytes >=
            logical_options.max_input_bytes_examined &&
        builder->fragment_count != 0) {
        builder->result.status = NOC_PREPROCESSOR_TRANSLATION_LIMIT_EXCEEDED;
        goto done;
    }
    if (builder->input_bytes < logical_options.max_input_bytes_examined) {
        logical_options.max_input_bytes_examined -= builder->input_bytes;
    }
    builder->result.logical_source_status =
        noc_logical_source_build_macro_expansions(
            views,
            builder->fragment_count,
            logical_options,
            &builder->implementation->logical_source);
    if (builder->result.logical_source_status != NOC_LOGICAL_SOURCE_OK) {
        builder->result.status =
            NOC_PREPROCESSOR_TRANSLATION_LOGICAL_SOURCE_FAILED;
        goto done;
    }
    success = true;

done:
    if (expansions) {
        for (index = 0; index < builder->fragment_count; ++index) {
            noc_macro_expansion_free(&expansions[index]);
        }
    }
    free(views);
    free(expansions);
    return success;
}

static void noc__translation_publish(
    Noc__Translation_Builder *builder,
    Noc_Preprocessor_Translation *output)
{
    builder->implementation->generation = output->generation + 1;
    noc_preprocessor_translation_free(output);
    output->impl = builder->implementation;
    output->generation = builder->implementation->generation;
    builder->implementation = NULL;
}

NOCDEF Noc_Preprocessor_Translation_Result noc_preprocessor_translation_build(
    Noc_Context *context,
    const Noc_Document_Snapshot *root,
    const Noc_Macro_Environment *initial_environment,
    size_t initial_entry_limit,
    Noc_Include_Resolver resolver,
    Noc_Preprocessor_Translation_Options options,
    Noc_Preprocessor_Translation *output)
{
    Noc__Translation_Builder builder;
    Noc_Document_Snapshot root_snapshot = {0};
    Noc__Translation_File *root_file = NULL;
    Noc_Preprocessor_Translation_Result result;
    Noc_Workspace_Status clone_status;
    if (!context || !root || !output ||
        !noc_document_snapshot_is_valid(root) || !resolver.resolve ||
        !noc__translation_options_are_valid(options) ||
        (!initial_environment && initial_entry_limit != 0) ||
        (initial_environment &&
         initial_entry_limit > initial_environment->count) ||
        (output->impl && !noc_preprocessor_translation_is_valid(output))) {
        return noc__translation_result(
            NOC_PREPROCESSOR_TRANSLATION_INVALID_ARGUMENT);
    }
    if (output->generation == SIZE_MAX) {
        return noc__translation_result(
            NOC_PREPROCESSOR_TRANSLATION_GENERATION_EXHAUSTED);
    }
    if (!noc__translation_builder_initialize(&builder,
                                             context,
                                             initial_environment,
                                             initial_entry_limit,
                                             resolver,
                                             options)) {
        result = builder.result;
        noc__translation_builder_destroy(&builder);
        return result;
    }
    if (noc__translation_environment_borrows_output(
            &builder.implementation->environment, output)) {
        builder.result.status =
            NOC_PREPROCESSOR_TRANSLATION_INVALID_ARGUMENT;
        goto done;
    }
    clone_status = noc_document_snapshot_clone(root, &root_snapshot);
    if (clone_status != NOC_WORKSPACE_OK) {
        builder.result.status = clone_status == NOC_WORKSPACE_OUT_OF_MEMORY
                                    ? NOC_PREPROCESSOR_TRANSLATION_OUT_OF_MEMORY
                                    : NOC_PREPROCESSOR_TRANSLATION_STALE;
        goto done;
    }
    if (!noc__translation_cache_file(
            &builder, &root_snapshot, &root_file) ||
        !noc__translation_push_frame(&builder, root_file)) {
        goto done;
    }
    while (builder.frame_count != 0) {
        Noc__Translation_Frame *frame =
            &builder.frames[builder.frame_count - 1];
        if (options.logical_source.should_cancel &&
            options.logical_source.should_cancel(
                options.logical_source.cancel_user_data)) {
            builder.result.status = NOC_PREPROCESSOR_TRANSLATION_CANCELLED;
            goto done;
        }
        if (frame->next_directive_index >= frame->file->unit.count) {
            if (!noc__translation_finish_frame(&builder)) goto done;
        } else if (!noc__translation_process_directive(&builder)) {
            goto done;
        }
    }
    if (!noc__translation_compose_logical_source(&builder)) goto done;
    noc__translation_publish(&builder, output);

done:
    result = builder.result;
    noc_document_snapshot_free(&root_snapshot);
    noc__translation_builder_destroy(&builder);
    return result;
}

#endif /* NOC_TRANSLATION_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
