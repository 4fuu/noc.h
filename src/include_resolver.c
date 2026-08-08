#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_INCLUDE_RESOLVER_IMPLEMENTATION_INCLUDED
#define NOC_INCLUDE_RESOLVER_IMPLEMENTATION_INCLUDED

NOCDEF const char *noc_include_operand_status_name(
    Noc_Include_Operand_Status status)
{
    switch (status) {
    case NOC_INCLUDE_OPERAND_DIRECT: return "direct";
    case NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED: return "expansion-required";
    case NOC_INCLUDE_OPERAND_EMPTY: return "empty";
    case NOC_INCLUDE_OPERAND_MISSING: return "missing";
    case NOC_INCLUDE_OPERAND_MALFORMED: return "malformed";
    case NOC_INCLUDE_OPERAND_INCOMPLETE: return "incomplete";
    }
    return "unknown";
}

NOCDEF const char *noc_include_form_name(Noc_Include_Form form)
{
    switch (form) {
    case NOC_INCLUDE_FORM_NONE: return "none";
    case NOC_INCLUDE_FORM_QUOTED: return "quoted";
    case NOC_INCLUDE_FORM_ANGLED: return "angled";
    }
    return "unknown";
}

NOCDEF const char *noc_include_operand_build_status_name(
    Noc_Include_Operand_Build_Status status)
{
    switch (status) {
    case NOC_INCLUDE_OPERAND_BUILD_OK: return "ok";
    case NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_INCLUDE_OPERAND_BUILD_STALE: return "stale";
    case NOC_INCLUDE_OPERAND_BUILD_GENERATION_EXHAUSTED:
        return "generation-exhausted";
    case NOC_INCLUDE_OPERAND_BUILD_OUT_OF_MEMORY: return "out-of-memory";
    }
    return "unknown";
}

NOCDEF const char *noc_include_resolve_status_name(
    Noc_Include_Resolve_Status status)
{
    switch (status) {
    case NOC_INCLUDE_RESOLVE_FOUND: return "found";
    case NOC_INCLUDE_RESOLVE_NOT_FOUND: return "not-found";
    case NOC_INCLUDE_RESOLVE_AMBIGUOUS: return "ambiguous";
    case NOC_INCLUDE_RESOLVE_DENIED: return "denied";
    case NOC_INCLUDE_RESOLVE_CANCELLED: return "cancelled";
    case NOC_INCLUDE_RESOLVE_FAILED: return "failed";
    case NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_INCLUDE_RESOLVE_STALE: return "stale";
    case NOC_INCLUDE_RESOLVE_OUT_OF_MEMORY: return "out-of-memory";
    case NOC_INCLUDE_RESOLVE_INVALID_RESULT: return "invalid-result";
    }
    return "unknown";
}

NOCDEF void noc_include_operand_free(Noc_Include_Operand *operand)
{
    size_t generation;
    if (!operand) return;
    generation = operand->generation;
    free((void *)operand->logical_name.data);
    memset(operand, 0, sizeof(*operand));
    operand->generation = generation;
}

static bool noc__include_form_is_valid(Noc_Include_Form form)
{
    return form == NOC_INCLUDE_FORM_QUOTED ||
           form == NOC_INCLUDE_FORM_ANGLED;
}

NOCDEF bool noc_include_operand_is_valid(const Noc_Include_Operand *operand)
{
    const Noc_Preprocessor_Directive *directive;
    Noc_Token_Range body;
    if (!operand || operand->generation == 0 ||
        !noc_preprocessor_unit_is_valid(operand->unit) ||
        operand->unit_stream_generation != operand->unit->stream.generation ||
        operand->directive_index >= operand->unit->count) {
        return false;
    }
    directive = &operand->unit->items[operand->directive_index];
    body = noc_preprocessor_directive_body_tokens(operand->unit,
                                                  operand->directive_index);
    if (directive->kind != NOC_PREPROCESSOR_DIRECTIVE_INCLUDE ||
        body.begin != operand->body_tokens.begin ||
        body.end != operand->body_tokens.end ||
        (operand->problem_token_index != NOC_TOKEN_INDEX_NONE &&
         (body.begin == NOC_TOKEN_INDEX_NONE ||
          operand->problem_token_index < body.begin ||
          operand->problem_token_index >= body.end))) {
        return false;
    }
    if (operand->status == NOC_INCLUDE_OPERAND_DIRECT ||
        operand->status == NOC_INCLUDE_OPERAND_EMPTY) {
        if (!noc__include_form_is_valid(operand->form) ||
            operand->header_token_index == NOC_TOKEN_INDEX_NONE ||
            operand->header_token_index < body.begin ||
            operand->header_token_index >= body.end ||
            operand->unit->preprocessing_tokens[
                operand->header_token_index].token.kind != NOC_TOKEN_HEADER_NAME) {
            return false;
        }
        if (operand->status == NOC_INCLUDE_OPERAND_DIRECT) {
            return operand->logical_name.data && operand->logical_name.count > 0 &&
                   operand->problem_token_index == NOC_TOKEN_INDEX_NONE;
        }
        return !operand->logical_name.data && operand->logical_name.count == 0 &&
               operand->problem_token_index == operand->header_token_index;
    }
    return operand->status >= NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED &&
           operand->status <= NOC_INCLUDE_OPERAND_INCOMPLETE &&
           operand->form == NOC_INCLUDE_FORM_NONE &&
           operand->header_token_index == NOC_TOKEN_INDEX_NONE &&
           !operand->logical_name.data && operand->logical_name.count == 0;
}

