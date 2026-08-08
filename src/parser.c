#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_PARSER_IMPLEMENTATION_INCLUDED
#define NOC_PARSER_IMPLEMENTATION_INCLUDED

NOCDEF bool noc_token_range_is_valid(const Noc_Token_Stream *stream,
                                     Noc_Token_Range range)
{
    return noc_token_stream_is_valid(stream) &&
           range.begin <= range.end && range.end <= stream->count;
}

NOCDEF Noc_Token_Range noc_token_range_trim_trivia(const Noc_Token_Stream *stream,
                                                    Noc_Token_Range range)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    if (!noc_token_range_is_valid(stream, range)) return invalid;
    while (range.begin < range.end && noc_token_is_trivia(stream->items[range.begin])) {
        range.begin += 1;
    }
    while (range.end > range.begin && noc_token_is_trivia(stream->items[range.end - 1])) {
        range.end -= 1;
    }
    return range;
}

NOCDEF Noc_Slice noc_token_range_source(const Noc_Token_Stream *stream,
                                        Noc_Token_Range range)
{
    Noc_Slice result = {0};
    size_t begin_offset;
    size_t end_offset;
    const Noc_Token *last;
    if (!noc_token_range_is_valid(stream, range)) return result;
    begin_offset = range.begin < stream->count
                       ? stream->items[range.begin].location.offset
                       : stream->source_count;
    if (range.begin == range.end) {
        result.data = stream->source + begin_offset;
        return result;
    }
    last = &stream->items[range.end - 1];
    end_offset = last->location.offset + last->text.count;
    if (begin_offset > end_offset || end_offset > stream->source_count) return result;
    result.data = stream->source + begin_offset;
    result.count = end_offset - begin_offset;
    return result;
}

NOCDEF Noc_Location noc_token_range_location(const Noc_Token_Stream *stream,
                                             Noc_Token_Range range)
{
    Noc_Location location = {0};
    if (!noc_token_range_is_valid(stream, range)) return location;
    if (range.begin < stream->count) return stream->items[range.begin].location;
    if (stream->count > 0) return stream->items[stream->count - 1].location;
    location.path = stream->path;
    location.offset = stream->source_count;
    return location;
}

NOCDEF void noc_token_cursor_init(Noc_Token_Cursor *cursor,
                                  const Noc_Token_Stream *stream)
{
    if (!noc_token_stream_is_valid(stream)) {
        memset(cursor, 0, sizeof(*cursor));
        return;
    }
    cursor->stream = stream;
    cursor->begin = 0;
    cursor->index = 0;
    cursor->end = stream ? stream->count : 0;
}

NOCDEF bool noc_token_cursor_init_range(Noc_Token_Cursor *cursor,
                                        const Noc_Token_Stream *stream,
                                        Noc_Token_Range range)
{
    if (!noc_token_range_is_valid(stream, range)) {
        memset(cursor, 0, sizeof(*cursor));
        return false;
    }
    cursor->stream = stream;
    cursor->begin = range.begin;
    cursor->index = range.begin;
    cursor->end = range.end;
    return true;
}

NOCDEF size_t noc_token_cursor_mark(const Noc_Token_Cursor *cursor)
{
    return cursor->index;
}

NOCDEF bool noc_token_cursor_rewind(Noc_Token_Cursor *cursor, size_t mark)
{
    if (!cursor->stream || mark < cursor->begin || mark > cursor->end) return false;
    cursor->index = mark;
    return true;
}

NOCDEF const Noc_Token *noc_token_cursor_peek_raw(const Noc_Token_Cursor *cursor,
                                                  size_t lookahead)
{
    if (!cursor->stream || cursor->index > cursor->end ||
        lookahead >= cursor->end - cursor->index) {
        return NULL;
    }
    return &cursor->stream->items[cursor->index + lookahead];
}

NOCDEF const Noc_Token *noc_token_cursor_peek(const Noc_Token_Cursor *cursor,
                                              size_t lookahead)
{
    size_t index;
    size_t found = 0;
    if (!cursor->stream) return NULL;
    index = cursor->index;
    while (index < cursor->end) {
        if (!noc_token_is_trivia(cursor->stream->items[index])) {
            if (found == lookahead) return &cursor->stream->items[index];
            found += 1;
        }
        index += 1;
    }
    return NULL;
}

