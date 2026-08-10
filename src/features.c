#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_FEATURES_IMPLEMENTATION_INCLUDED
#define NOC_FEATURES_IMPLEMENTATION_INCLUDED

static const char *noc__diagnostic_name(Noc_Diagnostic_Severity severity)
{
    switch (severity) {
    case NOC_DIAGNOSTIC_NOTE: return "note";
    case NOC_DIAGNOSTIC_WARNING: return "warning";
    case NOC_DIAGNOSTIC_ERROR: return "error";
    }
    return "diagnostic";
}

static void noc__default_diagnostic(void *user_data, const Noc_Diagnostic *diagnostic)
{
    FILE *stream = user_data ? (FILE *)user_data : stderr;
    if (diagnostic->location.path) {
        fprintf(stream,
                "%s:%zu:%zu: %s: %s\n",
                diagnostic->location.path,
                diagnostic->location.line,
                diagnostic->location.column,
                noc__diagnostic_name(diagnostic->severity),
                diagnostic->message);
    } else {
        fprintf(stream,
                "noc: %s: %s\n",
                noc__diagnostic_name(diagnostic->severity),
                diagnostic->message);
    }
}

NOC__PRIVATE void noc__reportv(Noc_Context *context,
                         Noc_Diagnostic_Severity severity,
                         Noc_Location location,
                         const char *format,
                         va_list arguments)
{
    Noc_Buffer message = {0};
    Noc_Diagnostic diagnostic;
    if (!noc__buffer_appendfv(&message, format, arguments) ||
        !noc_buffer_terminate(&message)) {
        static const char allocation_failure[] = "out of memory while formatting diagnostic";
        diagnostic.severity = NOC_DIAGNOSTIC_ERROR;
        diagnostic.location = location;
        diagnostic.message = allocation_failure;
    } else {
        diagnostic.severity = severity;
        diagnostic.location = location;
        diagnostic.message = message.items;
    }
    if (severity == NOC_DIAGNOSTIC_ERROR) context->error_count += 1;
    context->diagnostic(context->diagnostic_user_data, &diagnostic);
    noc_buffer_free(&message);
}

NOC__PRIVATE void noc__report(Noc_Context *context,
                        Noc_Diagnostic_Severity severity,
                        Noc_Location location,
                        const char *format,
                        ...)
{
    va_list arguments;
    va_start(arguments, format);
    noc__reportv(context, severity, location, format, arguments);
    va_end(arguments);
}

NOCDEF void noc_context_init(Noc_Context *context)
{
    memset(context, 0, sizeof(*context));
    context->diagnostic = noc__default_diagnostic;
    context->options.emit_line_directives = true;
    context->options.unknown_rule_is_error = true;
    context->options.disabled_rule_is_error = true;
}

NOCDEF void noc_context_deinit(Noc_Context *context)
{
    free(context->rules);
    free(context->rule_patterns);
    free(context->rule_enabled);
    free(context->rule_phases);
    memset(context, 0, sizeof(*context));
}

NOCDEF const char *noc_feature_name(Noc_Feature feature)
{
    static const char *names[] = {"defer", "templates", "ownership"};
    return (unsigned)feature < NOC_FEATURE_COUNT ? names[(unsigned)feature] : "unknown";
}

NOCDEF bool noc_feature_is_enabled(const Noc_Context *context, Noc_Feature feature)
{
    return context && (unsigned)feature < NOC_FEATURE_COUNT &&
           (context->enabled_features & (UINT32_C(1) << (unsigned)feature)) != 0;
}

