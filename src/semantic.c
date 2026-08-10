#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_SEMANTIC_IMPLEMENTATION_INCLUDED
#define NOC_SEMANTIC_IMPLEMENTATION_INCLUDED

typedef struct {
    Noc_Slice type;
    Noc_Slice drop;
    Noc_Slice name;
    size_t depth;
    bool moved;
} Noc__Owner;

typedef struct {
    Noc__Owner *items;
    size_t count;
    size_t capacity;
} Noc__Owners;

static bool noc__owner_slice_equal(Noc_Slice left, Noc_Slice right)
{
    return left.count == right.count &&
           (left.count == 0 || memcmp(left.data, right.data, left.count) == 0);
}

static bool noc__owner_is_punct(const Noc_Token_Stream *tokens,
                                size_t index,
                                const char *punctuator)
{
    return index < tokens->count &&
           noc_token_is_punct(tokens->items[index], punctuator);
}

static Noc__Owner *noc__owner_find(Noc__Owners *owners, Noc_Slice name)
{
    size_t index = owners->count;
    while (index > 0) {
        index -= 1;
        if (noc__owner_slice_equal(owners->items[index].name, name)) {
            return &owners->items[index];
        }
    }
    return NULL;
}

static bool noc__owner_append(Noc__Owners *owners, Noc__Owner owner)
{
    Noc__Owner *items;
    size_t capacity;
    if (owners->count == owners->capacity) {
        capacity = owners->capacity == 0 ? 8 : owners->capacity * 2;
        if (capacity <= owners->capacity ||
            capacity > SIZE_MAX / sizeof(*items)) return false;
        items = (Noc__Owner *)realloc(owners->items, capacity * sizeof(*items));
        if (!items) return false;
        owners->items = items;
        owners->capacity = capacity;
    }
    owners->items[owners->count++] = owner;
    return true;
}

static bool noc__owner_range_identifier(const Noc_Token_Stream *tokens,
                                        Noc_Token_Range range,
                                        Noc_Slice *name)
{
    range = noc__dialect_trim_range(tokens, range);
    if (range.end != range.begin + 1 ||
        tokens->items[range.begin].kind != NOC_TOKEN_IDENTIFIER) return false;
    *name = tokens->items[range.begin].text;
    return true;
}

static bool noc__owner_split_header(const Noc_Token_Stream *tokens,
                                    size_t open,
                                    size_t close,
                                    Noc_Token_Range *type,
                                    Noc_Token_Range *drop)
{
    size_t index;
    size_t comma = NOC_TOKEN_INDEX_NONE;
    size_t parens = 0;
    size_t brackets = 0;
    for (index = open + 1; index < close; ++index) {
        if (noc__owner_is_punct(tokens, index, "(")) parens += 1;
        else if (noc__owner_is_punct(tokens, index, ")")) {
            if (parens == 0) return false;
            parens -= 1;
        } else if (noc__owner_is_punct(tokens, index, "[")) brackets += 1;
        else if (noc__owner_is_punct(tokens, index, "]")) {
            if (brackets == 0) return false;
            brackets -= 1;
        } else if (noc__owner_is_punct(tokens, index, ",") &&
                   parens == 0 && brackets == 0) {
            if (comma != NOC_TOKEN_INDEX_NONE) return false;
            comma = index;
        }
    }
    if (comma == NOC_TOKEN_INDEX_NONE || parens != 0 || brackets != 0) {
        return false;
    }
    *type = noc__dialect_trim_range(
        tokens, (Noc_Token_Range){open + 1, comma});
    *drop = noc__dialect_trim_range(
        tokens, (Noc_Token_Range){comma + 1, close});
    return type->begin < type->end && drop->begin < drop->end;
}

static bool noc__owner_type_is_pointer(const Noc_Token_Stream *tokens,
                                       Noc_Token_Range type)
{
    size_t index;
    for (index = type.begin; index < type.end; ++index) {
        if (noc__owner_is_punct(tokens, index, "*")) return true;
    }
    return false;
}

