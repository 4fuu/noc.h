#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_WORKSPACE_IMPLEMENTATION_INCLUDED
#define NOC_WORKSPACE_IMPLEMENTATION_INCLUDED

typedef struct Noc__Workspace_Identity Noc__Workspace_Identity;

struct Noc__Workspace_Identity {
    size_t references;
    Noc_File_Id file_id;
    char *path;
};

struct Noc_Document_Snapshot_Impl {
    size_t references;
    Noc__Workspace_Identity *identity;
    size_t generation;
    Noc_Source_Class source_class;
    char *source;
    size_t source_count;
    size_t *line_starts;
    size_t line_count;
};

typedef struct {
    Noc__Workspace_Identity *identity;
    size_t generation;
    Noc_Document_Snapshot_Impl *current;
} Noc__Workspace_File;

struct Noc_Workspace_Impl {
    Noc__Workspace_File *files;
    size_t files_count;
    size_t files_capacity;
    Noc_File_Id next_file_id;
};

NOC__PRIVATE bool noc__source_class_is_valid(Noc_Source_Class source_class)
{
    switch (source_class) {
    case NOC_SOURCE_CLASS_PROJECT:
    case NOC_SOURCE_CLASS_TRUSTED:
    case NOC_SOURCE_CLASS_SYSTEM:
    case NOC_SOURCE_CLASS_GENERATED:
        return true;
    }
    return false;
}

static bool noc__workspace_identity_retain(Noc__Workspace_Identity *identity)
{
    if (!identity || identity->references == SIZE_MAX) return false;
    identity->references += 1;
    return true;
}

static void noc__workspace_identity_release(Noc__Workspace_Identity *identity)
{
    if (!identity) return;
    identity->references -= 1;
    if (identity->references == 0) {
        free(identity->path);
        free(identity);
    }
}

static Noc_Workspace_Status noc__workspace_identity_create(
    Noc_File_Id file_id,
    const char *path,
    Noc__Workspace_Identity **output)
{
    Noc__Workspace_Identity *identity;
    size_t path_count;
    if (!path || path[0] == '\0' || !output) return NOC_WORKSPACE_INVALID_ARGUMENT;
    path_count = strlen(path);
    if (path_count == SIZE_MAX) return NOC_WORKSPACE_LIMIT_EXCEEDED;
    identity = (Noc__Workspace_Identity *)malloc(sizeof(*identity));
    if (!identity) return NOC_WORKSPACE_OUT_OF_MEMORY;
    identity->path = (char *)malloc(path_count + 1);
    if (!identity->path) {
        free(identity);
        return NOC_WORKSPACE_OUT_OF_MEMORY;
    }
    memcpy(identity->path, path, path_count + 1);
    identity->references = 1;
    identity->file_id = file_id;
    *output = identity;
    return NOC_WORKSPACE_OK;
}

static void noc__document_snapshot_release(Noc_Document_Snapshot_Impl *snapshot)
{
    if (!snapshot) return;
    snapshot->references -= 1;
    if (snapshot->references == 0) {
        noc__workspace_identity_release(snapshot->identity);
        free(snapshot->source);
        free(snapshot->line_starts);
        free(snapshot);
    }
}

static bool noc__document_snapshot_retain(Noc_Document_Snapshot_Impl *snapshot)
{
    if (!snapshot || snapshot->references == SIZE_MAX) return false;
    snapshot->references += 1;
    return true;
}

