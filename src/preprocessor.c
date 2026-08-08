#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_PREPROCESSOR_IMPLEMENTATION_INCLUDED
#define NOC_PREPROCESSOR_IMPLEMENTATION_INCLUDED

static int noc__hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

NOCDEF bool noc_decode_string_token(Noc_Token token, Noc_Buffer *decoded)
{
    size_t i;
    if (token.kind != NOC_TOKEN_STRING || token.text.count < 2 ||
        token.text.data[0] != '"' || token.text.data[token.text.count - 1] != '"') {
        return false;
    }
    i = 1;
    while (i + 1 < token.text.count) {
        unsigned char value;
        char c = token.text.data[i++];
        if (c != '\\') {
            if (!noc_buffer_append(decoded, &c, 1)) return false;
            continue;
        }
        if (i + 1 > token.text.count) return false;
        c = token.text.data[i++];
        switch (c) {
        case '\'': value = '\''; break;
        case '"': value = '"'; break;
        case '?': value = '?'; break;
        case '\\': value = '\\'; break;
        case 'a': value = '\a'; break;
        case 'b': value = '\b'; break;
        case 'f': value = '\f'; break;
        case 'n': value = '\n'; break;
        case 'r': value = '\r'; break;
        case 't': value = '\t'; break;
        case 'v': value = '\v'; break;
        case 'x': {
            int digit;
            unsigned int number = 0;
            size_t digits = 0;
            while (i + 1 < token.text.count &&
                   (digit = noc__hex_digit(token.text.data[i])) >= 0) {
                number = number * 16u + (unsigned int)digit;
                i += 1;
                digits += 1;
            }
            if (digits == 0 || number > 255u) return false;
            value = (unsigned char)number;
            break;
        }
        case '\n': continue;
        case '\r':
            if (i + 1 < token.text.count && token.text.data[i] == '\n') i += 1;
            continue;
        default:
            if (c >= '0' && c <= '7') {
                unsigned int number = (unsigned int)(c - '0');
                size_t digits = 1;
                while (digits < 3 && i + 1 < token.text.count &&
                       token.text.data[i] >= '0' && token.text.data[i] <= '7') {
                    number = number * 8u + (unsigned int)(token.text.data[i] - '0');
                    i += 1;
                    digits += 1;
                }
                if (number > 255u) return false;
                value = (unsigned char)number;
            } else {
                return false;
            }
            break;
        }
        if (!noc_buffer_append(decoded, &value, 1)) return false;
    }
    return true;
}

NOC__PRIVATE bool noc__tokens_append(Noc__Tokens *tokens, Noc_Token token)
{
    Noc_Token *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (tokens->count > tokens->capacity) return false;
    if (tokens->count == tokens->capacity) {
        if (tokens->capacity >= maximum) return false;
        if (tokens->capacity == 0) {
            capacity = maximum < 256 ? maximum : 256;
        } else if (tokens->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = tokens->capacity * 2;
        }
        if (capacity <= tokens->capacity) return false;
        items = (Noc_Token *)realloc(tokens->items, capacity * sizeof(*items));
        if (!items) return false;
        tokens->items = items;
        tokens->capacity = capacity;
    }
    tokens->items[tokens->count++] = token;
    return true;
}

NOC__PRIVATE bool noc__emit_line_directive_at(Noc_Buffer *output,
                                        const char *path,
                                        size_t line)
{
    const unsigned char *cursor = (const unsigned char *)path;
    if (!noc_buffer_appendf(output, "#line %zu \"", line)) return false;
    while (*cursor) {
        if (*cursor == '\\' || *cursor == '"' || *cursor == '?') {
            if (!noc_buffer_append(output, "\\", 1)) return false;
        }
        if (*cursor == '\n') {
            if (!noc_buffer_append_cstr(output, "\\n")) return false;
        } else if (*cursor == '\r') {
            if (!noc_buffer_append_cstr(output, "\\r")) return false;
        } else if (*cursor == '\t') {
            if (!noc_buffer_append_cstr(output, "\\t")) return false;
        } else if (*cursor >= 32 && *cursor <= 126) {
            if (!noc_buffer_append(output, cursor, 1)) return false;
        } else if (!noc_buffer_appendf(output, "\\%03o", (unsigned int)*cursor)) {
            return false;
        }
        cursor += 1;
    }
    return noc_buffer_append_cstr(output, "\"\n");
}

NOC__PRIVATE bool noc__reject_trigraphs(Noc_Context *context,
                                  const char *path,
                                  const char *source,
                                  size_t source_count)
{
    size_t i;
    size_t line = 1;
    size_t column = 1;
    for (i = 0; i < source_count; ++i) {
        if (i + 2 < source_count && source[i] == '?' && source[i + 1] == '?' &&
            strchr("=/'()!<>-", source[i + 2]) != NULL) {
            Noc_Location location;
            location.path = path;
            location.offset = i;
            location.line = line;
            location.column = column;
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "C trigraphs are not supported by noc.h");
            return false;
        }
        if (source[i] == '\r') {
            if (i + 1 < source_count && source[i + 1] == '\n') i += 1;
            line += 1;
            column = 1;
        } else if (source[i] == '\n') {
            line += 1;
            column = 1;
        } else {
            column += 1;
        }
    }
    return true;
}

