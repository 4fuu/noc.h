#include "test_support.h"

static size_t text_count(const char *text, const char *needle)
{
    size_t count = 0;
    size_t length = strlen(needle);
    while ((text = strstr(text, needle)) != NULL) {
        count += 1;
        text += length;
    }
    return count;
}

typedef struct {
    Noc_Context *context;
    bool changed;
} Feature_Mutation;

typedef struct {
    Noc_Context *context;
    Diagnostic_State diagnostics;
    bool attempted;
    bool changed;
} Diagnostic_Mutation;

static bool reject_feature_mutation(Noc_Rewriter *rewriter,
                                    const Noc_Rule *rule,
                                    void *user_data)
{
    Feature_Mutation *mutation = (Feature_Mutation *)user_data;
    (void)rule;
    mutation->changed = noc_set_feature_enabled(mutation->context,
                                                NOC_FEATURE_TEMPLATES,
                                                true);
    return noc_rw_emit_cstr(rewriter, "0");
}

static void reject_diagnostic_mutation(void *user_data,
                                       const Noc_Diagnostic *diagnostic)
{
    Diagnostic_Mutation *mutation = (Diagnostic_Mutation *)user_data;
    if (!mutation->attempted) {
        mutation->attempted = true;
        mutation->changed = noc_set_feature_enabled(mutation->context,
                                                    NOC_FEATURE_TEMPLATES,
                                                    true);
    }
    count_diagnostics(&mutation->diagnostics, diagnostic);
}