NOC__PRIVATE Noc__Line_Map_Status noc__line_starts_build(
    const char *source,
    size_t source_count,
    Noc__Line_Map_Cancel_Fn should_cancel,
    void *cancel_user_data,
    size_t **output,
    size_t *output_count)
{
    enum { CANCEL_INTERVAL = 4096 };
    size_t line_count = 1;
    size_t line = 1;
    size_t index;
    size_t next_cancel_poll = 0;
    size_t *line_starts;
    for (index = 0; index < source_count; ++index) {
        if (should_cancel && index >= next_cancel_poll) {
            if (should_cancel(cancel_user_data)) {
                return NOC__LINE_MAP_CANCELLED;
            }
            next_cancel_poll = index > SIZE_MAX - CANCEL_INTERVAL
                                   ? SIZE_MAX
                                   : index + CANCEL_INTERVAL;
        }
        if (source[index] == '\r') {
            if (line_count == SIZE_MAX) return NOC__LINE_MAP_LIMIT_EXCEEDED;
            line_count += 1;
            if (index + 1 < source_count && source[index + 1] == '\n') index += 1;
        } else if (source[index] == '\n') {
            if (line_count == SIZE_MAX) return NOC__LINE_MAP_LIMIT_EXCEEDED;
            line_count += 1;
        }
    }
    if (line_count > SIZE_MAX / sizeof(*line_starts)) {
        return NOC__LINE_MAP_LIMIT_EXCEEDED;
    }
    line_starts = (size_t *)malloc(line_count * sizeof(*line_starts));
    if (!line_starts) return NOC__LINE_MAP_OUT_OF_MEMORY;
    line_starts[0] = 0;
    next_cancel_poll = 0;
    for (index = 0; index < source_count; ++index) {
        if (should_cancel && index >= next_cancel_poll) {
            if (should_cancel(cancel_user_data)) {
                free(line_starts);
                return NOC__LINE_MAP_CANCELLED;
            }
            next_cancel_poll = index > SIZE_MAX - CANCEL_INTERVAL
                                   ? SIZE_MAX
                                   : index + CANCEL_INTERVAL;
        }
        if (source[index] == '\r') {
            if (index + 1 < source_count && source[index + 1] == '\n') index += 1;
            line_starts[line++] = index + 1;
        } else if (source[index] == '\n') {
            line_starts[line++] = index + 1;
        }
    }
    *output = line_starts;
    *output_count = line_count;
    return NOC__LINE_MAP_OK;
}

static Noc_Workspace_Status noc__document_snapshot_create(
    Noc__Workspace_Identity *identity,
    size_t generation,
    const char *source,
    size_t source_count,
    Noc_Source_Class source_class,
    Noc_Document_Snapshot_Impl **output)
{
    Noc_Document_Snapshot_Impl *snapshot;
    Noc__Line_Map_Status line_status;
    char *source_copy;
    size_t *line_starts = NULL;
    size_t line_count = 0;
    if (!identity || !output || (!source && source_count != 0) ||
        !noc__source_class_is_valid(source_class)) {
        return NOC_WORKSPACE_INVALID_ARGUMENT;
    }
    if (source_count == SIZE_MAX) return NOC_WORKSPACE_LIMIT_EXCEEDED;
    source_copy = (char *)malloc(source_count + 1);
    if (!source_copy) return NOC_WORKSPACE_OUT_OF_MEMORY;
    if (source_count != 0) memcpy(source_copy, source, source_count);
    source_copy[source_count] = '\0';
    line_status = noc__line_starts_build(source_copy,
                                         source_count,
                                         NULL,
                                         NULL,
                                         &line_starts,
                                         &line_count);
    if (line_status != NOC__LINE_MAP_OK) {
        free(source_copy);
        return line_status == NOC__LINE_MAP_OUT_OF_MEMORY
                   ? NOC_WORKSPACE_OUT_OF_MEMORY
                   : NOC_WORKSPACE_LIMIT_EXCEEDED;
    }
    snapshot = (Noc_Document_Snapshot_Impl *)malloc(sizeof(*snapshot));
    if (!snapshot) {
        free(line_starts);
        free(source_copy);
        return NOC_WORKSPACE_OUT_OF_MEMORY;
    }
    if (!noc__workspace_identity_retain(identity)) {
        free(snapshot);
        free(line_starts);
        free(source_copy);
        return NOC_WORKSPACE_LIMIT_EXCEEDED;
    }
    snapshot->references = 2;
    snapshot->identity = identity;
    snapshot->generation = generation;
    snapshot->source_class = source_class;
    snapshot->source = source_copy;
    snapshot->source_count = source_count;
    snapshot->line_starts = line_starts;
    snapshot->line_count = line_count;
    *output = snapshot;
    return NOC_WORKSPACE_OK;
}

static void noc__document_snapshot_replace(Noc_Document_Snapshot *output,
                                           Noc_Document_Snapshot_Impl *replacement)
{
    Noc_Document_Snapshot_Impl *previous = output->impl;
    output->impl = replacement;
    noc__document_snapshot_release(previous);
}

static Noc__Workspace_File *noc__workspace_find_path(Noc_Workspace_Impl *workspace,
                                                     const char *path)
{
    size_t index;
    if (!workspace || !path) return NULL;
    for (index = 0; index < workspace->files_count; ++index) {
        if (strcmp(workspace->files[index].identity->path, path) == 0) {
            return &workspace->files[index];
        }
    }
    return NULL;
}

