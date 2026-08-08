#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_LEXER_IMPLEMENTATION_INCLUDED
#define NOC_LEXER_IMPLEMENTATION_INCLUDED

static bool noc__is_identifier_start(unsigned char c)
{
    return c == '_' || isalpha(c) || c >= 128;
}

static bool noc__is_identifier_continue(unsigned char c)
{
    return c == '_' || isalnum(c) || c >= 128;
}

static bool noc__is_horizontal_space(char c)
{
    return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

NOC__PRIVATE bool noc__contains_newline(const char *data, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        if (data[i] == '\n' || data[i] == '\r') return true;
    }
    return false;
}

NOC__PRIVATE size_t noc__splice_length(const char *source, size_t count, size_t position)
{
    if (position + 1 >= count || source[position] != '\\') return 0;
    if (source[position + 1] == '\n') return 2;
    if (source[position + 1] == '\r') {
        return position + 2 < count && source[position + 2] == '\n' ? 3 : 2;
    }
    return 0;
}

static size_t noc__skip_splices(const char *source, size_t count, size_t position)
{
    size_t splice;
    while ((splice = noc__splice_length(source, count, position)) != 0) {
        position += splice;
    }
    return position;
}

static size_t noc__universal_character_name_end(const char *source,
                                                size_t count,
                                                size_t start)
{
    size_t position;
    size_t digit_count;
    size_t index;
    if (start >= count || source[start] != '\\') return start;
    position = noc__skip_splices(source, count, start + 1);
    if (position >= count || (source[position] != 'u' && source[position] != 'U')) {
        return start;
    }
    digit_count = source[position] == 'u' ? 4 : 8;
    position += 1;
    for (index = 0; index < digit_count; ++index) {
        position = noc__skip_splices(source, count, position);
        if (position >= count ||
            !isxdigit((unsigned char)source[position])) {
            return start;
        }
        position += 1;
    }
    return position;
}

static bool noc__slice_logically_equal_cstr(Noc_Slice slice, const char *text)
{
    size_t source_index = 0;
    size_t text_index = 0;
    size_t text_count = text ? strlen(text) : 0;
    if (!slice.data && slice.count != 0) return false;
    while (source_index < slice.count) {
        size_t splice = noc__splice_length(slice.data, slice.count, source_index);
        if (splice != 0) {
            source_index += splice;
            continue;
        }
        if (text_index >= text_count || slice.data[source_index] != text[text_index]) {
            return false;
        }
        source_index += 1;
        text_index += 1;
    }
    return text_index == text_count;
}

NOC__PRIVATE bool noc__slices_logically_equal(Noc_Slice left, Noc_Slice right)
{
    size_t left_index = 0;
    size_t right_index = 0;
    if ((!left.data && left.count != 0) || (!right.data && right.count != 0)) {
        return false;
    }
    for (;;) {
        size_t splice;
        while (left_index < left.count &&
               (splice = noc__splice_length(left.data, left.count, left_index)) != 0) {
            left_index += splice;
        }
        while (right_index < right.count &&
               (splice = noc__splice_length(right.data, right.count, right_index)) != 0) {
            right_index += splice;
        }
        if (left_index == left.count || right_index == right.count) {
            return left_index == left.count && right_index == right.count;
        }
        if (left.data[left_index] != right.data[right_index]) return false;
        left_index += 1;
        right_index += 1;
    }
}

static void noc__lexer_advance(Noc_Lexer *lexer, size_t end)
{
    while (lexer->cursor < end) {
        char c = lexer->source[lexer->cursor++];
        if (c == '\r') {
            if (lexer->cursor < end && lexer->source[lexer->cursor] == '\n') {
                lexer->cursor += 1;
            }
            lexer->line += 1;
            lexer->column = 1;
        } else if (c == '\n') {
            lexer->line += 1;
            lexer->column = 1;
        } else {
            lexer->column += 1;
        }
    }
}

NOC__PRIVATE Noc_Token noc__make_token(Noc_Lexer *lexer,
                                 Noc_Token_Kind kind,
                                 size_t start,
                                 size_t end,
                                 Noc_Location location)
{
    Noc_Token token;
    token.kind = kind;
    token.text.data = lexer->source + start;
    token.text.count = end - start;
    token.location = location;
    noc__lexer_advance(lexer, end);
    return token;
}

