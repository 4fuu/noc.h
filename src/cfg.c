#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_CFG_IMPLEMENTATION_INCLUDED
#define NOC_CFG_IMPLEMENTATION_INCLUDED

typedef struct {
    Noc_Slice *items;
    size_t count;
    size_t capacity;
} Noc__Defer_Actions;

typedef struct {
    size_t action_start;
} Noc__Defer_Scope;

typedef struct {
    Noc__Defer_Scope *items;
    size_t count;
    size_t capacity;
} Noc__Defer_Scopes;

static bool noc__defer_is_punct(const Noc_Token_Stream *tokens,
                                size_t index,
                                const char *punctuator)
{
    return index < tokens->count &&
           noc_token_is_punct(tokens->items[index], punctuator);
}

static bool noc__defer_actions_append(Noc__Defer_Actions *actions,
                                      Noc_Slice action)
{
    Noc_Slice *items;
    size_t capacity;
    if (actions->count == actions->capacity) {
        capacity = actions->capacity == 0 ? 8 : actions->capacity * 2;
        if (capacity <= actions->capacity ||
            capacity > SIZE_MAX / sizeof(*items)) return false;
        items = (Noc_Slice *)realloc(actions->items, capacity * sizeof(*items));
        if (!items) return false;
        actions->items = items;
        actions->capacity = capacity;
    }
    actions->items[actions->count++] = action;
    return true;
}

static bool noc__defer_scopes_push(Noc__Defer_Scopes *scopes, size_t action_start)
{
    Noc__Defer_Scope *items;
    size_t capacity;
    if (scopes->count == scopes->capacity) {
        capacity = scopes->capacity == 0 ? 8 : scopes->capacity * 2;
        if (capacity <= scopes->capacity ||
            capacity > SIZE_MAX / sizeof(*items)) return false;
        items = (Noc__Defer_Scope *)realloc(scopes->items,
                                            capacity * sizeof(*items));
        if (!items) return false;
        scopes->items = items;
        scopes->capacity = capacity;
    }
    scopes->items[scopes->count++].action_start = action_start;
    return true;
}

static bool noc__defer_emit_actions(Noc_Buffer *output,
                                    const Noc__Defer_Actions *actions,
                                    size_t begin,
                                    size_t end)
{
    while (end > begin) {
        end -= 1;
        if (!noc_buffer_append_cstr(output, "\n") ||
            !noc_buffer_append_slice(output, actions->items[end]) ||
            !noc_buffer_append_cstr(output, "\n")) return false;
    }
    return true;
}

static bool noc__defer_action_is_straight_line(const Noc_Token_Stream *tokens,
                                               Noc_Token_Range range)
{
    size_t index;
    for (index = range.begin; index < range.end; ++index) {
        Noc_Token token = tokens->items[index];
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            (noc_token_is_identifier(token, "defer") ||
             noc_token_is_identifier(token, "return") ||
             noc_token_is_identifier(token, "goto") ||
             noc_token_is_identifier(token, "break") ||
             noc_token_is_identifier(token, "continue"))) {
            return false;
        }
    }
    return true;
}