NOCDEF void noc_token_stream_free(Noc_Token_Stream *stream)
{
    size_t generation = stream->generation;
    free(stream->items);
    free(stream->source);
    free(stream->path);
    memset(stream, 0, sizeof(*stream));
    stream->generation = generation;
}

NOCDEF bool noc_token_stream_is_valid(const Noc_Token_Stream *stream)
{
    return stream && stream->items && stream->source && stream->path &&
           stream->generation != 0 &&
           stream->count > 0 && stream->count <= stream->capacity &&
           stream->items[stream->count - 1].kind == NOC_TOKEN_EOF;
}

static bool noc__tokenize(Noc_Context *context,
                          const char *path,
                          const char *source,
                          size_t source_count,
                          bool recover_incomplete_directive,
                          Noc_Token_Stream *stream)
{
    const char *display_path = path ? path : "<memory>";
    size_t path_count;
    Noc_Location no_location = {0};
    Noc_Token_Stream parsed = {0};
    Noc_Lexer lexer;
    Noc_Token token;
    bool ok = true;

    if (stream->generation == SIZE_MAX) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "token stream generation is exhausted");
        return false;
    }
    if (source_count == SIZE_MAX) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "source is too large to tokenize");
        return false;
    }
    if (!noc__reject_trigraphs(context, display_path, source, source_count)) return false;
    path_count = strlen(display_path);
    parsed.source = (char *)malloc(source_count + 1);
    parsed.path = (char *)malloc(path_count + 1);
    if (!parsed.source || !parsed.path) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while copying source for '%s'",
                    display_path);
        noc_token_stream_free(&parsed);
        return false;
    }
    if (source_count > 0) memcpy(parsed.source, source, source_count);
    parsed.source[source_count] = '\0';
    parsed.source_count = source_count;
    memcpy(parsed.path, display_path, path_count + 1);

    noc_lexer_init(&lexer, parsed.path, parsed.source, parsed.source_count);
    do {
        token = noc_lexer_next(&lexer);
        if (recover_incomplete_directive && token.kind == NOC_TOKEN_INVALID &&
            token.text.count == 0 && lexer.cursor == lexer.source_count &&
            parsed.count > 0 &&
            parsed.items[parsed.count - 1].kind == NOC_TOKEN_PREPROCESSOR) {
            continue;
        }
        if (!noc__tokens_append(&parsed, token)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while tokenizing '%s'",
                        display_path);
            ok = false;
            break;
        }
        if (token.kind == NOC_TOKEN_INVALID) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "unterminated or invalid token '%.*s%s'",
                        (int)(token.text.count < 80 ? token.text.count : 80),
                        token.text.data,
                        token.text.count > 80 ? "..." : "");
            ok = false;
        }
    } while (token.kind != NOC_TOKEN_EOF);
    if (!ok) {
        noc_token_stream_free(&parsed);
        return false;
    }
    parsed.generation = stream->generation + 1;
    noc_token_stream_free(stream);
    *stream = parsed;
    return true;
}

NOCDEF bool noc_tokenize(Noc_Context *context,
                         const char *path,
                         const char *source,
                         size_t source_count,
                         Noc_Token_Stream *stream)
{
    return noc__tokenize(context,
                         path,
                         source,
                         source_count,
                         false,
                         stream);
}

NOCDEF Noc_Slice noc_token_stream_source(const Noc_Token_Stream *stream)
{
    Noc_Slice source = {0};
    if (!noc_token_stream_is_valid(stream)) return source;
    source.data = stream->source;
    source.count = stream->source_count;
    return source;
}

typedef enum {
    NOC__PREPROCESSOR_OTHER = 0,
    NOC__PREPROCESSOR_IF,
    NOC__PREPROCESSOR_IFDEF,
    NOC__PREPROCESSOR_IFNDEF,
    NOC__PREPROCESSOR_ELIF,
    NOC__PREPROCESSOR_ELSE,
    NOC__PREPROCESSOR_ENDIF,
} Noc__Preprocessor_Directive_Kind;

typedef struct {
    Noc__Preprocessor_Directive_Kind kind;
    Noc_Preprocessor_Activity condition;
} Noc__Preprocessor_Directive;

typedef struct {
    Noc_Preprocessor_Activity parent;
    Noc_Preprocessor_Activity prior_taken;
    Noc_Location opening_location;
    bool saw_else;
} Noc__Preprocessor_Frame;

static Noc_Token noc__preprocessor_next_significant(Noc_Lexer *lexer)
{
    Noc_Token token;
    do {
        token = noc_lexer_next(lexer);
    } while (noc_token_is_trivia(token));
    return token;
}

