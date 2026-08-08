#ifdef NOC_IMPLEMENTATION
#ifndef NOC_PREPROCESSOR_IMPLEMENTATION_INCLUDED
#define NOC_PREPROCESSOR_IMPLEMENTATION_INCLUDED

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

NOCDEF void noc_preprocessor_unit_free(Noc_Preprocessor_Unit *unit)
{
    if (!unit) return;
    free(unit->items);
    noc_token_stream_free(&unit->stream);
    memset(unit, 0, sizeof(*unit));
}

NOCDEF bool noc_preprocessor_unit_is_valid(const Noc_Preprocessor_Unit *unit)
{
    return unit && noc_token_stream_is_valid(&unit->stream) &&
           unit->file_id != NOC_FILE_ID_NONE && unit->document_generation != 0 &&
           noc__source_class_is_valid(unit->source_class) &&
           noc__macro_policy_is_valid(unit->macro_policy) &&
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
    if (!noc_tokenize(context,
                      noc_document_snapshot_path(snapshot),
                      source.data,
                      source.count,
                      &parsed.stream)) {
        return false;
    }
    parsed.file_id = noc_document_snapshot_file_id(snapshot);
    parsed.document_generation = noc_document_snapshot_generation(snapshot);
    parsed.source_class = noc_document_snapshot_source_class(snapshot);
    parsed.macro_policy = macro_policy;
    for (index = 0; index < parsed.stream.count; ++index) {
        Noc_Token token = parsed.stream.items[index];
        Noc_Preprocessor_Directive directive;
        if (token.kind != NOC_TOKEN_PREPROCESSOR) continue;
        noc__preprocessor_inventory_parse(token,
                                          index,
                                          macro_policy,
                                          parsed.source_class,
                                          &directive);
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
#endif /* NOC_IMPLEMENTATION */
