#ifdef NOC_IMPLEMENTATION
#ifndef NOC_MACRO_DIRECTIVES_IMPLEMENTATION_INCLUDED
#define NOC_MACRO_DIRECTIVES_IMPLEMENTATION_INCLUDED

NOCDEF const char *noc_macro_directive_kind_name(Noc_Macro_Directive_Kind kind)
{
    switch (kind) {
    case NOC_MACRO_DIRECTIVE_DEFINE_OBJECT: return "object-define";
    case NOC_MACRO_DIRECTIVE_DEFINE_FUNCTION: return "function-define";
    case NOC_MACRO_DIRECTIVE_UNDEF: return "undef";
    }
    return "unknown";
}

NOCDEF const char *noc_macro_directive_status_name(
    Noc_Macro_Directive_Status status)
{
    switch (status) {
    case NOC_MACRO_DIRECTIVE_STATUS_VALID: return "valid";
    case NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE: return "incomplete";
    case NOC_MACRO_DIRECTIVE_STATUS_MALFORMED: return "malformed";
    }
    return "unknown";
}

static bool noc__macro_parameter_append(Noc_Preprocessor_Unit *unit,
                                        Noc_Macro_Parameter parameter)
{
    Noc_Macro_Parameter *items;
    size_t capacity;
    if (unit->macro_parameter_count < unit->macro_parameter_capacity) {
        unit->macro_parameters[unit->macro_parameter_count++] = parameter;
        return true;
    }
    if (unit->macro_parameter_capacity == 0) {
        capacity = 16;
    } else {
        if (unit->macro_parameter_capacity > SIZE_MAX / 2) return false;
        capacity = unit->macro_parameter_capacity * 2;
    }
    if (capacity > SIZE_MAX / sizeof(*items)) return false;
    items = (Noc_Macro_Parameter *)realloc(unit->macro_parameters,
                                           capacity * sizeof(*items));
    if (!items) return false;
    unit->macro_parameters = items;
    unit->macro_parameter_capacity = capacity;
    unit->macro_parameters[unit->macro_parameter_count++] = parameter;
    return true;
}

static bool noc__macro_directive_append(Noc_Preprocessor_Unit *unit,
                                        Noc_Macro_Directive directive)
{
    Noc_Macro_Directive *items;
    size_t capacity;
    if (unit->macro_directive_count < unit->macro_directive_capacity) {
        unit->macro_directives[unit->macro_directive_count++] = directive;
        return true;
    }
    if (unit->macro_directive_capacity == 0) {
        capacity = 16;
    } else {
        if (unit->macro_directive_capacity > SIZE_MAX / 2) return false;
        capacity = unit->macro_directive_capacity * 2;
    }
    if (capacity > SIZE_MAX / sizeof(*items)) return false;
    items = (Noc_Macro_Directive *)realloc(unit->macro_directives,
                                           capacity * sizeof(*items));
    if (!items) return false;
    unit->macro_directives = items;
    unit->macro_directive_capacity = capacity;
    unit->macro_directives[unit->macro_directive_count++] = directive;
    return true;
}

static size_t noc__preprocessing_next_significant(
    const Noc_Preprocessor_Unit *unit,
    size_t *cursor,
    size_t end)
{
    while (*cursor < end &&
           noc_token_is_trivia(unit->preprocessing_tokens[*cursor].token)) {
        *cursor += 1;
    }
    if (*cursor >= end) return NOC_TOKEN_INDEX_NONE;
    return (*cursor)++;
}

static Noc_Token_Range noc__preprocessing_trim_trivia(
    const Noc_Preprocessor_Unit *unit,
    Noc_Token_Range range)
{
    while (range.begin < range.end &&
           noc_token_is_trivia(unit->preprocessing_tokens[range.begin].token)) {
        range.begin += 1;
    }
    while (range.end > range.begin &&
           noc_token_is_trivia(unit->preprocessing_tokens[range.end - 1].token)) {
        range.end -= 1;
    }
    return range;
}

