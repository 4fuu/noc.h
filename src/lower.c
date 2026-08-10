#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_LOWER_IMPLEMENTATION_INCLUDED
#define NOC_LOWER_IMPLEMENTATION_INCLUDED

NOCDEF const Noc_Token *noc_rw_peek_raw(const Noc_Rewriter *rewriter, size_t lookahead)
{
    size_t index = rewriter->cursor + lookahead;
    if (index >= rewriter->tokens_count) return NULL;
    return &rewriter->tokens[index];
}

NOCDEF const Noc_Token *noc_rw_peek(const Noc_Rewriter *rewriter, size_t lookahead)
{
    size_t index = rewriter->cursor;
    size_t found = 0;
    while (index < rewriter->tokens_count) {
        if (!noc_token_is_trivia(rewriter->tokens[index])) {
            if (found == lookahead) return &rewriter->tokens[index];
            found += 1;
        }
        index += 1;
    }
    return NULL;
}

NOCDEF bool noc_rw_take_raw(Noc_Rewriter *rewriter, Noc_Token *token)
{
    if (rewriter->cursor >= rewriter->tokens_count) return false;
    if (token) *token = rewriter->tokens[rewriter->cursor];
    rewriter->cursor += 1;
    return true;
}

NOCDEF void noc_rw_skip_trivia(Noc_Rewriter *rewriter)
{
    while (rewriter->cursor < rewriter->tokens_count &&
           noc_token_is_trivia(rewriter->tokens[rewriter->cursor])) {
        rewriter->cursor += 1;
    }
}

NOCDEF bool noc_rw_match_punct(Noc_Rewriter *rewriter, const char *punctuator)
{
    size_t cursor = rewriter->cursor;
    while (cursor < rewriter->tokens_count &&
           noc_token_is_trivia(rewriter->tokens[cursor])) {
        cursor += 1;
    }
    if (cursor < rewriter->tokens_count &&
        noc_token_is_punct(rewriter->tokens[cursor], punctuator)) {
        rewriter->cursor = cursor + 1;
        return true;
    }
    return false;
}

NOCDEF bool noc_rw_match_identifier(Noc_Rewriter *rewriter, const char *identifier)
{
    size_t cursor = rewriter->cursor;
    while (cursor < rewriter->tokens_count &&
           noc_token_is_trivia(rewriter->tokens[cursor])) {
        cursor += 1;
    }
    if (cursor < rewriter->tokens_count &&
        noc_token_is_identifier(rewriter->tokens[cursor], identifier)) {
        rewriter->cursor = cursor + 1;
        return true;
    }
    return false;
}

NOCDEF bool noc_rw_expect_punct(Noc_Rewriter *rewriter, const char *punctuator)
{
    const Noc_Token *token = noc_rw_peek(rewriter, 0);
    if (noc_rw_match_punct(rewriter, punctuator)) return true;
    if (token) {
        noc_rw_error_at(rewriter,
                        token->location,
                        "expected '%s' after trigger for rule '%s', got %s '%.*s%s'",
                        punctuator,
                        rewriter->rule->name,
                        noc_token_kind_name(token->kind),
                        (int)(token->text.count < 80 ? token->text.count : 80),
                        token->text.data,
                        token->text.count > 80 ? "..." : "");
    } else {
        noc_rw_error(rewriter, "expected '%s' after trigger for rule '%s'", punctuator, rewriter->rule->name);
    }
    return false;
}

NOCDEF bool noc_rw_expect_identifier(Noc_Rewriter *rewriter,
                                     const char *identifier,
                                     Noc_Token *token)
{
    const Noc_Token *next;
    noc_rw_skip_trivia(rewriter);
    next = noc_rw_peek_raw(rewriter, 0);
    if (next && next->kind == NOC_TOKEN_IDENTIFIER &&
        (!identifier || noc_token_is_identifier(*next, identifier))) {
        if (token) *token = *next;
        rewriter->cursor += 1;
        return true;
    }
    if (next) {
        noc_rw_error_at(rewriter,
                        next->location,
                        "expected %sidentifier after trigger for rule '%s', got %s '%.*s%s'",
                        identifier ? identifier : "",
                        rewriter->rule->name,
                        noc_token_kind_name(next->kind),
                        (int)(next->text.count < 80 ? next->text.count : 80),
                        next->text.data,
                        next->text.count > 80 ? "..." : "");
    } else {
        noc_rw_error(rewriter, "expected identifier after trigger for rule '%s'", rewriter->rule->name);
    }
    return false;
}