NOCDEF bool noc_set_feature_enabled(Noc_Context *context, Noc_Feature feature, bool enabled)
{
    Noc_Location none = {0};
    if (!context || (unsigned)feature >= NOC_FEATURE_COUNT) return false;
    if (context->active_transforms) {
        noc__report(context, NOC_DIAGNOSTIC_ERROR, none,
                    "cannot change feature enable state during an active transform");
        return false;
    }
    if (!enabled && feature == NOC_FEATURE_DEFER &&
        noc_feature_is_enabled(context, NOC_FEATURE_OWNERSHIP)) {
        noc__report(context, NOC_DIAGNOSTIC_ERROR, none,
                    "cannot disable defer while ownership is enabled");
        return false;
    }
    if (enabled && feature == NOC_FEATURE_OWNERSHIP)
        context->enabled_features |= UINT32_C(1) << NOC_FEATURE_DEFER;
    if (enabled) context->enabled_features |= UINT32_C(1) << (unsigned)feature;
    else context->enabled_features &= ~(UINT32_C(1) << (unsigned)feature);
    return true;
}

NOCDEF bool noc_enable_mvp_features(Noc_Context *context)
{
    return noc_set_feature_enabled(context, NOC_FEATURE_DEFER, true) &&
           noc_set_feature_enabled(context, NOC_FEATURE_TEMPLATES, true) &&
           noc_set_feature_enabled(context, NOC_FEATURE_OWNERSHIP, true);
}

NOCDEF void noc_context_set_diagnostic(Noc_Context *context,
                                       Noc_Diagnostic_Fn diagnostic,
                                       void *user_data)
{
    context->diagnostic = diagnostic ? diagnostic : noc__default_diagnostic;
    context->diagnostic_user_data = user_data;
}

NOCDEF const Noc_Rule *noc_find_rule(const Noc_Context *context, Noc_Slice name)
{
    size_t i;
    for (i = 0; i < context->rules_count; ++i) {
        if (noc_slice_equal_cstr(name, context->rules[i].name)) return &context->rules[i];
    }
    return NULL;
}