NOCDEF Noc_Slice noc_slice_from_cstr(const char *text)
{
    Noc_Slice slice;
    slice.data = text;
    slice.count = text ? strlen(text) : 0;
    return slice;
}

NOCDEF bool noc_slice_equal(Noc_Slice left, Noc_Slice right)
{
    return left.count == right.count &&
           (left.count == 0 || memcmp(left.data, right.data, left.count) == 0);
}

NOCDEF bool noc_slice_equal_cstr(Noc_Slice slice, const char *text)
{
    return noc_slice_equal(slice, noc_slice_from_cstr(text));
}

NOCDEF const char *noc_token_kind_name(Noc_Token_Kind kind)
{
    switch (kind) {
    case NOC_TOKEN_EOF: return "end of file";
    case NOC_TOKEN_WHITESPACE: return "whitespace";
    case NOC_TOKEN_NEWLINE: return "newline";
    case NOC_TOKEN_IDENTIFIER: return "identifier";
    case NOC_TOKEN_NUMBER: return "number";
    case NOC_TOKEN_STRING: return "string";
    case NOC_TOKEN_CHARACTER: return "character";
    case NOC_TOKEN_LINE_COMMENT: return "line comment";
    case NOC_TOKEN_BLOCK_COMMENT: return "block comment";
    case NOC_TOKEN_PREPROCESSOR: return "preprocessor directive";
    case NOC_TOKEN_HEADER_NAME: return "header name";
    case NOC_TOKEN_PUNCTUATOR: return "punctuator";
    case NOC_TOKEN_OTHER: return "non-whitespace character";
    case NOC_TOKEN_INVALID: return "invalid token";
    }
    return "unknown token";
}

NOCDEF bool noc_token_is_trivia(Noc_Token token)
{
    return token.kind == NOC_TOKEN_WHITESPACE ||
           token.kind == NOC_TOKEN_NEWLINE ||
           token.kind == NOC_TOKEN_LINE_COMMENT ||
           token.kind == NOC_TOKEN_BLOCK_COMMENT;
}

NOCDEF bool noc_token_is_punct(Noc_Token token, const char *punctuator)
{
    return token.kind == NOC_TOKEN_PUNCTUATOR &&
           noc__slice_logically_equal_cstr(token.text, punctuator);
}

NOCDEF bool noc_token_is_identifier(Noc_Token token, const char *identifier)
{
    return token.kind == NOC_TOKEN_IDENTIFIER &&
           noc__slice_logically_equal_cstr(token.text, identifier);
}

NOCDEF void noc_lexer_init(Noc_Lexer *lexer,
                           const char *path,
                           const char *source,
                           size_t source_count)
{
    lexer->path = path;
    lexer->source = source;
    lexer->source_count = source_count;
    lexer->cursor = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->beginning_of_line = true;
    lexer->continuing_block_comment = false;
}

NOC__PRIVATE bool noc__logical_pair(const char *source,
                              size_t count,
                              size_t position,
                              char first,
                              char second,
                              size_t *second_position)
{
    size_t next;
    if (position >= count || source[position] != first) return false;
    next = noc__skip_splices(source, count, position + 1);
    if (next >= count || source[next] != second) return false;
    if (second_position) *second_position = next;
    return true;
}

static bool noc__quoted_prefix(const char *source,
                               size_t count,
                               size_t start,
                               size_t *quote_position)
{
    size_t next;
    if (start >= count) return false;
    if (source[start] == '"' || source[start] == '\'') {
        *quote_position = start;
        return true;
    }
    next = noc__skip_splices(source, count, start + 1);
    if ((source[start] == 'L' || source[start] == 'u' || source[start] == 'U') &&
        next < count && (source[next] == '"' || source[next] == '\'')) {
        *quote_position = next;
        return true;
    }
    if (source[start] == 'u' && next < count && source[next] == '8') {
        next = noc__skip_splices(source, count, next + 1);
        if (next < count && source[next] == '"') {
            *quote_position = next;
            return true;
        }
    }
    return false;
}