NOCDEF bool noc_rw_capture_balanced(Noc_Rewriter *rewriter,
                                    const char *open,
                                    const char *close,
                                    Noc_Slice *inside)
{
    const Noc_Token *token;
    size_t start;
    size_t depth = 1;
    if (!noc_rw_expect_punct(rewriter, open)) return false;
    start = rewriter->cursor < rewriter->tokens_count
                ? rewriter->tokens[rewriter->cursor].location.offset
                : rewriter->source_count;
    while (rewriter->cursor < rewriter->tokens_count) {
        token = &rewriter->tokens[rewriter->cursor++];
        if (token->kind == NOC_TOKEN_EOF) break;
        if (noc_token_is_punct(*token, open)) {
            depth += 1;
        } else if (noc_token_is_punct(*token, close)) {
            depth -= 1;
            if (depth == 0) {
                inside->data = rewriter->source + start;
                inside->count = token->location.offset - start;
                return true;
            }
        }
    }
    noc_rw_error(rewriter,
                 "unterminated '%s ... %s' after trigger for rule '%s'",
                 open,
                 close,
                 rewriter->rule->name);
    return false;
}

NOCDEF const char *noc_rw_source_path(const Noc_Rewriter *rewriter)
{
    return rewriter->path;
}

NOCDEF Noc_Location noc_rw_trigger_location(const Noc_Rewriter *rewriter)
{
    return rewriter->trigger_location;
}

NOCDEF Noc_Token_Range noc_rw_trigger_range(const Noc_Rewriter *rewriter)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    return rewriter ? rewriter->trigger_range : invalid;
}

NOCDEF const Noc_Token_Stream *noc_rw_token_stream(const Noc_Rewriter *rewriter)
{
    return rewriter && noc_token_stream_is_valid(rewriter->stream)
               ? rewriter->stream
               : NULL;
}

NOCDEF Noc_Token_Range noc_rw_remaining_range(const Noc_Rewriter *rewriter)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    const Noc_Token_Stream *stream = noc_rw_token_stream(rewriter);
    if (!stream || rewriter->cursor >= stream->count) return invalid;
    invalid.begin = rewriter->cursor;
    invalid.end = stream->count - 1;
    return invalid;
}

NOCDEF bool noc_rw_consume_range(Noc_Rewriter *rewriter, Noc_Token_Range range)
{
    Noc_Token_Range remaining = noc_rw_remaining_range(rewriter);
    if (remaining.begin == NOC_TOKEN_INDEX_NONE || range.begin != remaining.begin ||
        range.end < range.begin || range.end > remaining.end) {
        return false;
    }
    rewriter->cursor = range.end;
    return true;
}

NOCDEF const Noc_Syntax_Tree *noc_rw_syntax_tree(Noc_Rewriter *rewriter)
{
    if (!rewriter || !noc_rw_token_stream(rewriter)) return NULL;
    if (!rewriter->syntax_tree_attempted) {
        rewriter->syntax_tree_attempted = true;
        if (!noc_syntax_tree_build(rewriter->context,
                                   rewriter->stream,
                                   &rewriter->syntax_tree)) {
            rewriter->failed = true;
            return NULL;
        }
    }
    return noc_syntax_tree_is_valid(&rewriter->syntax_tree)
               ? &rewriter->syntax_tree
               : NULL;
}

NOCDEF bool noc_rw_take_syntax(Noc_Rewriter *rewriter,
                               Noc_Syntax_Kind kind,
                               size_t *node)
{
    const Noc_Syntax_Tree *tree;
    size_t cursor;
    size_t i;
    if (!rewriter || kind == NOC_SYNTAX_ROOT) return false;
    cursor = rewriter->cursor;
    while (cursor < rewriter->tokens_count &&
           noc_token_is_trivia(rewriter->tokens[cursor])) {
        cursor += 1;
    }
    tree = noc_rw_syntax_tree(rewriter);
    if (!tree) return false;
    for (i = 1; i < tree->count; ++i) {
        const Noc_Syntax_Node *syntax = &tree->items[i];
        if (syntax->range.begin == cursor) {
            if (syntax->kind != kind) return false;
            rewriter->cursor = syntax->range.end;
            if (node) *node = i;
            return true;
        }
    }
    return false;
}

NOC__PRIVATE void noc__string_list_free(Noc__String_List *list)
{
    size_t i;
    for (i = 0; i < list->count; ++i) free(list->items[i]);
    free(list->items);
    memset(list, 0, sizeof(*list));
}