NOC__PRIVATE size_t noc__find_rule_token(const Noc_Context *context,
                                         Noc_Token token,
                                         Noc_Rule_Phase phase)
{
    size_t i;
    for (i = 0; i < context->rules_count; ++i) {
        if (context->rule_phases[i] == phase && !context->rule_patterns[i] &&
            noc_token_is_identifier(token, context->rules[i].name)) {
            return i;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

NOC__PRIVATE size_t noc__find_rule_token_any_phase(const Noc_Context *context,
                                                   Noc_Token token)
{
    size_t i;
    for (i = 0; i < context->rules_count; ++i) {
        if (!context->rule_patterns[i] &&
            noc_token_is_identifier(token, context->rules[i].name)) {
            return i;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static bool noc__pattern_has_trigraph(const char *pattern)
{
    while (*pattern) {
        if (pattern[0] == '?' && pattern[1] == '?' && pattern[2] != '\0' &&
            strchr("=/'()!<>-", pattern[2]) != NULL) {
            return true;
        }
        ++pattern;
    }
    return false;
}

static bool noc__patterns_equal(const char *left, const char *right)
{
    Noc_Lexer left_lexer;
    Noc_Lexer right_lexer;
    Noc_Token left_token;
    Noc_Token right_token;
    noc_lexer_init(&left_lexer, "<pattern>", left, strlen(left));
    noc_lexer_init(&right_lexer, "<pattern>", right, strlen(right));
    for (;;) {
        do {
            left_token = noc_lexer_next(&left_lexer);
        } while (noc_token_is_trivia(left_token));
        do {
            right_token = noc_lexer_next(&right_lexer);
        } while (noc_token_is_trivia(right_token));
        if (left_token.kind != right_token.kind) break;
        if (left_token.kind == NOC_TOKEN_EOF) return true;
        if (!noc__slices_logically_equal(left_token.text, right_token.text)) break;
    }
    return false;
}

static bool noc__register_rule(Noc_Context *context,
                               Noc_Rule_Phase phase,
                               const char *pattern,
                               Noc_Rule rule)
{
    Noc_Rule *rules = NULL;
    const char **patterns = NULL;
    bool *enabled = NULL;
    Noc_Rule_Phase *phases = NULL;
    size_t capacity;
    size_t i;
    Noc_Location no_location = {0};
    Noc_Lexer lexer;
    Noc_Token token;
    bool saw_token = false;
    if (context->active_transforms) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "cannot register a rule during an active transform");
        return false;
    }
    if ((unsigned)phase >= NOC_RULE_PHASE_COUNT) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "invalid phase while registering rule '%s'",
                    rule.name ? rule.name : "");
        return false;
    }
    if (!rule.name || !rule.name[0]) {
        noc__report(context, NOC_DIAGNOSTIC_ERROR, no_location, "rule name cannot be empty");
        return false;
    }
    if (!rule.expand) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "rule '%s' has no expansion callback",
                    rule.name);
        return false;
    }
    if (noc_find_rule(context, noc_slice_from_cstr(rule.name))) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "rule '%s' is already registered",
                    rule.name);
        return false;
    }
    if (pattern) {
        if (!pattern[0] || noc__pattern_has_trigraph(pattern)) goto invalid_pattern;
        noc_lexer_init(&lexer, "<rule-pattern>", pattern, strlen(pattern));
        for (;;) {
            token = noc_lexer_next(&lexer);
            if (token.kind == NOC_TOKEN_EOF) break;
            if (noc_token_is_trivia(token)) continue;
            if (!saw_token && noc_token_is_punct(token, "@")) goto invalid_pattern;
            saw_token = true;
            if (token.kind == NOC_TOKEN_INVALID ||
                token.kind == NOC_TOKEN_PREPROCESSOR) {
                goto invalid_pattern;
            }
        }
        if (!saw_token) goto invalid_pattern;
        for (i = 0; i < context->rules_count; ++i) {
            if (context->rule_phases[i] == phase && context->rule_patterns[i] &&
                noc__patterns_equal(pattern, context->rule_patterns[i])) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            no_location,
                            "rule pattern '%s' is already registered",
                            pattern);
                return false;
            }
        }
    }
    if (context->rules_count == context->rules_capacity) {
        if (context->rules_capacity > SIZE_MAX / 2) goto allocation_failed;
        capacity = context->rules_capacity ? context->rules_capacity * 2 : 8;
        if (capacity > SIZE_MAX / sizeof(*rules) ||
            capacity > SIZE_MAX / sizeof(*patterns) ||
            capacity > SIZE_MAX / sizeof(*enabled) ||
            capacity > SIZE_MAX / sizeof(*phases)) {
            goto allocation_failed;
        }
        rules = (Noc_Rule *)malloc(capacity * sizeof(*rules));
        patterns = (const char **)malloc(capacity * sizeof(*patterns));
        enabled = (bool *)malloc(capacity * sizeof(*enabled));
        phases = (Noc_Rule_Phase *)malloc(capacity * sizeof(*phases));
        if (!rules || !patterns || !enabled || !phases) {
allocation_failed:
            free(rules);
            free(patterns);
            free(enabled);
            free(phases);
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while registering rule '%s'",
                        rule.name);
            return false;
        }
        if (context->rules_count) {
            memcpy(rules, context->rules, context->rules_count * sizeof(*rules));
            memcpy(patterns,
                   context->rule_patterns,
                   context->rules_count * sizeof(*patterns));
            memcpy(enabled,
                   context->rule_enabled,
                   context->rules_count * sizeof(*enabled));
            memcpy(phases,
                   context->rule_phases,
                   context->rules_count * sizeof(*phases));
        }
        free(context->rules);
        free(context->rule_patterns);
        free(context->rule_enabled);
        free(context->rule_phases);
        context->rules = rules;
        context->rule_patterns = patterns;
        context->rule_enabled = enabled;
        context->rule_phases = phases;
        context->rules_capacity = capacity;
    }
    context->rules[context->rules_count] = rule;
    context->rule_patterns[context->rules_count] = pattern;
    context->rule_enabled[context->rules_count] = true;
    context->rule_phases[context->rules_count] = phase;
    context->rules_count += 1;
    return true;

