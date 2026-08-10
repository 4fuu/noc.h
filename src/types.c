#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_TYPES_IMPLEMENTATION_INCLUDED
#define NOC_TYPES_IMPLEMENTATION_INCLUDED

typedef struct {
    Noc_Slice parameter;
    Noc_Slice name;
    Noc_Token_Range declaration;
    size_t name_token;
} Noc__Template;

typedef struct {
    Noc__Template *items;
    size_t count;
    size_t capacity;
} Noc__Templates;

typedef struct {
    Noc_Slice *items;
    size_t count;
    size_t capacity;
} Noc__Dialect_Names;

static bool noc__dialect_slice_equal(Noc_Slice left, Noc_Slice right)
{
    return left.count == right.count &&
           (left.count == 0 || memcmp(left.data, right.data, left.count) == 0);
}

static bool noc__dialect_is_punct(const Noc_Token_Stream *tokens,
                                  size_t index,
                                  const char *punctuator)
{
    return index < tokens->count &&
           noc_token_is_punct(tokens->items[index], punctuator);
}

NOC__PRIVATE bool noc__dialect_tokenize(Noc_Context *context,
                                        const char *path,
                                        Noc_Slice source,
                                        Noc_Token_Stream *output)
{
    Noc_Lexer lexer;
    Noc_Token token;
    Noc_Location none = {0};
    memset(output, 0, sizeof(*output));
    output->source = (char *)(source.data ? source.data : "");
    output->source_count = source.count;
    output->path = (char *)(path ? path : "<memory>");
    output->generation = 1;
    noc_lexer_init(&lexer, output->path, output->source, output->source_count);
    do {
        token = noc_lexer_next(&lexer);
        if (!noc__tokens_append(output, token)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        none,
                        "out of memory while tokenizing structured dialect source");
            noc__dialect_tokens_free(output);
            return false;
        }
        if (token.kind == NOC_TOKEN_INVALID) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "invalid token while lowering structured dialect source");
            noc__dialect_tokens_free(output);
            return false;
        }
    } while (token.kind != NOC_TOKEN_EOF);
    return true;
}

NOC__PRIVATE void noc__dialect_tokens_free(Noc_Token_Stream *tokens)
{
    free(tokens->items);
    memset(tokens, 0, sizeof(*tokens));
}

NOC__PRIVATE size_t noc__dialect_next_significant(const Noc_Token_Stream *tokens,
                                                  size_t index)
{
    while (index < tokens->count && noc_token_is_trivia(tokens->items[index])) {
        index += 1;
    }
    if (index >= tokens->count || tokens->items[index].kind == NOC_TOKEN_EOF) {
        return NOC_TOKEN_INDEX_NONE;
    }
    return index;
}