static const Noc__Workspace_File *noc__workspace_find_path_const(
    const Noc_Workspace_Impl *workspace,
    const char *path)
{
    size_t index;
    if (!workspace || !path) return NULL;
    for (index = 0; index < workspace->files_count; ++index) {
        if (strcmp(workspace->files[index].identity->path, path) == 0) {
            return &workspace->files[index];
        }
    }
    return NULL;
}

static Noc__Workspace_File *noc__workspace_find_id(Noc_Workspace_Impl *workspace,
                                                   Noc_File_Id file_id)
{
    if (!workspace || file_id == NOC_FILE_ID_NONE ||
        file_id >= workspace->files_count ||
        workspace->files[file_id].identity->file_id != file_id) {
        return NULL;
    }
    return &workspace->files[file_id];
}

static const Noc__Workspace_File *noc__workspace_find_id_const(
    const Noc_Workspace_Impl *workspace,
    Noc_File_Id file_id)
{
    if (!workspace || file_id == NOC_FILE_ID_NONE ||
        file_id >= workspace->files_count ||
        workspace->files[file_id].identity->file_id != file_id) {
        return NULL;
    }
    return &workspace->files[file_id];
}

static Noc_Workspace_Status noc__workspace_prepare_append(Noc_Workspace *workspace)
{
    Noc_Workspace_Impl *implementation;
    Noc__Workspace_File *files;
    size_t capacity;
    if (!workspace) return NOC_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->impl) {
        implementation = (Noc_Workspace_Impl *)calloc(1, sizeof(*implementation));
        if (!implementation) return NOC_WORKSPACE_OUT_OF_MEMORY;
        workspace->impl = implementation;
    }
    implementation = workspace->impl;
    if (implementation->files_count < implementation->files_capacity) {
        return NOC_WORKSPACE_OK;
    }
    if (implementation->files_capacity == 0) {
        capacity = 8;
    } else {
        if (implementation->files_capacity > SIZE_MAX / 2) {
            return NOC_WORKSPACE_LIMIT_EXCEEDED;
        }
        capacity = implementation->files_capacity * 2;
    }
    if (capacity > SIZE_MAX / sizeof(*files)) return NOC_WORKSPACE_LIMIT_EXCEEDED;
    files = (Noc__Workspace_File *)realloc(implementation->files,
                                           capacity * sizeof(*files));
    if (!files) return NOC_WORKSPACE_OUT_OF_MEMORY;
    implementation->files = files;
    implementation->files_capacity = capacity;
    return NOC_WORKSPACE_OK;
}

NOCDEF const char *noc_workspace_status_name(Noc_Workspace_Status status)
{
    switch (status) {
    case NOC_WORKSPACE_OK: return "ok";
    case NOC_WORKSPACE_INVALID_ARGUMENT: return "invalid argument";
    case NOC_WORKSPACE_ALREADY_OPEN: return "already open";
    case NOC_WORKSPACE_NOT_CURRENT: return "not current";
    case NOC_WORKSPACE_NOT_FOUND: return "not found";
    case NOC_WORKSPACE_OUT_OF_RANGE: return "out of range";
    case NOC_WORKSPACE_INVALID_EDIT: return "invalid edit";
    case NOC_WORKSPACE_OUT_OF_MEMORY: return "out of memory";
    case NOC_WORKSPACE_LIMIT_EXCEEDED: return "limit exceeded";
    }
    return "unknown workspace status";
}

NOCDEF void noc_workspace_init(Noc_Workspace *workspace)
{
    if (workspace) workspace->impl = NULL;
}

NOCDEF void noc_workspace_deinit(Noc_Workspace *workspace)
{
    size_t index;
    if (!workspace || !workspace->impl) return;
    for (index = 0; index < workspace->impl->files_count; ++index) {
        noc__document_snapshot_release(workspace->impl->files[index].current);
        noc__workspace_identity_release(workspace->impl->files[index].identity);
    }
    free(workspace->impl->files);
    free(workspace->impl);
    workspace->impl = NULL;
}