invalid_pattern:
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "invalid rule pattern for rule '%s'",
                rule.name ? rule.name : "");
    return false;
}

NOCDEF bool noc_register_rule(Noc_Context *context, Noc_Rule rule)
{
    return noc__register_rule(context, NOC_RULE_PHASE_OUTPUT, NULL, rule);
}

NOCDEF bool noc_register_rule_pattern(Noc_Context *context,
                                      const char *pattern,
                                      Noc_Rule rule)
{
    if (!pattern) {
        Noc_Location none = {0};
        noc__report(context, NOC_DIAGNOSTIC_ERROR, none, "rule pattern cannot be NULL");
        return false;
    }
    return noc__register_rule(context, NOC_RULE_PHASE_OUTPUT, pattern, rule);
}

NOCDEF bool noc_register_rule_in_phase(Noc_Context *context,
                                       Noc_Rule_Phase phase,
                                       Noc_Rule rule)
{
    return noc__register_rule(context, phase, NULL, rule);
}

NOCDEF bool noc_register_rule_pattern_in_phase(Noc_Context *context,
                                               Noc_Rule_Phase phase,
                                               const char *pattern,
                                               Noc_Rule rule)
{
    if (!pattern) {
        Noc_Location none = {0};
        noc__report(context, NOC_DIAGNOSTIC_ERROR, none,
                    "rule pattern cannot be NULL");
        return false;
    }
    return noc__register_rule(context, phase, pattern, rule);
}

NOCDEF bool noc_rule_is_enabled(const Noc_Context *context, Noc_Slice name)
{
    size_t i;
    if (!name.data && name.count != 0) return false;
    for (i = 0; i < context->rules_count; ++i) {
        if (noc_slice_equal_cstr(name, context->rules[i].name)) {
            return context->rule_enabled[i];
        }
    }
    return false;
}

NOCDEF bool noc_set_rule_enabled(Noc_Context *context, Noc_Slice name, bool value)
{
    size_t i;
    Noc_Location none = {0};
    if (context->active_transforms) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    none,
                    "cannot change rule enable state during an active transform");
        return false;
    }
    if (!name.data && name.count != 0) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    none,
                    "rule name slice is invalid");
        return false;
    }
    for (i = 0; i < context->rules_count; ++i) {
        if (noc_slice_equal_cstr(name, context->rules[i].name)) {
            context->rule_enabled[i] = value;
            return true;
        }
    }
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                none,
                "unknown rule '%.*s%s'",
                (int)(name.count < 80 ? name.count : 80),
                name.data ? name.data : "",
                name.count > 80 ? "..." : "");
    return false;
}

NOCDEF const char *noc_rule_scope_name(Noc_Rule_Scope scope)
{
    switch (scope) {
    case NOC_RULE_TOKEN: return "token";
    case NOC_RULE_EXPRESSION: return "expression";
    case NOC_RULE_STATEMENT: return "statement";
    case NOC_RULE_DECLARATION: return "declaration";
    case NOC_RULE_ATTRIBUTE: return "attribute";
    case NOC_RULE_DIRECTIVE: return "directive";
    }
    return "unknown";
}

NOCDEF const char *noc_rule_phase_name(Noc_Rule_Phase phase)
{
    switch (phase) {
    case NOC_RULE_PHASE_OUTPUT: return "output";
    case NOC_RULE_PHASE_SYNTAX: return "syntax";
    case NOC_RULE_PHASE_COUNT: break;
    }
    return "unknown";
}

