#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_INCLUDE_EXPANSION_IMPLEMENTATION_INCLUDED
#define NOC_INCLUDE_EXPANSION_IMPLEMENTATION_INCLUDED

static bool noc__include_expansion_form_is_valid(Noc_Include_Form form)
{
    return form == NOC_INCLUDE_FORM_QUOTED ||
           form == NOC_INCLUDE_FORM_ANGLED;
}

NOCDEF void noc_include_expansion_free(Noc_Include_Expansion *expansion)
{
    size_t generation;
    if (!expansion) return;
    generation = expansion->generation;
    noc_macro_expansion_free(&expansion->macro_expansion);
    free((void *)expansion->logical_name.data);
    memset(expansion, 0, sizeof(*expansion));
    expansion->generation = generation;
}

NOCDEF bool noc_include_expansion_is_valid(
    const Noc_Include_Expansion *expansion)
{
    const Noc_Preprocessor_Unit *unit;
    Noc_Token_Range body;
    bool has_header;
    if (!expansion || expansion->generation == 0 ||
        !noc_macro_expansion_is_valid(&expansion->macro_expansion)) {
        return false;
    }
    unit = expansion->macro_expansion.input_unit;
    if (expansion->directive_index >= unit->count ||
        unit->items[expansion->directive_index].kind !=
            NOC_PREPROCESSOR_DIRECTIVE_INCLUDE) {
        return false;
    }
    body = noc_preprocessor_directive_body_tokens(unit,
                                                  expansion->directive_index);
    if (body.begin != expansion->body_tokens.begin ||
        body.end != expansion->body_tokens.end) {
        return false;
    }
    has_header = expansion->header_tokens.begin != NOC_TOKEN_INDEX_NONE;
    if (has_header) {
        if (expansion->header_tokens.begin >= expansion->header_tokens.end ||
            expansion->header_tokens.end > expansion->macro_expansion.count ||
            !noc__include_expansion_form_is_valid(expansion->form)) {
            return false;
        }
    } else if (expansion->header_tokens.end != NOC_TOKEN_INDEX_NONE ||
               expansion->form != NOC_INCLUDE_FORM_NONE) {
        return false;
    }
    if (expansion->status == NOC_INCLUDE_OPERAND_DIRECT) {
        return has_header && expansion->logical_name.data &&
               expansion->logical_name.count > 0 &&
               expansion->logical_name.data[expansion->logical_name.count] == '\0' &&
               expansion->problem_token_index == NOC_TOKEN_INDEX_NONE;
    }
    if (expansion->status == NOC_INCLUDE_OPERAND_EMPTY) {
        return !expansion->logical_name.data &&
               expansion->logical_name.count == 0 &&
               ((!has_header &&
                 expansion->problem_token_index == NOC_TOKEN_INDEX_NONE) ||
                (has_header && expansion->problem_token_index ==
                                   expansion->header_tokens.begin));
    }
    return (expansion->status == NOC_INCLUDE_OPERAND_MALFORMED ||
            expansion->status == NOC_INCLUDE_OPERAND_INCOMPLETE) &&
           !has_header && !expansion->logical_name.data &&
           expansion->logical_name.count == 0 &&
           expansion->problem_token_index < expansion->macro_expansion.count;
}

static bool noc__include_expansion_input_trivia_survives(
    const Noc_Macro_Expansion *macro,
    size_t token_index)
{
    size_t begin = token_index;
    size_t end = token_index + 1;
    size_t index;
    while (begin > 0 &&
           noc_token_is_trivia(macro->items[begin - 1].token)) {
        begin -= 1;
    }
    while (end < macro->count &&
           noc_token_is_trivia(macro->items[end].token)) {
        end += 1;
    }
    for (index = begin; index < end; ++index) {
        if (macro->items[index].origin != NOC_MACRO_EXPANSION_TOKEN_INPUT) {
            return false;
        }
    }
    return begin > 0 && end < macro->count &&
           macro->items[begin - 1].origin == NOC_MACRO_EXPANSION_TOKEN_INPUT &&
           macro->items[end].origin == NOC_MACRO_EXPANSION_TOKEN_INPUT;
}