static bool noc__preprocessor_parse_directive(Noc_Token token,
                                              Noc__Preprocessor_Directive *directive)
{
    Noc_Buffer logical = {0};
    Noc_Lexer lexer;
    Noc_Token marker;
    Noc_Token keyword;
    bool ok = false;
    directive->kind = NOC__PREPROCESSOR_OTHER;
    directive->condition = NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
    if (token.kind != NOC_TOKEN_PREPROCESSOR) return true;
    if (!noc_token_logical_text(token, &logical)) return false;
    noc_lexer_init(&lexer, token.location.path, logical.items, logical.count);
    lexer.beginning_of_line = false;
    marker = noc__preprocessor_next_significant(&lexer);
    if (!noc_token_is_punct(marker, "#") && !noc_token_is_punct(marker, "%:")) {
        ok = true;
        goto done;
    }
    keyword = noc__preprocessor_next_significant(&lexer);
    if (noc_token_is_identifier(keyword, "if")) {
        directive->kind = NOC__PREPROCESSOR_IF;
    } else if (noc_token_is_identifier(keyword, "ifdef")) {
        directive->kind = NOC__PREPROCESSOR_IFDEF;
    } else if (noc_token_is_identifier(keyword, "ifndef")) {
        directive->kind = NOC__PREPROCESSOR_IFNDEF;
    } else if (noc_token_is_identifier(keyword, "elif")) {
        directive->kind = NOC__PREPROCESSOR_ELIF;
    } else if (noc_token_is_identifier(keyword, "elifdef") ||
               noc_token_is_identifier(keyword, "elifndef")) {
        directive->kind = NOC__PREPROCESSOR_ELIF;
    } else if (noc_token_is_identifier(keyword, "else")) {
        directive->kind = NOC__PREPROCESSOR_ELSE;
    } else if (noc_token_is_identifier(keyword, "endif")) {
        directive->kind = NOC__PREPROCESSOR_ENDIF;
    }
    if (directive->kind == NOC__PREPROCESSOR_IF ||
        directive->kind == NOC__PREPROCESSOR_ELIF) {
        Noc_Token expression = noc__preprocessor_next_significant(&lexer);
        Noc_Token trailing = noc__preprocessor_next_significant(&lexer);
        bool trailing_multiline_comment =
            trailing.kind == NOC_TOKEN_INVALID &&
            noc__logical_pair(trailing.text.data,
                              trailing.text.count,
                              0,
                              '/',
                              '*',
                              NULL) &&
            noc__contains_newline(trailing.text.data, trailing.text.count);
        if (expression.kind == NOC_TOKEN_NUMBER &&
            (trailing.kind == NOC_TOKEN_EOF || trailing_multiline_comment)) {
            if (noc_slice_equal_cstr(expression.text, "0")) {
                directive->condition = NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
            } else if (noc_slice_equal_cstr(expression.text, "1")) {
                directive->condition = NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
            }
        }
    }
    ok = true;

done:
    noc_buffer_free(&logical);
    return ok;
}

