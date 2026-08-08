#include "test_support.h"

static bool slice_is_identifier(Noc_Slice slice, const char *identifier)
{
    Noc_Token token = {0};
    token.kind = NOC_TOKEN_IDENTIFIER;
    token.text = slice;
    return noc_token_is_identifier(token, identifier);
}

static void test_macro_policy_matrix(void)
{
    Noc_Source_Class source_class;
    CHECK(strcmp(noc_source_class_name(NOC_SOURCE_CLASS_PROJECT), "project") == 0);
    CHECK(strcmp(noc_source_class_name(NOC_SOURCE_CLASS_TRUSTED), "trusted") == 0);
    CHECK(strcmp(noc_source_class_name(NOC_SOURCE_CLASS_SYSTEM), "system") == 0);
    CHECK(strcmp(noc_source_class_name(NOC_SOURCE_CLASS_GENERATED), "generated") == 0);
    CHECK(strcmp(noc_source_class_name((Noc_Source_Class)99), "unknown") == 0);
    CHECK(strcmp(noc_macro_policy_name(NOC_MACROS_DISABLED), "disabled") == 0);
    CHECK(strcmp(noc_macro_policy_name(NOC_MACROS_TRUSTED_ONLY),
                 "trusted-only") == 0);
    CHECK(strcmp(noc_macro_policy_name(NOC_MACROS_PROJECT), "project") == 0);
    CHECK(strcmp(noc_macro_policy_name(NOC_MACROS_FULL), "full") == 0);
    CHECK(strcmp(noc_macro_policy_name((Noc_Macro_Policy)99), "unknown") == 0);

    for (source_class = NOC_SOURCE_CLASS_PROJECT;
         source_class <= NOC_SOURCE_CLASS_GENERATED;
         source_class = (Noc_Source_Class)(source_class + 1)) {
        CHECK(!noc_macro_policy_allows_definition(NOC_MACROS_DISABLED,
                                                  source_class));
        CHECK(noc_macro_policy_allows_definition(NOC_MACROS_FULL, source_class));
    }
    CHECK(!noc_macro_policy_allows_definition(NOC_MACROS_TRUSTED_ONLY,
                                              NOC_SOURCE_CLASS_PROJECT));
    CHECK(noc_macro_policy_allows_definition(NOC_MACROS_TRUSTED_ONLY,
                                             NOC_SOURCE_CLASS_TRUSTED));
    CHECK(noc_macro_policy_allows_definition(NOC_MACROS_TRUSTED_ONLY,
                                             NOC_SOURCE_CLASS_SYSTEM));
    CHECK(!noc_macro_policy_allows_definition(NOC_MACROS_TRUSTED_ONLY,
                                              NOC_SOURCE_CLASS_GENERATED));
    CHECK(noc_macro_policy_allows_definition(NOC_MACROS_PROJECT,
                                             NOC_SOURCE_CLASS_PROJECT));
    CHECK(noc_macro_policy_allows_definition(NOC_MACROS_PROJECT,
                                             NOC_SOURCE_CLASS_TRUSTED));
    CHECK(noc_macro_policy_allows_definition(NOC_MACROS_PROJECT,
                                             NOC_SOURCE_CLASS_SYSTEM));
    CHECK(!noc_macro_policy_allows_definition(NOC_MACROS_PROJECT,
                                              NOC_SOURCE_CLASS_GENERATED));
    CHECK(!noc_macro_policy_allows_definition((Noc_Macro_Policy)99,
                                              NOC_SOURCE_CLASS_PROJECT));
    CHECK(!noc_macro_policy_allows_definition(NOC_MACROS_FULL,
                                              (Noc_Source_Class)99));
}