NOCDEF Noc_Workspace_Status noc_workspace_open_document(
    Noc_Workspace *workspace,
    const char *path,
    const char *source,
    size_t source_count,
    Noc_Source_Class source_class,
    Noc_Document_Snapshot *output)
{
    Noc__Workspace_File *file;
    Noc__Workspace_Identity *identity = NULL;
    Noc_Document_Snapshot_Impl *snapshot;
    Noc_Workspace_Status status;
    size_t generation;
    bool new_file;
    if (!workspace || !path || path[0] == '\0' || !output ||
        (!source && source_count != 0) ||
        !noc__source_class_is_valid(source_class)) {
        return NOC_WORKSPACE_INVALID_ARGUMENT;
    }
    file = noc__workspace_find_path(workspace->impl, path);
    if (file && file->current) return NOC_WORKSPACE_ALREADY_OPEN;
    new_file = file == NULL;
    if (new_file) {
        status = noc__workspace_prepare_append(workspace);
        if (status != NOC_WORKSPACE_OK) return status;
        if (workspace->impl->next_file_id == NOC_FILE_ID_NONE) {
            return NOC_WORKSPACE_LIMIT_EXCEEDED;
        }
        status = noc__workspace_identity_create(workspace->impl->next_file_id,
                                                path,
                                                &identity);
        if (status != NOC_WORKSPACE_OK) return status;
        generation = 1;
    } else {
        identity = file->identity;
        if (file->generation == SIZE_MAX) return NOC_WORKSPACE_LIMIT_EXCEEDED;
        generation = file->generation + 1;
    }
    status = noc__document_snapshot_create(identity,
                                           generation,
                                           source,
                                           source_count,
                                           source_class,
                                           &snapshot);
    if (status != NOC_WORKSPACE_OK) {
        if (new_file) noc__workspace_identity_release(identity);
        return status;
    }
    if (new_file) {
        file = &workspace->impl->files[workspace->impl->files_count++];
        file->identity = identity;
        workspace->impl->next_file_id += 1;
    }
    file->generation = generation;
    file->current = snapshot;
    noc__document_snapshot_replace(output, snapshot);
    return NOC_WORKSPACE_OK;
}

NOCDEF Noc_Workspace_Status noc_workspace_update_document(
    Noc_Workspace *workspace,
    const Noc_Document_Snapshot *expected,
    const char *source,
    size_t source_count,
    Noc_Document_Snapshot *output)
{
    Noc__Workspace_File *file;
    Noc_Document_Snapshot_Impl *previous;
    Noc_Document_Snapshot_Impl *snapshot;
    Noc_Workspace_Status status;
    size_t generation;
    if (!workspace || !expected || !expected->impl || !output ||
        (!source && source_count != 0)) {
        return NOC_WORKSPACE_INVALID_ARGUMENT;
    }
    file = noc__workspace_find_id(workspace->impl, expected->impl->identity->file_id);
    if (!file || file->current != expected->impl) return NOC_WORKSPACE_NOT_CURRENT;
    if (file->generation == SIZE_MAX) return NOC_WORKSPACE_LIMIT_EXCEEDED;
    generation = file->generation + 1;
    status = noc__document_snapshot_create(file->identity,
                                           generation,
                                           source,
                                           source_count,
                                           expected->impl->source_class,
                                           &snapshot);
    if (status != NOC_WORKSPACE_OK) return status;
    previous = file->current;
    file->generation = generation;
    file->current = snapshot;
    noc__document_snapshot_release(previous);
    noc__document_snapshot_replace(output, snapshot);
    return NOC_WORKSPACE_OK;
}