static Noc_Preprocessor_Activity noc__preprocessor_and(Noc_Preprocessor_Activity left,
                                                       Noc_Preprocessor_Activity right)
{
    if (left == NOC_PREPROCESSOR_ACTIVITY_INACTIVE ||
        right == NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    }
    if (left == NOC_PREPROCESSOR_ACTIVITY_ACTIVE &&
        right == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

static Noc_Preprocessor_Activity noc__preprocessor_or(Noc_Preprocessor_Activity left,
                                                      Noc_Preprocessor_Activity right)
{
    if (left == NOC_PREPROCESSOR_ACTIVITY_ACTIVE ||
        right == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    }
    if (left == NOC_PREPROCESSOR_ACTIVITY_INACTIVE &&
        right == NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

static Noc_Preprocessor_Activity noc__preprocessor_not(Noc_Preprocessor_Activity activity)
{
    if (activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    }
    if (activity == NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

NOCDEF void noc_preprocessor_map_free(Noc_Preprocessor_Map *map)
{
    free(map->items);
    memset(map, 0, sizeof(*map));
}

NOCDEF bool noc_preprocessor_map_is_valid(const Noc_Preprocessor_Map *map)
{
    return map && noc_token_stream_is_valid(map->stream) &&
           map->stream_generation == map->stream->generation && map->items &&
           map->count == map->stream->count;
}

NOCDEF Noc_Preprocessor_Activity noc_preprocessor_activity_at(
    const Noc_Preprocessor_Map *map,
    size_t token_index)
{
    if (!noc_preprocessor_map_is_valid(map) || token_index >= map->count) {
        return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
    }
    return map->items[token_index];
}

NOCDEF bool noc_preprocessor_map_build(Noc_Context *context,
                                       const Noc_Token_Stream *stream,
                                       Noc_Preprocessor_Map *map)
{
    Noc_Preprocessor_Map parsed = {0};
    Noc__Preprocessor_Frame *frames = NULL;
    Noc_Preprocessor_Activity current = NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    Noc_Location no_location = {0};
    size_t frame_count = 0;
    size_t frame_capacity = 0;
    size_t i;
    if (!map || !noc_token_stream_is_valid(stream)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "cannot analyze preprocessor activity from an invalid token stream");
        return false;
    }
    if (stream->count > SIZE_MAX / sizeof(*parsed.items)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "token stream is too large for preprocessor activity analysis");
        return false;
    }
    parsed.items = (Noc_Preprocessor_Activity *)malloc(stream->count *
                                                       sizeof(*parsed.items));
    if (!parsed.items) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while starting preprocessor activity analysis");
        goto failed;
    }
    parsed.stream = stream;
    parsed.stream_generation = stream->generation;
    parsed.count = stream->count;
    for (i = 0; i < stream->count; ++i) {
        Noc_Token token = stream->items[i];
        Noc__Preprocessor_Directive directive;
        parsed.items[i] = current;
        if (token.kind != NOC_TOKEN_PREPROCESSOR) continue;
        if (!noc__preprocessor_parse_directive(token, &directive)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "out of memory while parsing preprocessor directive");
            goto failed;
        }
        if (directive.kind == NOC__PREPROCESSOR_IF ||
            directive.kind == NOC__PREPROCESSOR_IFDEF ||
            directive.kind == NOC__PREPROCESSOR_IFNDEF) {
            Noc__Preprocessor_Frame *frame;
            if (frame_count == frame_capacity) {
                Noc__Preprocessor_Frame *grown;
                size_t capacity = frame_capacity ? frame_capacity * 2 : 8;
                if (capacity < frame_capacity || capacity > SIZE_MAX / sizeof(*frames)) {
                    noc__report(context,
                                NOC_DIAGNOSTIC_ERROR,
                                token.location,
                                "preprocessor conditional nesting is too deep");
                    goto failed;
                }
                grown = (Noc__Preprocessor_Frame *)realloc(frames,
                                                           capacity * sizeof(*frames));
                if (!grown) {
                    noc__report(context,
                                NOC_DIAGNOSTIC_ERROR,
                                token.location,
                                "out of memory while nesting preprocessor conditionals");
                    goto failed;
                }
                frames = grown;
                frame_capacity = capacity;
            }
            frame = &frames[frame_count++];
            frame->parent = current;
            frame->prior_taken = directive.condition;
            frame->opening_location = token.location;
            frame->saw_else = false;
            current = noc__preprocessor_and(frame->parent, directive.condition);
        } else if (directive.kind == NOC__PREPROCESSOR_ELIF) {
            Noc__Preprocessor_Frame *frame;
            Noc_Preprocessor_Activity available;
            if (frame_count == 0) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "#elif has no matching conditional directive");
                goto failed;
            }
            frame = &frames[frame_count - 1];
            if (frame->saw_else) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "#elif cannot follow #else in the same conditional group");
                goto failed;
            }
            available = noc__preprocessor_not(frame->prior_taken);
            current = noc__preprocessor_and(
                frame->parent,
                noc__preprocessor_and(available, directive.condition));
            frame->prior_taken = noc__preprocessor_or(frame->prior_taken,
                                                      directive.condition);
        } else if (directive.kind == NOC__PREPROCESSOR_ELSE) {
            Noc__Preprocessor_Frame *frame;
            if (frame_count == 0) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "#else has no matching conditional directive");
                goto failed;
            }
            frame = &frames[frame_count - 1];
            if (frame->saw_else) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "conditional group contains more than one #else");
                goto failed;
            }
            current = noc__preprocessor_and(
                frame->parent,
                noc__preprocessor_not(frame->prior_taken));
            frame->prior_taken = NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
            frame->saw_else = true;
        } else if (directive.kind == NOC__PREPROCESSOR_ENDIF) {
            if (frame_count == 0) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "#endif has no matching conditional directive");
                goto failed;
            }
            current = frames[frame_count - 1].parent;
            frame_count -= 1;
        }
    }
    if (frame_count != 0) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    frames[frame_count - 1].opening_location,
                    "conditional directive has no matching #endif");
        goto failed;
    }
    free(frames);
    noc_preprocessor_map_free(map);
    *map = parsed;
    return true;

failed:
    free(frames);
    noc_preprocessor_map_free(&parsed);
    return false;
}

static bool noc__macro_policy_is_valid(Noc_Macro_Policy policy)
{
    switch (policy) {
    case NOC_MACROS_DISABLED:
    case NOC_MACROS_TRUSTED_ONLY:
    case NOC_MACROS_PROJECT:
    case NOC_MACROS_FULL:
        return true;
    }
    return false;
}

NOCDEF const char *noc_source_class_name(Noc_Source_Class source_class)
{
    switch (source_class) {
    case NOC_SOURCE_CLASS_PROJECT: return "project";
    case NOC_SOURCE_CLASS_TRUSTED: return "trusted";
    case NOC_SOURCE_CLASS_SYSTEM: return "system";
    case NOC_SOURCE_CLASS_GENERATED: return "generated";
    }
    return "unknown";
}

NOCDEF const char *noc_macro_policy_name(Noc_Macro_Policy policy)
{
    switch (policy) {
    case NOC_MACROS_DISABLED: return "disabled";
    case NOC_MACROS_TRUSTED_ONLY: return "trusted-only";
    case NOC_MACROS_PROJECT: return "project";
    case NOC_MACROS_FULL: return "full";
    }
    return "unknown";
}