static size_t noc__scan_line_comment(const Noc_Lexer *lexer, size_t second_slash)
{
    size_t i = second_slash + 1;
    while (i < lexer->source_count) {
        size_t splice = noc__splice_length(lexer->source, lexer->source_count, i);
        if (splice != 0) {
            i += splice;
            continue;
        }
        if (lexer->source[i] == '\n' || lexer->source[i] == '\r') break;
        i += 1;
    }
    return i;
}

static size_t noc__scan_block_comment(const Noc_Lexer *lexer,
                                      size_t content_start,
                                      bool *closed)
{
    size_t i = content_start;
    *closed = false;
    while (i < lexer->source_count) {
        size_t slash;
        size_t splice;
        if (noc__logical_pair(lexer->source,
                              lexer->source_count,
                              i,
                              '*',
                              '/',
                              &slash)) {
            *closed = true;
            return slash + 1;
        }
        splice = noc__splice_length(lexer->source, lexer->source_count, i);
        i += splice ? splice : 1;
    }
    return i;
}

static size_t noc__scan_preprocessor(Noc_Lexer *lexer, size_t start)
{
    size_t i = start;
    char quote = 0;
    bool block_comment = false;
    bool line_comment = false;
    while (i < lexer->source_count) {
        size_t second;
        size_t splice = noc__splice_length(lexer->source, lexer->source_count, i);
        char c;
        if (splice != 0) {
            i += splice;
            continue;
        }
        c = lexer->source[i];
        if (block_comment) {
            if (noc__logical_pair(lexer->source,
                                  lexer->source_count,
                                  i,
                                  '*',
                                  '/',
                                  &second)) {
                block_comment = false;
                i = second + 1;
            } else {
                i += 1;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            i += 1;
            if (c == '\r' && i < lexer->source_count && lexer->source[i] == '\n') i += 1;
            return i;
        }
        if (line_comment) {
            i += 1;
            continue;
        }
        if (quote != 0) {
            if (c == '\\' && i + 1 < lexer->source_count) {
                i += 2;
            } else {
                if (c == quote) quote = 0;
                i += 1;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            i += 1;
            continue;
        }
        if (noc__logical_pair(lexer->source,
                              lexer->source_count,
                              i,
                              '/',
                              '/',
                              &second)) {
            line_comment = true;
            i = second + 1;
            continue;
        }
        if (noc__logical_pair(lexer->source,
                              lexer->source_count,
                              i,
                              '/',
                              '*',
                              &second)) {
            block_comment = true;
            i = second + 1;
            continue;
        }
        i += 1;
    }
    if (block_comment) lexer->continuing_block_comment = true;
    return i;
}

static bool noc__match_logical_punctuator(const char *source,
                                          size_t count,
                                          size_t start,
                                          const char *punctuator,
                                          size_t *end)
{
    size_t position = start;
    size_t i;
    for (i = 0; punctuator[i] != '\0'; ++i) {
        if (i != 0) position = noc__skip_splices(source, count, position);
        if (position >= count || source[position] != punctuator[i]) return false;
        position += 1;
    }
    *end = position;
    return true;
}

static size_t noc__punctuator_end(const char *source, size_t count, size_t start)
{
    static const char *const punctuators4[] = {
        "%:%:", NULL
    };
    static const char *const punctuators3[] = {
        "<<=", ">>=", "...", NULL
    };
    static const char *const punctuators2[] = {
        "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
        "&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=",
        "|=", "##", "<:", ":>", "<%", "%>", "%:", NULL
    };
    size_t i;
    size_t end;
    for (i = 0; punctuators4[i]; ++i) {
        if (noc__match_logical_punctuator(source,
                                          count,
                                          start,
                                          punctuators4[i],
                                          &end)) return end;
    }
    for (i = 0; punctuators3[i]; ++i) {
        if (noc__match_logical_punctuator(source,
                                          count,
                                          start,
                                          punctuators3[i],
                                          &end)) return end;
    }
    for (i = 0; punctuators2[i]; ++i) {
        if (noc__match_logical_punctuator(source,
                                          count,
                                          start,
                                          punctuators2[i],
                                          &end)) return end;
    }
    return start + 1;
}

NOCDEF Noc_Token noc_lexer_next(Noc_Lexer *lexer)
{
    size_t start = lexer->cursor;
    size_t end = start;
    size_t quote = 0;
    bool closed = false;
    Noc_Token_Kind kind;
    Noc_Location location;
    Noc_Token token;

    location.path = lexer->path;
    location.offset = start;
    location.line = lexer->line;
    location.column = lexer->column;

    if (start >= lexer->source_count) {
        if (lexer->continuing_block_comment) {
            lexer->continuing_block_comment = false;
            token.kind = NOC_TOKEN_INVALID;
            token.text.data = lexer->source + lexer->source_count;
            token.text.count = 0;
            token.location = location;
            return token;
        }
        token.kind = NOC_TOKEN_EOF;
        token.text.data = lexer->source + lexer->source_count;
        token.text.count = 0;
        token.location = location;
        return token;
    }

    if (lexer->continuing_block_comment) {
        end = noc__scan_block_comment(lexer, start, &closed);
        lexer->continuing_block_comment = !closed;
        token = noc__make_token(lexer,
                                closed ? NOC_TOKEN_BLOCK_COMMENT : NOC_TOKEN_INVALID,
                                start,
                                end,
                                location);
        lexer->beginning_of_line = true;
        return token;
    }

    if (lexer->beginning_of_line &&
        (lexer->source[start] == '#' ||
         noc__logical_pair(lexer->source,
                           lexer->source_count,
                           start,
                           '%',
                           ':',
                           NULL))) {
        Noc_Slice directive_marker;
        end = noc__punctuator_end(lexer->source, lexer->source_count, start);
        directive_marker.data = lexer->source + start;
        directive_marker.count = end - start;
        if (noc__slice_logically_equal_cstr(directive_marker, "#") ||
            noc__slice_logically_equal_cstr(directive_marker, "%:")) {
            end = noc__scan_preprocessor(lexer, start);
            token = noc__make_token(lexer, NOC_TOKEN_PREPROCESSOR, start, end, location);
            lexer->beginning_of_line = noc__contains_newline(token.text.data,
                                                             token.text.count);
            return token;
        }
    }

    if (lexer->source[start] == '\r' || lexer->source[start] == '\n') {
        end = start + 1;
        if (lexer->source[start] == '\r' && end < lexer->source_count &&
            lexer->source[end] == '\n') {
            end += 1;
        }
        token = noc__make_token(lexer, NOC_TOKEN_NEWLINE, start, end, location);
        lexer->beginning_of_line = true;
        return token;
    }

    if (noc__is_horizontal_space(lexer->source[start]) ||
        (lexer->source[start] == '\\' && start + 1 < lexer->source_count &&
         (lexer->source[start + 1] == '\n' || lexer->source[start + 1] == '\r'))) {
        end = start;
        while (end < lexer->source_count) {
            if (noc__is_horizontal_space(lexer->source[end])) {
                end += 1;
            } else if (lexer->source[end] == '\\' && end + 1 < lexer->source_count &&
                       lexer->source[end + 1] == '\n') {
                end += 2;
            } else if (lexer->source[end] == '\\' && end + 1 < lexer->source_count &&
                       lexer->source[end + 1] == '\r') {
                end += 2;
                if (end < lexer->source_count && lexer->source[end] == '\n') end += 1;
            } else {
                break;
            }
        }
        return noc__make_token(lexer, NOC_TOKEN_WHITESPACE, start, end, location);
    }

    if (noc__logical_pair(lexer->source,
                          lexer->source_count,
                          start,
                          '/',
                          '/',
                          &end)) {
        end = noc__scan_line_comment(lexer, end);
        return noc__make_token(lexer, NOC_TOKEN_LINE_COMMENT, start, end, location);
    }

    if (noc__logical_pair(lexer->source,
                          lexer->source_count,
                          start,
                          '/',
                          '*',
                          &end)) {
        end = noc__scan_block_comment(lexer, end + 1, &closed);
        token = noc__make_token(lexer,
                                closed ? NOC_TOKEN_BLOCK_COMMENT : NOC_TOKEN_INVALID,
                                start,
                                end,
                                location);
        return token;
    }

    if (noc__quoted_prefix(lexer->source, lexer->source_count, start, &quote)) {
        char quote_character = lexer->source[quote];
        end = quote + 1;
        while (end < lexer->source_count) {
            if (lexer->source[end] == '\\') {
                end += 1;
                if (end < lexer->source_count) {
                    if (lexer->source[end] == '\r' && end + 1 < lexer->source_count &&
                        lexer->source[end + 1] == '\n') {
                        end += 2;
                    } else {
                        end += 1;
                    }
                }
            } else if (lexer->source[end] == quote_character) {
                end += 1;
                closed = true;
                break;
            } else if (lexer->source[end] == '\n' || lexer->source[end] == '\r') {
                break;
            } else {
                end += 1;
            }
        }
        kind = quote_character == '"' ? NOC_TOKEN_STRING : NOC_TOKEN_CHARACTER;
        token = noc__make_token(lexer, closed ? kind : NOC_TOKEN_INVALID, start, end, location);
        lexer->beginning_of_line = false;
        return token;
    }

    end = noc__universal_character_name_end(lexer->source,
                                            lexer->source_count,
                                            start);
    if (noc__is_identifier_start((unsigned char)lexer->source[start]) ||
        end != start) {
        if (end == start) end = start + 1;
        while (end < lexer->source_count) {
            size_t next = noc__skip_splices(lexer->source, lexer->source_count, end);
            size_t ucn_end;
            if (next >= lexer->source_count) break;
            ucn_end = noc__universal_character_name_end(lexer->source,
                                                        lexer->source_count,
                                                        next);
            if (ucn_end != next) {
                end = ucn_end;
            } else if (noc__is_identifier_continue(
                           (unsigned char)lexer->source[next])) {
                end = next + 1;
            } else {
                break;
            }
        }
        lexer->beginning_of_line = false;
        return noc__make_token(lexer, NOC_TOKEN_IDENTIFIER, start, end, location);
    }

    if (isdigit((unsigned char)lexer->source[start]) ||
        (lexer->source[start] == '.' &&
         (end = noc__skip_splices(lexer->source,
                                  lexer->source_count,
                                  start + 1)) < lexer->source_count &&
         isdigit((unsigned char)lexer->source[end]))) {
        unsigned char previous = (unsigned char)lexer->source[start];
        end = start + 1;
        while (end < lexer->source_count) {
            size_t next = noc__skip_splices(lexer->source,
                                            lexer->source_count,
                                            end);
            size_t ucn_end;
            unsigned char c;
            if (next >= lexer->source_count) break;
            c = (unsigned char)lexer->source[next];
            ucn_end = noc__universal_character_name_end(lexer->source,
                                                        lexer->source_count,
                                                        next);
            if (isalnum(c) || c == '_' || c == '.') {
                previous = c;
                end = next + 1;
            } else if (ucn_end != next) {
                previous = 0;
                end = ucn_end;
            } else if ((c == '+' || c == '-') &&
                       (previous == 'e' || previous == 'E' ||
                        previous == 'p' || previous == 'P')) {
                previous = c;
                end = next + 1;
            } else {
                break;
            }
        }
        lexer->beginning_of_line = false;
        return noc__make_token(lexer, NOC_TOKEN_NUMBER, start, end, location);
    }

    end = noc__punctuator_end(lexer->source, lexer->source_count, start);
    lexer->beginning_of_line = false;
    return noc__make_token(lexer, NOC_TOKEN_PUNCTUATOR, start, end, location);
}

NOCDEF void noc_buffer_free(Noc_Buffer *buffer)
{
    free(buffer->items);
    buffer->items = NULL;
    buffer->count = 0;
    buffer->capacity = 0;
}

NOCDEF bool noc_buffer_reserve(Noc_Buffer *buffer, size_t additional_count)
{
    size_t needed;
    size_t capacity;
    char *items;
    if (additional_count > SIZE_MAX - buffer->count) return false;
    needed = buffer->count + additional_count;
    if (needed <= buffer->capacity) return true;
    capacity = buffer->capacity ? buffer->capacity : 256;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    items = (char *)realloc(buffer->items, capacity);
    if (!items) return false;
    buffer->items = items;
    buffer->capacity = capacity;
    return true;
}

NOCDEF bool noc_buffer_append(Noc_Buffer *buffer, const void *data, size_t count)
{
    uintptr_t source_address;
    uintptr_t buffer_address;
    size_t source_offset = 0;
    bool source_is_internal = false;
    if (count == 0) return true;
    source_address = (uintptr_t)data;
    buffer_address = (uintptr_t)buffer->items;
    if (buffer->items && source_address >= buffer_address &&
        source_address <= buffer_address + buffer->count) {
        source_offset = (size_t)(source_address - buffer_address);
        if (count > buffer->count - source_offset) return false;
        source_is_internal = true;
    }
    if (!noc_buffer_reserve(buffer, count)) return false;
    if (source_is_internal) data = buffer->items + source_offset;
    memmove(buffer->items + buffer->count, data, count);
    buffer->count += count;
    return true;
}

NOCDEF bool noc_buffer_append_slice(Noc_Buffer *buffer, Noc_Slice slice)
{
    return noc_buffer_append(buffer, slice.data, slice.count);
}

NOCDEF bool noc_buffer_append_cstr(Noc_Buffer *buffer, const char *text)
{
    return noc_buffer_append(buffer, text, strlen(text));
}

NOC__PRIVATE bool noc__buffer_appendfv(Noc_Buffer *buffer, const char *format, va_list arguments)
{
    va_list copy;
    int required;
    va_copy(copy, arguments);
    required = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (required < 0) return false;
    if (!noc_buffer_reserve(buffer, (size_t)required + 1)) return false;
    va_copy(copy, arguments);
    (void)vsnprintf(buffer->items + buffer->count, (size_t)required + 1, format, copy);
    va_end(copy);
    buffer->count += (size_t)required;
    return true;
}

NOCDEF bool noc_buffer_appendf(Noc_Buffer *buffer, const char *format, ...)
{
    bool result;
    va_list arguments;
    va_start(arguments, format);
    result = noc__buffer_appendfv(buffer, format, arguments);
    va_end(arguments);
    return result;
}

NOCDEF bool noc_buffer_terminate(Noc_Buffer *buffer)
{
    if (!noc_buffer_reserve(buffer, 1)) return false;
    buffer->items[buffer->count] = '\0';
    return true;
}

NOCDEF bool noc_token_logical_text(Noc_Token token, Noc_Buffer *output)
{
    Noc_Buffer generated = {0};
    size_t position = 0;
    size_t run_start = 0;
    if (!output || (!token.text.data && token.text.count != 0)) return false;
    while (position < token.text.count) {
        size_t splice = noc__splice_length(token.text.data, token.text.count, position);
        if (splice == 0) {
            position += 1;
            continue;
        }
        if (!noc_buffer_append(&generated,
                               token.text.data + run_start,
                               position - run_start)) {
            goto failed;
        }
        position += splice;
        run_start = position;
    }
    if (!noc_buffer_append(&generated,
                           token.text.data ? token.text.data + run_start : NULL,
                           position - run_start) ||
        !noc_buffer_terminate(&generated)) {
        goto failed;
    }
    noc_buffer_free(output);
    *output = generated;
    return true;

failed:
    noc_buffer_free(&generated);
    return false;
}

NOC__PRIVATE bool noc__buffer_append_c_string(Noc_Buffer *buffer,
                                        const void *data,
                                        size_t count)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    if (!noc_buffer_append_cstr(buffer, "\"")) return false;
    for (i = 0; i < count; ++i) {
        unsigned char c = bytes[i];
        switch (c) {
        case '\\': if (!noc_buffer_append_cstr(buffer, "\\\\")) return false; break;
        case '"': if (!noc_buffer_append_cstr(buffer, "\\\"")) return false; break;
        case '?': if (!noc_buffer_append_cstr(buffer, "\\?")) return false; break;
        case '\n': if (!noc_buffer_append_cstr(buffer, "\\n")) return false; break;
        case '\r': if (!noc_buffer_append_cstr(buffer, "\\r")) return false; break;
        case '\t': if (!noc_buffer_append_cstr(buffer, "\\t")) return false; break;
        default:
            if (c >= 32 && c <= 126) {
                if (!noc_buffer_append(buffer, &c, 1)) return false;
            } else if (!noc_buffer_appendf(buffer, "\\%03o", (unsigned int)c)) {
                return false;
            }
            break;
        }
    }
    return noc_buffer_append_cstr(buffer, "\"");
}

#endif /* NOC_LEXER_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