static bool noc__preprocessing_only_splices(const char *begin, const char *end)
{
    size_t count;
    size_t position = 0;
    if (begin > end) return false;
    count = (size_t)(end - begin);
    while (position < count) {
        size_t splice = noc__splice_length(begin, count, position);
        if (splice == 0) return false;
        position += splice;
    }
    return true;
}

static void noc__macro_directive_problem(Noc_Macro_Directive *macro,
                                         Noc_Macro_Directive_Status status,
                                         size_t token_index)
{
    macro->status = status;
    macro->problem_token_index = token_index;
}

static Noc_Macro_Directive_Status noc__macro_unexpected_status(
    const Noc_Preprocessor_Unit *unit,
    size_t token_index)
{
    return unit->preprocessing_tokens[token_index].token.kind == NOC_TOKEN_INVALID
               ? NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE
               : NOC_MACRO_DIRECTIVE_STATUS_MALFORMED;
}

static void noc__macro_replacement_range(Noc_Preprocessor_Unit *unit,
                                         size_t begin,
                                         size_t end,
                                         Noc_Macro_Directive *macro)
{
    size_t index;
    macro->replacement_tokens = noc__preprocessing_trim_trivia(
        unit,
        (Noc_Token_Range){begin, end});
    if (macro->status != NOC_MACRO_DIRECTIVE_STATUS_VALID) return;
    for (index = macro->replacement_tokens.begin;
         index < macro->replacement_tokens.end;
         ++index) {
        if (unit->preprocessing_tokens[index].token.kind == NOC_TOKEN_INVALID) {
            noc__macro_directive_problem(macro,
                                         NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
                                         index);
            return;
        }
    }
}

static void noc__macro_recover_function_close(Noc_Preprocessor_Unit *unit,
                                              size_t begin,
                                              size_t end,
                                              Noc_Macro_Directive *macro)
{
    size_t cursor = begin;
    size_t nesting = 0;
    size_t token_index;
    while ((token_index = noc__preprocessing_next_significant(unit,
                                                              &cursor,
                                                              end)) !=
           NOC_TOKEN_INDEX_NONE) {
        Noc_Token token = unit->preprocessing_tokens[token_index].token;
        if (noc_token_is_punct(token, "(")) {
            nesting += 1;
        } else if (noc_token_is_punct(token, ")")) {
            if (nesting != 0) {
                nesting -= 1;
                continue;
            }
            macro->parameter_tokens.end = token_index;
            noc__macro_replacement_range(unit, token_index + 1, end, macro);
            return;
        }
    }
}