NOCDEF const char *noc_preprocessor_directive_kind_name(
    Noc_Preprocessor_Directive_Kind kind)
{
    switch (kind) {
    case NOC_PREPROCESSOR_DIRECTIVE_NULL: return "null";
    case NOC_PREPROCESSOR_DIRECTIVE_DEFINE: return "define";
    case NOC_PREPROCESSOR_DIRECTIVE_UNDEF: return "undef";
    case NOC_PREPROCESSOR_DIRECTIVE_INCLUDE: return "include";
    case NOC_PREPROCESSOR_DIRECTIVE_IF: return "if";
    case NOC_PREPROCESSOR_DIRECTIVE_IFDEF: return "ifdef";
    case NOC_PREPROCESSOR_DIRECTIVE_IFNDEF: return "ifndef";
    case NOC_PREPROCESSOR_DIRECTIVE_ELIF: return "elif";
    case NOC_PREPROCESSOR_DIRECTIVE_ELIFDEF: return "elifdef";
    case NOC_PREPROCESSOR_DIRECTIVE_ELIFNDEF: return "elifndef";
    case NOC_PREPROCESSOR_DIRECTIVE_ELSE: return "else";
    case NOC_PREPROCESSOR_DIRECTIVE_ENDIF: return "endif";
    case NOC_PREPROCESSOR_DIRECTIVE_LINE: return "line";
    case NOC_PREPROCESSOR_DIRECTIVE_ERROR: return "error";
    case NOC_PREPROCESSOR_DIRECTIVE_WARNING: return "warning";
    case NOC_PREPROCESSOR_DIRECTIVE_PRAGMA: return "pragma";
    case NOC_PREPROCESSOR_DIRECTIVE_UNKNOWN: return "unknown";
    }
    return "unknown";
}

NOCDEF const char *noc_preprocessing_token_role_name(
    Noc_Preprocessing_Token_Role role)
{
    switch (role) {
    case NOC_PREPROCESSING_TOKEN_SOURCE: return "source";
    case NOC_PREPROCESSING_TOKEN_DIRECTIVE_MARKER: return "directive-marker";
    case NOC_PREPROCESSING_TOKEN_DIRECTIVE_KEYWORD: return "directive-keyword";
    case NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY: return "directive-body";
    case NOC_PREPROCESSING_TOKEN_DIRECTIVE_TRIVIA: return "directive-trivia";
    }
    return "unknown";
}

NOCDEF bool noc_macro_policy_allows_definition(Noc_Macro_Policy policy,
                                               Noc_Source_Class source_class)
{
    if (!noc__macro_policy_is_valid(policy) ||
        !noc__source_class_is_valid(source_class)) {
        return false;
    }
    switch (policy) {
    case NOC_MACROS_DISABLED:
        return false;
    case NOC_MACROS_TRUSTED_ONLY:
        return source_class == NOC_SOURCE_CLASS_TRUSTED ||
               source_class == NOC_SOURCE_CLASS_SYSTEM;
    case NOC_MACROS_PROJECT:
        return source_class == NOC_SOURCE_CLASS_PROJECT ||
               source_class == NOC_SOURCE_CLASS_TRUSTED ||
               source_class == NOC_SOURCE_CLASS_SYSTEM;
    case NOC_MACROS_FULL:
        return true;
    }
    return false;
}

static Noc_Token noc__preprocessor_inventory_next_significant(Noc_Lexer *lexer)
{
    Noc_Token token;
    do {
        token = noc_lexer_next(lexer);
    } while (noc_token_is_trivia(token));
    return token;
}

static Noc_Preprocessor_Directive_Kind noc__preprocessor_inventory_kind(
    Noc_Token keyword)
{
    if (noc_token_is_identifier(keyword, "define")) {
        return NOC_PREPROCESSOR_DIRECTIVE_DEFINE;
    }
    if (noc_token_is_identifier(keyword, "undef")) {
        return NOC_PREPROCESSOR_DIRECTIVE_UNDEF;
    }
    if (noc_token_is_identifier(keyword, "include")) {
        return NOC_PREPROCESSOR_DIRECTIVE_INCLUDE;
    }
    if (noc_token_is_identifier(keyword, "if")) {
        return NOC_PREPROCESSOR_DIRECTIVE_IF;
    }
    if (noc_token_is_identifier(keyword, "ifdef")) {
        return NOC_PREPROCESSOR_DIRECTIVE_IFDEF;
    }
    if (noc_token_is_identifier(keyword, "ifndef")) {
        return NOC_PREPROCESSOR_DIRECTIVE_IFNDEF;
    }
    if (noc_token_is_identifier(keyword, "elif")) {
        return NOC_PREPROCESSOR_DIRECTIVE_ELIF;
    }
    if (noc_token_is_identifier(keyword, "elifdef")) {
        return NOC_PREPROCESSOR_DIRECTIVE_ELIFDEF;
    }
    if (noc_token_is_identifier(keyword, "elifndef")) {
        return NOC_PREPROCESSOR_DIRECTIVE_ELIFNDEF;
    }
    if (noc_token_is_identifier(keyword, "else")) {
        return NOC_PREPROCESSOR_DIRECTIVE_ELSE;
    }
    if (noc_token_is_identifier(keyword, "endif")) {
        return NOC_PREPROCESSOR_DIRECTIVE_ENDIF;
    }
    if (noc_token_is_identifier(keyword, "line")) {
        return NOC_PREPROCESSOR_DIRECTIVE_LINE;
    }
    if (noc_token_is_identifier(keyword, "error")) {
        return NOC_PREPROCESSOR_DIRECTIVE_ERROR;
    }
    if (noc_token_is_identifier(keyword, "warning")) {
        return NOC_PREPROCESSOR_DIRECTIVE_WARNING;
    }
    if (noc_token_is_identifier(keyword, "pragma")) {
        return NOC_PREPROCESSOR_DIRECTIVE_PRAGMA;
    }
    if (keyword.kind == NOC_TOKEN_EOF) return NOC_PREPROCESSOR_DIRECTIVE_NULL;
    return NOC_PREPROCESSOR_DIRECTIVE_UNKNOWN;
}