NOCDEF bool noc_token_cursor_at_end(const Noc_Token_Cursor *cursor)
{
    const Noc_Token *token = noc_token_cursor_peek(cursor, 0);
    return !token || token->kind == NOC_TOKEN_EOF;
}

NOCDEF bool noc_token_cursor_take_raw(Noc_Token_Cursor *cursor, Noc_Token *token)
{
    const Noc_Token *next = noc_token_cursor_peek_raw(cursor, 0);
    if (!next) return false;
    if (token) *token = *next;
    cursor->index += 1;
    return true;
}

NOCDEF void noc_token_cursor_skip_trivia(Noc_Token_Cursor *cursor)
{
    if (!cursor->stream) return;
    while (cursor->index < cursor->end &&
           noc_token_is_trivia(cursor->stream->items[cursor->index])) {
        cursor->index += 1;
    }
}

NOCDEF bool noc_token_cursor_take(Noc_Token_Cursor *cursor, Noc_Token *token)
{
    noc_token_cursor_skip_trivia(cursor);
    return noc_token_cursor_take_raw(cursor, token);
}

NOCDEF bool noc_token_cursor_match_kind(Noc_Token_Cursor *cursor,
                                        Noc_Token_Kind kind,
                                        Noc_Token *token)
{
    Noc_Token_Cursor candidate = *cursor;
    Noc_Token matched;
    if (!noc_token_cursor_take(&candidate, &matched) || matched.kind != kind) return false;
    *cursor = candidate;
    if (token) *token = matched;
    return true;
}

NOCDEF bool noc_token_cursor_match_punct(Noc_Token_Cursor *cursor,
                                         const char *punctuator,
                                         Noc_Token *token)
{
    Noc_Token_Cursor candidate = *cursor;
    Noc_Token matched;
    if (!noc_token_cursor_take(&candidate, &matched) ||
        !noc_token_is_punct(matched, punctuator)) {
        return false;
    }
    *cursor = candidate;
    if (token) *token = matched;
    return true;
}

NOCDEF bool noc_token_cursor_match_identifier(Noc_Token_Cursor *cursor,
                                              const char *identifier,
                                              Noc_Token *token)
{
    Noc_Token_Cursor candidate = *cursor;
    Noc_Token matched;
    if (!noc_token_cursor_take(&candidate, &matched) ||
        matched.kind != NOC_TOKEN_IDENTIFIER ||
        (identifier && !noc_slice_equal_cstr(matched.text, identifier))) {
        return false;
    }
    *cursor = candidate;
    if (token) *token = matched;
    return true;
}

NOCDEF bool noc_token_cursor_take_balanced(Noc_Token_Cursor *cursor,
                                           const char *open,
                                           const char *close,
                                           Noc_Token_Range *whole,
                                           Noc_Token_Range *inside)
{
    Noc_Token_Cursor candidate = *cursor;
    Noc_Buffer delimiter_stack = {0};
    size_t open_index;
    Noc_Token token;
    char initial_close;
    bool ok = false;
    if (strcmp(open, "(") == 0 && strcmp(close, ")") == 0) initial_close = ')';
    else if (strcmp(open, "[") == 0 && strcmp(close, "]") == 0) initial_close = ']';
    else if (strcmp(open, "{") == 0 && strcmp(close, "}") == 0) initial_close = '}';
    else return false;
    noc_token_cursor_skip_trivia(&candidate);
    open_index = candidate.index;
    if (!noc_token_cursor_match_punct(&candidate, open, NULL)) return false;
    if (!noc_buffer_append(&delimiter_stack, &initial_close, 1)) return false;
    while (noc_token_cursor_take_raw(&candidate, &token)) {
        size_t token_index = candidate.index - 1;
        char expected = 0;
        bool closing = false;
        if (noc_token_is_punct(token, "(")) expected = ')';
        else if (noc_token_is_punct(token, "[")) expected = ']';
        else if (noc_token_is_punct(token, "{")) expected = '}';
        else if (noc_token_is_punct(token, ")") || noc_token_is_punct(token, "]") ||
                 noc_token_is_punct(token, "}")) {
            closing = true;
        }
        if (expected != 0) {
            if (!noc_buffer_append(&delimiter_stack, &expected, 1)) goto done;
        } else if (closing) {
            if (delimiter_stack.count == 0 ||
                delimiter_stack.items[delimiter_stack.count - 1] != token.text.data[0]) {
                goto done;
            }
            delimiter_stack.count -= 1;
            if (delimiter_stack.count == 0) {
                if (whole) {
                    whole->begin = open_index;
                    whole->end = candidate.index;
                }
                if (inside) {
                    inside->begin = open_index + 1;
                    inside->end = token_index;
                }
                *cursor = candidate;
                ok = true;
                goto done;
            }
        }
    }

done:
    noc_buffer_free(&delimiter_stack);
    return ok;
}