/* 1 is decoded, 0 is an allocation failure, and -1 is malformed input. */
NOC__PRIVATE int noc__include_decode_header(Noc_Token token,
                                            Noc_Include_Form *form,
                                            Noc_Slice *name)
{
    Noc_Buffer logical = {0};
    size_t name_count;
    char open;
    char close;
    if (!noc_token_logical_text(token, &logical)) return 0;
    if (logical.count < 2) goto malformed;
    open = logical.items[0];
    close = logical.items[logical.count - 1];
    if (open == '"' && close == '"') {
        *form = NOC_INCLUDE_FORM_QUOTED;
    } else if (open == '<' && close == '>') {
        *form = NOC_INCLUDE_FORM_ANGLED;
    } else {
        goto malformed;
    }
    name_count = logical.count - 2;
    if (name_count == 0) {
        noc_buffer_free(&logical);
        return 1;
    }
    memmove(logical.items, logical.items + 1, name_count);
    logical.items[name_count] = '\0';
    name->data = logical.items;
    name->count = name_count;
    return 1;

malformed:
    noc_buffer_free(&logical);
    return -1;
}

NOCDEF Noc_Include_Operand_Build_Status noc_include_operand_build(
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index,
    Noc_Include_Operand *output)
{
    Noc_Include_Operand parsed;
    Noc_Token_Range body;
    size_t significant_count = 0;
    size_t first = NOC_TOKEN_INDEX_NONE;
    size_t second = NOC_TOKEN_INDEX_NONE;
    size_t invalid = NOC_TOKEN_INDEX_NONE;
    size_t generation;
    size_t index;
    if (!unit || !output) {
        return NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT;
    }
    if (!noc_preprocessor_unit_is_valid(unit)) {
        return NOC_INCLUDE_OPERAND_BUILD_STALE;
    }
    if (directive_index >= unit->count) {
        return NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT;
    }
    if (unit->items[directive_index].kind !=
        NOC_PREPROCESSOR_DIRECTIVE_INCLUDE) {
        return NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_INCLUDE_OPERAND_BUILD_GENERATION_EXHAUSTED;
    }
    memset(&parsed, 0, sizeof(parsed));
    parsed.unit = unit;
    parsed.unit_stream_generation = unit->stream.generation;
    parsed.directive_index = directive_index;
    parsed.header_token_index = NOC_TOKEN_INDEX_NONE;
    parsed.problem_token_index = NOC_TOKEN_INDEX_NONE;
    parsed.form = NOC_INCLUDE_FORM_NONE;
    body = noc_preprocessor_directive_body_tokens(unit, directive_index);
    parsed.body_tokens = body;
    if (body.begin == NOC_TOKEN_INDEX_NONE) {
        parsed.status = NOC_INCLUDE_OPERAND_MISSING;
    } else {
        for (index = body.begin; index < body.end; ++index) {
            Noc_Token token = unit->preprocessing_tokens[index].token;
            if (noc_token_is_trivia(token)) continue;
            if (first == NOC_TOKEN_INDEX_NONE) {
                first = index;
            } else if (second == NOC_TOKEN_INDEX_NONE) {
                second = index;
            }
            significant_count += 1;
            if (invalid == NOC_TOKEN_INDEX_NONE &&
                token.kind == NOC_TOKEN_INVALID) {
                invalid = index;
            }
        }
        if (invalid != NOC_TOKEN_INDEX_NONE) {
            parsed.status = NOC_INCLUDE_OPERAND_INCOMPLETE;
            parsed.problem_token_index = invalid;
        } else if (significant_count == 1 &&
                   unit->preprocessing_tokens[first].token.kind ==
                       NOC_TOKEN_HEADER_NAME) {
            int decoded = noc__include_decode_header(
                unit->preprocessing_tokens[first].token,
                &parsed.form,
                &parsed.logical_name);
            if (decoded == 0) {
                return NOC_INCLUDE_OPERAND_BUILD_OUT_OF_MEMORY;
            }
            if (decoded < 0) {
                parsed.status = NOC_INCLUDE_OPERAND_MALFORMED;
                parsed.form = NOC_INCLUDE_FORM_NONE;
                parsed.problem_token_index = first;
            } else {
                parsed.header_token_index = first;
                if (parsed.logical_name.count == 0) {
                    parsed.status = NOC_INCLUDE_OPERAND_EMPTY;
                    parsed.problem_token_index = first;
                } else {
                    parsed.status = NOC_INCLUDE_OPERAND_DIRECT;
                }
            }
        } else if (unit->preprocessing_tokens[first].token.kind ==
                       NOC_TOKEN_HEADER_NAME &&
                   unit->preprocessing_tokens[second].token.kind !=
                       NOC_TOKEN_IDENTIFIER) {
            parsed.status = NOC_INCLUDE_OPERAND_MALFORMED;
            parsed.problem_token_index = second;
        } else {
            parsed.status = NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED;
            parsed.problem_token_index =
                unit->preprocessing_tokens[first].token.kind ==
                        NOC_TOKEN_HEADER_NAME
                    ? second
                    : first;
        }
    }
    generation = output->generation + 1;
    noc_include_operand_free(output);
    *output = parsed;
    output->generation = generation;
    return NOC_INCLUDE_OPERAND_BUILD_OK;
}