static void noc__preprocessor_inventory_parse(
    Noc_Token token,
    size_t token_index,
    Noc_Macro_Policy macro_policy,
    Noc_Source_Class source_class,
    Noc_Preprocessor_Directive *directive)
{
    Noc_Lexer lexer;
    Noc_Token marker;
    Noc_Token keyword;
    Noc_Token payload;
    const char *begin = token.text.data;
    size_t end = token.text.count;
    memset(directive, 0, sizeof(*directive));
    directive->kind = NOC_PREPROCESSOR_DIRECTIVE_UNKNOWN;
    directive->token_index = token_index;
    directive->macro_directive_index = NOC_TOKEN_INDEX_NONE;
    directive->spelling = token.text;
    directive->location = token.location;
    directive->macro_definition_allowed = true;
    if (end > 0 && begin[end - 1] == '\n') {
        end -= 1;
        if (end > 0 && begin[end - 1] == '\r') end -= 1;
    } else if (end > 0 && begin[end - 1] == '\r') {
        end -= 1;
    }
    noc_lexer_init(&lexer, token.location.path, token.text.data, token.text.count);
    lexer.beginning_of_line = false;
    marker = noc__preprocessor_inventory_next_significant(&lexer);
    if (!noc_token_is_punct(marker, "#") && !noc_token_is_punct(marker, "%:")) {
        directive->keyword.data = begin + end;
        directive->payload.data = begin + end;
        return;
    }
    keyword = noc__preprocessor_inventory_next_significant(&lexer);
    directive->kind = noc__preprocessor_inventory_kind(keyword);
    if (keyword.kind == NOC_TOKEN_EOF) {
        directive->keyword.data = begin + end;
        directive->payload.data = begin + end;
        return;
    }
    directive->keyword = keyword.text;
    payload = noc__preprocessor_inventory_next_significant(&lexer);
    if (payload.kind == NOC_TOKEN_EOF || payload.text.data >= begin + end) {
        directive->payload.data = begin + end;
    } else {
        directive->payload.data = payload.text.data;
        directive->payload.count = (size_t)((begin + end) - payload.text.data);
    }
    if (directive->kind == NOC_PREPROCESSOR_DIRECTIVE_DEFINE ||
        directive->kind == NOC_PREPROCESSOR_DIRECTIVE_UNDEF) {
        directive->macro_definition_allowed =
            noc_macro_policy_allows_definition(macro_policy, source_class);
    }
}

static bool noc__preprocessor_unit_append(Noc_Preprocessor_Unit *unit,
                                          Noc_Preprocessor_Directive directive)
{
    Noc_Preprocessor_Directive *items;
    size_t capacity;
    if (unit->count < unit->capacity) {
        unit->items[unit->count++] = directive;
        return true;
    }
    if (unit->capacity == 0) {
        capacity = 16;
    } else {
        if (unit->capacity > SIZE_MAX / 2) return false;
        capacity = unit->capacity * 2;
    }
    if (capacity > SIZE_MAX / sizeof(*items)) return false;
    items = (Noc_Preprocessor_Directive *)realloc(unit->items,
                                                  capacity * sizeof(*items));
    if (!items) return false;
    unit->items = items;
    unit->capacity = capacity;
    unit->items[unit->count++] = directive;
    return true;
}

static bool noc__preprocessor_token_append(
    Noc_Preprocessor_Unit *unit,
    Noc_Token token,
    Noc_Preprocessing_Token_Role role,
    size_t directive_index)
{
    Noc_Preprocessing_Token *items;
    size_t capacity;
    if (unit->preprocessing_token_count < unit->preprocessing_token_capacity) {
        Noc_Preprocessing_Token *item =
            &unit->preprocessing_tokens[unit->preprocessing_token_count++];
        item->token = token;
        item->role = role;
        item->directive_index = directive_index;
        return true;
    }
    if (unit->preprocessing_token_capacity == 0) {
        capacity = 32;
    } else {
        if (unit->preprocessing_token_capacity > SIZE_MAX / 2) return false;
        capacity = unit->preprocessing_token_capacity * 2;
    }
    if (capacity > SIZE_MAX / sizeof(*items)) return false;
    items = (Noc_Preprocessing_Token *)realloc(
        unit->preprocessing_tokens,
        capacity * sizeof(*items));
    if (!items) return false;
    unit->preprocessing_tokens = items;
    unit->preprocessing_token_capacity = capacity;
    return noc__preprocessor_token_append(unit, token, role, directive_index);
}

