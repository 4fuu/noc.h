#define NOC_IMPLEMENTATION
#include "../noc.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n",                      \
                    __FILE__, __LINE__, #condition);                            \
            failures += 1;                                                      \
        }                                                                       \
    } while (0)

typedef struct {
    size_t errors;
} Diagnostic_State;

static void count_diagnostics(void *user_data, const Noc_Diagnostic *diagnostic)
{
    Diagnostic_State *state = (Diagnostic_State *)user_data;
    if (diagnostic->severity == NOC_DIAGNOSTIC_ERROR) state->errors += 1;
}

static void test_lexer(void)
{
    static const char source[] =
        "  #define VALUE(x) x \\\n"
        "    + 1\n"
        "@twice(21) /* comment */\n";
    Noc_Lexer lexer;
    Noc_Token token;

    noc_lexer_init(&lexer, "lexer.c", source, sizeof(source) - 1);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_WHITESPACE);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_PREPROCESSOR);
    CHECK(token.location.line == 1);
    token = noc_lexer_next(&lexer);
    CHECK(noc_token_is_punct(token, "@"));
    CHECK(token.location.line == 3);
    token = noc_lexer_next(&lexer);
    CHECK(noc_token_is_identifier(token, "twice"));
}

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

static void test_custom_rule(void)
{
    static const char source[] = "int answer = @twice(20 + 1);\n";
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

static void test_unknown_rule(void)
{
    static const char source[] = "int value = @missing(1);\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};

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

static void test_buffer_self_append(void)
{
    char data[200];
    Noc_Buffer buffer = {0};
    memset(data, 'x', sizeof(data));
    CHECK(noc_buffer_append(&buffer, data, sizeof(data)));
    CHECK(noc_buffer_append(&buffer, buffer.items, buffer.count));
    CHECK(buffer.count == sizeof(data) * 2);
    CHECK(memcmp(buffer.items, buffer.items + sizeof(data), sizeof(data)) == 0);
    noc_buffer_free(&buffer);
}

static void test_file_alias_rejection(void)
{
    static const char contents[] = "int untouched;\n";
    const char *path = "build/noc-alias-source.c";
    FILE *file = fopen(path, "wb");
    char actual[sizeof(contents)] = {0};
    Noc_Context context;
    Diagnostic_State diagnostics = {0};
    CHECK(file != NULL);
    if (!file) return;
    CHECK(fwrite(contents, 1, sizeof(contents) - 1, file) == sizeof(contents) - 1);
    CHECK(fclose(file) == 0);

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_transform_file(&context, path, "./build/noc-alias-source.c"));
    CHECK(diagnostics.errors == 1);
    noc_context_deinit(&context);

    file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file) {
        CHECK(fread(actual, 1, sizeof(contents) - 1, file) == sizeof(contents) - 1);
        CHECK(fclose(file) == 0);
        CHECK(memcmp(actual, contents, sizeof(contents) - 1) == 0);
    }
    CHECK(remove(path) == 0);
}

static void test_string_codec(void)
{
    static const char text[] = "\"line\\nvalue\\x21\"";
    Noc_Token token;
    Noc_Buffer decoded = {0};
    token.kind = NOC_TOKEN_STRING;
    token.text.data = text;
    token.text.count = sizeof(text) - 1;
    token.location = (Noc_Location){0};
    CHECK(noc_decode_string_token(token, &decoded));
    CHECK(decoded.count == 11);
    CHECK(memcmp(decoded.items, "line\nvalue!", 11) == 0);
    noc_buffer_free(&decoded);
}

int main(void)
{
    test_lexer();
    test_custom_rule();
    test_transactional_match();
    test_unknown_rule();
    test_silent_callback_failure();
    test_comment_and_preprocessor_opacity();
    test_trigraph_rejection();
    test_safe_c_string_and_line_path();
    test_buffer_self_append();
    test_file_alias_rejection();
    test_string_codec();
    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    puts("noc tests passed");
    return 0;
}