static void test_feature_api_and_disabled_preservation(void)
{
    static const char source[] =
        "int defer(int value) { return value; }\n"
        "int template(int value);\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(strcmp(noc_feature_name(NOC_FEATURE_DEFER), "defer") == 0);
    CHECK(strcmp(noc_feature_name(NOC_FEATURE_TEMPLATES), "templates") == 0);
    CHECK(strcmp(noc_feature_name(NOC_FEATURE_OWNERSHIP), "ownership") == 0);
    CHECK(strcmp(noc_feature_name(NOC_FEATURE_COUNT), "unknown") == 0);
    CHECK(!noc_feature_is_enabled(&context, NOC_FEATURE_DEFER));
    CHECK(!noc_feature_is_enabled(&context, NOC_FEATURE_TEMPLATES));
    CHECK(!noc_feature_is_enabled(&context, NOC_FEATURE_OWNERSHIP));
    CHECK(noc_transform_source(&context,
                               "disabled.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(slice_equals((Noc_Slice){result.output, result.output_count}, source));
    noc_transform_result_free(&result);
    CHECK(noc_enable_mvp_features(&context));
    CHECK(noc_feature_is_enabled(&context, NOC_FEATURE_DEFER));
    CHECK(noc_feature_is_enabled(&context, NOC_FEATURE_TEMPLATES));
    CHECK(noc_feature_is_enabled(&context, NOC_FEATURE_OWNERSHIP));
    noc_context_deinit(&context);
}

static void test_inactive_branches_remain_opaque(void)
{
    static const char source[] =
        "#if 0\n"
        "void dead(void) { defer missing(); own(not valid); }\n"
        "template(T, expected) T other(T value) { return value; }\n"
        "#endif\n"
        "int live;\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    context.options.skip_inactive_preprocessor_branches = true;
    CHECK(noc_enable_mvp_features(&context));
    CHECK(noc_transform_source(&context,
                               "inactive.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(slice_equals((Noc_Slice){result.output, result.output_count}, source));
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_nested_lifo_and_returns(void)
{
    static const char source[] =
        "void cleanup(int value);\n"
        "int run(int stop) {\n"
        "    defer cleanup(1);\n"
        "    {\n"
        "        defer { cleanup(2); }\n"
        "        if (stop) return 7;\n"
        "    }\n"
        "    return 9;\n"
        "}\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    const char *evaluation;
    const char *inner;
    const char *outer;
    const char *returned;

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_DEFER, true));
    CHECK(noc_transform_source(&context,
                               "defer.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(result.output != NULL);
    CHECK(strstr(result.output, "defer cleanup") == NULL);
    CHECK(strstr(result.output, "defer {") == NULL);
    CHECK(text_count(result.output, "cleanup(1);") == 3);
    CHECK(text_count(result.output, "cleanup(2);") == 2);
    evaluation = strstr(result.output, "= (7);");
    inner = evaluation ? strstr(evaluation, "{ cleanup(2); }") : NULL;
    outer = inner ? strstr(inner, "cleanup(1);") : NULL;
    returned = outer ? strstr(outer, "return noc_defer_result_") : NULL;
    CHECK(evaluation != NULL && inner != NULL && outer != NULL && returned != NULL);
    CHECK(evaluation < inner && inner < outer && outer < returned);
    check_complete_generated_c("generated-defer.c",
                               result.output,
                               result.output_count);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_rejected_control_flow_and_placement(void)
{
    static const char crossing[] =
        "void cleanup(void);\n"
        "void bad(void) { defer cleanup(); while (1) { break; } }\n";
    static const char forward_goto[] =
        "void cleanup(void);\n"
        "void bad(void) { goto done; defer cleanup(); done:; }\n";
    static const char unbraced[] =
        "void cleanup(void);\n"
        "void bad(int value) { if (value) defer cleanup(); }\n";
    static const char empty[] = "void bad(void) { defer(); }\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_DEFER, true));
    CHECK(!noc_transform_source(&context,
                                "crossing.c",
                                crossing,
                                sizeof(crossing) - 1,
                                &result));
    CHECK(result.output == NULL);
    CHECK(strstr(diagnostics.last_message, "goto/break/continue") != NULL);
    CHECK(!noc_transform_source(&context,
                                "forward-goto.c",
                                forward_goto,
                                sizeof(forward_goto) - 1,
                                &result));
    CHECK(result.output == NULL);
    CHECK(strstr(diagnostics.last_message, "goto/break/continue") != NULL);
    CHECK(!noc_transform_source(&context,
                                "unbraced.c",
                                unbraced,
                                sizeof(unbraced) - 1,
                                &result));
    CHECK(result.output == NULL);
    CHECK(strstr(diagnostics.last_message, "direct child") != NULL);
    CHECK(!noc_transform_source(&context,
                                "empty-defer.c",
                                empty,
                                sizeof(empty) - 1,
                                &result));
    CHECK(result.output == NULL);
    CHECK(strstr(diagnostics.last_message, "cannot be empty") != NULL);
    noc_context_deinit(&context);
}

static void test_feature_mutation_is_transactional(void)
{
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};
    Feature_Mutation mutation = {&context, false};
    Noc_Rule rule = {
        "change_feature",
        NOC_RULE_EXPRESSION,
        "@change_feature",
        "Attempt a forbidden feature mutation.",
        reject_feature_mutation,
        &mutation,
    };

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule(&context, rule));
    CHECK(!noc_transform_source(&context,
                                "mutation.c",
                                "int value = @change_feature;",
                                sizeof("int value = @change_feature;") - 1,
                                &result));
    CHECK(!mutation.changed);
    CHECK(!noc_feature_is_enabled(&context, NOC_FEATURE_TEMPLATES));
    CHECK(result.output == NULL && result.error_count == 1);
    CHECK(strstr(diagnostics.last_message, "during an active transform") != NULL);
    noc_context_deinit(&context);
}

static void test_diagnostic_callback_cannot_mutate_features(void)
{
    static const char source[] =
        "void cleanup(void);\n"
        "void bad(int condition) { if (condition) defer cleanup(); }\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_Mutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.context = &context;
    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, reject_diagnostic_mutation, &mutation);
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_DEFER, true));
    CHECK(!noc_transform_source(&context,
                                "diagnostic-mutation.c",
                                source,
                                sizeof(source) - 1,
                                &result));
    CHECK(mutation.attempted && !mutation.changed);
    CHECK(!noc_feature_is_enabled(&context, NOC_FEATURE_TEMPLATES));
    CHECK(result.output == NULL && result.error_count == 2);
    CHECK(mutation.diagnostics.errors == 2);
    noc_context_deinit(&context);
}

int main(void)
{
    test_feature_api_and_disabled_preservation();
    test_inactive_branches_remain_opaque();
    test_nested_lifo_and_returns();
    test_rejected_control_flow_and_placement();
    test_feature_mutation_is_transactional();
    test_diagnostic_callback_cannot_mutate_features();
    return finish_suite("defer");
}