static bool noc__preprocessor_scan_header_name(Noc_Lexer *lexer,
                                               Noc_Token *token)
{
    size_t start = lexer->cursor;
    size_t end;
    char close;
    Noc_Location location;
    if (start >= lexer->source_count ||
        (lexer->source[start] != '<' && lexer->source[start] != '"')) {
        return false;
    }
    close = lexer->source[start] == '<' ? '>' : '"';
    location.path = lexer->path;
    location.offset = start;
    location.line = lexer->line;
    location.column = lexer->column;
    end = start + 1;
    while (end < lexer->source_count) {
        size_t splice = noc__splice_length(lexer->source,
                                           lexer->source_count,
                                           end);
        if (splice != 0) {
            end += splice;
            continue;
        }
        if (lexer->source[end] == close) {
            end += 1;
            *token = noc__make_token(lexer,
                                     NOC_TOKEN_HEADER_NAME,
                                     start,
                                     end,
                                     location);
            lexer->beginning_of_line = false;
            return true;
        }
        if (lexer->source[end] == '\r' || lexer->source[end] == '\n') return false;
        end += 1;
    }
    return false;
}

static bool noc__preprocessor_is_c_punctuator(Noc_Token token)
{
    static const char single_character_punctuators[] =
        "[](){}.&*+-~!/%<>^|?:;=,#";
    return token.kind == NOC_TOKEN_PUNCTUATOR &&
           (token.text.count != 1 ||
            strchr(single_character_punctuators, token.text.data[0]) != NULL);
}

static bool noc__preprocessor_tokenize_directive(
    Noc_Preprocessor_Unit *unit,
    Noc_Token directive_token,
    Noc_Preprocessor_Directive_Kind directive_kind,
    size_t directive_index)
{
    Noc_Lexer lexer;
    unsigned int significant_state = 0;
    bool include_operand_pending = false;
    noc_lexer_init(&lexer,
                   directive_token.location.path,
                   directive_token.text.data,
                   directive_token.text.count);
    lexer.beginning_of_line = false;
    lexer.line = directive_token.location.line;
    lexer.column = directive_token.location.column;
    for (;;) {
        Noc_Preprocessing_Token_Role role;
        Noc_Token token;
        if (include_operand_pending &&
            noc__preprocessor_scan_header_name(&lexer, &token)) {
            include_operand_pending = false;
        } else {
            token = noc_lexer_next(&lexer);
        }
        token.location.offset += directive_token.location.offset;
        if (token.kind == NOC_TOKEN_EOF) break;
        if (token.kind == NOC_TOKEN_PUNCTUATOR &&
            !noc__preprocessor_is_c_punctuator(token)) {
            token.kind = NOC_TOKEN_OTHER;
        }
        if (noc_token_is_trivia(token)) {
            role = NOC_PREPROCESSING_TOKEN_DIRECTIVE_TRIVIA;
        } else if (significant_state == 0) {
            role = NOC_PREPROCESSING_TOKEN_DIRECTIVE_MARKER;
            significant_state = 1;
        } else if (significant_state == 1) {
            role = NOC_PREPROCESSING_TOKEN_DIRECTIVE_KEYWORD;
            significant_state = 2;
            include_operand_pending =
                directive_kind == NOC_PREPROCESSOR_DIRECTIVE_INCLUDE;
        } else {
            role = NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY;
            if (include_operand_pending) {
                include_operand_pending = false;
            }
        }
        if (!noc__preprocessor_token_append(unit,
                                            token,
                                            role,
                                            directive_index)) {
            return false;
        }
    }
    return true;
}

NOCDEF void noc_preprocessor_unit_free(Noc_Preprocessor_Unit *unit)
{
    size_t stream_generation;
    if (!unit) return;
    stream_generation = unit->stream.generation;
    free(unit->macro_parameters);
    free(unit->macro_directives);
    free(unit->preprocessing_tokens);
    free(unit->items);
    noc_token_stream_free(&unit->stream);
    memset(unit, 0, sizeof(*unit));
    unit->stream.generation = stream_generation;
}

NOCDEF bool noc_preprocessor_unit_is_valid(const Noc_Preprocessor_Unit *unit)
{
    return unit && noc_token_stream_is_valid(&unit->stream) &&
           unit->token_stream_generation == unit->stream.generation &&
           unit->file_id != NOC_FILE_ID_NONE && unit->document_generation != 0 &&
           noc__source_class_is_valid(unit->source_class) &&
           noc__macro_policy_is_valid(unit->macro_policy) &&
           unit->preprocessing_token_count > 0 &&
           unit->preprocessing_token_count <=
               unit->preprocessing_token_capacity &&
           unit->preprocessing_tokens &&
           unit->preprocessing_tokens[
               unit->preprocessing_token_count - 1].token.kind == NOC_TOKEN_EOF &&
           unit->macro_directive_count <= unit->macro_directive_capacity &&
           (unit->macro_directive_count == 0 || unit->macro_directives) &&
           unit->macro_parameter_count <= unit->macro_parameter_capacity &&
           (unit->macro_parameter_count == 0 || unit->macro_parameters) &&
           unit->invalid_macro_directive_count <= unit->macro_directive_count &&
           unit->count <= unit->capacity && (unit->count == 0 || unit->items);
}