/* Appends phase-2 logical bytes for one angle-header token. Retained C comments
   semantically become one space in translation phase 3; preserving that
   distinction here keeps filesystem policy out of token provenance. */
static int noc__include_expansion_append_piece(
    Noc_Buffer *name,
    const Noc_Macro_Expansion *macro,
    size_t token_index)
{
    Noc_Buffer logical = {0};
    bool appended;
    const Noc_Macro_Expansion_Token *expanded = &macro->items[token_index];
    Noc_Token token = expanded->token;
    if (expanded->origin == NOC_MACRO_EXPANSION_TOKEN_INPUT &&
        noc_token_is_trivia(token)) {
        if (!noc__include_expansion_input_trivia_survives(macro, token_index)) {
            return 1;
        }
        if (token_index > 0 &&
            noc_token_is_trivia(macro->items[token_index - 1].token)) {
            return 1;
        }
        return noc_buffer_append(name, " ", 1) ? 1 : 0;
    }
    if (token.kind == NOC_TOKEN_LINE_COMMENT ||
        token.kind == NOC_TOKEN_BLOCK_COMMENT) {
        return noc_buffer_append(name, " ", 1) ? 1 : 0;
    }
    if (token.kind == NOC_TOKEN_NEWLINE) return -1;
    if (!noc_token_logical_text(token, &logical)) return 0;
    appended = noc_buffer_append(name, logical.items, logical.count);
    noc_buffer_free(&logical);
    return appended ? 1 : 0;
}

/* Parses a completed macro expansion. Returns 1 on a recoverable syntax result
   and 0 only when logical-name allocation fails. */