static bool noc__owner_exact_call(const Noc_Token_Stream *tokens,
                                  Noc_Token_Range range,
                                  const char *callee,
                                  Noc_Slice *argument)
{
    size_t name;
    size_t open;
    size_t close;
    Noc_Token_Range argument_range;
    range = noc__dialect_trim_range(tokens, range);
    name = range.begin;
    open = name < range.end
               ? noc__dialect_next_significant(tokens, name + 1)
               : NOC_TOKEN_INDEX_NONE;
    close = open == NOC_TOKEN_INDEX_NONE
                ? NOC_TOKEN_INDEX_NONE
                : noc__dialect_matching_close(tokens, open, "(", ")");
    if (name >= range.end || tokens->items[name].kind != NOC_TOKEN_IDENTIFIER ||
        !noc_token_is_identifier(tokens->items[name], callee) ||
        open == NOC_TOKEN_INDEX_NONE || !noc__owner_is_punct(tokens, open, "(") ||
        close == NOC_TOKEN_INDEX_NONE || close >= range.end) {
        return false;
    }
    {
        size_t tail;
        for (tail = close + 1; tail < range.end; ++tail) {
            if (!noc_token_is_trivia(tokens->items[tail])) return false;
        }
    }
    argument_range = noc__dialect_trim_range(
        tokens, (Noc_Token_Range){open + 1, close});
    return noc__owner_range_identifier(tokens, argument_range, argument);
}

static bool noc__owner_range_has_feature_call(const Noc_Token_Stream *tokens,
                                              Noc_Token_Range range)
{
    size_t index;
    for (index = range.begin; index < range.end; ++index) {
        if (tokens->items[index].kind == NOC_TOKEN_IDENTIFIER &&
            (noc_token_is_identifier(tokens->items[index], "move") ||
             noc_token_is_identifier(tokens->items[index], "borrow") ||
             noc_token_is_identifier(tokens->items[index], "own"))) {
            return true;
        }
    }
    return false;
}

static Noc__Owner *noc__owner_range_reference(Noc__Owners *owners,
                                              const Noc_Token_Stream *tokens,
                                              Noc_Token_Range range)
{
    size_t index;
    for (index = range.begin; index < range.end; ++index) {
        if (tokens->items[index].kind == NOC_TOKEN_IDENTIFIER) {
            Noc__Owner *owner = noc__owner_find(owners, tokens->items[index].text);
            if (owner) return owner;
        }
    }
    return NULL;
}

static bool noc__owner_identifier_exists(const Noc_Token_Stream *tokens,
                                         const char *name)
{
    size_t index;
    for (index = 0; index < tokens->count; ++index) {
        if (tokens->items[index].kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(tokens->items[index], name)) return true;
    }
    return false;
}

static bool noc__owner_temporary(const Noc_Token_Stream *tokens,
                                 size_t *counter,
                                 char output[64])
{
    do {
        int count = snprintf(output, 64, "noc_move_result_%zu", (*counter)++);
        if (count < 0 || count >= 64) return false;
    } while (noc__owner_identifier_exists(tokens, output));
    return true;
}

static bool noc__owner_emit_cleanup(Noc_Buffer *output, const Noc__Owner *owner)
{
    return noc_buffer_append_cstr(output, "; defer { if (") &&
           noc_buffer_append_slice(output, owner->name) &&
           noc_buffer_append_cstr(output, " != 0) { ") &&
           noc_buffer_append_slice(output, owner->drop) &&
           noc_buffer_append_cstr(output, "(") &&
           noc_buffer_append_slice(output, owner->name) &&
           noc_buffer_append_cstr(output, "); } }");
}

/* Ownership MVP.  `own(pointer_type, drop_fn)` records one lexical owner,
   explicit borrow keeps it live, and move transfers by nulling the source.
   Generated guarded defer actions make branch execution safe without relying
   on non-standard C expression extensions. */