static bool noc__macro_parse_function(Noc_Preprocessor_Unit *unit,
                                      size_t open_index,
                                      size_t end,
                                      Noc_Macro_Directive *macro)
{
    size_t cursor = open_index + 1;
    size_t token_index;
    Noc_Token_Range contents = noc__preprocessing_trim_trivia(
        unit,
        (Noc_Token_Range){open_index + 1, end});
    macro->kind = NOC_MACRO_DIRECTIVE_DEFINE_FUNCTION;
    macro->parameter_tokens.begin = open_index + 1;
    macro->parameter_tokens.end = contents.end;
    token_index = noc__preprocessing_next_significant(unit, &cursor, end);
    if (token_index == NOC_TOKEN_INDEX_NONE) {
        noc__macro_directive_problem(macro,
                                     NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
                                     open_index);
        return true;
    }
    if (noc_token_is_punct(unit->preprocessing_tokens[token_index].token, ")")) {
        macro->parameter_tokens.end = token_index;
        noc__macro_replacement_range(unit, token_index + 1, end, macro);
        return true;
    }
    for (;;) {
        Noc_Token token = unit->preprocessing_tokens[token_index].token;
        Noc_Macro_Parameter parameter;
        size_t separator;
        if (token.kind == NOC_TOKEN_INVALID) {
            noc__macro_directive_problem(macro,
                                         NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
                                         token_index);
            noc__macro_recover_function_close(unit, cursor, end, macro);
            return true;
        }
        if (token.kind == NOC_TOKEN_IDENTIFIER) {
            parameter.token_index = token_index;
            parameter.variadic = false;
        } else if (noc_token_is_punct(token, "...")) {
            parameter.token_index = token_index;
            parameter.variadic = true;
            macro->variadic = true;
        } else {
            noc__macro_directive_problem(macro,
                                         noc__macro_unexpected_status(unit,
                                                                      token_index),
                                         token_index);
            noc__macro_recover_function_close(unit, token_index, end, macro);
            return true;
        }
        if (!noc__macro_parameter_append(unit, parameter)) return false;
        macro->parameter_count += 1;
        separator = noc__preprocessing_next_significant(unit, &cursor, end);
        if (separator == NOC_TOKEN_INDEX_NONE) {
            noc__macro_directive_problem(macro,
                                         NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
                                         token_index);
            return true;
        }
        if (noc_token_is_punct(unit->preprocessing_tokens[separator].token, ")")) {
            macro->parameter_tokens.end = separator;
            noc__macro_replacement_range(unit, separator + 1, end, macro);
            return true;
        }
        if (parameter.variadic ||
            !noc_token_is_punct(unit->preprocessing_tokens[separator].token, ",")) {
            noc__macro_directive_problem(macro,
                                         noc__macro_unexpected_status(unit,
                                                                      separator),
                                         separator);
            noc__macro_recover_function_close(unit, separator, end, macro);
            return true;
        }
        token_index = noc__preprocessing_next_significant(unit, &cursor, end);
        if (token_index == NOC_TOKEN_INDEX_NONE) {
            noc__macro_directive_problem(macro,
                                         NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
                                         separator);
            return true;
        }
        if (noc_token_is_punct(unit->preprocessing_tokens[token_index].token,
                               ")")) {
            noc__macro_directive_problem(macro,
                                         NOC_MACRO_DIRECTIVE_STATUS_MALFORMED,
                                         token_index);
            noc__macro_recover_function_close(unit, token_index, end, macro);
            return true;
        }
    }
}

static bool noc__macro_parse_directive(Noc_Preprocessor_Unit *unit,
                                       Noc_Preprocessor_Directive *directive,
                                       size_t directive_index)
{
    Noc_Macro_Directive macro;
    size_t cursor = directive->preprocessing_tokens.begin;
    size_t end = directive->preprocessing_tokens.end;
    size_t keyword_index = NOC_TOKEN_INDEX_NONE;
    size_t name_index;
    size_t probe;
    size_t next_index;
    memset(&macro, 0, sizeof(macro));
    macro.kind = directive->kind == NOC_PREPROCESSOR_DIRECTIVE_UNDEF
                     ? NOC_MACRO_DIRECTIVE_UNDEF
                     : NOC_MACRO_DIRECTIVE_DEFINE_OBJECT;
    macro.status = NOC_MACRO_DIRECTIVE_STATUS_VALID;
    macro.directive_index = directive_index;
    macro.name_token_index = NOC_TOKEN_INDEX_NONE;
    macro.parameter_tokens.begin = NOC_TOKEN_INDEX_NONE;
    macro.parameter_tokens.end = NOC_TOKEN_INDEX_NONE;
    macro.replacement_tokens.begin = NOC_TOKEN_INDEX_NONE;
    macro.replacement_tokens.end = NOC_TOKEN_INDEX_NONE;
    macro.parameter_begin = unit->macro_parameter_count;
    macro.problem_token_index = NOC_TOKEN_INDEX_NONE;
    while (cursor < end) {
        if (unit->preprocessing_tokens[cursor].role ==
            NOC_PREPROCESSING_TOKEN_DIRECTIVE_KEYWORD) {
            keyword_index = cursor++;
            break;
        }
        cursor += 1;
    }
    if (keyword_index == NOC_TOKEN_INDEX_NONE) {
        noc__macro_directive_problem(&macro,
                                     NOC_MACRO_DIRECTIVE_STATUS_MALFORMED,
                                     directive->preprocessing_tokens.begin);
        goto append;
    }
    name_index = noc__preprocessing_next_significant(unit, &cursor, end);
    if (name_index == NOC_TOKEN_INDEX_NONE) {
        noc__macro_directive_problem(&macro,
                                     NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
                                     keyword_index);
        goto append;
    }
    macro.name_token_index = name_index;
    if (unit->preprocessing_tokens[name_index].token.kind != NOC_TOKEN_IDENTIFIER) {
        noc__macro_directive_problem(&macro,
                                     noc__macro_unexpected_status(unit, name_index),
                                     name_index);
        goto append;
    }
    probe = name_index + 1;
    next_index = noc__preprocessing_next_significant(unit, &probe, end);
    if (directive->kind == NOC_PREPROCESSOR_DIRECTIVE_UNDEF) {
        if (next_index != NOC_TOKEN_INDEX_NONE) {
            noc__macro_directive_problem(&macro,
                                         noc__macro_unexpected_status(unit,
                                                                      next_index),
                                         next_index);
        }
        goto append;
    }
    if (next_index != NOC_TOKEN_INDEX_NONE &&
        noc_token_is_punct(unit->preprocessing_tokens[next_index].token, "(") &&
        noc__preprocessing_only_splices(
            unit->preprocessing_tokens[name_index].token.text.data +
                unit->preprocessing_tokens[name_index].token.text.count,
            unit->preprocessing_tokens[next_index].token.text.data)) {
        if (!noc__macro_parse_function(unit, next_index, end, &macro)) return false;
    } else {
        noc__macro_replacement_range(unit, name_index + 1, end, &macro);
    }

append:
    directive->macro_directive_index = unit->macro_directive_count;
    if (macro.status != NOC_MACRO_DIRECTIVE_STATUS_VALID) {
        unit->invalid_macro_directive_count += 1;
    }
    return noc__macro_directive_append(unit, macro);
}

