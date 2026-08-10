#include "test_support.h"

typedef struct {
    size_t calls;
} Syntax_Answer_State;

static size_t previous_significant(const Noc_Token_Stream *stream, size_t index)
{
    while (index > 0) {
        index -= 1;
        if (!noc_token_is_trivia(stream->items[index])) return index;
    }
    return NOC_TOKEN_INDEX_NONE;
}

static size_t matching_open_paren(const Noc_Token_Stream *stream, size_t close)
{
    size_t depth = 0;
    size_t index = close + 1;
    while (index > 0) {
        index -= 1;
        if (noc_token_is_punct(stream->items[index], ")")) {
            depth += 1;
        } else if (noc_token_is_punct(stream->items[index], "(")) {
            if (depth == 0) return NOC_TOKEN_INDEX_NONE;
            depth -= 1;
            if (depth == 0) return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

/* This adapter demonstrates that `generic(T)` is project syntax rather than a
   spelling known by the template lowerer. It discovers the following ordinary
   C function name and emits the current canonical template header. */
static bool expand_generic(Noc_Rewriter *rewriter,
                           const Noc_Rule *rule,
                           void *user_data)
{
    const Noc_Token_Stream *stream = noc_rw_token_stream(rewriter);
    Noc_Token_Range remaining;
    Noc_Slice parameter;
    size_t index;
    size_t parens = 0;
    size_t brackets = 0;
    size_t body = NOC_TOKEN_INDEX_NONE;
    size_t parameter_close;
    size_t parameter_open;
    size_t name;
    (void)rule;
    (void)user_data;
    if (!noc_rw_capture_balanced(rewriter, "(", ")", &parameter)) return false;
    remaining = noc_rw_remaining_range(rewriter);
    for (index = remaining.begin; index < remaining.end; ++index) {
        Noc_Token token = stream->items[index];
        if (noc_token_is_punct(token, "(")) parens += 1;
        else if (noc_token_is_punct(token, ")")) {
            if (parens == 0) break;
            parens -= 1;
        } else if (noc_token_is_punct(token, "[")) brackets += 1;
        else if (noc_token_is_punct(token, "]")) {
            if (brackets == 0) break;
            brackets -= 1;
        } else if (parens == 0 && brackets == 0 &&
                   noc_token_is_punct(token, "{")) {
            body = index;
            break;
        } else if (parens == 0 && brackets == 0 &&
                   noc_token_is_punct(token, ";")) {
            break;
        }
    }
    parameter_close = body == NOC_TOKEN_INDEX_NONE
                          ? NOC_TOKEN_INDEX_NONE
                          : previous_significant(stream, body);
    parameter_open = parameter_close == NOC_TOKEN_INDEX_NONE ||
                             !noc_token_is_punct(stream->items[parameter_close], ")")
                         ? NOC_TOKEN_INDEX_NONE
                         : matching_open_paren(stream, parameter_close);
    name = parameter_open == NOC_TOKEN_INDEX_NONE
               ? NOC_TOKEN_INDEX_NONE
               : previous_significant(stream, parameter_open);
    if (name == NOC_TOKEN_INDEX_NONE ||
        stream->items[name].kind != NOC_TOKEN_IDENTIFIER) {
        noc_rw_error(rewriter,
                     "generic(type) must be followed by a C function definition");
        return false;
    }
    return noc_rw_add_dependency(rewriter, "syntax/shared.h") &&
           noc_rw_emit_cstr(rewriter, "template(") &&
           noc_rw_emit_slice(rewriter, parameter) &&
           noc_rw_emit_cstr(rewriter, ", ") &&
           noc_rw_emit_slice(rewriter, stream->items[name].text) &&
           noc_rw_emit_cstr(rewriter, ")");
}

/* Project syntax: `specialize identity for int as identity_int;`. The adapter
   owns this grammar and lowers it to the canonical template instance record. */
static bool expand_specialize(Noc_Rewriter *rewriter,
                              const Noc_Rule *rule,
                              void *user_data)
{
    const Noc_Token_Stream *stream = noc_rw_token_stream(rewriter);
    Noc_Token_Range remaining;
    Noc_Token template_name;
    Noc_Token generated_name;
    Noc_Token_Range type_range;
    Noc_Slice type;
    size_t index;
    size_t parens = 0;
    size_t brackets = 0;
    size_t as_token = NOC_TOKEN_INDEX_NONE;
    (void)rule;
    (void)user_data;
    if (!noc_rw_expect_identifier(rewriter, NULL, &template_name) ||
        !noc_rw_expect_identifier(rewriter, "for", NULL)) return false;
    remaining = noc_rw_remaining_range(rewriter);
    for (index = remaining.begin; index < remaining.end; ++index) {
        Noc_Token token = stream->items[index];
        if (noc_token_is_punct(token, "(")) parens += 1;
        else if (noc_token_is_punct(token, ")")) {
            if (parens == 0) break;
            parens -= 1;
        } else if (noc_token_is_punct(token, "[")) brackets += 1;
        else if (noc_token_is_punct(token, "]")) {
            if (brackets == 0) break;
            brackets -= 1;
        } else if (parens == 0 && brackets == 0 &&
                   noc_token_is_identifier(token, "as")) {
            as_token = index;
            break;
        }
    }
    if (as_token == NOC_TOKEN_INDEX_NONE) {
        noc_rw_error(rewriter, "specialize requires 'as generated_name'");
        return false;
    }
    type_range = (Noc_Token_Range){remaining.begin, as_token};
    type = noc_token_range_source(stream, type_range);
    if (!type.data || !noc_rw_consume_range(rewriter, type_range) ||
        !noc_rw_expect_identifier(rewriter, "as", NULL) ||
        !noc_rw_expect_identifier(rewriter, NULL, &generated_name) ||
        !noc_rw_expect_punct(rewriter, ";")) return false;
    return noc_rw_emit_cstr(rewriter, "instantiate(") &&
           noc_rw_emit_slice(rewriter, template_name.text) &&
           noc_rw_emit_cstr(rewriter, ", ") &&
           noc_rw_emit_slice(rewriter, type) &&
           noc_rw_emit_cstr(rewriter, ", ") &&
           noc_rw_emit_slice(rewriter, generated_name.text) &&
           noc_rw_emit_cstr(rewriter, ");");
}

static bool expand_later(Noc_Rewriter *rewriter,
                         const Noc_Rule *rule,
                         void *user_data)
{
    Noc_Slice action;
    (void)rule;
    (void)user_data;
    return noc_rw_capture_balanced(rewriter, "(", ")", &action) &&
           noc_rw_expect_punct(rewriter, ";") &&
           noc_rw_add_dependency(rewriter, "syntax/cleanup.h") &&
           noc_rw_emit_cstr(rewriter, "defer ") &&
           noc_rw_emit_slice(rewriter, action) &&
           noc_rw_emit_cstr(rewriter, ";");
}

static bool expand_answer(Noc_Rewriter *rewriter,
                          const Noc_Rule *rule,
                          void *user_data)
{
    (void)rule;
    (void)user_data;
    return noc_rw_add_dependency(rewriter, "syntax/shared.h") &&
           noc_rw_add_dependency(rewriter, "output/answer.h") &&
           noc_rw_emit_cstr(rewriter, "7");
}

static bool reject_bare_answer(Noc_Rewriter *rewriter,
                               const Noc_Rule *rule,
                               void *user_data)
{
    Syntax_Answer_State *state = (Syntax_Answer_State *)user_data;
    (void)rule;
    state->calls += 1;
    return noc_rw_emit_cstr(rewriter, "99");
}

static void test_surface_rules_feed_structured_lowering(void)
{
    static const char source[] =
        "generic(T)\n"
        "T identity(T value) { return value; }\n"
        "specialize identity for int as identity_int;\n"
        "void cleanup(void);\n"
        "int run(void) { later(cleanup()); "
        "return identity_int(@ /* phase gap */ answer); }\n";
    Noc_Rule generic_rule = {
        "generic-syntax", NOC_RULE_DECLARATION, "generic(T) C-function-definition",
        "Map project generic syntax to the canonical template IR.",
        expand_generic, NULL,
    };
    Noc_Rule specialize_rule = {
        "specialize-syntax", NOC_RULE_DECLARATION,
        "specialize template for type as generated_name;",
        "Map project specialization syntax to the canonical instance IR.",
        expand_specialize, NULL,
    };
    Noc_Rule later_rule = {
        "later-syntax", NOC_RULE_STATEMENT, "later(cleanup);",
        "Map project cleanup syntax to defer.", expand_later, NULL,
    };
    Noc_Rule answer_rule = {
        "answer", NOC_RULE_EXPRESSION, "@answer", "Emit seven.",
        expand_answer, NULL,
    };
    Syntax_Answer_State syntax_answer = {0};
    Noc_Rule bare_answer_rule = {
        "bare-answer-syntax", NOC_RULE_EXPRESSION, "answer",
        "Must not consume the name inside an output-phase @answer trigger.",
        reject_bare_answer, &syntax_answer,
    };
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_TEMPLATES, true));
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_DEFER, true));
    CHECK(noc_register_rule_pattern_in_phase(&context, NOC_RULE_PHASE_SYNTAX,
                                             "generic", generic_rule));
    CHECK(noc_register_rule_pattern_in_phase(&context, NOC_RULE_PHASE_SYNTAX,
                                             "specialize", specialize_rule));
    CHECK(noc_register_rule_pattern_in_phase(&context, NOC_RULE_PHASE_SYNTAX,
                                             "later", later_rule));
    CHECK(noc_register_rule_pattern_in_phase(&context, NOC_RULE_PHASE_SYNTAX,
                                             "answer", bare_answer_rule));
    CHECK(noc_register_rule(&context, answer_rule));
    CHECK(noc_rule_phase(&context, noc_slice_from_cstr("generic-syntax")) ==
          NOC_RULE_PHASE_SYNTAX);
    CHECK(noc_rule_phase(&context, noc_slice_from_cstr("answer")) ==
          NOC_RULE_PHASE_OUTPUT);
    CHECK(strcmp(noc_rule_phase_name(NOC_RULE_PHASE_SYNTAX), "syntax") == 0);
    CHECK(strcmp(noc_rule_phase_name(NOC_RULE_PHASE_OUTPUT), "output") == 0);

    CHECK(noc_transform_source(&context, "phases.c", source, sizeof(source) - 1,
                               &result));
    CHECK(strstr(result.output, "generic(") == NULL);
    CHECK(strstr(result.output, "specialize ") == NULL);
    CHECK(strstr(result.output, "template(") == NULL);
    CHECK(strstr(result.output, "instantiate(") == NULL);
    CHECK(strstr(result.output, "later(") == NULL);
    CHECK(strstr(result.output, "defer ") == NULL);
    CHECK(strstr(result.output, "int identity_int(int value)") != NULL);
    CHECK(strstr(result.output, "identity_int(7)") != NULL);
    CHECK(strstr(result.output, "cleanup();") != NULL);
    CHECK(syntax_answer.calls == 0);
    CHECK(result.dependency_count == 3);
    CHECK(strcmp(result.dependencies[0], "syntax/shared.h") == 0);
    CHECK(strcmp(result.dependencies[1], "syntax/cleanup.h") == 0);
    CHECK(strcmp(result.dependencies[2], "output/answer.h") == 0);
    check_complete_generated_c("generated-rule-phases.c",
                               result.output, result.output_count);
    noc_transform_result_free(&result);
    CHECK(!noc_transform_source(&context, "unknown-output-rule.c",
                                "int value = @missing;",
                                sizeof("int value = @missing;") - 1,
                                &result));
    CHECK(result.output == NULL && result.dependencies == NULL);
    CHECK(strstr(diagnostics.last_message, "unknown dialect rule") != NULL);
    noc_context_deinit(&context);
}

static void test_phase_validation(void)
{
    Noc_Context context;
    Diagnostic_State diagnostics = {0};
    Noc_Rule rule = {
        "invalid-phase", NOC_RULE_TOKEN, "invalid", "Invalid phase test.",
        expand_answer, NULL,
    };
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_register_rule_in_phase(&context, NOC_RULE_PHASE_COUNT, rule));
    CHECK(diagnostics.errors == 1);
    CHECK(strstr(diagnostics.last_message, "invalid phase") != NULL);
    CHECK(noc_rule_phase(&context, noc_slice_from_cstr("missing")) ==
          NOC_RULE_PHASE_COUNT);
    CHECK(strcmp(noc_rule_phase_name(NOC_RULE_PHASE_COUNT), "unknown") == 0);
    noc_context_deinit(&context);
}

static void test_syntax_at_name_passthrough_stays_opaque(void)
{
    static const char source[] = "int value = @ /* phase gap */ syntax_hold;\n";
    Noc_Rule syntax_rule = {
        "syntax_hold", NOC_RULE_EXPRESSION, "@syntax_hold",
        "A disabled syntax rule retained for another tool.", expand_answer, NULL,
    };
    Syntax_Answer_State output_bare = {0};
    Noc_Rule output_bare_rule = {
        "bare-syntax-hold-output", NOC_RULE_EXPRESSION, "syntax_hold",
        "Must not consume an opaque syntax-phase at-name trigger.",
        reject_bare_answer, &output_bare,
    };
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule_in_phase(&context, NOC_RULE_PHASE_SYNTAX,
                                     syntax_rule));
    CHECK(noc_register_rule_pattern(&context, "syntax_hold", output_bare_rule));
    CHECK(noc_set_rule_enabled(&context,
                               noc_slice_from_cstr("syntax_hold"), false));
    context.options.disabled_rule_is_error = false;
    CHECK(context.options.unknown_rule_is_error);

    CHECK(noc_transform_source(&context, "syntax-passthrough.c",
                               source, sizeof(source) - 1, &result));
    CHECK(strcmp(result.output, source) == 0);
    CHECK(output_bare.calls == 0);
    CHECK(diagnostics.errors == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

int main(void)
{
    test_surface_rules_feed_structured_lowering();
    test_phase_validation();
    test_syntax_at_name_passthrough_stays_opaque();
    return finish_suite("rule-phases");
}