static int noc__include_expansion_parse(Noc_Include_Expansion *parsed)
{
    Noc_Macro_Expansion *macro = &parsed->macro_expansion;
    size_t first = NOC_TOKEN_INDEX_NONE;
    size_t second = NOC_TOKEN_INDEX_NONE;
    size_t invalid = NOC_TOKEN_INDEX_NONE;
    size_t significant_count = 0;
    size_t index;
    for (index = 0; index < macro->count; ++index) {
        Noc_Token token = macro->items[index].token;
        if (token.kind == NOC_TOKEN_INVALID && invalid == NOC_TOKEN_INDEX_NONE) {
            invalid = index;
        }
        if (noc_token_is_trivia(token)) continue;
        if (first == NOC_TOKEN_INDEX_NONE) {
            first = index;
        } else if (second == NOC_TOKEN_INDEX_NONE) {
            second = index;
        }
        significant_count += 1;
    }
    if (invalid != NOC_TOKEN_INDEX_NONE) {
        parsed->status = NOC_INCLUDE_OPERAND_INCOMPLETE;
        parsed->problem_token_index = invalid;
        return 1;
    }
    if (significant_count == 0) {
        parsed->status = NOC_INCLUDE_OPERAND_EMPTY;
        return 1;
    }
    if (macro->items[first].token.kind == NOC_TOKEN_HEADER_NAME ||
        macro->items[first].token.kind == NOC_TOKEN_STRING) {
        int decoded;
        if (significant_count != 1) {
            parsed->status = NOC_INCLUDE_OPERAND_MALFORMED;
            parsed->problem_token_index = second;
            return 1;
        }
        decoded = noc__include_decode_header(macro->items[first].token,
                                             &parsed->form,
                                             &parsed->logical_name);
        if (decoded == 0) return 0;
        if (decoded < 0) {
            parsed->form = NOC_INCLUDE_FORM_NONE;
            parsed->status = NOC_INCLUDE_OPERAND_MALFORMED;
            parsed->problem_token_index = first;
            return 1;
        }
        parsed->header_tokens.begin = first;
        parsed->header_tokens.end = first + 1;
        if (parsed->logical_name.count == 0) {
            parsed->status = NOC_INCLUDE_OPERAND_EMPTY;
            parsed->problem_token_index = first;
        } else {
            parsed->status = NOC_INCLUDE_OPERAND_DIRECT;
        }
        return 1;
    }
    if (noc_token_is_punct(macro->items[first].token, "<")) {
        Noc_Buffer name = {0};
        size_t close = NOC_TOKEN_INDEX_NONE;
        size_t after_close = NOC_TOKEN_INDEX_NONE;
        for (index = first + 1; index < macro->count; ++index) {
            Noc_Token token = macro->items[index].token;
            if (noc_token_is_punct(token, ">")) {
                close = index;
                break;
            }
        }
        if (close == NOC_TOKEN_INDEX_NONE) {
            parsed->status = NOC_INCLUDE_OPERAND_INCOMPLETE;
            parsed->problem_token_index = first;
            return 1;
        }
        for (index = close + 1; index < macro->count; ++index) {
            if (!noc_token_is_trivia(macro->items[index].token)) {
                after_close = index;
                break;
            }
        }
        if (after_close != NOC_TOKEN_INDEX_NONE) {
            parsed->status = NOC_INCLUDE_OPERAND_MALFORMED;
            parsed->problem_token_index = after_close;
            return 1;
        }
        for (index = first + 1; index < close; ++index) {
            int appended = noc__include_expansion_append_piece(
                &name, macro, index);
            if (appended == 0) {
                noc_buffer_free(&name);
                return 0;
            }
            if (appended < 0) {
                noc_buffer_free(&name);
                parsed->status = NOC_INCLUDE_OPERAND_MALFORMED;
                parsed->problem_token_index = index;
                return 1;
            }
        }
        parsed->header_tokens.begin = first;
        parsed->header_tokens.end = close + 1;
        parsed->form = NOC_INCLUDE_FORM_ANGLED;
        if (name.count == 0) {
            noc_buffer_free(&name);
            parsed->status = NOC_INCLUDE_OPERAND_EMPTY;
            parsed->problem_token_index = first;
        } else {
            if (!noc_buffer_terminate(&name)) {
                noc_buffer_free(&name);
                return 0;
            }
            parsed->logical_name.data = name.items;
            parsed->logical_name.count = name.count;
            parsed->status = NOC_INCLUDE_OPERAND_DIRECT;
        }
        return 1;
    }
    parsed->status = NOC_INCLUDE_OPERAND_MALFORMED;
    parsed->problem_token_index = first;
    return 1;
}

NOCDEF Noc_Macro_Expansion_Status noc_include_expansion_build_with_options(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Include_Operand *operand,
    Noc_Macro_Expansion_Options options,
    Noc_Include_Expansion *output)
{
    Noc_Include_Expansion parsed;
    Noc_Macro_Expansion_Status status;
    size_t generation;
    if (!environment || !operand || !output ||
        entry_limit > environment->count ||
        !noc__macro_expansion_options_are_valid(options)) {
        return NOC_MACRO_EXPANSION_INVALID_ARGUMENT;
    }
    if (!noc_include_operand_is_valid(operand)) {
        if (operand->unit &&
            (!noc_preprocessor_unit_is_valid(operand->unit) ||
             operand->unit_stream_generation !=
                 operand->unit->stream.generation)) {
            return NOC_MACRO_EXPANSION_STALE;
        }
        return NOC_MACRO_EXPANSION_INVALID_ARGUMENT;
    }
    if (!noc_macro_environment_is_valid(environment)) {
        return NOC_MACRO_EXPANSION_STALE;
    }
    if (operand->status != NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED) {
        return NOC_MACRO_EXPANSION_INVALID_ARGUMENT;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_MACRO_EXPANSION_GENERATION_EXHAUSTED;
    }
    memset(&parsed, 0, sizeof(parsed));
    parsed.body_tokens = operand->body_tokens;
    parsed.directive_index = operand->directive_index;
    parsed.header_tokens.begin = NOC_TOKEN_INDEX_NONE;
    parsed.header_tokens.end = NOC_TOKEN_INDEX_NONE;
    parsed.problem_token_index = NOC_TOKEN_INDEX_NONE;
    parsed.form = NOC_INCLUDE_FORM_NONE;
    status = noc_macro_expansion_build_with_options(environment,
                                                    entry_limit,
                                                    operand->unit,
                                                    operand->body_tokens,
                                                    options,
                                                    &parsed.macro_expansion);
    if (status != NOC_MACRO_EXPANSION_OK) return status;
    if (!noc__include_expansion_parse(&parsed)) {
        noc_include_expansion_free(&parsed);
        return NOC_MACRO_EXPANSION_OUT_OF_MEMORY;
    }
    generation = output->generation + 1;
    noc_include_expansion_free(output);
    *output = parsed;
    output->generation = generation;
    return NOC_MACRO_EXPANSION_OK;
}