NOC__PRIVATE Noc_Include_Resolve_Status noc__include_resolve_request(
    Noc_Include_Resolver resolver,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *resolved_snapshot)
{
    Noc_Document_Snapshot resolved = {0};
    Noc_Include_Resolve_Status status;
    if (!resolver.resolve || !request || !resolved_snapshot) {
        return NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT;
    }
    status = resolver.resolve(resolver.user_data, request, &resolved);
    if (status == NOC_INCLUDE_RESOLVE_FOUND) {
        if (!noc_document_snapshot_is_valid(&resolved)) {
            return NOC_INCLUDE_RESOLVE_INVALID_RESULT;
        }
        noc_document_snapshot_free(resolved_snapshot);
        *resolved_snapshot = resolved;
        return NOC_INCLUDE_RESOLVE_FOUND;
    }
    if (noc_document_snapshot_is_valid(&resolved)) {
        noc_document_snapshot_free(&resolved);
        return NOC_INCLUDE_RESOLVE_INVALID_RESULT;
    }
    if (status == NOC_INCLUDE_RESOLVE_NOT_FOUND ||
        status == NOC_INCLUDE_RESOLVE_AMBIGUOUS ||
        status == NOC_INCLUDE_RESOLVE_DENIED ||
        status == NOC_INCLUDE_RESOLVE_CANCELLED ||
        status == NOC_INCLUDE_RESOLVE_FAILED ||
        status == NOC_INCLUDE_RESOLVE_OUT_OF_MEMORY) {
        return status;
    }
    return NOC_INCLUDE_RESOLVE_INVALID_RESULT;
}

NOCDEF Noc_Include_Resolve_Status noc_include_resolve(
    Noc_Include_Resolver resolver,
    const Noc_Include_Operand *operand,
    Noc_Document_Snapshot *resolved_snapshot)
{
    Noc_Include_Request request;
    const Noc_Preprocessor_Directive *directive;
    if (!resolver.resolve || !operand || !resolved_snapshot) {
        return NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT;
    }
    if (!noc_include_operand_is_valid(operand)) {
        if (operand->unit &&
            (!noc_preprocessor_unit_is_valid(operand->unit) ||
             operand->unit_stream_generation !=
                 operand->unit->stream.generation)) {
            return NOC_INCLUDE_RESOLVE_STALE;
        }
        return NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT;
    }
    if (operand->status != NOC_INCLUDE_OPERAND_DIRECT) {
        return NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT;
    }
    directive = &operand->unit->items[operand->directive_index];
    memset(&request, 0, sizeof(request));
    request.including_path = operand->unit->stream.path;
    request.including_file_id = operand->unit->file_id;
    request.including_document_generation = operand->unit->document_generation;
    request.including_source_class = operand->unit->source_class;
    request.directive_index = operand->directive_index;
    request.directive_location = directive->location;
    request.form = operand->form;
    request.logical_name = operand->logical_name;
    return noc__include_resolve_request(resolver, &request, resolved_snapshot);
}

#endif /* NOC_INCLUDE_RESOLVER_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