static bool noc__argument_list_append(Noc_Argument_List *arguments, Noc_Token_Range range)
{
    Noc_Token_Range *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (arguments->count > arguments->capacity) return false;
    if (arguments->count == arguments->capacity) {
        if (arguments->capacity >= maximum) return false;
        if (arguments->capacity == 0) {
            capacity = maximum < 4 ? maximum : 4;
        } else if (arguments->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = arguments->capacity * 2;
        }
        if (capacity <= arguments->capacity) return false;
        items = (Noc_Token_Range *)realloc(arguments->items, capacity * sizeof(*items));
        if (!items) return false;
        arguments->items = items;
        arguments->capacity = capacity;
    }
    arguments->items[arguments->count++] = range;
    return true;
}

NOCDEF void noc_argument_list_free(Noc_Argument_List *arguments)
{
    free(arguments->items);
    memset(arguments, 0, sizeof(*arguments));
}

NOCDEF bool noc_parse_arguments(const Noc_Token_Stream *stream,
                                Noc_Token_Range range,
                                Noc_Argument_List *arguments)
{
    Noc_Argument_List parsed = {0};
    Noc_Buffer delimiter_stack = {0};
    Noc_Token_Range trimmed;
    size_t argument_begin;
    size_t i;
    bool saw_comma = false;
    bool ok = false;
    if (!noc_token_range_is_valid(stream, range)) return false;
    if (range.end > range.begin && stream->items[range.end - 1].kind == NOC_TOKEN_EOF) {
        range.end -= 1;
    }
    trimmed = noc_token_range_trim_trivia(stream, range);
    if (trimmed.begin == trimmed.end) {
        noc_argument_list_free(arguments);
        return true;
    }
    argument_begin = range.begin;
    for (i = range.begin; i < range.end; ++i) {
        Noc_Token token = stream->items[i];
        char expected = 0;
        if (token.kind != NOC_TOKEN_PUNCTUATOR) continue;
        if (noc_token_is_punct(token, "(")) expected = ')';
        else if (noc_token_is_punct(token, "[")) expected = ']';
        else if (noc_token_is_punct(token, "{")) expected = '}';
        if (expected != 0) {
            if (!noc_buffer_append(&delimiter_stack, &expected, 1)) goto done;
            continue;
        }
        if (noc_token_is_punct(token, ")") || noc_token_is_punct(token, "]") ||
            noc_token_is_punct(token, "}")) {
            if (delimiter_stack.count == 0 ||
                delimiter_stack.items[delimiter_stack.count - 1] != token.text.data[0]) {
                goto done;
            }
            delimiter_stack.count -= 1;
            continue;
        }
        if (noc_token_is_punct(token, ",") && delimiter_stack.count == 0) {
            Noc_Token_Range argument = {argument_begin, i};
            argument = noc_token_range_trim_trivia(stream, argument);
            if (!noc__argument_list_append(&parsed, argument)) goto done;
            argument_begin = i + 1;
            saw_comma = true;
        }
    }
    if (delimiter_stack.count != 0) goto done;
    {
        Noc_Token_Range argument = {argument_begin, range.end};
        argument = noc_token_range_trim_trivia(stream, argument);
        if (argument.begin != argument.end || saw_comma) {
            if (!noc__argument_list_append(&parsed, argument)) goto done;
        }
    }
    noc_argument_list_free(arguments);
    *arguments = parsed;
    memset(&parsed, 0, sizeof(parsed));
    ok = true;

done:
    noc_buffer_free(&delimiter_stack);
    noc_argument_list_free(&parsed);
    return ok;
}

#endif /* NOC_PARSER_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