static void test_directive_inventory(void)
{
    static const char source[] =
        "#define LOCAL 1\r\n"
        "#undef LOCAL\n"
        "#include \"x.h\"\n"
        "#if 1\n"
        "#ifdef LOCAL\n"
        "#ifndef OTHER\n"
        "#elif 0\n"
        "#elifdef LOCAL\n"
        "#elifndef OTHER\n"
        "#else\n"
        "#endif\n"
        "#line 40 \"logical.c\"\n"
        "#error stop here\n"
        "#warning take care\n"
        "#pragma once\n"
        "#\n"
        "#vendor payload\n"
        "%:de\\\nfine SPLICE 2\n";
    static const Noc_Preprocessor_Directive_Kind expected[] = {
        NOC_PREPROCESSOR_DIRECTIVE_DEFINE,
        NOC_PREPROCESSOR_DIRECTIVE_UNDEF,
        NOC_PREPROCESSOR_DIRECTIVE_INCLUDE,
        NOC_PREPROCESSOR_DIRECTIVE_IF,
        NOC_PREPROCESSOR_DIRECTIVE_IFDEF,
        NOC_PREPROCESSOR_DIRECTIVE_IFNDEF,
        NOC_PREPROCESSOR_DIRECTIVE_ELIF,
        NOC_PREPROCESSOR_DIRECTIVE_ELIFDEF,
        NOC_PREPROCESSOR_DIRECTIVE_ELIFNDEF,
        NOC_PREPROCESSOR_DIRECTIVE_ELSE,
        NOC_PREPROCESSOR_DIRECTIVE_ENDIF,
        NOC_PREPROCESSOR_DIRECTIVE_LINE,
        NOC_PREPROCESSOR_DIRECTIVE_ERROR,
        NOC_PREPROCESSOR_DIRECTIVE_WARNING,
        NOC_PREPROCESSOR_DIRECTIVE_PRAGMA,
        NOC_PREPROCESSOR_DIRECTIVE_NULL,
        NOC_PREPROCESSOR_DIRECTIVE_UNKNOWN,
        NOC_PREPROCESSOR_DIRECTIVE_DEFINE,
    };
    static const char *const names[] = {
        "define", "undef", "include", "if", "ifdef", "ifndef", "elif",
        "elifdef", "elifndef", "else", "endif", "line", "error",
        "warning", "pragma", "null", "unknown", "define",
    };
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Diagnostic_State diagnostics = {0};
    Noc_File_Id file_id;
    size_t index;
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "src/policy.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    file_id = noc_document_snapshot_file_id(&snapshot);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_TRUSTED_ONLY,
                                      &unit));
    CHECK(noc_preprocessor_unit_is_valid(&unit));
    CHECK(unit.file_id == file_id);
    CHECK(unit.document_generation == 1);
    CHECK(unit.source_class == NOC_SOURCE_CLASS_PROJECT);
    CHECK(unit.macro_policy == NOC_MACROS_TRUSTED_ONLY);
    CHECK(unit.count == sizeof(expected) / sizeof(expected[0]));
    CHECK(unit.disabled_macro_definition_count == 3);
    for (index = 0; index < unit.count; ++index) {
        const Noc_Preprocessor_Directive *directive =
            noc_preprocessor_directive_at(&unit, index);
        CHECK(directive != NULL);
        if (!directive) continue;
        CHECK(directive->kind == expected[index]);
        CHECK(strcmp(noc_preprocessor_directive_kind_name(directive->kind),
                     names[index]) == 0);
        CHECK(directive->token_index < unit.stream.count);
        CHECK(unit.stream.items[directive->token_index].kind ==
              NOC_TOKEN_PREPROCESSOR);
        CHECK(noc_slice_equal(directive->spelling,
                              unit.stream.items[directive->token_index].text));
        CHECK(strcmp(directive->location.path, "src/policy.c") == 0);
        if (directive->kind == NOC_PREPROCESSOR_DIRECTIVE_DEFINE ||
            directive->kind == NOC_PREPROCESSOR_DIRECTIVE_UNDEF) {
            CHECK(!directive->macro_definition_allowed);
        } else {
            CHECK(directive->macro_definition_allowed);
        }
    }
    CHECK(slice_is_identifier(unit.items[0].keyword, "define"));
    CHECK(slice_equals(unit.items[0].payload, "LOCAL 1"));
    CHECK(slice_equals(unit.items[1].payload, "LOCAL"));
    CHECK(slice_equals(unit.items[2].payload, "\"x.h\""));
    CHECK(unit.items[15].keyword.count == 0);
    CHECK(unit.items[15].payload.count == 0);
    CHECK(slice_is_identifier(unit.items[16].keyword, "vendor"));
    CHECK(slice_equals(unit.items[16].payload, "payload"));
    CHECK(slice_is_identifier(unit.items[17].keyword, "define"));
    CHECK(slice_equals(unit.items[17].keyword, "de\\\nfine"));
    CHECK(slice_equals(unit.items[17].payload, "SPLICE 2"));
    CHECK(noc_preprocessor_directive_at(&unit, unit.count) == NULL);

    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    CHECK(noc_preprocessor_unit_is_valid(&unit));
    CHECK(slice_equals(unit.items[0].payload, "LOCAL 1"));
    CHECK(!noc_preprocessor_unit_validate_macro_policy(&context, &unit));
    CHECK(diagnostics.errors == 3);
    CHECK(context.error_count == 3);
    CHECK(strcmp(diagnostics.last_path, "src/policy.c") == 0);
    CHECK(strstr(diagnostics.last_message, "trusted-only") != NULL);
    CHECK(strstr(diagnostics.last_message, "project") != NULL);

    noc_preprocessor_unit_free(&unit);
    CHECK(!noc_preprocessor_unit_is_valid(&unit));
    noc_context_deinit(&context);
}