NOCDEF Noc_Workspace_Status noc_workspace_edit_document(
    Noc_Workspace *workspace,
    const Noc_Document_Snapshot *expected,
    const Noc_Text_Edit *edits,
    size_t edits_count,
    Noc_Document_Snapshot *output)
{
    Noc_Slice source;
    Noc_Workspace_Status status;
    char *edited;
    size_t edited_count;
    size_t source_cursor = 0;
    size_t output_cursor = 0;
    size_t index;
    if (!workspace || !expected || !expected->impl || !output ||
        (!edits && edits_count != 0)) {
        return NOC_WORKSPACE_INVALID_ARGUMENT;
    }
    if (!noc_document_snapshot_is_current(workspace, expected)) {
        return NOC_WORKSPACE_NOT_CURRENT;
    }
    source = noc_document_snapshot_source(expected);
    edited_count = source.count;
    for (index = 0; index < edits_count; ++index) {
        const Noc_Text_Edit *edit = &edits[index];
        size_t removed;
        if (edit->begin > edit->end || edit->end > source.count ||
            edit->begin < source_cursor ||
            (!edit->replacement.data && edit->replacement.count != 0)) {
            return NOC_WORKSPACE_INVALID_EDIT;
        }
        removed = edit->end - edit->begin;
        if (edit->replacement.count > SIZE_MAX - (edited_count - removed)) {
            return NOC_WORKSPACE_LIMIT_EXCEEDED;
        }
        edited_count = edited_count - removed + edit->replacement.count;
        source_cursor = edit->end;
    }
    if (edited_count == SIZE_MAX) return NOC_WORKSPACE_LIMIT_EXCEEDED;
    edited = (char *)malloc(edited_count + 1);
    if (!edited) return NOC_WORKSPACE_OUT_OF_MEMORY;
    source_cursor = 0;
    for (index = 0; index < edits_count; ++index) {
        const Noc_Text_Edit *edit = &edits[index];
        size_t unchanged = edit->begin - source_cursor;
        if (unchanged != 0) {
            memcpy(edited + output_cursor, source.data + source_cursor, unchanged);
            output_cursor += unchanged;
        }
        if (edit->replacement.count != 0) {
            memcpy(edited + output_cursor,
                   edit->replacement.data,
                   edit->replacement.count);
            output_cursor += edit->replacement.count;
        }
        source_cursor = edit->end;
    }
    if (source_cursor < source.count) {
        size_t unchanged = source.count - source_cursor;
        memcpy(edited + output_cursor, source.data + source_cursor, unchanged);
        output_cursor += unchanged;
    }
    edited[output_cursor] = '\0';
    status = noc_workspace_update_document(workspace,
                                           expected,
                                           edited,
                                           output_cursor,
                                           output);
    free(edited);
    return status;
}

NOCDEF Noc_Workspace_Status noc_workspace_close_document(
    Noc_Workspace *workspace,
    const Noc_Document_Snapshot *expected)
{
    Noc__Workspace_File *file;
    Noc_Document_Snapshot_Impl *previous;
    if (!workspace || !expected || !expected->impl) {
        return NOC_WORKSPACE_INVALID_ARGUMENT;
    }
    file = noc__workspace_find_id(workspace->impl, expected->impl->identity->file_id);
    if (!file || file->current != expected->impl) return NOC_WORKSPACE_NOT_CURRENT;
    previous = file->current;
    file->current = NULL;
    noc__document_snapshot_release(previous);
    return NOC_WORKSPACE_OK;
}

NOCDEF Noc_Workspace_Status noc_workspace_current_document(
    const Noc_Workspace *workspace,
    Noc_File_Id file_id,
    Noc_Document_Snapshot *output)
{
    const Noc__Workspace_File *file;
    if (!workspace || !output || file_id == NOC_FILE_ID_NONE) {
        return NOC_WORKSPACE_INVALID_ARGUMENT;
    }
    file = noc__workspace_find_id_const(workspace->impl, file_id);
    if (!file || !file->current) return NOC_WORKSPACE_NOT_FOUND;
    if (!noc__document_snapshot_retain(file->current)) {
        return NOC_WORKSPACE_LIMIT_EXCEEDED;
    }
    noc__document_snapshot_replace(output, file->current);
    return NOC_WORKSPACE_OK;
}

NOCDEF Noc_Workspace_Status noc_workspace_find_document(
    const Noc_Workspace *workspace,
    const char *path,
    Noc_Document_Snapshot *output)
{
    const Noc__Workspace_File *file;
    if (!workspace || !path || path[0] == '\0' || !output) {
        return NOC_WORKSPACE_INVALID_ARGUMENT;
    }
    file = noc__workspace_find_path_const(workspace->impl, path);
    if (!file || !file->current) return NOC_WORKSPACE_NOT_FOUND;
    if (!noc__document_snapshot_retain(file->current)) {
        return NOC_WORKSPACE_LIMIT_EXCEEDED;
    }
    noc__document_snapshot_replace(output, file->current);
    return NOC_WORKSPACE_OK;
}

NOCDEF bool noc_document_snapshot_is_current(
    const Noc_Workspace *workspace,
    const Noc_Document_Snapshot *snapshot)
{
    const Noc__Workspace_File *file;
    if (!workspace || !snapshot || !snapshot->impl) return false;
    file = noc__workspace_find_id_const(workspace->impl,
                                        snapshot->impl->identity->file_id);
    return file && file->current == snapshot->impl;
}