NOC__PRIVATE bool noc__lower_ownership(Noc_Context *context,
                                       const char *path,
                                       Noc_Slice source,
                                       Noc_Buffer *output)
{
    Noc_Token_Stream tokens = {0};
    Noc_Preprocessor_Map activity = {0};
    Noc_Edit_Set edits = {0};
    Noc__Owners owners = {0};
    size_t index;
    size_t depth = 0;
    size_t temporary_counter = 0;
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
        if (noc__owner_is_punct(&tokens, index, "{")) {
            depth += 1;
            index += 1;
            continue;
        }
        if (noc__owner_is_punct(&tokens, index, "}")) {
            while (owners.count > 0 && owners.items[owners.count - 1].depth >= depth) {
                owners.count -= 1;
            }
            if (depth > 0) depth -= 1;
            index += 1;
            continue;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(token, "own")) {
            size_t previous = noc__dialect_previous_significant(&tokens, index);
            size_t open = noc__dialect_next_significant(&tokens, index + 1);
            size_t close = open == NOC_TOKEN_INDEX_NONE
                               ? NOC_TOKEN_INDEX_NONE
                               : noc__dialect_matching_close(&tokens, open, "(", ")");
            size_t name_index = close == NOC_TOKEN_INDEX_NONE
                                    ? NOC_TOKEN_INDEX_NONE
                                    : noc__dialect_next_significant(&tokens, close + 1);
            size_t equals = name_index == NOC_TOKEN_INDEX_NONE
                                ? NOC_TOKEN_INDEX_NONE
                                : noc__dialect_next_significant(&tokens, name_index + 1);
            size_t semicolon = equals == NOC_TOKEN_INDEX_NONE
                                   ? NOC_TOKEN_INDEX_NONE
                                   : noc__dialect_statement_end(&tokens, equals + 1);
            Noc_Token_Range type_range;
            Noc_Token_Range drop_range;
            Noc_Token_Range initializer;
            Noc_Slice drop;
            Noc_Slice moved_name = {0};
            Noc__Owner owner;
            Noc__Owner *moved_owner = NULL;
            Noc__Owner *initializer_owner = NULL;
            Noc__Owner *same_scope;
            Noc_Buffer replacement = {0};
            bool is_move;
            if (depth == 0 || previous == NOC_TOKEN_INDEX_NONE ||
                !(noc__owner_is_punct(&tokens, previous, "{") ||
                  noc__owner_is_punct(&tokens, previous, ";") ||
                  noc__owner_is_punct(&tokens, previous, "}")) ||
                open == NOC_TOKEN_INDEX_NONE || close == NOC_TOKEN_INDEX_NONE ||
                name_index == NOC_TOKEN_INDEX_NONE || equals == NOC_TOKEN_INDEX_NONE ||
                semicolon == NOC_TOKEN_INDEX_NONE ||
                !noc__owner_is_punct(&tokens, open, "(") ||
                tokens.items[name_index].kind != NOC_TOKEN_IDENTIFIER ||
                !noc__owner_is_punct(&tokens, equals, "=") ||
                !noc__owner_split_header(&tokens, open, close,
                                         &type_range, &drop_range) ||
                !noc__owner_type_is_pointer(&tokens, type_range) ||
                !noc__owner_range_identifier(&tokens, drop_range, &drop)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "expected direct-block declaration "
                            "own(pointer_type, drop_function) name = expression;");
                goto done;
            }
            if (!noc__dialect_type_is_prefixable(&tokens, type_range)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "owned pointer type must be a declaration-prefix type "
                            "without parentheses or brackets in the MVP");
                goto done;
            }
            owner.type = noc_token_range_source(&tokens, type_range);
            owner.drop = drop;
            owner.name = tokens.items[name_index].text;
            owner.depth = depth;
            owner.moved = false;
            same_scope = noc__owner_find(&owners, owner.name);
            if (same_scope && same_scope->depth == depth) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "duplicate owner '%.*s' in one scope",
                            (int)owner.name.count, owner.name.data);
                goto done;
            }
            initializer = noc__dialect_trim_range(
                &tokens, (Noc_Token_Range){equals + 1, semicolon});
            is_move = noc__owner_exact_call(&tokens, initializer, "move", &moved_name);
            if (is_move) {
                moved_owner = noc__owner_find(&owners, moved_name);
                if (!moved_owner || moved_owner->moved) {
                    noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                                "cannot move unavailable owner '%.*s'",
                                (int)moved_name.count, moved_name.data);
                    goto done;
                }
                if (noc__owner_slice_equal(owner.name, moved_name)) {
                    noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                                "an owner cannot shadow-move itself in the MVP");
                    goto done;
                }
            } else if ((initializer_owner = noc__owner_range_reference(
                            &owners, &tokens, initializer)) != NULL) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "owner '%.*s' in an initializer must be transferred "
                            "with an exact move(owner)",
                            (int)initializer_owner->name.count,
                            initializer_owner->name.data);
                goto done;
            } else if (noc__owner_range_has_feature_call(&tokens, initializer)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "ownership calls in an initializer must be exactly move(owner)");
                goto done;
            }
            if (!noc_buffer_append_slice(&replacement, owner.type) ||
                !noc_buffer_append_cstr(&replacement, " ") ||
                !noc_buffer_append_slice(&replacement, owner.name) ||
                !noc_buffer_append_cstr(&replacement, " = ")) goto allocation_failed;
            if (is_move) {
                if (!noc_buffer_append_slice(&replacement, moved_name) ||
                    !noc_buffer_append_cstr(&replacement, "; ") ||
                    !noc_buffer_append_slice(&replacement, moved_name) ||
                    !noc_buffer_append_cstr(&replacement, " = 0")) {
                    goto allocation_failed;
                }
                moved_owner->moved = true;
            } else {
                Noc_Slice initializer_source =
                    noc_token_range_source(&tokens, initializer);
                if (!noc_buffer_append_slice(&replacement, initializer_source)) {
                    goto allocation_failed;
                }
            }
            if (!noc__owner_emit_cleanup(&replacement, &owner) ||
                !noc_edit_set_add(&edits,
                                  &tokens,
                                  (Noc_Token_Range){index, semicolon + 1},
                                  (Noc_Slice){replacement.items,
                                              replacement.count}) ||
                !noc__owner_append(&owners, owner)) {
                goto allocation_failed;
            }
            noc_buffer_free(&replacement);
            index = semicolon + 1;
            continue;