NOCDEF Noc_Rule_Phase noc_rule_phase(const Noc_Context *context, Noc_Slice name)
{
    size_t i;
    if (!context || (!name.data && name.count != 0)) return NOC_RULE_PHASE_COUNT;
    for (i = 0; i < context->rules_count; ++i) {
        if (noc_slice_equal_cstr(name, context->rules[i].name)) {
            return context->rule_phases[i];
        }
    }
    return NOC_RULE_PHASE_COUNT;
}

NOCDEF void noc_describe(const Noc_Context *context, FILE *stream)
{
    size_t i;
    fprintf(stream, "Project dialect (%zu rule%s):\n",
            context->rules_count,
            context->rules_count == 1 ? "" : "s");
    for (i = 0; i < context->rules_count; ++i) {
        const Noc_Rule *rule = &context->rules[i];
        fprintf(stream, "\n  %s%s [%s, %s]%s\n",
                context->rule_patterns[i] ? "" : "@",
                context->rule_patterns[i] ? context->rule_patterns[i] : rule->name,
                noc_rule_scope_name(rule->scope),
                noc_rule_phase_name(context->rule_phases[i]),
                context->rule_enabled[i] ? "" : " (disabled)");
        if (rule->syntax) fprintf(stream, "    Syntax: %s\n", rule->syntax);
        if (rule->description) fprintf(stream, "    %s\n", rule->description);
    }
}

static bool noc__is_c_identifier(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    if (!cursor || !(cursor[0] == '_' ||
                     (cursor[0] >= 'A' && cursor[0] <= 'Z') ||
                     (cursor[0] >= 'a' && cursor[0] <= 'z'))) {
        return false;
    }
    cursor += 1;
    while (*cursor) {
        if (!(*cursor == '_' || (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= '0' && *cursor <= '9'))) {
            return false;
        }
        cursor += 1;
    }
    return true;
}