NOC__PRIVATE size_t noc__dialect_previous_significant(const Noc_Token_Stream *tokens,
                                                      size_t index)
{
    while (index > 0) {
        index -= 1;
        if (!noc_token_is_trivia(tokens->items[index]) &&
            tokens->items[index].kind != NOC_TOKEN_EOF) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

NOC__PRIVATE size_t noc__dialect_matching_close(const Noc_Token_Stream *tokens,
                                                size_t open,
                                                const char *open_spelling,
                                                const char *close_spelling)
{
    size_t index;
    size_t depth = 0;
    if (!noc__dialect_is_punct(tokens, open, open_spelling)) {
        return NOC_TOKEN_INDEX_NONE;
    }
    for (index = open; index < tokens->count; ++index) {
        if (noc__dialect_is_punct(tokens, index, open_spelling)) {
            depth += 1;
        } else if (noc__dialect_is_punct(tokens, index, close_spelling)) {
            depth -= 1;
            if (depth == 0) return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

NOC__PRIVATE size_t noc__dialect_statement_end(const Noc_Token_Stream *tokens,
                                               size_t start)
{
    size_t index;
    size_t parens = 0;
    size_t brackets = 0;
    size_t braces = 0;
    for (index = start; index < tokens->count; ++index) {
        if (noc__dialect_is_punct(tokens, index, "(")) parens += 1;
        else if (noc__dialect_is_punct(tokens, index, ")")) {
            if (parens == 0) return NOC_TOKEN_INDEX_NONE;
            parens -= 1;
        } else if (noc__dialect_is_punct(tokens, index, "[")) brackets += 1;
        else if (noc__dialect_is_punct(tokens, index, "]")) {
            if (brackets == 0) return NOC_TOKEN_INDEX_NONE;
            brackets -= 1;
        } else if (noc__dialect_is_punct(tokens, index, "{")) braces += 1;
        else if (noc__dialect_is_punct(tokens, index, "}")) {
            if (braces == 0) return NOC_TOKEN_INDEX_NONE;
            braces -= 1;
        } else if (noc__dialect_is_punct(tokens, index, ";") &&
                   parens == 0 && brackets == 0 && braces == 0) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

NOC__PRIVATE Noc_Token_Range noc__dialect_trim_range(const Noc_Token_Stream *tokens,
                                                     Noc_Token_Range range)
{
    while (range.begin < range.end &&
           noc_token_is_trivia(tokens->items[range.begin])) {
        range.begin += 1;
    }
    while (range.end > range.begin &&
           noc_token_is_trivia(tokens->items[range.end - 1])) {
        range.end -= 1;
    }
    return range;
}

NOC__PRIVATE bool noc__dialect_type_is_prefixable(const Noc_Token_Stream *tokens,
                                                  Noc_Token_Range range)
{
    size_t index;
    bool has_identifier = false;
    range = noc__dialect_trim_range(tokens, range);
    for (index = range.begin; index < range.end; ++index) {
        Noc_Token token = tokens->items[index];
        if (noc_token_is_trivia(token)) continue;
        if (token.kind == NOC_TOKEN_IDENTIFIER) {
            has_identifier = true;
        } else if (!noc_token_is_punct(token, "*")) {
            return false;
        }
    }
    return has_identifier;
}

NOC__PRIVATE bool noc__dialect_preserve_newlines(Noc_Buffer *output, Noc_Slice source)
{
    size_t index;
    for (index = 0; index < source.count; ++index) {
        if (source.data[index] == '\r') {
            if (!noc_buffer_append(output, "\r", 1)) return false;
            if (index + 1 < source.count && source.data[index + 1] == '\n') {
                if (!noc_buffer_append(output, "\n", 1)) return false;
                index += 1;
            }
        } else if (source.data[index] == '\n' &&
                   !noc_buffer_append(output, "\n", 1)) {
            return false;
        }
    }
    return true;
}

NOC__PRIVATE bool noc__dialect_finish_edits(Noc_Edit_Set *edits,
                                            const Noc_Token_Stream *tokens,
                                            Noc_Slice source,
                                            Noc_Buffer *output)
{
    bool ok;
    if (edits->count == 0) {
        ok = noc_buffer_append(output, source.data, source.count) &&
             noc_buffer_terminate(output);
    } else {
        ok = noc_edit_set_apply(edits, tokens, output);
    }
    return ok;
}

static bool noc__templates_append(Noc__Templates *templates, Noc__Template value)
{
    Noc__Template *items;
    size_t capacity;
    if (templates->count == templates->capacity) {
        capacity = templates->capacity == 0 ? 4 : templates->capacity * 2;
        if (capacity <= templates->capacity ||
            capacity > SIZE_MAX / sizeof(*items)) return false;
        items = (Noc__Template *)realloc(templates->items,
                                         capacity * sizeof(*items));
        if (!items) return false;
        templates->items = items;
        templates->capacity = capacity;
    }
    templates->items[templates->count++] = value;
    return true;
}

static bool noc__dialect_names_append(Noc__Dialect_Names *names, Noc_Slice value)
{
    Noc_Slice *items;
    size_t capacity;
    if (names->count == names->capacity) {
        capacity = names->capacity == 0 ? 8 : names->capacity * 2;
        if (capacity <= names->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return false;
        }
        items = (Noc_Slice *)realloc(names->items, capacity * sizeof(*items));
        if (!items) return false;
        names->items = items;
        names->capacity = capacity;
    }
    names->items[names->count++] = value;
    return true;
}

static const Noc__Template *noc__find_template(const Noc__Templates *templates,
                                               Noc_Slice name)
{
    size_t index;
    for (index = 0; index < templates->count; ++index) {
        if (noc__dialect_slice_equal(templates->items[index].name, name)) {
            return &templates->items[index];
        }
    }
    return NULL;
}

static bool noc__dialect_name_exists(const Noc__Dialect_Names *names,
                                     Noc_Slice name)
{
    size_t index;
    for (index = 0; index < names->count; ++index) {
        if (noc__dialect_slice_equal(names->items[index], name)) return true;
    }
    return false;
}

static bool noc__range_is_one_identifier(const Noc_Token_Stream *tokens,
                                         Noc_Token_Range range,
                                         Noc_Slice *identifier)
{
    range = noc__dialect_trim_range(tokens, range);
    if (range.end != range.begin + 1 ||
        tokens->items[range.begin].kind != NOC_TOKEN_IDENTIFIER) {
        return false;
    }
    if (identifier) *identifier = tokens->items[range.begin].text;
    return true;
}

static bool noc__template_header(const Noc_Token_Stream *tokens,
                                 size_t keyword,
                                 Noc_Slice *parameter,
                                 Noc_Slice *name,
                                 size_t *close)
{
    size_t open = noc__dialect_next_significant(tokens, keyword + 1);
    size_t first;
    size_t comma;
    size_t second;
    size_t extra;
    if (open == NOC_TOKEN_INDEX_NONE ||
        !noc__dialect_is_punct(tokens, open, "(")) return false;
    *close = noc__dialect_matching_close(tokens, open, "(", ")");
    if (*close == NOC_TOKEN_INDEX_NONE) return false;
    first = noc__dialect_next_significant(tokens, open + 1);
    comma = first == NOC_TOKEN_INDEX_NONE
                ? NOC_TOKEN_INDEX_NONE
                : noc__dialect_next_significant(tokens, first + 1);
    second = comma == NOC_TOKEN_INDEX_NONE
                 ? NOC_TOKEN_INDEX_NONE
                 : noc__dialect_next_significant(tokens, comma + 1);
    extra = second == NOC_TOKEN_INDEX_NONE
                ? NOC_TOKEN_INDEX_NONE
                : noc__dialect_next_significant(tokens, second + 1);
    if (first == NOC_TOKEN_INDEX_NONE || comma == NOC_TOKEN_INDEX_NONE ||
        second == NOC_TOKEN_INDEX_NONE ||
        tokens->items[first].kind != NOC_TOKEN_IDENTIFIER ||
        !noc__dialect_is_punct(tokens, comma, ",") ||
        tokens->items[second].kind != NOC_TOKEN_IDENTIFIER || extra != *close) {
        return false;
    }
    *parameter = tokens->items[first].text;
    *name = tokens->items[second].text;
    return true;
}

static size_t noc__template_matching_open(const Noc_Token_Stream *tokens,
                                          size_t close,
                                          const char *open_spelling,
                                          const char *close_spelling)
{
    size_t depth = 0;
    size_t index = close + 1;
    while (index > 0) {
        index -= 1;
        if (noc__dialect_is_punct(tokens, index, close_spelling)) {
            depth += 1;
        } else if (noc__dialect_is_punct(tokens, index, open_spelling)) {
            if (depth == 0) return NOC_TOKEN_INDEX_NONE;
            depth -= 1;
            if (depth == 0) return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static size_t noc__template_function_body(const Noc_Token_Stream *tokens,
                                          size_t start,
                                          Noc_Slice name,
                                          size_t *name_token)
{
    size_t index;
    size_t parens = 0;
    size_t brackets = 0;
    *name_token = NOC_TOKEN_INDEX_NONE;
    for (index = start; index < tokens->count; ++index) {
        if (noc__dialect_is_punct(tokens, index, "(")) parens += 1;
        else if (noc__dialect_is_punct(tokens, index, ")")) {
            if (parens == 0) return NOC_TOKEN_INDEX_NONE;
            parens -= 1;
        } else if (noc__dialect_is_punct(tokens, index, "[")) brackets += 1;
        else if (noc__dialect_is_punct(tokens, index, "]")) {
            if (brackets == 0) return NOC_TOKEN_INDEX_NONE;
            brackets -= 1;
        } else if (noc__dialect_is_punct(tokens, index, "{") &&
                   parens == 0 && brackets == 0) {
            size_t parameter_close =
                noc__dialect_previous_significant(tokens, index);
            size_t parameter_open =
                parameter_close == NOC_TOKEN_INDEX_NONE ||
                        !noc__dialect_is_punct(tokens, parameter_close, ")")
                    ? NOC_TOKEN_INDEX_NONE
                    : noc__template_matching_open(tokens,
                                                  parameter_close,
                                                  "(", ")");
            size_t function_name =
                parameter_open == NOC_TOKEN_INDEX_NONE
                    ? NOC_TOKEN_INDEX_NONE
                    : noc__dialect_previous_significant(tokens, parameter_open);
            if (function_name == NOC_TOKEN_INDEX_NONE ||
                tokens->items[function_name].kind != NOC_TOKEN_IDENTIFIER ||
                !noc__dialect_slice_equal(tokens->items[function_name].text, name)) {
                return NOC_TOKEN_INDEX_NONE;
            }
            *name_token = function_name;
            return index;
        } else if (noc__dialect_is_punct(tokens, index, ";") &&
                   parens == 0 && brackets == 0) {
            return NOC_TOKEN_INDEX_NONE;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static bool noc__split_instantiation(const Noc_Token_Stream *tokens,
                                     size_t open,
                                     size_t close,
                                     Noc_Token_Range arguments[3])
{
    size_t commas[2];
    size_t comma_count = 0;
    size_t parens = 0;
    size_t brackets = 0;
    size_t braces = 0;
    size_t index;
    for (index = open + 1; index < close; ++index) {
        if (noc__dialect_is_punct(tokens, index, "(")) parens += 1;
        else if (noc__dialect_is_punct(tokens, index, ")")) {
            if (parens == 0) return false;
            parens -= 1;
        } else if (noc__dialect_is_punct(tokens, index, "[")) brackets += 1;
        else if (noc__dialect_is_punct(tokens, index, "]")) {
            if (brackets == 0) return false;
            brackets -= 1;
        } else if (noc__dialect_is_punct(tokens, index, "{")) braces += 1;
        else if (noc__dialect_is_punct(tokens, index, "}")) {
            if (braces == 0) return false;
            braces -= 1;
        } else if (noc__dialect_is_punct(tokens, index, ",") &&
                   parens == 0 && brackets == 0 && braces == 0) {
            if (comma_count == 2) return false;
            commas[comma_count++] = index;
        }
    }
    if (comma_count != 2 || parens != 0 || brackets != 0 || braces != 0) {
        return false;
    }
    arguments[0] = noc__dialect_trim_range(
        tokens, (Noc_Token_Range){open + 1, commas[0]});
    arguments[1] = noc__dialect_trim_range(
        tokens, (Noc_Token_Range){commas[0] + 1, commas[1]});
    arguments[2] = noc__dialect_trim_range(
        tokens, (Noc_Token_Range){commas[1] + 1, close});
    return arguments[0].begin < arguments[0].end &&
           arguments[1].begin < arguments[1].end &&
           arguments[2].begin < arguments[2].end;
}

static bool noc__instantiate_template(const Noc_Token_Stream *tokens,
                                      const Noc__Template *template_value,
                                      Noc_Slice type,
                                      Noc_Slice generated_name,
                                      Noc_Buffer *output)
{
    size_t index;
    size_t cursor = tokens->items[template_value->declaration.begin].location.offset;
    size_t end = tokens->items[template_value->declaration.end - 1].location.offset +
                 tokens->items[template_value->declaration.end - 1].text.count;
    for (index = template_value->declaration.begin;
         index < template_value->declaration.end;
         ++index) {
        Noc_Token token = tokens->items[index];
        size_t offset = token.location.offset;
        Noc_Slice replacement = token.text;
        if (offset < cursor || offset > end ||
            !noc_buffer_append(output, tokens->source + cursor, offset - cursor)) {
            return false;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            noc__dialect_slice_equal(token.text, template_value->parameter)) {
            size_t previous = noc__dialect_previous_significant(tokens, index);
            if (previous == NOC_TOKEN_INDEX_NONE ||
                (!noc__dialect_is_punct(tokens, previous, ".") &&
                 !noc__dialect_is_punct(tokens, previous, "->"))) {
                replacement = type;
            }
        } else if (index == template_value->name_token) {
            replacement = generated_name;
        }
        if (!noc_buffer_append_slice(output, replacement)) return false;
        cursor = offset + token.text.count;
    }
    return cursor <= end &&
           noc_buffer_append(output, tokens->source + cursor, end - cursor);
}

/* Explicit template MVP.  Parsing uses lexer tokens rather than substring
   replacement, so identifiers in comments, literals, and preprocessing tokens
   remain untouched.  One declaration-prefix type parameter and explicit named
   instances keep generated C deterministic. Self references are rejected until
   semantic name binding can distinguish recursion from a shadowed identifier. */
NOC__PRIVATE bool noc__lower_templates(Noc_Context *context,
                                       const char *path,
                                       Noc_Slice source,
                                       Noc_Buffer *output)
{
    Noc_Token_Stream tokens = {0};
    Noc_Preprocessor_Map activity = {0};
    Noc_Edit_Set edits = {0};
    Noc__Templates templates = {0};
    Noc__Dialect_Names generated_names = {0};
    size_t index;
    bool ok = false;
    if (!noc__dialect_tokenize(context, path, source, &tokens)) goto done;
    if (context->options.skip_inactive_preprocessor_branches &&
        !noc_preprocessor_map_build(context, &tokens, &activity)) goto done;
    for (index = 0; index < tokens.count;) {
        Noc_Token token = tokens.items[index];
        if (activity.items &&
            noc_preprocessor_activity_at(&activity, index) ==
                NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
            index += 1;
            continue;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(token, "template")) {
            Noc__Template value;
            size_t header_close;
            size_t declaration_start;
            size_t body_open;
            size_t body_close;
            Noc_Buffer replacement = {0};
            Noc_Slice removed;
            if (!noc__template_header(&tokens,
                                      index,
                                      &value.parameter,
                                      &value.name,
                                      &header_close)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "expected template(type_parameter, name)");
                goto done;
            }
            if (noc__find_template(&templates, value.name)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "duplicate template '%.*s'",
                            (int)value.name.count, value.name.data);
                goto done;
            }
            declaration_start = noc__dialect_next_significant(&tokens,
                                                               header_close + 1);
            body_open = declaration_start == NOC_TOKEN_INDEX_NONE
                            ? NOC_TOKEN_INDEX_NONE
                            : noc__template_function_body(&tokens,
                                                          declaration_start,
                                                          value.name,
                                                          &value.name_token);
            body_close = body_open == NOC_TOKEN_INDEX_NONE
                             ? NOC_TOKEN_INDEX_NONE
                             : noc__dialect_matching_close(&tokens,
                                                           body_open,
                                                           "{", "}");
            if (body_close == NOC_TOKEN_INDEX_NONE) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "template '%.*s' must be followed by its function definition",
                            (int)value.name.count, value.name.data);
                goto done;
            }
            value.declaration = (Noc_Token_Range){declaration_start,
                                                   body_close + 1};
            {
                size_t name_use;
                for (name_use = value.declaration.begin;
                     name_use < value.declaration.end;
                     ++name_use) {
                    size_t previous;
                    if (name_use == value.name_token ||
                        tokens.items[name_use].kind != NOC_TOKEN_IDENTIFIER ||
                        !noc__dialect_slice_equal(tokens.items[name_use].text,
                                                  value.name)) continue;
                    previous = noc__dialect_previous_significant(&tokens, name_use);
                    if (previous != NOC_TOKEN_INDEX_NONE &&
                        (noc__dialect_is_punct(&tokens, previous, ".") ||
                         noc__dialect_is_punct(&tokens, previous, "->"))) continue;
                    noc__report(context, NOC_DIAGNOSTIC_ERROR,
                                tokens.items[name_use].location,
                                "template self references and shadowing are "
                                "unsupported by the MVP");
                    goto done;
                }
            }
            if (!noc__templates_append(&templates, value)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "out of memory while recording template");
                goto done;
            }
            removed = noc_token_range_source(
                &tokens, (Noc_Token_Range){index, body_close + 1});
            if (!noc_buffer_append_cstr(&replacement,
                                        "/* noc: lowered template definition */") ||
                !noc__dialect_preserve_newlines(&replacement, removed) ||
                !noc_edit_set_add(&edits,
                                  &tokens,
                                  (Noc_Token_Range){index, body_close + 1},
                                  (Noc_Slice){replacement.items,
                                              replacement.count})) {
                noc_buffer_free(&replacement);
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "out of memory while removing template definition");
                goto done;
            }
            noc_buffer_free(&replacement);
            index = body_close + 1;
            continue;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(token, "instantiate")) {
            size_t open = noc__dialect_next_significant(&tokens, index + 1);
            size_t close = open == NOC_TOKEN_INDEX_NONE
                               ? NOC_TOKEN_INDEX_NONE
                               : noc__dialect_matching_close(&tokens, open, "(", ")");
            size_t semicolon = close == NOC_TOKEN_INDEX_NONE
                                   ? NOC_TOKEN_INDEX_NONE
                                   : noc__dialect_next_significant(&tokens, close + 1);
            Noc_Token_Range arguments[3];
            Noc_Slice template_name;
            Noc_Slice generated_name;
            Noc_Slice type;
            const Noc__Template *template_value;
            Noc_Buffer replacement = {0};
            if (open == NOC_TOKEN_INDEX_NONE || close == NOC_TOKEN_INDEX_NONE ||
                semicolon == NOC_TOKEN_INDEX_NONE ||
                !noc__dialect_is_punct(&tokens, open, "(") ||
                !noc__dialect_is_punct(&tokens, semicolon, ";") ||
                !noc__split_instantiation(&tokens, open, close, arguments) ||
                !noc__range_is_one_identifier(&tokens, arguments[0], &template_name) ||
                !noc__range_is_one_identifier(&tokens, arguments[2], &generated_name)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "expected instantiate(template, type, generated_name);");
                goto done;
            }
            template_value = noc__find_template(&templates, template_name);
            if (!template_value) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "unknown or not-yet-declared template '%.*s'",
                            (int)template_name.count, template_name.data);
                goto done;
            }
            if (noc__dialect_name_exists(&generated_names, generated_name)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "duplicate generated template name '%.*s'",
                            (int)generated_name.count, generated_name.data);
                goto done;
            }
            if (!noc__dialect_type_is_prefixable(&tokens, arguments[1])) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "template type must be a declaration-prefix type "
                            "without parentheses or brackets in the MVP");
                goto done;
            }
            type = noc_token_range_source(&tokens, arguments[1]);
            if (!type.data ||
                !noc__instantiate_template(&tokens,
                                           template_value,
                                           type,
                                           generated_name,
                                           &replacement) ||
                !noc_edit_set_add(&edits,
                                  &tokens,
                                  (Noc_Token_Range){index, semicolon + 1},
                                  (Noc_Slice){replacement.items,
                                              replacement.count}) ||
                !noc__dialect_names_append(&generated_names, generated_name)) {
                noc_buffer_free(&replacement);
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "out of memory while instantiating template");
                goto done;
            }
            noc_buffer_free(&replacement);
            index = semicolon + 1;
            continue;
        }
        index += 1;
    }
    if (!noc__dialect_finish_edits(&edits, &tokens, source, output)) {
        Noc_Location none = {0};
        noc__report(context, NOC_DIAGNOSTIC_ERROR, none,
                    "out of memory while applying template instances");
        goto done;
    }
    ok = true;

done:
    free(generated_names.items);
    free(templates.items);
    noc_edit_set_free(&edits);
    noc_preprocessor_map_free(&activity);
    noc__dialect_tokens_free(&tokens);
    if (!ok) noc_buffer_free(output);
    return ok;
}
#endif
#endif