allocation_failed:
            noc_buffer_free(&replacement);
            noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                        "out of memory while lowering owner '%.*s'",
                        (int)owner.name.count, owner.name.data);
            goto done;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(token, "borrow")) {
            size_t open = noc__dialect_next_significant(&tokens, index + 1);
            size_t close = open == NOC_TOKEN_INDEX_NONE
                               ? NOC_TOKEN_INDEX_NONE
                               : noc__dialect_matching_close(&tokens, open, "(", ")");
            Noc_Slice name;
            Noc__Owner *owner;
            if (open == NOC_TOKEN_INDEX_NONE || close == NOC_TOKEN_INDEX_NONE ||
                !noc__owner_is_punct(&tokens, open, "(") ||
                !noc__owner_range_identifier(
                    &tokens,
                    noc__dialect_trim_range(
                        &tokens, (Noc_Token_Range){open + 1, close}),
                    &name)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "expected borrow(owner)");
                goto done;
            }
            owner = noc__owner_find(&owners, name);
            if (!owner || owner->moved) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "cannot borrow unavailable owner '%.*s'",
                            (int)name.count, name.data);
                goto done;
            }
            if (!noc_edit_set_add(&edits,
                                  &tokens,
                                  (Noc_Token_Range){index, close + 1},
                                  name)) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            "out of memory while lowering borrow");
                goto done;
            }
            index = close + 1;
            continue;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(token, "return")) {
            size_t semicolon = noc__dialect_statement_end(&tokens, index + 1);
            Noc_Token_Range expression = semicolon == NOC_TOKEN_INDEX_NONE
                                             ? (Noc_Token_Range){0, 0}
                                             : noc__dialect_trim_range(
                                                   &tokens,
                                                   (Noc_Token_Range){index + 1,
                                                                     semicolon});
            Noc_Slice name;
            if (semicolon != NOC_TOKEN_INDEX_NONE &&
                noc__owner_exact_call(&tokens, expression, "move", &name)) {
                Noc__Owner *owner = noc__owner_find(&owners, name);
                Noc_Buffer replacement = {0};
                char temporary[64];
                if (!owner || owner->moved) {
                    noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                                "cannot return-move unavailable owner '%.*s'",
                                (int)name.count, name.data);
                    goto done;
                }
                if (!noc__owner_temporary(&tokens,
                                          &temporary_counter,
                                          temporary) ||
                    !noc_buffer_append_cstr(&replacement, "{ ") ||
                    !noc_buffer_append_slice(&replacement, owner->type) ||
                    !noc_buffer_appendf(&replacement, " %s = ", temporary) ||
                    !noc_buffer_append_slice(&replacement, name) ||
                    !noc_buffer_append_cstr(&replacement, "; ") ||
                    !noc_buffer_append_slice(&replacement, name) ||
                    !noc_buffer_appendf(&replacement,
                                        " = 0; return %s; }",
                                        temporary) ||
                    !noc_edit_set_add(&edits,
                                      &tokens,
                                      (Noc_Token_Range){index, semicolon + 1},
                                      (Noc_Slice){replacement.items,
                                                  replacement.count})) {
                    noc_buffer_free(&replacement);
                    noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                                "out of memory while lowering return move");
                    goto done;
                }
                noc_buffer_free(&replacement);
                owner->moved = true;
                index = semicolon + 1;
                continue;
            }
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(token, "move")) {
            noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                        "move(owner) is supported only in an own initializer "
                        "or as the complete return expression");
            goto done;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER) {
            Noc__Owner *owner = noc__owner_find(&owners, token.text);
            if (owner) {
                noc__report(context, NOC_DIAGNOSTIC_ERROR, token.location,
                            owner->moved
                                ? "use of moved owner '%.*s'"
                                : "owned value '%.*s' must be used through borrow() or move()",
                            (int)owner->name.count, owner->name.data);
                goto done;
            }
        }
        index += 1;
    }
    if (!noc__dialect_finish_edits(&edits, &tokens, source, output)) {
        Noc_Location none = {0};
        noc__report(context, NOC_DIAGNOSTIC_ERROR, none,
                    "out of memory while applying ownership lowering");
        goto done;
    }
    ok = true;

done:
    free(owners.items);
    noc_edit_set_free(&edits);
    noc_preprocessor_map_free(&activity);
    noc__dialect_tokens_free(&tokens);
    if (!ok) noc_buffer_free(output);
    return ok;
}
#endif
#endif