NOCDEF bool noc_generate_ide_metadata_header(
    Noc_Context *context,
    const Noc_Ide_Metadata_Options *options,
    Noc_Buffer *output)
{
    const char *guard = options && options->include_guard
                            ? options->include_guard
                            : "NOC_IDE_METADATA_H_INCLUDED";
    const char *prefix = options && options->macro_prefix
                             ? options->macro_prefix
                             : "NOC_IDE";
    const char *dialect = options && options->dialect_name
                              ? options->dialect_name
                              : "project";
    bool omit_descriptions = options && options->omit_descriptions;
    Noc_Buffer generated = {0};
    Noc_Location no_location = {0};
    size_t i;
    if (!context || !output) return false;
    if (!noc__is_c_identifier(guard) || !noc__is_c_identifier(prefix)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "IDE metadata guard and macro prefix must be C identifiers");
        return false;
    }
    if (!noc_buffer_appendf(&generated,
                            "/* Generated by noc.h %s. Do not edit. */\n"
                            "#ifndef %s\n"
                            "#define %s\n\n"
                            "#define %s_SCHEMA_VERSION 4\n"
                            "#define %s_NOC_VERSION ",
                            NOC_VERSION,
                            guard,
                            guard,
                            prefix,
                            prefix) ||
        !noc__buffer_append_c_string(&generated, NOC_VERSION, strlen(NOC_VERSION)) ||
        !noc_buffer_appendf(&generated, "\n#define %s_DIALECT_NAME ", prefix) ||
        !noc__buffer_append_c_string(&generated, dialect, strlen(dialect)) ||
        !noc_buffer_appendf(&generated,
                            "\n#define %s_FEATURE_COUNT %d\n"
                            "#define %s_RULE_COUNT %zu\n",
                            prefix, NOC_FEATURE_COUNT,
                            prefix,
                            context->rules_count)) {
        goto failed;
    }
    for (i = 0; i < NOC_FEATURE_COUNT; ++i) {
        if (!noc_buffer_appendf(&generated, "\n#define %s_FEATURE_%zu_NAME \"%s\""
                                "\n#define %s_FEATURE_%zu_ENABLED %d\n",
                                prefix, i, noc_feature_name((Noc_Feature)i), prefix, i,
                                noc_feature_is_enabled(context, (Noc_Feature)i) ? 1 : 0))
            goto failed;
    }
    for (i = 0; i < context->rules_count; ++i) {
        const Noc_Rule *rule = &context->rules[i];
        const char *syntax = rule->syntax ? rule->syntax : "";
        const char *description = rule->description ? rule->description : "";
        const char *scope = noc_rule_scope_name(rule->scope);
        const char *pattern = context->rule_patterns[i];
        Noc_Buffer trigger = {0};
        if (pattern) {
            if (!noc_buffer_append_cstr(&trigger, pattern)) {
                noc_buffer_free(&trigger);
                goto failed;
            }
        } else if (!noc_buffer_appendf(&trigger, "@%s", rule->name)) {
            noc_buffer_free(&trigger);
            goto failed;
        }
        if (!noc_buffer_appendf(&generated, "\n#define %s_RULE_%zu_NAME ", prefix, i) ||
            !noc__buffer_append_c_string(&generated, rule->name, strlen(rule->name)) ||
            !noc_buffer_appendf(&generated,
                                "\n#define %s_RULE_%zu_TRIGGER_KIND %d"
                                "\n#define %s_RULE_%zu_TRIGGER_KIND_NAME \"%s\""
                                "\n#define %s_RULE_%zu_TRIGGER ",
                                prefix, i,
                                pattern ? NOC_RULE_TRIGGER_PATTERN
                                        : NOC_RULE_TRIGGER_AT_NAME,
                                prefix, i, pattern ? "pattern" : "at-name",
                                prefix, i) ||
            !noc__buffer_append_c_string(&generated, trigger.items, trigger.count) ||
            !noc_buffer_appendf(&generated, "\n#define %s_RULE_%zu_ENABLED %d",
                                prefix, i, context->rule_enabled[i] ? 1 : 0) ||
            !noc_buffer_appendf(&generated,
                                "\n#define %s_RULE_%zu_PHASE %d"
                                "\n#define %s_RULE_%zu_PHASE_NAME \"%s\""
                                "\n#define %s_RULE_%zu_SCOPE %d"
                                "\n#define %s_RULE_%zu_SCOPE_NAME ",
                                prefix,
                                i,
                                (int)context->rule_phases[i],
                                prefix,
                                i,
                                noc_rule_phase_name(context->rule_phases[i]),
                                prefix,
                                i,
                                (int)rule->scope,
                                prefix,
                                i) ||
            !noc__buffer_append_c_string(&generated, scope, strlen(scope)) ||
            !noc_buffer_appendf(&generated,
                                "\n#define %s_RULE_%zu_SYNTAX ",
                                prefix,
                                i) ||
            !noc__buffer_append_c_string(&generated, syntax, strlen(syntax))) {
            noc_buffer_free(&trigger);
            goto failed;
        }
        if (!omit_descriptions &&
            (!noc_buffer_appendf(&generated,
                                 "\n#define %s_RULE_%zu_DESCRIPTION ",
                                 prefix,
                                 i) ||
             !noc__buffer_append_c_string(&generated,
                                          description,
                                          strlen(description)))) {
            noc_buffer_free(&trigger);
            goto failed;
        }
        noc_buffer_free(&trigger);
        if (!noc_buffer_append_cstr(&generated, "\n")) goto failed;
    }
    if (!noc_buffer_appendf(&generated, "\n#endif /* %s */\n", guard) ||
        !noc_buffer_terminate(&generated)) {
        goto failed;
    }
    noc_buffer_free(output);
    *output = generated;
    return true;

failed:
    noc_buffer_free(&generated);
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "out of memory while generating IDE metadata header");
    return false;
}

