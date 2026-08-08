#include "test_support.h"

typedef struct {
    char first[32];
    char second[32];
} Dependency_Paths;

typedef struct {
    const char *replacement;
    const char *expected_trigger;
    const char *expected_next;
    size_t calls;
    bool preserve_trigger_newlines;
} Pattern_State;

typedef struct {
    Noc_Context *context;
    Noc_Rule legacy_rule;
    Noc_Rule pattern_rule;
    bool registered_legacy;
    bool registered_pattern;
    bool changed_enabled;
} Mutation_State;

static bool expand_twice(Noc_Rewriter *rewriter,
                         const Noc_Rule *rule,
                         void *user_data)
{
    Noc_Slice expression;
    (void)rule;
    (void)user_data;
    if (!noc_rw_capture_balanced(rewriter, "(", ")", &expression)) return false;
    return noc_rw_emit_cstr(rewriter, "((") &&
           noc_rw_emit_slice(rewriter, expression) &&
           noc_rw_emit_cstr(rewriter, ") + (") &&
           noc_rw_emit_slice(rewriter, expression) &&
           noc_rw_emit_cstr(rewriter, "))");
}

static bool expand_optional(Noc_Rewriter *rewriter,
                            const Noc_Rule *rule,
                            void *user_data)
{
    (void)rule;
    (void)user_data;
    CHECK(!noc_rw_match_punct(rewriter, "("));
    return noc_rw_emit_cstr(rewriter, "long");
}

static bool expand_pattern_text(Noc_Rewriter *rewriter,
                                const Noc_Rule *rule,
                                void *user_data)
{
    Pattern_State *state = (Pattern_State *)user_data;
    Noc_Token_Range trigger = noc_rw_trigger_range(rewriter);
    const Noc_Token_Stream *stream = noc_rw_token_stream(rewriter);
    Noc_Slice trigger_source = noc_token_range_source(stream, trigger);
    const Noc_Token *next;
    (void)rule;
    state->calls += 1;
    CHECK(stream != NULL);
    CHECK(trigger.begin != NOC_TOKEN_INDEX_NONE && trigger.end > trigger.begin);
    if (state->expected_trigger) {
        CHECK(slice_equals(trigger_source, state->expected_trigger));
    }
    if (state->expected_next) {
        next = noc_rw_peek_raw(rewriter, 0);
        CHECK(next != NULL && noc_token_is_trivia(*next));
        next = noc_rw_peek(rewriter, 0);
        CHECK(next != NULL && noc_token_is_identifier(*next, state->expected_next));
    }
    if (state->preserve_trigger_newlines &&
        !noc_rw_preserve_newlines(rewriter, trigger_source)) {
        return false;
    }
    return noc_rw_emit_cstr(rewriter, state->replacement);
}

static bool expand_registry_mutation(Noc_Rewriter *rewriter,
                                     const Noc_Rule *rule,
                                     void *user_data)
{
    Mutation_State *state = (Mutation_State *)user_data;
    state->registered_legacy = noc_register_rule(state->context, state->legacy_rule);
    state->registered_pattern =
        noc_register_rule_pattern(state->context, "late", state->pattern_rule);
    state->changed_enabled = noc_set_rule_enabled(state->context,
                                                  noc_slice_from_cstr(rule->name),
                                                  false);
    return noc_rw_emit_cstr(rewriter, "mutated");
}

static bool expand_failure(Noc_Rewriter *rewriter,
                           const Noc_Rule *rule,
                           void *user_data)
{
    (void)rewriter;
    (void)rule;
    (void)user_data;
    return false;
}

static bool expand_question_bytes(Noc_Rewriter *rewriter,
                                  const Noc_Rule *rule,
                                  void *user_data)
{
    static const char bytes[] = {'?', '?', '/'};
    (void)rule;
    (void)user_data;
    return noc_rw_expect_punct(rewriter, "(") &&
           noc_rw_expect_punct(rewriter, ")") &&
           noc_rw_emit_c_string(rewriter, bytes, sizeof(bytes));
}

static bool expand_dependencies(Noc_Rewriter *rewriter,
                                const Noc_Rule *rule,
                                void *user_data)
{
    Dependency_Paths *paths = (Dependency_Paths *)user_data;
    (void)rule;
    return noc_rw_expect_punct(rewriter, "(") &&
           noc_rw_expect_punct(rewriter, ")") &&
           noc_rw_add_dependency(rewriter, paths->first) &&
           noc_rw_add_dependency(rewriter, paths->second) &&
           noc_rw_add_dependency(rewriter, paths->first) &&
           noc_rw_emit_cstr(rewriter, "0");
}

static bool expand_dependency_then_error(Noc_Rewriter *rewriter,
                                         const Noc_Rule *rule,
                                         void *user_data)
{
    Noc_Context *context = (Noc_Context *)user_data;
    Noc_Token_Stream invalid = {0};
    (void)rule;
    if (!noc_rw_add_dependency(rewriter, "partial/dependency.h")) return false;
    CHECK(!noc_tokenize(context, "dependency-error.c", "/*", 2, &invalid));
    noc_token_stream_free(&invalid);
    return true;
}

