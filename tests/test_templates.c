#include "test_support.h"

static void test_multiple_explicit_instances(void)
{
    static const char source[] =
        "template(T, identity)\n"
        "T identity(T value) {\n"
        "    (void)\"T identity\";\n"
        "    return value;\n"
        "}\n"
        "instantiate(identity, int, identity_int);\n"
        "instantiate(identity, const char *, identity_text);\n"
        "int use_identity(void) { return identity_int(4); }\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_TEMPLATES, true));
    CHECK(noc_transform_source(&context,
                               "templates.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strstr(result.output, "template(") == NULL);
    CHECK(strstr(result.output, "instantiate(") == NULL);
    CHECK(strstr(result.output, "int identity_int(int value)") != NULL);
    CHECK(strstr(result.output,
                 "const char * identity_text(const char * value)") != NULL);
    CHECK(strstr(result.output, "\"T identity\"") != NULL);
    check_complete_generated_c("generated-templates.c",
                               result.output,
                               result.output_count);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_lexical_protection_and_diagnostics(void)
{
    static const char opaque[] =
        "/* template(T, hidden) T hidden(T x) { return x; } */\n"
        "const char *text = \"instantiate(hidden, int, hidden_int)\";\n";
    static const char unknown[] = "instantiate(missing, int, missing_int);\n";
    static const char duplicate[] =
        "template(T, id) T id(T value) { return value; }\n"
        "instantiate(id, int, same);\n"
        "instantiate(id, long, same);\n";
    static const char wrong_definition[] =
        "template(T, expected) T other(T value) { return value; }\n";
    static const char abstract_type[] =
        "template(T, id) T id(T value) { return value; }\n"
        "instantiate(id, int (*)(void), id_function);\n";
    static const char shadowed_name[] =
        "template(T, id) T id(T value) {\n"
        "  T (*id)(T) = 0;\n"
        "  return id(value);\n"
        "}\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Diagnostic_State diagnostics = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_TEMPLATES, true));
    CHECK(noc_transform_source(&context,
                               "opaque.c",
                               opaque,
                               sizeof(opaque) - 1,
                               &result));
    CHECK(slice_equals((Noc_Slice){result.output, result.output_count}, opaque));
    noc_transform_result_free(&result);
    CHECK(!noc_transform_source(&context,
                                "unknown.c",
                                unknown,
                                sizeof(unknown) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "unknown") != NULL);
    CHECK(!noc_transform_source(&context,
                                "duplicate.c",
                                duplicate,
                                sizeof(duplicate) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "duplicate generated") != NULL);
    CHECK(!noc_transform_source(&context,
                                "wrong-definition.c",
                                wrong_definition,
                                sizeof(wrong_definition) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "must be followed") != NULL);
    CHECK(!noc_transform_source(&context,
                                "abstract-type.c",
                                abstract_type,
                                sizeof(abstract_type) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "declaration-prefix") != NULL);
    CHECK(!noc_transform_source(&context,
                                "shadowed-name.c",
                                shadowed_name,
                                sizeof(shadowed_name) - 1,
                                &result));
    CHECK(strstr(diagnostics.last_message, "self references and shadowing") != NULL);
    noc_context_deinit(&context);
}

static void test_function_name_does_not_rename_members(void)
{
    static const char source[] =
        "typedef struct Box { int identity; } Box;\n"
        "template(T, identity)\n"
        "T identity(Box *box, T fallback) {\n"
        "    return box->identity ? fallback : fallback;\n"
        "}\n"
        "instantiate(identity, int, identity_int);\n";
    Noc_Context context;
    Noc_Transform_Result result = {0};

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_set_feature_enabled(&context, NOC_FEATURE_TEMPLATES, true));
    CHECK(noc_transform_source(&context,
                               "template-members.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strstr(result.output, "box->identity ?") != NULL);
    CHECK(strstr(result.output, "box->identity_int") == NULL);
    check_complete_generated_c("generated-template-members.c",
                               result.output,
                               result.output_count);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

int main(void)
{
    test_multiple_explicit_instances();
    test_lexical_protection_and_diagnostics();
    test_function_name_does_not_rename_members();
    return finish_suite("templates");
}