NOC__PRIVATE bool noc__depfile_path_is_valid(const char *path)
{
    const unsigned char *cursor = (const unsigned char *)path;
    if (!cursor || !cursor[0]) return false;
    while (*cursor) {
        if (*cursor == '\n' || *cursor == '\r') return false;
        cursor += 1;
    }
    return true;
}

static bool noc__depfile_append_path(Noc_Buffer *output, const char *path)
{
    const unsigned char *cursor = (const unsigned char *)path;
    while (*cursor) {
        if (*cursor == '$') {
            if (!noc_buffer_append_cstr(output, "$$")) return false;
        } else {
            if ((*cursor == ' ' || *cursor == '\t' || *cursor == '#' ||
                 *cursor == ':' || *cursor == '\\') &&
                !noc_buffer_append(output, "\\", 1)) {
                return false;
            }
            if (!noc_buffer_append(output, cursor, 1)) return false;
        }
        cursor += 1;
    }
    return true;
}

NOCDEF bool noc_generate_depfile(Noc_Context *context,
                                 const char *target_path,
                                 const char *source_path,
                                 const Noc_Transform_Result *result,
                                 Noc_Buffer *output)
{
    Noc_Buffer generated = {0};
    Noc_Location no_location = {0};
    size_t i;
    if (!context || !result || !output) return false;
    if (!noc__depfile_path_is_valid(target_path) ||
        !noc__depfile_path_is_valid(source_path) ||
        (result->dependency_count > 0 && !result->dependencies)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "depfile paths must be non-empty and cannot contain newlines");
        return false;
    }
    for (i = 0; i < result->dependency_count; ++i) {
        if (!noc__depfile_path_is_valid(result->dependencies[i])) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "depfile paths must be non-empty and cannot contain newlines");
            return false;
        }
    }
    if (!noc__depfile_append_path(&generated, target_path) ||
        !noc_buffer_append_cstr(&generated, ": ") ||
        !noc__depfile_append_path(&generated, source_path)) {
        goto failed;
    }
    for (i = 0; i < result->dependency_count; ++i) {
        if (strcmp(result->dependencies[i], source_path) == 0) continue;
        if (!noc_buffer_append_cstr(&generated, " ") ||
            !noc__depfile_append_path(&generated, result->dependencies[i])) {
            goto failed;
        }
    }
    if (!noc_buffer_append_cstr(&generated, "\n") ||
        !noc_buffer_terminate(&generated)) {
        goto failed;
    }
    noc_buffer_free(output);
    *output = generated;
    return true;

failed:
    noc_buffer_free(&generated);
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "out of memory while generating depfile");
    return false;
}

NOCDEF bool noc_generate_command_signature(Noc_Context *context,
                                           const char *const *arguments,
                                           size_t argument_count,
                                           Noc_Buffer *output)
{
    Noc_Buffer generated = {0};
    Noc_Location no_location = {0};
    size_t i;
    if (!context || !output || (argument_count > 0 && !arguments)) return false;
    for (i = 0; i < argument_count; ++i) {
        if (!arguments[i]) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "command signature arguments cannot be NULL");
            return false;
        }
    }
    if (!noc_buffer_appendf(&generated,
                            "noc-command-signature 1\n"
                            "noc-version %zu:%s\n"
                            "arguments %zu\n",
                            strlen(NOC_VERSION),
                            NOC_VERSION,
                            argument_count)) {
        goto failed;
    }
    for (i = 0; i < argument_count; ++i) {
        size_t count = strlen(arguments[i]);
        if (!noc_buffer_appendf(&generated, "argument %zu:", count) ||
            !noc_buffer_append(&generated, arguments[i], count) ||
            !noc_buffer_append_cstr(&generated, "\n")) {
            goto failed;
        }
    }
    if (!noc_buffer_terminate(&generated)) goto failed;
    noc_buffer_free(output);
    *output = generated;
    return true;

failed:
    noc_buffer_free(&generated);
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "out of memory while generating command signature");
    return false;
}

#endif /* NOC_FEATURES_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
