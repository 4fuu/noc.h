#include "preprocessor_logical_source_test_support.h"

static void test_defaults_names_active_ranges_provenance_and_ast(void)
{
    static const Noc_Preprocessor_Logical_Source_Status statuses[] = {
        NOC_PREPROCESSOR_LOGICAL_SOURCE_OK,
        NOC_PREPROCESSOR_LOGICAL_SOURCE_INVALID_ARGUMENT,
        NOC_PREPROCESSOR_LOGICAL_SOURCE_STALE,
        NOC_PREPROCESSOR_LOGICAL_SOURCE_UNRESOLVED,
        NOC_PREPROCESSOR_LOGICAL_SOURCE_UNSUPPORTED_DIRECTIVE,
        NOC_PREPROCESSOR_LOGICAL_SOURCE_EXPANSION_FAILED,
        NOC_PREPROCESSOR_LOGICAL_SOURCE_LOGICAL_SOURCE_FAILED,
        NOC_PREPROCESSOR_LOGICAL_SOURCE_CANCELLED,
        NOC_PREPROCESSOR_LOGICAL_SOURCE_LIMIT_EXCEEDED,
        NOC_PREPROCESSOR_LOGICAL_SOURCE_OUT_OF_MEMORY,
    };
    static const char *const status_names[] = {
        "ok",
        "invalid-argument",
        "stale",
        "unresolved",
        "unsupported-directive",
        "expansion-failed",
        "logical-source-failed",
        "cancelled",
        "limit-exceeded",
        "out-of-memory",
    };
    static const char definitions[] = "#define BASE 7\n";
    static const char input[] =
        "#define ITEM(name) int name = BASE;\n"
        "#if BASE == 7\n"
        "ITEM(active)\n"
        "#else\n"
        "ITEM(inactive)\n"
        "#endif\n"
        "#undef BASE\n"
        "#define BASE 9\n"
        "int tail = BASE;\n";
    Preprocessor_Logical_Source_Fixture fixture;
    Noc_Preprocessor_Logical_Source_Options options =
        noc_preprocessor_logical_source_default_options();
    Noc_Preprocessor_Logical_Source_Result result;
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    Noc_Logical_C_Ast ast = {0};
    size_t index;
    bool saw_input_provenance = false;
    bool saw_definition_provenance = false;

    for (index = 0; index < sizeof(statuses) / sizeof(statuses[0]); ++index) {
        CHECK(strcmp(noc_preprocessor_logical_source_status_name(statuses[index]),
                     status_names[index]) == 0);
    }
    CHECK(strcmp(noc_preprocessor_logical_source_status_name(
                     (Noc_Preprocessor_Logical_Source_Status)99),
                 "unknown") == 0);
    CHECK(options.macro_expansion.limits.max_depth != 0);
    CHECK(options.logical_source.max_fragments != 0);

    preprocessor_logical_source_fixture_init(&fixture,
                                             definitions,
                                             input,
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_FULL);
    CHECK(noc_preprocessor_conditional_groups_is_fully_resolved(
        &fixture.groups));
    result = noc_preprocessor_logical_source_build(&fixture.groups,
                                                   options,
                                                   &source);
    CHECK(result.status == NOC_PREPROCESSOR_LOGICAL_SOURCE_OK);
    CHECK(result.problem_directive_index == NOC_TOKEN_INDEX_NONE);
    CHECK(result.problem_tokens.begin == NOC_TOKEN_INDEX_NONE);
    CHECK(result.expansion_status == NOC_MACRO_EXPANSION_OK);
    CHECK(result.logical_source_status == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_is_valid(&source));
    CHECK(preprocessor_logical_source_text_contains(&source, "int active = 7"));
    CHECK(preprocessor_logical_source_text_contains(&source, "int tail = 9"));
    CHECK(!preprocessor_logical_source_text_contains(&source, "inactive"));
    CHECK(!preprocessor_logical_source_text_contains(&source, "#define"));
    CHECK(!preprocessor_logical_source_text_contains(&source, "#if"));

    for (index = 0;
         index < noc_logical_source_token_count(&source);
         ++index) {
        Noc_Logical_Token_Macro_Provenance provenance;
        const Noc_Logical_Source_File *file;
        if (!noc_logical_source_token_macro_provenance(&source,
                                                        index,
                                                        &provenance)) {
            continue;
        }
        file = noc_logical_source_file_at(&source,
                                          provenance.anchor.file_index);
        CHECK(file != NULL);
        if (file && file->file_id == fixture.input.file_id) {
            saw_input_provenance = true;
        }
        if (file && file->file_id == fixture.definitions.file_id) {
            saw_definition_provenance = true;
        }
    }
    CHECK(saw_input_provenance);
    CHECK(saw_definition_provenance);
    CHECK(noc_logical_source_macro_frame_count(&source) >= 3);

    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    CHECK(!noc_logical_c_parse_tree_has_error(&tree));
    CHECK(noc_logical_c_ast_build(&tree,
                                  noc_c_ast_default_options(),
                                  &ast) == NOC_C_AST_OK);
    CHECK(noc_logical_c_ast_is_syntax_complete(&ast));

    noc_logical_c_ast_free(&ast);
    noc_logical_c_parse_tree_free(&tree);
    noc_logical_source_free(&source);
    preprocessor_logical_source_fixture_deinit(&fixture);
}