static size_t noc__defer_matching_open(const Noc_Token_Stream *tokens,
                                       size_t close,
                                       const char *open_spelling,
                                       const char *close_spelling)
{
    size_t depth = 0;
    size_t index = close + 1;
    while (index > 0) {
        index -= 1;
        if (noc__defer_is_punct(tokens, index, close_spelling)) {
            depth += 1;
        } else if (noc__defer_is_punct(tokens, index, open_spelling)) {
            if (depth == 0) return NOC_TOKEN_INDEX_NONE;
            depth -= 1;
            if (depth == 0) return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static bool noc__defer_storage_specifier(Noc_Token token)
{
    return token.kind == NOC_TOKEN_IDENTIFIER &&
           (noc_token_is_identifier(token, "static") ||
            noc_token_is_identifier(token, "extern") ||
            noc_token_is_identifier(token, "inline") ||
            noc_token_is_identifier(token, "_Noreturn"));
}

static bool noc__defer_function_return_type(const Noc_Token_Stream *tokens,
                                            size_t body_open,
                                            Noc_Slice *type,
                                            bool *returns_void)
{
    size_t parameter_close = noc__dialect_previous_significant(tokens, body_open);
    size_t parameter_open;
    size_t function_name;
    size_t begin = 0;
    size_t cursor;
    Noc_Token_Range range;
    if (parameter_close == NOC_TOKEN_INDEX_NONE ||
        !noc__defer_is_punct(tokens, parameter_close, ")")) return false;
    parameter_open = noc__defer_matching_open(tokens,
                                              parameter_close,
                                              "(", ")");
    function_name = parameter_open == NOC_TOKEN_INDEX_NONE
                        ? NOC_TOKEN_INDEX_NONE
                        : noc__dialect_previous_significant(tokens, parameter_open);
    if (function_name == NOC_TOKEN_INDEX_NONE ||
        tokens->items[function_name].kind != NOC_TOKEN_IDENTIFIER) return false;
    cursor = function_name;
    while (cursor > 0) {
        size_t previous = noc__dialect_previous_significant(tokens, cursor);
        if (previous == NOC_TOKEN_INDEX_NONE) break;
        if (noc__defer_is_punct(tokens, previous, ";") ||
            noc__defer_is_punct(tokens, previous, "}") ||
            tokens->items[previous].kind == NOC_TOKEN_PREPROCESSOR) {
            begin = previous + 1;
            break;
        }
        cursor = previous;
    }
    begin = noc__dialect_next_significant(tokens, begin);
    while (begin != NOC_TOKEN_INDEX_NONE && begin < function_name &&
           noc__defer_storage_specifier(tokens->items[begin])) {
        begin = noc__dialect_next_significant(tokens, begin + 1);
    }
    if (begin == NOC_TOKEN_INDEX_NONE || begin >= function_name) return false;
    range = noc__dialect_trim_range(
        tokens, (Noc_Token_Range){begin, function_name});
    if (range.begin >= range.end) return false;
    for (cursor = range.begin; cursor < range.end; ++cursor) {
        if (noc__defer_is_punct(tokens, cursor, "(") ||
            noc__defer_is_punct(tokens, cursor, ")") ||
            noc__defer_is_punct(tokens, cursor, "[") ||
            noc__defer_is_punct(tokens, cursor, "]") ||
            noc__defer_is_punct(tokens, cursor, ",") ||
            noc__defer_is_punct(tokens, cursor, "=")) return false;
    }
    *type = noc_token_range_source(tokens, range);
    *returns_void = range.end == range.begin + 1 &&
                    noc_token_is_identifier(tokens->items[range.begin], "void");
    return type->data != NULL;
}

static bool noc__defer_identifier_exists(const Noc_Token_Stream *tokens,
                                         const char *name)
{
    size_t index;
    for (index = 0; index < tokens->count; ++index) {
        if (tokens->items[index].kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(tokens->items[index], name)) return true;
    }
    return false;
}

static bool noc__defer_temporary(const Noc_Token_Stream *tokens,
                                 size_t *counter,
                                 char output[64])
{
    do {
        int count = snprintf(output, 64, "noc_defer_result_%zu", (*counter)++);
        if (count < 0 || count >= 64) return false;
    } while (noc__defer_identifier_exists(tokens, output));
    return true;
}

/* Structured defer MVP.  Actions are attached to lexical compound scopes,
   emitted in LIFO order on fallthrough, and copied before each return.  Returns
   are wrapped as one compound statement so lowering preserves an enclosing
   if/else. Any goto/break/continue in a function containing defer is rejected
   until the full CFG pass can identify exact cleanup-crossing edges. */
NOC__PRIVATE bool noc__lower_defer(Noc_Context *context,
                                   const char *path,
                                   Noc_Slice source,
                                   Noc_Buffer *output)
{
    Noc_Token_Stream tokens = {0};
    Noc_Preprocessor_Map activity = {0};
    Noc_Edit_Set edits = {0};
    Noc__Defer_Actions actions = {0};
    Noc__Defer_Scopes scopes = {0};
    Noc_Slice function_return_type = {0};
    size_t function_scope_depth = NOC_TOKEN_INDEX_NONE;
    size_t temporary_counter = 0;
    bool function_returns_void = false;
    bool function_has_defer = false;
    bool function_has_control_flow = false;
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
        if (noc__defer_is_punct(&tokens, index, "{")) {
            Noc_Slice candidate_type = {0};
            bool candidate_void = false;
            bool starts_function =
                scopes.count == 0 &&
                noc__defer_function_return_type(&tokens,
                                                index,
                                                &candidate_type,
                                                &candidate_void);
            if (!noc__defer_scopes_push(&scopes, actions.count)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "out of memory while entering defer scope");
                goto done;
            }
            if (starts_function) {
                function_return_type = candidate_type;
                function_returns_void = candidate_void;
                function_scope_depth = scopes.count;
                function_has_defer = false;
                function_has_control_flow = false;
            }
            index += 1;
            continue;
        }
        if (noc__defer_is_punct(&tokens, index, "}")) {
            Noc_Buffer replacement = {0};
            size_t action_start;
            if (scopes.count == 0) {
                index += 1;
                continue;
            }
            action_start = scopes.items[scopes.count - 1].action_start;
            if (actions.count > action_start &&
                (!noc__defer_emit_actions(&replacement,
                                          &actions,
                                          action_start,
                                          actions.count) ||
                 !noc_edit_set_add(&edits,
                                   &tokens,
                                   (Noc_Token_Range){index, index},
                                   (Noc_Slice){replacement.items,
                                               replacement.count}))) {
                noc_buffer_free(&replacement);
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "out of memory while emitting deferred fallthrough actions");
                goto done;
            }
            noc_buffer_free(&replacement);
            actions.count = action_start;
            if (scopes.count == function_scope_depth) {
                function_return_type = (Noc_Slice){0};
                function_returns_void = false;
                function_scope_depth = NOC_TOKEN_INDEX_NONE;
                function_has_defer = false;
                function_has_control_flow = false;
            }
            scopes.count -= 1;
            index += 1;
            continue;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(token, "defer")) {
            size_t previous = noc__dialect_previous_significant(&tokens, index);
            size_t action_begin = noc__dialect_next_significant(&tokens, index + 1);
            size_t action_end;
            size_t removal_end;
            Noc_Token_Range action_range;
            Noc_Slice action_source;
            Noc_Slice removed;
            Noc_Buffer replacement = {0};
            if (scopes.count == 0 || previous == NOC_TOKEN_INDEX_NONE ||
                !(noc__defer_is_punct(&tokens, previous, "{") ||
                  noc__defer_is_punct(&tokens, previous, ";") ||
                  noc__defer_is_punct(&tokens, previous, "}")) ||
                action_begin == NOC_TOKEN_INDEX_NONE) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "defer must be a direct child of a compound block");
                goto done;
            }
            if (function_has_control_flow) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "goto/break/continue in a function containing defer "
                            "is unsupported by the MVP cleanup planner");
                goto done;
            }
            if (noc__defer_is_punct(&tokens, action_begin, "{")) {
                action_end = noc__dialect_matching_close(&tokens,
                                                          action_begin,
                                                          "{", "}");
                if (action_end == NOC_TOKEN_INDEX_NONE) {
                    noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                                "unterminated defer block");
                    goto done;
                }
                action_range = (Noc_Token_Range){action_begin, action_end + 1};
                removal_end = action_end + 1;
                {
                    size_t optional_semicolon =
                        noc__dialect_next_significant(&tokens, removal_end);
                    if (optional_semicolon != NOC_TOKEN_INDEX_NONE &&
                        noc__defer_is_punct(&tokens, optional_semicolon, ";")) {
                        removal_end = optional_semicolon + 1;
                    }
                }
            } else {
                action_end = noc__dialect_statement_end(&tokens, action_begin);
                if (action_end == NOC_TOKEN_INDEX_NONE) {
                    noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                                "defer expression must end with ';'");
                    goto done;
                }
                action_range = (Noc_Token_Range){action_begin, action_end + 1};
                removal_end = action_end + 1;
                {
                    size_t action_token;
                    bool has_operand = false;
                    for (action_token = action_begin;
                         action_token < action_end;
                         ++action_token) {
                        Noc_Token action = tokens.items[action_token];
                        if (!noc_token_is_trivia(action) &&
                            !(action.kind == NOC_TOKEN_PUNCTUATOR &&
                              (noc_token_is_punct(action, "(") ||
                               noc_token_is_punct(action, ")")))) {
                            has_operand = true;
                            break;
                        }
                    }
                    if (!has_operand) {
                        noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                                    "defer expression cannot be empty");
                        goto done;
                    }
                }
            }
            if (!noc__defer_action_is_straight_line(&tokens, action_range)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "defer action cannot contain defer or control-flow exits in the MVP");
                goto done;
            }
            action_source = noc_token_range_source(&tokens, action_range);
            removed = noc_token_range_source(
                &tokens, (Noc_Token_Range){index, removal_end});
            if (!action_source.data || !removed.data ||
                !noc__defer_actions_append(&actions, action_source) ||
                !noc_buffer_append_cstr(&replacement, " ") ||
                !noc__dialect_preserve_newlines(&replacement, removed) ||
                !noc_edit_set_add(&edits,
                                  &tokens,
                                  (Noc_Token_Range){index, removal_end},
                                  (Noc_Slice){replacement.items,
                                              replacement.count})) {
                noc_buffer_free(&replacement);
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "out of memory while recording defer action");
                goto done;
            }
            noc_buffer_free(&replacement);
            function_has_defer = true;
            index = removal_end;
            continue;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(token, "return")) {
            size_t semicolon = noc__dialect_statement_end(&tokens, index + 1);
            if (semicolon == NOC_TOKEN_INDEX_NONE) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "return statement is missing ';'");
                goto done;
            }
            if (actions.count > 0) {
                Noc_Buffer replacement = {0};
                Noc_Slice statement = noc_token_range_source(
                    &tokens, (Noc_Token_Range){index, semicolon + 1});
                Noc_Token_Range expression = noc__dialect_trim_range(
                    &tokens, (Noc_Token_Range){index + 1, semicolon});
                if (!statement.data ||
                    !noc_buffer_append_cstr(&replacement, "{")) {
                    goto return_allocation_failed;
                }
                if (expression.begin < expression.end) {
                    Noc_Slice expression_source =
                        noc_token_range_source(&tokens, expression);
                    char temporary[64];
                    if (!function_return_type.data || function_returns_void) {
                        noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                                    "value return with defer requires a simple, "
                                    "non-void function return type in the MVP");
                        noc_buffer_free(&replacement);
                        goto done;
                    }
                    if (!expression_source.data ||
                        !noc__defer_temporary(&tokens,
                                              &temporary_counter,
                                              temporary) ||
                        !noc_buffer_append_slice(&replacement,
                                                 function_return_type) ||
                        !noc_buffer_appendf(&replacement,
                                            " %s = (",
                                            temporary) ||
                        !noc_buffer_append_slice(&replacement,
                                                 expression_source) ||
                        !noc_buffer_append_cstr(&replacement, ");") ||
                        !noc__defer_emit_actions(&replacement,
                                                  &actions,
                                                  0,
                                                  actions.count) ||
                        !noc_buffer_appendf(&replacement,
                                            "return %s;}",
                                            temporary)) {
                        goto return_allocation_failed;
                    }
                } else if (!noc__defer_emit_actions(&replacement,
                                                     &actions,
                                                     0,
                                                     actions.count) ||
                           !noc_buffer_append_slice(&replacement, statement) ||
                           !noc_buffer_append_cstr(&replacement, "}")) {
                    goto return_allocation_failed;
                }
                if (
                    !noc_edit_set_add(&edits,
                                      &tokens,
                                      (Noc_Token_Range){index, semicolon + 1},
                                      (Noc_Slice){replacement.items,
                                                  replacement.count})) {
return_allocation_failed:
                    noc_buffer_free(&replacement);
                    noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                                "out of memory while lowering deferred return");
                    goto done;
                }
                noc_buffer_free(&replacement);
            }
            index = semicolon + 1;
            continue;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            (noc_token_is_identifier(token, "goto") ||
             noc_token_is_identifier(token, "break") ||
             noc_token_is_identifier(token, "continue"))) {
            if (function_has_defer || actions.count > 0) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "goto/break/continue in a function containing defer "
                            "is unsupported by the MVP cleanup planner");
                goto done;
            }
            if (function_scope_depth != NOC_TOKEN_INDEX_NONE) {
                function_has_control_flow = true;
            }
        }
        index += 1;
    }
    if (scopes.count != 0) {
        Noc_Location none = {path, 0, 1, 1};
        noc__report(context, NOC_DIAGNOSTIC_ERROR, none,
                    "unclosed compound block while lowering defer");
        goto done;
    }
    if (!noc__dialect_finish_edits(&edits, &tokens, source, output)) {
        Noc_Location none = {0};
        noc__report(context, NOC_DIAGNOSTIC_ERROR, none,
                    "out of memory while applying defer lowering");
        goto done;
    }
    ok = true;

done:
    free(scopes.items);
    free(actions.items);
    noc_edit_set_free(&edits);
    noc_preprocessor_map_free(&activity);
    noc__dialect_tokens_free(&tokens);
    if (!ok) noc_buffer_free(output);
    return ok;
}
#endif
#endif