NOCDEF bool noc_preprocessor_unit_build(Noc_Context *context,
                                        const Noc_Document_Snapshot *snapshot,
                                        Noc_Macro_Policy macro_policy,
                                        Noc_Preprocessor_Unit *unit)
{
    Noc_Preprocessor_Unit parsed = {0};
    Noc_Slice source;
    Noc_Location location = {0};
    size_t index;
    if (!context || !unit || !noc_document_snapshot_is_valid(snapshot)) {
        return false;
    }
    (void)noc_document_snapshot_location(snapshot, 0, &location);
    if (!noc__macro_policy_is_valid(macro_policy)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "invalid macro policy");
        return false;
    }
    source = noc_document_snapshot_source(snapshot);
    parsed.stream.generation = unit->stream.generation;
    if (!noc__tokenize(context,
                       noc_document_snapshot_path(snapshot),
                       source.data,
                       source.count,
                       true,
                       &parsed.stream)) {
        return false;
    }
    parsed.token_stream_generation = parsed.stream.generation;
    parsed.file_id = noc_document_snapshot_file_id(snapshot);
    parsed.document_generation = noc_document_snapshot_generation(snapshot);
    parsed.source_class = noc_document_snapshot_source_class(snapshot);
    parsed.macro_policy = macro_policy;
    for (index = 0; index < parsed.stream.count; ++index) {
        Noc_Token token = parsed.stream.items[index];
        Noc_Preprocessor_Directive directive;
        if (token.kind != NOC_TOKEN_PREPROCESSOR) {
            if (!noc__preprocessor_token_append(&parsed,
                                                token,
                                                NOC_PREPROCESSING_TOKEN_SOURCE,
                                                NOC_TOKEN_INDEX_NONE)) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "out of memory while recording preprocessing tokens");
                noc_preprocessor_unit_free(&parsed);
                return false;
            }
            continue;
        }
        noc__preprocessor_inventory_parse(token,
                                          index,
                                          macro_policy,
                                          parsed.source_class,
                                          &directive);
        directive.preprocessing_tokens.begin =
            parsed.preprocessing_token_count;
        if (!noc__preprocessor_tokenize_directive(&parsed,
                                                  token,
                                                  directive.kind,
                                                  parsed.count)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "out of memory while recording preprocessing tokens");
            noc_preprocessor_unit_free(&parsed);
            return false;
        }
        directive.preprocessing_tokens.end = parsed.preprocessing_token_count;
        if ((directive.kind == NOC_PREPROCESSOR_DIRECTIVE_DEFINE ||
             directive.kind == NOC_PREPROCESSOR_DIRECTIVE_UNDEF) &&
            !noc__macro_parse_directive(&parsed, &directive, parsed.count)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "out of memory while recording macro directives");
            noc_preprocessor_unit_free(&parsed);
            return false;
        }
        if (!directive.macro_definition_allowed) {
            parsed.disabled_macro_definition_count += 1;
        }
        if (!noc__preprocessor_unit_append(&parsed, directive)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "out of memory while recording preprocessor directives");
            noc_preprocessor_unit_free(&parsed);
            return false;
        }
    }
    noc_preprocessor_unit_free(unit);
    *unit = parsed;
    return true;
}

NOCDEF const Noc_Preprocessor_Directive *noc_preprocessor_directive_at(
    const Noc_Preprocessor_Unit *unit,
    size_t index)
{
    if (!noc_preprocessor_unit_is_valid(unit) || index >= unit->count) return NULL;
    return &unit->items[index];
}

NOCDEF Noc_Token_Range noc_preprocessor_directive_body_tokens(
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index)
{
    Noc_Token_Range result = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    const Noc_Preprocessor_Directive *directive;
    size_t index;
    if (!noc_preprocessor_unit_is_valid(unit) || directive_index >= unit->count) {
        return result;
    }
    directive = &unit->items[directive_index];
    for (index = directive->preprocessing_tokens.begin;
         index < directive->preprocessing_tokens.end;
         ++index) {
        if (unit->preprocessing_tokens[index].role !=
            NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY) {
            continue;
        }
        if (result.begin == NOC_TOKEN_INDEX_NONE) result.begin = index;
        result.end = index + 1;
    }
    return result;
}

NOCDEF const Noc_Preprocessing_Token *noc_preprocessor_token_at(
    const Noc_Preprocessor_Unit *unit,
    size_t index)
{
    if (!noc_preprocessor_unit_is_valid(unit) ||
        index >= unit->preprocessing_token_count) {
        return NULL;
    }
    return &unit->preprocessing_tokens[index];
}

NOCDEF bool noc_preprocessor_unit_validate_macro_policy(
    Noc_Context *context,
    const Noc_Preprocessor_Unit *unit)
{
    bool valid = true;
    size_t index;
    if (!context || !noc_preprocessor_unit_is_valid(unit)) return false;
    for (index = 0; index < unit->count; ++index) {
        const Noc_Preprocessor_Directive *directive = &unit->items[index];
        if (directive->macro_definition_allowed) continue;
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    directive->location,
                    "#%s is disabled by '%s' macro policy for %s source",
                    noc_preprocessor_directive_kind_name(directive->kind),
                    noc_macro_policy_name(unit->macro_policy),
                    noc_source_class_name(unit->source_class));
        valid = false;
    }
    return valid;
}

#endif /* NOC_PREPROCESSOR_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