NOCDEF Noc_Workspace_Status noc_document_snapshot_clone(
    const Noc_Document_Snapshot *source,
    Noc_Document_Snapshot *output)
{
    if (!source || !source->impl || !output) return NOC_WORKSPACE_INVALID_ARGUMENT;
    if (source == output) return NOC_WORKSPACE_OK;
    if (!noc__document_snapshot_retain(source->impl)) {
        return NOC_WORKSPACE_LIMIT_EXCEEDED;
    }
    noc__document_snapshot_replace(output, source->impl);
    return NOC_WORKSPACE_OK;
}

NOCDEF void noc_document_snapshot_free(Noc_Document_Snapshot *snapshot)
{
    Noc_Document_Snapshot_Impl *implementation;
    if (!snapshot) return;
    implementation = snapshot->impl;
    snapshot->impl = NULL;
    noc__document_snapshot_release(implementation);
}

NOCDEF bool noc_document_snapshot_is_valid(const Noc_Document_Snapshot *snapshot)
{
    return snapshot && snapshot->impl;
}

NOCDEF Noc_File_Id noc_document_snapshot_file_id(
    const Noc_Document_Snapshot *snapshot)
{
    return snapshot && snapshot->impl
               ? snapshot->impl->identity->file_id
               : NOC_FILE_ID_NONE;
}

NOCDEF size_t noc_document_snapshot_generation(
    const Noc_Document_Snapshot *snapshot)
{
    return snapshot && snapshot->impl ? snapshot->impl->generation : 0;
}

NOCDEF const char *noc_document_snapshot_path(
    const Noc_Document_Snapshot *snapshot)
{
    return snapshot && snapshot->impl ? snapshot->impl->identity->path : NULL;
}

NOCDEF Noc_Slice noc_document_snapshot_source(
    const Noc_Document_Snapshot *snapshot)
{
    Noc_Slice source = {0};
    if (snapshot && snapshot->impl) {
        source.data = snapshot->impl->source;
        source.count = snapshot->impl->source_count;
    }
    return source;
}

NOCDEF Noc_Source_Class noc_document_snapshot_source_class(
    const Noc_Document_Snapshot *snapshot)
{
    return snapshot && snapshot->impl
               ? snapshot->impl->source_class
               : NOC_SOURCE_CLASS_PROJECT;
}

NOCDEF Noc_Workspace_Status noc_document_snapshot_location(
    const Noc_Document_Snapshot *snapshot,
    size_t offset,
    Noc_Location *output)
{
    const Noc_Document_Snapshot_Impl *implementation;
    Noc_Location location;
    size_t lower;
    size_t upper;
    if (!snapshot || !snapshot->impl || !output) {
        return NOC_WORKSPACE_INVALID_ARGUMENT;
    }
    implementation = snapshot->impl;
    if (offset > implementation->source_count) return NOC_WORKSPACE_OUT_OF_RANGE;
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
    location.path = implementation->identity->path;
    location.offset = offset;
    location.line = lower + 1;
    location.column = offset - implementation->line_starts[lower] + 1;
    *output = location;
    return NOC_WORKSPACE_OK;
}

NOCDEF Noc_Workspace_Status noc_document_snapshot_offset(
    const Noc_Document_Snapshot *snapshot,
    size_t line,
    size_t byte_column,
    size_t *output)
{
    const Noc_Document_Snapshot_Impl *implementation;
    size_t start;
    size_t end;
    size_t column_offset;
    if (!snapshot || !snapshot->impl || !output || line == 0 || byte_column == 0) {
        return NOC_WORKSPACE_INVALID_ARGUMENT;
    }
    implementation = snapshot->impl;
    if (line > implementation->line_count) return NOC_WORKSPACE_OUT_OF_RANGE;
    start = implementation->line_starts[line - 1];
    end = line < implementation->line_count
              ? implementation->line_starts[line] - 1
              : implementation->source_count;
    column_offset = byte_column - 1;
    if (column_offset > end - start) return NOC_WORKSPACE_OUT_OF_RANGE;
    *output = start + column_offset;
    return NOC_WORKSPACE_OK;
}

#endif /* NOC_WORKSPACE_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