NOCDEF Noc_Macro_Expansion_Status noc_include_expansion_build(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Include_Operand *operand,
    Noc_Macro_Expansion_Limits limits,
    Noc_Include_Expansion *output)
{
    Noc_Macro_Expansion_Options options = noc_macro_expansion_default_options();
    options.limits = limits;
    return noc_include_expansion_build_with_options(environment,
                                                    entry_limit,
                                                    operand,
                                                    options,
                                                    output);
}

static bool noc__include_expansion_borrowed_state_is_stale(
    const Noc_Include_Expansion *expansion)
{
    const Noc_Macro_Expansion *macro;
    size_t index;
    if (!expansion || expansion->generation == 0) return false;
    macro = &expansion->macro_expansion;
    if (!noc_macro_environment_is_valid(macro->environment) ||
        macro->environment_generation != macro->environment->generation ||
        macro->environment_entry_count != macro->environment->count ||
        !noc_preprocessor_unit_is_valid(macro->input_unit) ||
        macro->input_unit_stream_generation !=
            macro->input_unit->stream.generation) {
        return true;
    }
    for (index = 0; index < macro->count; ++index) {
        if (!noc_preprocessor_unit_is_valid(macro->items[index].unit) ||
            macro->items[index].unit_stream_generation !=
                macro->items[index].unit->stream.generation) {
            return true;
        }
    }
    for (index = 0; index < macro->frame_count; ++index) {
        if (!noc_preprocessor_unit_is_valid(
                macro->frames[index].invocation_unit) ||
            macro->frames[index].invocation_unit_stream_generation !=
                macro->frames[index].invocation_unit->stream.generation) {
            return true;
        }
    }
    return false;
}

NOCDEF Noc_Include_Resolve_Status noc_include_expansion_resolve(
    Noc_Include_Resolver resolver,
    const Noc_Include_Expansion *expansion,
    Noc_Document_Snapshot *resolved_snapshot)
{
    const Noc_Preprocessor_Unit *unit;
    const Noc_Preprocessor_Directive *directive;
    Noc_Include_Request request;
    if (!resolver.resolve || !expansion || !resolved_snapshot) {
        return NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT;
    }
    if (!noc_include_expansion_is_valid(expansion)) {
        return noc__include_expansion_borrowed_state_is_stale(expansion)
                   ? NOC_INCLUDE_RESOLVE_STALE
                   : NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT;
    }
    if (expansion->status != NOC_INCLUDE_OPERAND_DIRECT) {
        return NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT;
    }
    unit = expansion->macro_expansion.input_unit;
    directive = &unit->items[expansion->directive_index];
    memset(&request, 0, sizeof(request));
    request.including_path = unit->stream.path;
    request.including_file_id = unit->file_id;
    request.including_document_generation = unit->document_generation;
    request.including_source_class = unit->source_class;
    request.directive_index = expansion->directive_index;
    request.directive_location = directive->location;
    request.form = expansion->form;
    request.logical_name = expansion->logical_name;
    return noc__include_resolve_request(resolver, &request, resolved_snapshot);
}

#endif /* NOC_INCLUDE_EXPANSION_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