NOC__PRIVATE bool noc__string_list_append_unique(Noc__String_List *list, const char *text)
{
    char **items;
    char *copy;
    size_t text_count;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    size_t i;
    for (i = 0; i < list->count; ++i) {
        if (strcmp(list->items[i], text) == 0) return true;
    }
    text_count = strlen(text);
    if (text_count == SIZE_MAX) return false;
    copy = (char *)malloc(text_count + 1);
    if (!copy) return false;
    memcpy(copy, text, text_count + 1);
    if (list->count == list->capacity) {
        if (list->capacity >= maximum) goto failed;
        if (list->capacity == 0) {
            capacity = maximum < 4 ? maximum : 4;
        } else if (list->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = list->capacity * 2;
        }
        if (capacity <= list->capacity) goto failed;
        items = (char **)realloc(list->items, capacity * sizeof(*items));
        if (!items) goto failed;
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = copy;
    return true;

failed:
    free(copy);
    return false;
}

NOCDEF bool noc_rw_add_dependency(Noc_Rewriter *rewriter, const char *path)
{
    if (!rewriter || !rewriter->dependencies || !path || !path[0] ||
        !noc__string_list_append_unique(rewriter->dependencies, path)) {
        if (rewriter) {
            noc_rw_error(rewriter,
                         "could not record dependency while expanding rule '%s'",
                         rewriter->rule->name);
        }
        return false;
    }
    return true;
}

NOCDEF bool noc_rw_emit(Noc_Rewriter *rewriter, const void *data, size_t count)
{
    if (!noc_buffer_append(rewriter->output, data, count)) {
        noc_rw_error(rewriter, "out of memory while expanding rule '%s'", rewriter->rule->name);
        return false;
    }
    return true;
}

NOCDEF bool noc_rw_emit_slice(Noc_Rewriter *rewriter, Noc_Slice slice)
{
    return noc_rw_emit(rewriter, slice.data, slice.count);
}

NOCDEF bool noc_rw_emit_cstr(Noc_Rewriter *rewriter, const char *text)
{
    return noc_rw_emit(rewriter, text, strlen(text));
}

NOCDEF bool noc_rw_preserve_newlines(Noc_Rewriter *rewriter, Noc_Slice source)
{
    size_t i = 0;
    if (!rewriter || (source.count > 0 && !source.data)) {
        if (rewriter) noc_rw_error(rewriter, "invalid source while preserving newlines");
        return false;
    }
    while (i < source.count) {
        if (source.data[i] == '\r') {
            size_t count = i + 1 < source.count && source.data[i + 1] == '\n' ? 2 : 1;
            if (!noc_rw_emit(rewriter, source.data + i, count)) return false;
            i += count;
        } else if (source.data[i] == '\n') {
            if (!noc_rw_emit(rewriter, source.data + i, 1)) return false;
            i += 1;
        } else {
            i += 1;
        }
    }
    return true;
}

NOCDEF bool noc_rw_emit_line_directive(Noc_Rewriter *rewriter,
                                       Noc_Location location)
{
    const char *path;
    if (!rewriter || location.line == 0) {
        if (rewriter) noc_rw_error(rewriter, "cannot emit a #line directive for line 0");
        return false;
    }
    path = location.path ? location.path : rewriter->path;
    if (!path) {
        noc_rw_error(rewriter, "cannot emit a #line directive without a source path");
        return false;
    }
    if (rewriter->output->count > 0 &&
        rewriter->output->items[rewriter->output->count - 1] != '\n' &&
        rewriter->output->items[rewriter->output->count - 1] != '\r' &&
        !noc_rw_emit_cstr(rewriter, "\n")) {
        return false;
    }
    if (!noc__emit_line_directive_at(rewriter->output, path, location.line)) {
        noc_rw_error(rewriter, "out of memory while emitting a #line directive");
        return false;
    }
    return true;
}

NOCDEF bool noc_rw_emit_transformed(Noc_Rewriter *rewriter, Noc_Slice source)
{
    Noc_Transform_Result nested = {0};
    size_t i;
    bool ok = false;
    if ((source.count > 0 && !source.data) || rewriter->expansion_depth >= 64) {
        noc_rw_error(rewriter,
                     rewriter->expansion_depth >= 64
                         ? "nested expansion limit reached while expanding rule '%s'"
                         : "invalid nested source while expanding rule '%s'",
                     rewriter->rule->name);
        return false;
    }
    if (!noc__transform_source(rewriter->context,
                               rewriter->path,
                               source.data ? source.data : "",
                               source.count,
                               &nested,
                               rewriter->expansion_depth + 1,
                               false,
                               false,
                               rewriter->rule_phase)) {
        rewriter->failed = true;
        goto done;
    }
    for (i = 0; i < nested.dependency_count; ++i) {
        if (!noc__string_list_append_unique(rewriter->dependencies,
                                            nested.dependencies[i])) {
            noc_rw_error(rewriter,
                         "could not merge nested dependencies while expanding rule '%s'",
                         rewriter->rule->name);
            goto done;
        }
    }
    if (!noc_rw_emit(rewriter, nested.output, nested.output_count)) goto done;
    ok = true;

done:
    noc_transform_result_free(&nested);
    return ok;
}

NOCDEF bool noc_rw_emitf(Noc_Rewriter *rewriter, const char *format, ...)
{
    bool result;
    va_list arguments;
    va_start(arguments, format);
    result = noc__buffer_appendfv(rewriter->output, format, arguments);
    va_end(arguments);
    if (!result) noc_rw_error(rewriter, "out of memory while expanding rule '%s'", rewriter->rule->name);
    return result;
}

NOCDEF bool noc_rw_emit_c_string(Noc_Rewriter *rewriter,
                                 const void *data,
                                 size_t count)
{
    if (!rewriter || (count > 0 && !data) ||
        !noc__buffer_append_c_string(rewriter->output, data, count)) {
        if (rewriter) {
            noc_rw_error(rewriter,
                         "could not emit a C string while expanding rule '%s'",
                         rewriter->rule->name);
        }
        return false;
    }
    return true;
}

NOCDEF void noc_rw_error_at(Noc_Rewriter *rewriter,
                            Noc_Location location,
                            const char *format,
                            ...)
{
    va_list arguments;
    rewriter->failed = true;
    va_start(arguments, format);
    noc__reportv(rewriter->context, NOC_DIAGNOSTIC_ERROR, location, format, arguments);
    va_end(arguments);
}

NOCDEF void noc_rw_error(Noc_Rewriter *rewriter, const char *format, ...)
{
    va_list arguments;
    rewriter->failed = true;
    va_start(arguments, format);
    noc__reportv(rewriter->context,
                 NOC_DIAGNOSTIC_ERROR,
                 rewriter->trigger_location,
                 format,
                 arguments);
    va_end(arguments);
}

static bool noc__edit_range_is_valid(const Noc_Token_Stream *stream,
                                     Noc_Token_Range range)
{
    return noc_token_range_is_valid(stream, range) && stream->count > 0 &&
           range.end <= stream->count - 1;
}

static bool noc__edit_ranges_conflict(Noc_Token_Range left,
                                      Noc_Token_Range right)
{
    bool left_empty = left.begin == left.end;
    bool right_empty = right.begin == right.end;
    if (left_empty && right_empty) return left.begin == right.begin;
    if (left_empty) return left.begin > right.begin && left.begin < right.end;
    if (right_empty) return right.begin > left.begin && right.begin < left.end;
    return left.begin < right.end && right.begin < left.end;
}

NOCDEF bool noc_edit_set_is_valid(const Noc_Edit_Set *edits,
                                  const Noc_Token_Stream *stream)
{
    size_t i;
    if (!edits || !noc_token_stream_is_valid(stream) || edits->count > edits->capacity ||
        (edits->count > 0 && !edits->items)) {
        return false;
    }
    if (edits->count == 0) return true;
    if (edits->stream != stream || edits->stream_generation != stream->generation) {
        return false;
    }
    for (i = 0; i < edits->count; ++i) {
        const Noc_Edit *edit = &edits->items[i];
        if (!noc__edit_range_is_valid(stream, edit->range) ||
            (edit->replacement_count > 0 && !edit->replacement)) {
            return false;
        }
        if (i > 0) {
            const Noc_Edit *previous = &edits->items[i - 1];
            if (previous->range.begin > edit->range.begin ||
                (previous->range.begin == edit->range.begin &&
                 previous->range.begin != previous->range.end &&
                 edit->range.begin == edit->range.end) ||
                noc__edit_ranges_conflict(previous->range, edit->range)) {
                return false;
            }
        }
    }
    return true;
}

NOCDEF bool noc_edit_set_add(Noc_Edit_Set *edits,
                             const Noc_Token_Stream *stream,
                             Noc_Token_Range range,
                             Noc_Slice replacement)
{
    Noc_Edit *items;
    char *replacement_copy = NULL;
    size_t position = 0;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    size_t i;
    if (!edits || !noc__edit_range_is_valid(stream, range) ||
        (replacement.count > 0 && !replacement.data) || edits->count > edits->capacity ||
        (edits->count > 0 && !noc_edit_set_is_valid(edits, stream))) {
        return false;
    }
    for (i = 0; i < edits->count; ++i) {
        if (noc__edit_ranges_conflict(edits->items[i].range, range)) return false;
        if (edits->items[i].range.begin < range.begin ||
            (edits->items[i].range.begin == range.begin &&
             edits->items[i].range.begin == edits->items[i].range.end &&
             range.begin != range.end)) {
            position = i + 1;
        }
    }
    if (replacement.count > 0) {
        replacement_copy = (char *)malloc(replacement.count);
        if (!replacement_copy) return false;
        memcpy(replacement_copy, replacement.data, replacement.count);
    }
    if (edits->count == edits->capacity) {
        if (edits->capacity >= maximum) goto failed;
        if (edits->capacity == 0) {
            capacity = maximum < 8 ? maximum : 8;
        } else if (edits->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = edits->capacity * 2;
        }
        if (capacity <= edits->capacity) goto failed;
        items = (Noc_Edit *)realloc(edits->items, capacity * sizeof(*items));
        if (!items) goto failed;
        edits->items = items;
        edits->capacity = capacity;
    }
    if (position < edits->count) {
        memmove(&edits->items[position + 1],
                &edits->items[position],
                (edits->count - position) * sizeof(*edits->items));
    }
    edits->items[position].range = range;
    edits->items[position].replacement = replacement_copy;
    edits->items[position].replacement_count = replacement.count;
    edits->count += 1;
    edits->stream = stream;
    edits->stream_generation = stream->generation;
    return true;

failed:
    free(replacement_copy);
    return false;
}

NOCDEF bool noc_edit_set_add_cstr(Noc_Edit_Set *edits,
                                  const Noc_Token_Stream *stream,
                                  Noc_Token_Range range,
                                  const char *replacement)
{
    Noc_Slice slice;
    if (!replacement) return false;
    slice.data = replacement;
    slice.count = strlen(replacement);
    return noc_edit_set_add(edits, stream, range, slice);
}

NOCDEF bool noc_edit_set_add_syntax(Noc_Edit_Set *edits,
                                    const Noc_Syntax_Tree *tree,
                                    size_t node,
                                    Noc_Slice replacement)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax && noc_edit_set_add(edits, tree->stream, syntax->range, replacement);
}