static void test_empty_inactive_directives_and_boundary_separator(void)
{
    static const char input[] =
        "int\n"
        "#define JOIN_GUARD 1\n"
        "value;\n"
        "#if 0\n"
        "#include \"inactive.h\"\n"
        "#endif\n";
    Preprocessor_Logical_Source_Fixture fixture;
    Preprocessor_Logical_Source_Fixture empty_fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source empty = {0};
    Noc_Preprocessor_Logical_Source_Result result;
    size_t index;
    bool saw_generated_separator = false;

    preprocessor_logical_source_fixture_init(&fixture,
                                             "",
                                             input,
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_FULL);
    result = noc_preprocessor_logical_source_build(
        &fixture.groups,
        noc_preprocessor_logical_source_default_options(),
        &source);
    CHECK(result.status == NOC_PREPROCESSOR_LOGICAL_SOURCE_OK);
    CHECK(preprocessor_logical_source_text_contains(&source, "int"));
    CHECK(preprocessor_logical_source_text_contains(&source, "value"));
    CHECK(!preprocessor_logical_source_text_contains(&source, "inactive.h"));
    for (index = 0;
         index < noc_logical_source_token_count(&source);
         ++index) {
        const Noc_Logical_Token *token =
            noc_logical_source_token_at(&source, index);
        if (token &&
            (token->flags & NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) != 0) {
            saw_generated_separator = true;
        }
    }
    CHECK(saw_generated_separator);

    preprocessor_logical_source_fixture_init(&empty_fixture,
                                             "",
                                             "#define ONLY 1\n#if ONLY\n#endif\n",
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_FULL);
    result = noc_preprocessor_logical_source_build(
        &empty_fixture.groups,
        noc_preprocessor_logical_source_default_options(),
        &empty);
    CHECK(result.status == NOC_PREPROCESSOR_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_is_valid(&empty));
    CHECK(noc_logical_source_text(&empty).data != NULL);
    CHECK(slice_equals(noc_logical_source_text(&empty), ""));
    CHECK(noc_logical_source_token_count(&empty) == 0);

    noc_logical_source_free(&empty);
    noc_logical_source_free(&source);
    preprocessor_logical_source_fixture_deinit(&empty_fixture);
    preprocessor_logical_source_fixture_deinit(&fixture);
}

static void test_rejected_directives_unresolved_and_expansion_failure(void)
{
    Preprocessor_Logical_Source_Fixture include_fixture;
    Preprocessor_Logical_Source_Fixture policy_fixture;
    Preprocessor_Logical_Source_Fixture unresolved_fixture;
    Preprocessor_Logical_Source_Fixture expansion_fixture;
    Noc_Logical_Source output = {0};
    Noc_Preprocessor_Logical_Source_Result result;

    preprocessor_logical_source_fixture_init(&include_fixture,
                                             "",
                                             "#include \"active.h\"\nint x;\n",
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_FULL);
    result = noc_preprocessor_logical_source_build(
        &include_fixture.groups,
        noc_preprocessor_logical_source_default_options(),
        &output);
    CHECK(result.status ==
          NOC_PREPROCESSOR_LOGICAL_SOURCE_UNSUPPORTED_DIRECTIVE);
    CHECK(result.problem_directive_index == 0);
    CHECK(!noc_logical_source_is_valid(&output));

    preprocessor_logical_source_fixture_init(&policy_fixture,
                                             "",
                                             "#define DENIED 1\nint x;\n",
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_DISABLED);
    result = noc_preprocessor_logical_source_build(
        &policy_fixture.groups,
        noc_preprocessor_logical_source_default_options(),
        &output);
    CHECK(result.status ==
          NOC_PREPROCESSOR_LOGICAL_SOURCE_UNSUPPORTED_DIRECTIVE);
    CHECK(result.problem_directive_index == 0);

    preprocessor_logical_source_fixture_init(&unresolved_fixture,
                                             "",
                                             "#if (\nint x;\n#endif\n",
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_FULL);
    result = noc_preprocessor_logical_source_build(
        &unresolved_fixture.groups,
        noc_preprocessor_logical_source_default_options(),
        &output);
    CHECK(result.status == NOC_PREPROCESSOR_LOGICAL_SOURCE_UNRESOLVED);

    preprocessor_logical_source_fixture_init(&expansion_fixture,
                                             "",
                                             "#define F(x) x\nint x = F(;\n",
                                             NOC_SOURCE_CLASS_PROJECT,
                                             NOC_MACROS_FULL);
    result = noc_preprocessor_logical_source_build(
        &expansion_fixture.groups,
        noc_preprocessor_logical_source_default_options(),
        &output);
    CHECK(result.status ==
          NOC_PREPROCESSOR_LOGICAL_SOURCE_EXPANSION_FAILED);
    CHECK(result.expansion_status ==
          NOC_MACRO_EXPANSION_INCOMPLETE_INVOCATION);
    CHECK(result.problem_tokens.begin != NOC_TOKEN_INDEX_NONE);

    noc_logical_source_free(&output);
    preprocessor_logical_source_fixture_deinit(&expansion_fixture);
    preprocessor_logical_source_fixture_deinit(&unresolved_fixture);
    preprocessor_logical_source_fixture_deinit(&policy_fixture);
    preprocessor_logical_source_fixture_deinit(&include_fixture);
}

int main(void)
{
    test_defaults_names_active_ranges_provenance_and_ast();
    test_empty_inactive_directives_and_boundary_separator();
    test_rejected_directives_unresolved_and_expansion_failure();
    return finish_suite("preprocessor logical source");
}