static void test_transform_dependencies(void)
{
    static const char source[] = "int first = @depends(); int second = @depends();\n";
    static const char embed_source[] = "static const char text[] = @embed(\"message.txt\");\n";
    Dependency_Paths paths = {{0}, {0}};
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};
    Noc_Rule rule = {
        "depends",
        NOC_RULE_EXPRESSION,
        "@depends()",
        "Record deterministic test dependencies.",
        expand_dependencies,
        &paths,
    };
    Noc_Rule failing_rule = {
        "dependency_error",
        NOC_RULE_TOKEN,
        "@dependency_error",
        "Record a dependency and then report an error.",
        expand_dependency_then_error,
        &context,
    };
    (void)snprintf(paths.first, sizeof(paths.first), "%s", "include/first.h");
    (void)snprintf(paths.second, sizeof(paths.second), "%s", "assets/second.bin");

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_transform_source(&context,
                               "dependencies.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strcmp(result.output, "int first = 0; int second = 0;\n") == 0);
    CHECK(result.dependency_count == 2);
    CHECK(strcmp(result.dependencies[0], "include/first.h") == 0);
    CHECK(strcmp(result.dependencies[1], "assets/second.bin") == 0);
    paths.first[0] = 'X';
    paths.second[0] = 'X';
    CHECK(strcmp(result.dependencies[0], "include/first.h") == 0);
    CHECK(strcmp(result.dependencies[1], "assets/second.bin") == 0);
    noc_transform_result_free(&result);
    CHECK(result.dependencies == NULL && result.dependency_count == 0);

    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule(&context, failing_rule));
    CHECK(!noc_transform_source(&context,
                                "dependency-error.c",
                                "@dependency_error",
                                sizeof("@dependency_error") - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(result.output == NULL);
    CHECK(result.dependencies == NULL);
    CHECK(result.dependency_count == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_embed_rule(&context, "embed"));
    CHECK(noc_transform_source(&context,
                               "examples/embed/dependency-test.c",
                               embed_source,
                               sizeof(embed_source) - 1,
                               &result));
    CHECK(result.dependency_count == 1);
    CHECK(strcmp(result.dependencies[0], "examples/embed/message.txt") == 0);
    CHECK(strstr(result.output, "Hello from a normal .c file") != NULL);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static bool expand_preserved_newlines(Noc_Rewriter *rewriter,
                                      const Noc_Rule *rule,
                                      void *user_data)
{
    Noc_Slice inside;
    (void)rule;
    (void)user_data;
    return noc_rw_capture_balanced(rewriter, "(", ")", &inside) &&
           noc_rw_emit_cstr(rewriter, "0") &&
           noc_rw_preserve_newlines(rewriter, inside);
}

static bool expand_restored_line(Noc_Rewriter *rewriter,
                                 const Noc_Rule *rule,
                                 void *user_data)
{
    Noc_Location location = {0};
    (void)rule;
    (void)user_data;
    location.path = "virtual?\t/a\"b\\c.h";
    location.line = 91;
    return noc_rw_expect_punct(rewriter, "(") &&
           noc_rw_expect_punct(rewriter, ")") &&
           noc_rw_emit_cstr(rewriter, "0") &&
           noc_rw_emit_line_directive(rewriter, location);
}

static bool expand_restored_source_line(Noc_Rewriter *rewriter,
                                        const Noc_Rule *rule,
                                        void *user_data)
{
    Noc_Location location = {0};
    (void)rule;
    (void)user_data;
    location.line = 17;
    return noc_rw_emit_line_directive(rewriter, location);
}

static bool expand_invalid_line(Noc_Rewriter *rewriter,
                                const Noc_Rule *rule,
                                void *user_data)
{
    Noc_Location location = {0};
    (void)rule;
    (void)user_data;
    return noc_rw_emit_cstr(rewriter, "partial") &&
           noc_rw_emit_line_directive(rewriter, location);
}

static void test_rewriter_source_mapping(void)
{
    static const char source[] =
        "int value = @squash(first\r\nsecond\nthird\rfourth);\n"
        "int mapped = @restore();\n"
        "@source_line int tail;\n";
    static const char expected[] =
        "int value = 0\r\n\n\r;\n"
        "int mapped = 0\n"
        "#line 91 \"virtual\\?\\t/a\\\"b\\\\c.h\"\n"
        ";\n"
        "#line 17 \"source-map.c\"\n"
        " int tail;\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};
    Noc_Rule squash = {
        "squash",
        NOC_RULE_EXPRESSION,
        "@squash(expression)",
        "Replace an expression while retaining its physical newlines.",
        expand_preserved_newlines,
        NULL,
    };
    Noc_Rule restore = {
        "restore",
        NOC_RULE_EXPRESSION,
        "@restore()",
        "Restore an explicit source location.",
        expand_restored_line,
        NULL,
    };
    Noc_Rule source_line = {
        "source_line",
        NOC_RULE_TOKEN,
        "@source_line",
        "Restore a line while retaining the current source path.",
        expand_restored_source_line,
        NULL,
    };
    Noc_Rule invalid = {
        "invalid_line",
        NOC_RULE_TOKEN,
        "@invalid_line",
        "Exercise line-directive validation.",
        expand_invalid_line,
        NULL,
    };

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, squash));
    CHECK(noc_register_rule(&context, restore));
    CHECK(noc_register_rule(&context, source_line));
    CHECK(noc_register_rule(&context, invalid));
    CHECK(noc_transform_source(&context,
                               "source-map.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(result.output_count == sizeof(expected) - 1);
    CHECK(memcmp(result.output, expected, sizeof(expected) - 1) == 0);
    noc_transform_result_free(&result);

    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_transform_source(&context,
                                "source-map-error.c",
                                "@invalid_line",
                                sizeof("@invalid_line") - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(strstr(diagnostics.last_message, "line 0") != NULL);
    CHECK(result.output == NULL && result.output_count == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static bool expand_token_range(Noc_Rewriter *rewriter,
                               const Noc_Rule *rule,
                               void *user_data)
{
    const Noc_Token_Stream *stream = noc_rw_token_stream(rewriter);
    Noc_Token_Range remaining = noc_rw_remaining_range(rewriter);
    Noc_Token_Range wrong;
    Noc_Token_Range whole;
    Noc_Token_Range inside;
    Noc_Token_Range consumed;
    Noc_Token_Cursor cursor;
    Noc_Slice source;
    (void)rule;
    (void)user_data;
    CHECK(stream != NULL);
    CHECK(noc_token_range_is_valid(stream, remaining));
    if (!stream || !noc_token_cursor_init_range(&cursor, stream, remaining)) return false;
    wrong.begin = remaining.begin + 1;
    wrong.end = wrong.begin;
    CHECK(!noc_rw_consume_range(rewriter, wrong));
    CHECK(noc_rw_remaining_range(rewriter).begin == remaining.begin);
    if (!noc_token_cursor_take_balanced(&cursor, "(", ")", &whole, &inside)) {
        return false;
    }
    consumed.begin = remaining.begin;
    consumed.end = cursor.index;
    if (!noc_rw_consume_range(rewriter, consumed)) return false;
    CHECK(noc_rw_remaining_range(rewriter).begin == consumed.end);
    source = noc_token_range_source(stream, inside);
    return source.data && noc_rw_emit_slice(rewriter, source);
}

static bool expand_syntax_node(Noc_Rewriter *rewriter,
                               const Noc_Rule *rule,
                               void *user_data)
{
    const Noc_Syntax_Tree *tree = noc_rw_syntax_tree(rewriter);
    bool expect_tree = *(const bool *)user_data;
    Noc_Token_Range before = noc_rw_remaining_range(rewriter);
    Noc_Token_Range inner;
    Noc_Slice source;
    size_t node = NOC_SYNTAX_NONE;
    (void)rule;
    if (expect_tree) CHECK(tree != NULL);
    CHECK(tree == noc_rw_syntax_tree(rewriter));
    CHECK(!noc_rw_take_syntax(rewriter, NOC_SYNTAX_ROOT, &node));
    CHECK(!noc_rw_take_syntax(rewriter, NOC_SYNTAX_BRACE_GROUP, &node));
    CHECK(noc_rw_remaining_range(rewriter).begin == before.begin);
    if (!tree ||
        !noc_rw_take_syntax(rewriter, NOC_SYNTAX_PAREN_GROUP, &node)) {
        return false;
    }
    CHECK(noc_syntax_node(tree, node)->kind == NOC_SYNTAX_PAREN_GROUP);
    CHECK(noc_syntax_parent(tree, node) == noc_syntax_root(tree));
    inner = noc_syntax_inner_range(tree, node);
    CHECK(slice_equals(noc_token_range_source(tree->stream, inner), "(alpha), beta"));
    source = noc_syntax_source(tree, node);
    return source.data && noc_rw_emit_slice(rewriter, source);
}

static void test_rewriter_structure_bridge(void)
{
    static const char source[] =
        "int ranged = @range /* lead */ (alpha, nested(1));\n"
        "int syntax = @syntax /* gap */ ((alpha), beta);\n";
    static const char expected[] =
        "int ranged = alpha, nested(1);\n"
        "int syntax = ((alpha), beta);\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};
    bool expect_tree = true;
    Noc_Rule range_rule = {
        "range",
        NOC_RULE_EXPRESSION,
        "@range(expression)",
        "Parse and consume a token range through the public stream.",
        expand_token_range,
        NULL,
    };
    Noc_Rule syntax_rule = {
        "syntax",
        NOC_RULE_EXPRESSION,
        "@syntax(group)",
        "Consume a complete lossless syntax node.",
        expand_syntax_node,
        &expect_tree,
    };

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, range_rule));
    CHECK(noc_register_rule(&context, syntax_rule));
    CHECK(noc_transform_source(&context,
                               "structure-bridge.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(result.output_count == sizeof(expected) - 1);
    CHECK(memcmp(result.output, expected, sizeof(expected) - 1) == 0);
    noc_transform_result_free(&result);

    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    expect_tree = false;
    CHECK(!noc_transform_source(&context,
                                "malformed-structure.c",
                                "prefix @syntax (value] suffix",
                                sizeof("prefix @syntax (value] suffix") - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(strstr(diagnostics.last_message, "expected closing delimiter") != NULL);
    CHECK(result.output == NULL && result.output_count == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static bool expand_nested_value(Noc_Rewriter *rewriter,
                                const Noc_Rule *rule,
                                void *user_data)
{
    (void)rule;
    (void)user_data;
    return noc_rw_expect_punct(rewriter, "(") &&
           noc_rw_expect_punct(rewriter, ")") &&
           noc_rw_add_dependency(rewriter, "nested/value.def") &&
           noc_rw_emit_cstr(rewriter, "42");
}

static bool expand_nested_group(Noc_Rewriter *rewriter,
                                const Noc_Rule *rule,
                                void *user_data)
{
    Noc_Slice inside;
    (void)rule;
    (void)user_data;
    return noc_rw_capture_balanced(rewriter, "(", ")", &inside) &&
           noc_rw_add_dependency(rewriter, "nested/value.def") &&
           noc_rw_emit_cstr(rewriter, "(") &&
           noc_rw_emit_transformed(rewriter, inside) &&
           noc_rw_emit_cstr(rewriter, ")");
}

static bool expand_nested_failure(Noc_Rewriter *rewriter,
                                  const Noc_Rule *rule,
                                  void *user_data)
{
    (void)rule;
    (void)user_data;
    if (!noc_rw_add_dependency(rewriter, "nested/failure.def") ||
        !noc_rw_emit_cstr(rewriter, "nested partial")) {
        return false;
    }
    noc_rw_error(rewriter, "nested failure");
    return false;
}

static bool expand_outer_failure(Noc_Rewriter *rewriter,
                                 const Noc_Rule *rule,
                                 void *user_data)
{
    static const char nested[] = "@nested_failure";
    (void)rule;
    (void)user_data;
    return noc_rw_emit_cstr(rewriter, "outer partial") &&
           noc_rw_add_dependency(rewriter, "outer/failure.def") &&
           noc_rw_emit_transformed(rewriter,
                                   (Noc_Slice){nested, sizeof(nested) - 1});
}

static bool expand_recursive(Noc_Rewriter *rewriter,
                             const Noc_Rule *rule,
                             void *user_data)
{
    static const char recursive[] = "@recursive";
    (void)rule;
    (void)user_data;
    return noc_rw_emit_transformed(rewriter,
                                   (Noc_Slice){recursive, sizeof(recursive) - 1});
}

static void test_nested_transformation(void)
{
    static const char source[] = "int value = @group(@group(@value()));\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};
    Noc_Rule value_rule = {
        "value", NOC_RULE_EXPRESSION, "@value()", "Emit a nested value.",
        expand_nested_value, NULL,
    };
    Noc_Rule group_rule = {
        "group", NOC_RULE_EXPRESSION, "@group(expression)", "Transform and group.",
        expand_nested_group, NULL,
    };
    Noc_Rule recursive_rule = {
        "recursive", NOC_RULE_TOKEN, "@recursive", "Exercise recursion limits.",
        expand_recursive, NULL,
    };
    Noc_Rule nested_failure_rule = {
        "nested_failure", NOC_RULE_TOKEN, "@nested_failure", "Fail after partial output.",
        expand_nested_failure, NULL,
    };
    Noc_Rule outer_failure_rule = {
        "outer_failure", NOC_RULE_TOKEN, "@outer_failure", "Nest a failing expansion.",
        expand_outer_failure, NULL,
    };

    noc_context_init(&context);
    CHECK(noc_register_rule(&context, value_rule));
    CHECK(noc_register_rule(&context, group_rule));
    CHECK(noc_register_rule(&context, recursive_rule));
    CHECK(noc_register_rule(&context, nested_failure_rule));
    CHECK(noc_register_rule(&context, outer_failure_rule));
    CHECK(noc_transform_source(&context,
                               "nested.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strcmp(result.output, "#line 1 \"nested.c\"\nint value = ((42));\n") == 0);
    CHECK(strstr(result.output + 1, "#line") == NULL);
    CHECK(result.dependency_count == 1);
    CHECK(strcmp(result.dependencies[0], "nested/value.def") == 0);
    noc_transform_result_free(&result);

    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_transform_source(&context,
                                "nested-failure.c",
                                "int prefix; @outer_failure",
                                sizeof("int prefix; @outer_failure") - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(result.error_count == 1);
    CHECK(result.output == NULL && result.dependencies == NULL);
    CHECK(result.dependency_count == 0);
    noc_transform_result_free(&result);

    memset(&diagnostics, 0, sizeof(diagnostics));
    CHECK(!noc_transform_source(&context,
                                "recursive.c",
                                "@recursive",
                                sizeof("@recursive") - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(strstr(diagnostics.last_message, "nested expansion limit") != NULL);
    CHECK(result.output == NULL && result.dependencies == NULL);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_custom_rule(void)
{
    static const char source[] = "int answer = @twice(20 + 1);\n";
    static const char spliced_source[] =
        "int answer = @tw\\" "\n" "ice(20 + 1);\n";
    static const char expected[] = "int answer = ((20 + 1) + (20 + 1));\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Noc_Rule rule = {
        "twice",
        NOC_RULE_EXPRESSION,
        "@twice(expression)",
        "Evaluate the textual expression twice and add the results.",
        expand_twice,
        NULL,
    };

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_transform_source(&context,
                               "custom.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(result.output != NULL);
    CHECK(strcmp(result.output, expected) == 0);
    noc_transform_result_free(&result);
    CHECK(noc_transform_source(&context,
                               "custom-spliced.c",
                               spliced_source,
                               sizeof(spliced_source) - 1,
                               &result));
    CHECK(result.output != NULL);
    CHECK(strcmp(result.output, expected) == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_transactional_match(void)
{
    static const char source[] = "@optional value;\n";
    static const char expected[] = "long value;\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Noc_Rule rule = {
        "optional",
        NOC_RULE_TOKEN,
        "@optional",
        "Test optional token matching.",
        expand_optional,
        NULL,
    };

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_transform_source(&context,
                               "optional.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strcmp(result.output, expected) == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_inactive_preprocessor_transformation(void)
{
    static const char source[] =
        "#if 0\n"
        "@missing(1)\n"
        "#elif 1\n"
        "@optional value;\n"
        "#else\n"
        "@missing(2)\n"
        "#endif\n";
    static const char expected[] =
        "#if 0\n"
        "@missing(1)\n"
        "#elif 1\n"
        "long value;\n"
        "#else\n"
        "@missing(2)\n"
        "#endif\n";
    static const char unknown[] = "#if FLAG\n@missing\n#endif\n";
    static const char malformed[] = "#endif\n@optional value;\n";
    static const char malformed_default_expected[] = "#endif\nlong value;\n";
    static const char nested_fragment[] =
        "int value =\n"
        "#if FLAG\n"
        "@group(\n"
        "#endif\n"
        "42\n"
        "#if FLAG\n"
        ")\n"
        "#endif\n"
        ";\n";
    static const char nested_expected[] =
        "int value =\n"
        "#if FLAG\n"
        "(\n"
        "#endif\n"
        "42\n"
        "#if FLAG\n"
        ")\n"
        "#endif\n"
        ";\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};
    Noc_Rule rule = {
        "optional",
        NOC_RULE_TOKEN,
        "@optional",
        "Test inactive conditional branches.",
        expand_optional,
        NULL,
    };
    Noc_Rule group_rule = {
        "group",
        NOC_RULE_EXPRESSION,
        "@group(expression)",
        "Transform a fragment crossing enclosing conditional boundaries.",
        expand_nested_group,
        NULL,
    };

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_register_rule(&context, group_rule));
    CHECK(!noc_transform_source(&context,
                                "inactive-default.c",
                                source,
                                sizeof(source) - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    noc_transform_result_free(&result);
    CHECK(noc_transform_source(&context,
                               "malformed-default.c",
                               malformed,
                               sizeof(malformed) - 1,
                               &result));
    CHECK(result.output != NULL &&
          strcmp(result.output, malformed_default_expected) == 0);
    noc_transform_result_free(&result);

    context.options.skip_inactive_preprocessor_branches = true;
    CHECK(noc_transform_source(&context,
                               "inactive.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(result.output != NULL && strcmp(result.output, expected) == 0);
    noc_transform_result_free(&result);
    CHECK(noc_transform_source(&context,
                               "nested-conditional-fragment.c",
                               nested_fragment,
                               sizeof(nested_fragment) - 1,
                               &result));
    CHECK(result.output != NULL && strcmp(result.output, nested_expected) == 0);
    noc_transform_result_free(&result);
    CHECK(!noc_transform_source(&context,
                                "unknown-activity.c",
                                unknown,
                                sizeof(unknown) - 1,
                                &result));
    CHECK(diagnostics.errors == 2);
    noc_transform_result_free(&result);
    CHECK(!noc_transform_source(&context,
                                "malformed-conditional.c",
                                malformed,
                                sizeof(malformed) - 1,
                                &result));
    CHECK(diagnostics.errors == 3);
    CHECK(strstr(diagnostics.last_message, "#endif") != NULL);
    CHECK(result.output == NULL && result.dependencies == NULL);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_unknown_rule(void)
{
    static const char source[] = "int value = @missing(1);\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};
    Pattern_State pattern_state = {"MATCH", NULL, NULL, 0, false};
    Noc_Rule pattern_rule = {
        "missing-pattern",
        NOC_RULE_TOKEN,
        "missing",
        "Ensure an unknown legacy range stays opaque.",
        expand_pattern_text,
        &pattern_state,
    };

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_transform_source(&context,
                                "unknown.c",
                                source,
                                sizeof(source) - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(result.error_count == 1);
    noc_transform_result_free(&result);

    context.options.emit_line_directives = false;
    context.options.unknown_rule_is_error = false;
    CHECK(noc_register_rule_pattern(&context, "missing", pattern_rule));
    CHECK(noc_transform_source(&context,
                               "unknown-passthrough.c",
                               "@ /* gap */ missing + missing",
                               sizeof("@ /* gap */ missing + missing") - 1,
                               &result));
    CHECK(strcmp(result.output, "@ /* gap */ missing + MATCH") == 0);
    CHECK(pattern_state.calls == 1);
    CHECK(diagnostics.errors == 1);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_silent_callback_failure(void)
{
    static const char source[] = "@failure\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};
    Noc_Rule rule = {
        "failure",
        NOC_RULE_TOKEN,
        "@failure",
        "Always fail without a custom diagnostic.",
        expand_failure,
        NULL,
    };

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule(&context, rule));
    CHECK(!noc_transform_source(&context,
                                "failure.c",
                                source,
                                sizeof(source) - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(result.error_count == 1);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_rule_pattern_matching(void)
{
    static const char opaque_source[] =
        "#define twice(x) x\n"
        "const char *name = \"twice\"; int twice_more; int value = twice(2);\n";
    static const char opaque_expected[] =
        "#define twice(x) x\n"
        "const char *name = \"twice\"; int twice_more; "
        "int value = ((2) + (2));\n";
    static const char spliced_source[] = "tw\\\nice(3)";
    static const char trigger_source[] = "checked /* gap\n */ add tail";
    static const char phase2_token_source[] =
        "1\\" "\n" "e2 1e3 u\\" "\n" "8\"mark\" u8\"other\"";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Noc_Rule legacy;
    Noc_Rule bare;
    Noc_Rule probe;
    Noc_Rule number;
    Noc_Rule string;
    Noc_Rule contextual;
    Pattern_State probe_state = {
        "SUM", "checked /* gap\n */ add", "tail", 0, true,
    };
    Pattern_State number_state = {"ONE", NULL, NULL, 0, false};
    Pattern_State string_state = {"TEXT", NULL, NULL, 0, false};
    Pattern_State contextual_state = {"D", NULL, NULL, 0, false};
    Noc_Token_Range invalid = noc_rw_trigger_range(NULL);

    legacy.name = "legacy_twice";
    legacy.scope = NOC_RULE_EXPRESSION;
    legacy.syntax = "@legacy_twice(expression)";
    legacy.description = "Exercise the unchanged legacy trigger.";
    legacy.expand = expand_twice;
    legacy.user_data = NULL;
    bare.name = "bare_twice";
    bare.scope = NOC_RULE_EXPRESSION;
    bare.syntax = "twice(expression)";
    bare.description = "Exercise a bare keyword trigger.";
    bare.expand = expand_twice;
    bare.user_data = NULL;
    probe = (Noc_Rule){
        "checked-add", NOC_RULE_TOKEN, "checked add value", "Inspect a trigger range.",
        expand_pattern_text, &probe_state,
    };
    number = (Noc_Rule){
        "one-literal", NOC_RULE_TOKEN, "1e2", "Match one complete number token.",
        expand_pattern_text, &number_state,
    };
    string = (Noc_Rule){
        "mark-literal", NOC_RULE_TOKEN, "u8\"mark\"", "Match one complete string token.",
        expand_pattern_text, &string_state,
    };
    contextual = (Noc_Rule){
        "defer-word", NOC_RULE_TOKEN, "defer", "Demonstrate lexical matching.",
        expand_pattern_text, &contextual_state,
    };

    CHECK(NOC_RULE_TRIGGER_AT_NAME == 0);
    CHECK(NOC_RULE_TRIGGER_PATTERN == 1);
    CHECK(invalid.begin == NOC_TOKEN_INDEX_NONE && invalid.end == NOC_TOKEN_INDEX_NONE);
    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, legacy));
    CHECK(noc_register_rule_pattern(&context, "twice", bare));
    CHECK(noc_register_rule_pattern(&context, "checked add", probe));
    CHECK(noc_register_rule_pattern(&context, "1e2", number));
    CHECK(noc_register_rule_pattern(&context, "u8\"mark\"", string));
    CHECK(noc_register_rule_pattern(&context, "defer", contextual));

    CHECK(noc_transform_source(&context,
                               "legacy-pattern.c",
                               "@ /* gap */ legacy_tw\\\nice(1)",
                               sizeof("@ /* gap */ legacy_tw\\\nice(1)") - 1,
                               &result));
    CHECK(strcmp(result.output, "((1) + (1))") == 0);
    noc_transform_result_free(&result);
    CHECK(noc_transform_source(&context,
                               "opaque-pattern.c",
                               opaque_source,
                               sizeof(opaque_source) - 1,
                               &result));
    CHECK(strcmp(result.output, opaque_expected) == 0);
    noc_transform_result_free(&result);
    CHECK(noc_transform_source(&context,
                               "spliced-pattern.c",
                               spliced_source,
                               sizeof(spliced_source) - 1,
                               &result));
    CHECK(strcmp(result.output, "((3) + (3))") == 0);
    noc_transform_result_free(&result);
    CHECK(noc_transform_source(&context,
                               "trigger-range.c",
                               trigger_source,
                               sizeof(trigger_source) - 1,
                               &result));
    CHECK(strcmp(result.output, "\nSUM tail") == 0);
    CHECK(probe_state.calls == 1);
    noc_transform_result_free(&result);
    CHECK(noc_transform_source(&context,
                               "token-kinds.c",
                               phase2_token_source,
                               sizeof(phase2_token_source) - 1,
                               &result));
    CHECK(strcmp(result.output, "ONE 1e3 TEXT u8\"other\"") == 0);
    CHECK(number_state.calls == 1 && string_state.calls == 1);
    noc_transform_result_free(&result);
    CHECK(noc_transform_source(&context,
                               "lexical-context.c",
                               "object.defer deferred defer:",
                               sizeof("object.defer deferred defer:") - 1,
                               &result));
    CHECK(strcmp(result.output, "object.D deferred D:") == 0);
    CHECK(contextual_state.calls == 2);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_rule_pattern_registration(void)
{
    static const char spliced_duplicate[] = "checked\\\n add";
    static const char spliced_number[] = "1\\\ne2";
    static const char spliced_string[] = "u\\\n8\"x\"";
    static const char trigraph[] = {'?', '?', '=', '\0'};
    static const char two_questions[] = {'?', '?', '\0'};
    static const char safe_questions[] = {'?', '?', 'x', '\0'};
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};
    Pattern_State state = {"MATCH", NULL, NULL, 0, false};
    Noc_Rule rule;
    size_t errors;

    rule.name = "checked";
    rule.scope = NOC_RULE_TOKEN;
    rule.syntax = "checked add";
    rule.description = "Validate pattern registration.";
    rule.expand = expand_pattern_text;
    rule.user_data = &state;
    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule_pattern(&context, "checked add", rule));
    CHECK(context.rules_count == 1);
    CHECK(noc_rule_is_enabled(&context, noc_slice_from_cstr("checked")));

    rule.name = "duplicate-comment";
    CHECK(!noc_register_rule_pattern(&context, "checked/* gap */add", rule));
    rule.name = "duplicate-splice";
    CHECK(!noc_register_rule_pattern(&context, spliced_duplicate, rule));
    rule.name = "number";
    CHECK(noc_register_rule_pattern(&context, "1e2", rule));
    rule.name = "duplicate-number";
    CHECK(!noc_register_rule_pattern(&context, spliced_number, rule));
    rule.name = "string";
    CHECK(noc_register_rule_pattern(&context, "u8\"x\"", rule));
    rule.name = "duplicate-string";
    CHECK(!noc_register_rule_pattern(&context, spliced_string, rule));
    rule.name = "checked";
    CHECK(!noc_register_rule_pattern(&context, "other", rule));
    CHECK(!noc_register_rule(&context, rule));
    rule.name = "invalid";
    CHECK(!noc_register_rule_pattern(&context, NULL, rule));
    CHECK(!noc_register_rule_pattern(&context, "", rule));
    CHECK(!noc_register_rule_pattern(&context, " \n/* only trivia */", rule));
    CHECK(!noc_register_rule_pattern(&context, "/* unterminated", rule));
    CHECK(!noc_register_rule_pattern(&context, "#define VALUE 1", rule));
    CHECK(!noc_register_rule_pattern(&context, trigraph, rule));
    CHECK(!noc_register_rule_pattern(&context, "@invalid", rule));
    CHECK(context.rules_count == 3);
    CHECK(noc_rule_is_enabled(&context, noc_slice_from_cstr("checked")));

    rule.name = "unless";
    rule.syntax = "unless ( condition";
    CHECK(noc_register_rule_pattern(&context, "unless (", rule));
    rule.name = "safe-question";
    rule.syntax = "??x";
    CHECK(noc_register_rule_pattern(&context, safe_questions, rule));
    rule.name = "two-questions";
    rule.syntax = "two question marks";
    CHECK(noc_register_rule_pattern(&context, two_questions, rule));
    CHECK(context.rules_count == 6);
    errors = diagnostics.errors;
    CHECK(noc_transform_source(&context,
                               "registration-transaction.c",
                               "checked add",
                               sizeof("checked add") - 1,
                               &result));
    CHECK(strcmp(result.output, "MATCH") == 0);
    CHECK(diagnostics.errors == errors);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void check_longest_pattern_order(bool longer_first)
{
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Pattern_State short_state = {"SHORT", NULL, NULL, 0, false};
    Pattern_State long_state = {"LONG", NULL, NULL, 0, false};
    Noc_Rule short_rule = {
        "checked-short", NOC_RULE_TOKEN, "checked", "Short trigger.",
        expand_pattern_text, &short_state,
    };
    Noc_Rule long_rule = {
        "checked-long", NOC_RULE_TOKEN, "checked add", "Long trigger.",
        expand_pattern_text, &long_state,
    };
    noc_context_init(&context);
    context.options.emit_line_directives = false;
    if (longer_first) {
        CHECK(noc_register_rule_pattern(&context, "checked add", long_rule));
        CHECK(noc_register_rule_pattern(&context, "checked", short_rule));
    } else {
        CHECK(noc_register_rule_pattern(&context, "checked", short_rule));
        CHECK(noc_register_rule_pattern(&context, "checked add", long_rule));
    }
    CHECK(noc_transform_source(&context,
                               "longest.c",
                               "checked add",
                               sizeof("checked add") - 1,
                               &result));
    CHECK(strcmp(result.output, "LONG") == 0);
    CHECK(short_state.calls == 0 && long_state.calls == 1);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_rule_feature_controls(void)
{
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};
    Pattern_State short_state = {"SHORT", NULL, NULL, 0, false};
    Pattern_State long_state = {"LONG", NULL, NULL, 0, false};
    Pattern_State legacy_state = {"LEGACY", NULL, NULL, 0, false};
    Noc_Rule short_rule = {
        "checked-short", NOC_RULE_TOKEN, "checked", "Short trigger.",
        expand_pattern_text, &short_state,
    };
    Noc_Rule long_rule = {
        "checked-long", NOC_RULE_TOKEN, "checked add", "Long trigger.",
        expand_pattern_text, &long_state,
    };
    Noc_Rule legacy_rule = {
        "legacy", NOC_RULE_TOKEN, "@legacy", "Legacy feature control.",
        expand_pattern_text, &legacy_state,
    };
    Noc_Slice invalid_name = {NULL, 1};
    size_t errors;

    check_longest_pattern_order(false);
    check_longest_pattern_order(true);
    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule_pattern(&context, "checked", short_rule));
    CHECK(noc_register_rule_pattern(&context, "checked add", long_rule));
    CHECK(noc_register_rule(&context, legacy_rule));
    CHECK(noc_set_rule_enabled(&context, noc_slice_from_cstr("checked-long"), false));
    CHECK(!noc_rule_is_enabled(&context, noc_slice_from_cstr("checked-long")));
    CHECK(noc_find_rule(&context, noc_slice_from_cstr("checked-long")) != NULL);
    CHECK(!noc_transform_source(&context,
                                "disabled-strict.c",
                                "checked /* exact\n */ add",
                                sizeof("checked /* exact\n */ add") - 1,
                                &result));
    CHECK(result.output == NULL && result.error_count == 1);
    CHECK(short_state.calls == 0 && long_state.calls == 0);
    CHECK(strstr(diagnostics.last_message, "disabled") != NULL);
    noc_transform_result_free(&result);

    context.options.disabled_rule_is_error = false;
    CHECK(noc_transform_source(&context,
                               "disabled-passthrough.c",
                               "che\\\ncked /* exact\n */ add + checked",
                               sizeof("che\\\ncked /* exact\n */ add + checked") - 1,
                               &result));
    CHECK(strcmp(result.output, "che\\\ncked /* exact\n */ add + SHORT") == 0);
    CHECK(short_state.calls == 1 && long_state.calls == 0);
    noc_transform_result_free(&result);
    CHECK(noc_set_rule_enabled(&context, noc_slice_from_cstr("checked-long"), true));
    CHECK(noc_rule_is_enabled(&context, noc_slice_from_cstr("checked-long")));
    CHECK(noc_transform_source(&context,
                               "reenabled.c",
                               "checked add + checked",
                               sizeof("checked add + checked") - 1,
                               &result));
    CHECK(strcmp(result.output, "LONG + SHORT") == 0);
    CHECK(long_state.calls == 1 && short_state.calls == 2);
    noc_transform_result_free(&result);

    CHECK(noc_set_rule_enabled(&context, noc_slice_from_cstr("legacy"), false));
    CHECK(noc_transform_source(&context,
                               "legacy-disabled.c",
                               "@ /* gap */ legacy + checked",
                               sizeof("@ /* gap */ legacy + checked") - 1,
                               &result));
    CHECK(strcmp(result.output, "@ /* gap */ legacy + SHORT") == 0);
    CHECK(legacy_state.calls == 0);
    noc_transform_result_free(&result);
    errors = diagnostics.errors;
    CHECK(!noc_rule_is_enabled(&context, noc_slice_from_cstr("missing")));
    CHECK(diagnostics.errors == errors);
    CHECK(!noc_set_rule_enabled(&context, noc_slice_from_cstr("missing"), false));
    CHECK(diagnostics.errors == errors + 1);
    CHECK(!noc_rule_is_enabled(&context, invalid_name));
    CHECK(!noc_set_rule_enabled(&context, invalid_name, true));
    CHECK(diagnostics.errors == errors + 2);
    noc_context_deinit(&context);
}

static void test_registry_mutation_during_transform(void)
{
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};
    Mutation_State state;
    Noc_Rule active;

    memset(&state, 0, sizeof(state));
    state.context = &context;
    state.legacy_rule.name = "late-legacy";
    state.legacy_rule.scope = NOC_RULE_TOKEN;
    state.legacy_rule.syntax = "@late-legacy";
    state.legacy_rule.description = "Register after a transform.";
    state.legacy_rule.expand = expand_optional;
    state.legacy_rule.user_data = NULL;
    state.pattern_rule.name = "late-pattern";
    state.pattern_rule.scope = NOC_RULE_TOKEN;
    state.pattern_rule.syntax = "late";
    state.pattern_rule.description = "Reject pattern registration in a callback.";
    state.pattern_rule.expand = expand_optional;
    state.pattern_rule.user_data = NULL;
    active.name = "mutate";
    active.scope = NOC_RULE_TOKEN;
    active.syntax = "mutate";
    active.description = "Attempt registry mutation during expansion.";
    active.expand = expand_registry_mutation;
    active.user_data = &state;

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule_pattern(&context, "mutate", active));
    CHECK(!noc_transform_source(&context,
                                "mutation.c",
                                "mutate",
                                sizeof("mutate") - 1,
                                &result));
    CHECK(!state.registered_legacy && !state.registered_pattern && !state.changed_enabled);
    CHECK(diagnostics.errors == 3);
    CHECK(result.output == NULL && context.active_transforms == 0);
    CHECK(noc_rule_is_enabled(&context, noc_slice_from_cstr("mutate")));
    noc_transform_result_free(&result);
    CHECK(noc_register_rule(&context, state.legacy_rule));
    CHECK(noc_register_rule_pattern(&context, "late", state.pattern_rule));
    CHECK(noc_set_rule_enabled(&context, noc_slice_from_cstr("mutate"), false));
    noc_context_deinit(&context);
}

static void test_pattern_preprocessor_and_nested_transform(void)
{
    static const char source[] =
        "#define keyword ignored\n"
        "#if 0\n"
        "keyword inactive;\n"
        "#endif\n"
        "#if FLAG\n"
        "keyword unknown;\n"
        "#endif\n"
        "keyword active;\n";
    static const char expected[] =
        "#define keyword ignored\n"
        "#if 0\n"
        "keyword inactive;\n"
        "#endif\n"
        "#if FLAG\n"
        "long unknown;\n"
        "#endif\n"
        "long active;\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Noc_Rule keyword = {
        "keyword", NOC_RULE_TOKEN, "keyword", "Exercise preprocessor activity.",
        expand_optional, NULL,
    };
    Noc_Rule value = {
        "bare-value", NOC_RULE_EXPRESSION, "value()", "Nested bare value.",
        expand_nested_value, NULL,
    };
    Noc_Rule group = {
        "group", NOC_RULE_EXPRESSION, "@group(expression)", "Transform nested source.",
        expand_nested_group, NULL,
    };

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    context.options.skip_inactive_preprocessor_branches = true;
    CHECK(noc_register_rule_pattern(&context, "keyword", keyword));
    CHECK(noc_register_rule_pattern(&context, "value", value));
    CHECK(noc_register_rule(&context, group));
    CHECK(noc_transform_source(&context,
                               "pattern-preprocessor.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strcmp(result.output, expected) == 0);
    noc_transform_result_free(&result);
    CHECK(noc_transform_source(&context,
                               "pattern-nested.c",
                               "@group(value())",
                               sizeof("@group(value())") - 1,
                               &result));
    CHECK(strcmp(result.output, "(42)") == 0);
    CHECK(result.dependency_count == 1);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void check_passthrough(const char *source)
{
    Noc_Context context;
    Noc_Transform_Result result;
    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_transform_source(&context, "passthrough.c", source, strlen(source), &result));
    CHECK(result.output != NULL);
    CHECK(strcmp(result.output, source) == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_comment_and_preprocessor_opacity(void)
{
    static const char continued_line_comment[] =
        "// hidden " "\\" "\n"
        "@missing(1)\n"
        "int visible;\n";
    static const char split_comment_opener[] =
        "/" "\\" "\n" "/ @missing(1)\n"
        "int visible;\n";
    static const char directive_block_comment[] =
        "#define VALUE /*\n"
        "@missing(1)\n"
        "*/\n"
        "int visible;\n";
    static const char digraph_directive[] =
        "%:define VALUE @missing(1)\n"
        "int visible;\n";

    check_passthrough(continued_line_comment);
    check_passthrough(split_comment_opener);
    check_passthrough(directive_block_comment);
    check_passthrough(digraph_directive);
}

static void test_trigraph_rejection(void)
{
    static const char source[] = {'?', '?', '/', '\0'};
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_transform_source(&context, "trigraph.c", source, 3, &result));
    CHECK(diagnostics.errors == 1);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_safe_c_string_and_line_path(void)
{
    static const char source[] = "@bytes()";
    static const char path[] = {'a', '?', '?', '/', 'b', '.', 'c', '\0'};
    Noc_Context context;
    Noc_Transform_Result result;
    Noc_Rule rule = {
        "bytes",
        NOC_RULE_EXPRESSION,
        "@bytes()",
        "Emit bytes that could otherwise form a trigraph.",
        expand_question_bytes,
        NULL,
    };

    noc_context_init(&context);
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_transform_source(&context,
                               path,
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strstr(result.output, "\\?\\?/") != NULL);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

int main(void)
{
    test_transform_dependencies();
    test_rewriter_source_mapping();
    test_rewriter_structure_bridge();
    test_nested_transformation();
    test_custom_rule();
    test_transactional_match();
    test_inactive_preprocessor_transformation();
    test_unknown_rule();
    test_silent_callback_failure();
    test_rule_pattern_matching();
    test_rule_pattern_registration();
    test_rule_feature_controls();
    test_registry_mutation_during_transform();
    test_pattern_preprocessor_and_nested_transform();
    test_comment_and_preprocessor_opacity();
    test_trigraph_rejection();
    test_safe_c_string_and_line_path();
    return finish_suite("rewriter");
}