NOCDEF bool noc_edit_set_apply(const Noc_Edit_Set *edits,
                               const Noc_Token_Stream *stream,
                               Noc_Buffer *output)
{
    Noc_Buffer parsed = {0};
    size_t source_offset = 0;
    size_t i;
    if (!output || !noc_edit_set_is_valid(edits, stream)) return false;
    for (i = 0; i < edits->count; ++i) {
        const Noc_Edit *edit = &edits->items[i];
        Noc_Slice source = noc_token_range_source(stream, edit->range);
        size_t begin_offset;
        size_t end_offset;
        if (!source.data) goto failed;
        begin_offset = (size_t)(source.data - stream->source);
        end_offset = begin_offset + source.count;
        if (begin_offset < source_offset || end_offset < begin_offset ||
            end_offset > stream->source_count ||
            !noc_buffer_append(&parsed,
                               stream->source + source_offset,
                               begin_offset - source_offset) ||
            !noc_buffer_append(&parsed,
                               edit->replacement,
                               edit->replacement_count)) {
            goto failed;
        }
        source_offset = end_offset;
    }
    if (!noc_buffer_append(&parsed,
                           stream->source + source_offset,
                           stream->source_count - source_offset) ||
        !noc_buffer_terminate(&parsed)) {
        goto failed;
    }
    noc_buffer_free(output);
    *output = parsed;
    return true;

failed:
    noc_buffer_free(&parsed);
    return false;
}

NOCDEF void noc_edit_set_free(Noc_Edit_Set *edits)
{
    size_t i;
    for (i = 0; i < edits->count; ++i) free(edits->items[i].replacement);
    free(edits->items);
    memset(edits, 0, sizeof(*edits));
}

#endif /* NOC_LOWER_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