static void test_policy_validation_and_transactionality(void)
{
    static const char macros[] = "#define VALUE 1\n#undef VALUE\n";
    static const char plain[] = "int value;\n";
    static const char trigraph[] = {'?', '?', '=', 'x', '\n'};
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot trusted = {0};
    Noc_Document_Snapshot generated = {0};
    Noc_Document_Snapshot invalid = {0};
    Noc_Document_Snapshot plain_snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Diagnostic_State diagnostics = {0};
    Noc_Preprocessor_Directive *preserved_items;
    Noc_File_Id preserved_file;
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "trusted.h",
                                      macros,
                                      sizeof(macros) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &trusted) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &trusted,
                                      NOC_MACROS_TRUSTED_ONLY,
                                      &unit));
    CHECK(unit.disabled_macro_definition_count == 0);
    CHECK(noc_preprocessor_unit_validate_macro_policy(&context, &unit));
    CHECK(diagnostics.errors == 0);

    preserved_items = unit.items;
    preserved_file = unit.file_id;
    CHECK(!noc_preprocessor_unit_build(&context,
                                       &trusted,
                                       (Noc_Macro_Policy)99,
                                       &unit));
    CHECK(unit.items == preserved_items && unit.file_id == preserved_file);
    CHECK(diagnostics.errors == 1);
    CHECK(!noc_preprocessor_unit_build(&context,
                                       &invalid,
                                       NOC_MACROS_FULL,
                                       &unit));
    CHECK(unit.items == preserved_items && unit.file_id == preserved_file);

    CHECK(noc_preprocessor_unit_build(&context,
                                      &trusted,
                                      NOC_MACROS_DISABLED,
                                      &unit));
    CHECK(unit.disabled_macro_definition_count == 2);
    CHECK(!noc_preprocessor_unit_validate_macro_policy(&context, &unit));
    CHECK(diagnostics.errors == 3);

    CHECK(noc_workspace_open_document(&workspace,
                                      "generated.h",
                                      macros,
                                      sizeof(macros) - 1,
                                      NOC_SOURCE_CLASS_GENERATED,
                                      &generated) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &generated,
                                      NOC_MACROS_PROJECT,
                                      &unit));
    CHECK(unit.disabled_macro_definition_count == 2);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &generated,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(unit.disabled_macro_definition_count == 0);
    CHECK(noc_preprocessor_unit_validate_macro_policy(&context, &unit));

    CHECK(noc_workspace_open_document(&workspace,
                                      "plain.c",
                                      plain,
                                      sizeof(plain) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &plain_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &plain_snapshot,
                                      NOC_MACROS_TRUSTED_ONLY,
                                      &unit));
    CHECK(unit.count == 0 && unit.items == NULL);
    CHECK(noc_preprocessor_unit_is_valid(&unit));

    CHECK(noc_workspace_open_document(&workspace,
                                      "trigraph.c",
                                      trigraph,
                                      sizeof(trigraph),
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &invalid) == NOC_WORKSPACE_OK);
    preserved_file = unit.file_id;
    CHECK(!noc_preprocessor_unit_build(&context,
                                       &invalid,
                                       NOC_MACROS_FULL,
                                       &unit));
    CHECK(unit.file_id == preserved_file && unit.count == 0);
    CHECK(diagnostics.errors == 4);

    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&plain_snapshot);
    noc_document_snapshot_free(&invalid);
    noc_document_snapshot_free(&generated);
    noc_document_snapshot_free(&trusted);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

int main(void)
{
    test_macro_policy_matrix();
    test_directive_inventory();
    test_policy_validation_and_transactionality();
    return finish_suite("preprocessor");
}