NOCDEF const Noc_Macro_Directive *noc_macro_directive_at(
    const Noc_Preprocessor_Unit *unit,
    size_t index)
{
    if (!noc_preprocessor_unit_is_valid(unit) ||
        index >= unit->macro_directive_count) {
        return NULL;
    }
    return &unit->macro_directives[index];
}

NOCDEF const Noc_Macro_Parameter *noc_macro_parameter_at(
    const Noc_Preprocessor_Unit *unit,
    const Noc_Macro_Directive *directive,
    size_t index)
{
    size_t directive_index;
    size_t parameter_index;
    bool belongs_to_unit = false;
    if (!noc_preprocessor_unit_is_valid(unit) || !directive) return NULL;
    for (directive_index = 0;
         directive_index < unit->macro_directive_count;
         ++directive_index) {
        if (&unit->macro_directives[directive_index] == directive) {
            belongs_to_unit = true;
            break;
        }
    }
    if (!belongs_to_unit || index >= directive->parameter_count ||
        directive->parameter_begin > SIZE_MAX - index) {
        return NULL;
    }
    parameter_index = directive->parameter_begin + index;
    if (parameter_index >= unit->macro_parameter_count) return NULL;
    return &unit->macro_parameters[parameter_index];
}

NOCDEF bool noc_preprocessor_unit_validate_macro_directives(
    Noc_Context *context,
    const Noc_Preprocessor_Unit *unit)
{
    bool valid = true;
    size_t index;
    if (!context || !noc_preprocessor_unit_is_valid(unit)) return false;
    for (index = 0; index < unit->macro_directive_count; ++index) {
        const Noc_Macro_Directive *macro = &unit->macro_directives[index];
        const Noc_Preprocessor_Directive *directive;
        Noc_Location location;
        if (macro->status == NOC_MACRO_DIRECTIVE_STATUS_VALID) continue;
        directive = &unit->items[macro->directive_index];
        location = directive->location;
        if (macro->problem_token_index != NOC_TOKEN_INDEX_NONE &&
            macro->problem_token_index < unit->preprocessing_token_count) {
            location = unit->preprocessing_tokens[
                macro->problem_token_index].token.location;
        }
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "%s #%s macro directive",
                    noc_macro_directive_status_name(macro->status),
                    noc_preprocessor_directive_kind_name(directive->kind));
        valid = false;
    }
    return valid;
}

#endif /* NOC_MACRO_DIRECTIVES_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION */
