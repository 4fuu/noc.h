#include "test_support.h"

static void check_expansion_names_and_defaults(void)
{
    Noc_Macro_Expansion_Limits limits = noc_macro_expansion_default_limits();
    CHECK(strcmp(noc_macro_expansion_status_name(NOC_MACRO_EXPANSION_OK),
                 "ok") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_INVALID_ARGUMENT),
                 "invalid-argument") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(NOC_MACRO_EXPANSION_STALE),
                 "stale") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_INCOMPLETE_INVOCATION),
                 "incomplete-invocation") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH),
                 "argument-count-mismatch") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_INVALID_DEFINITION),
                 "invalid-definition") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_DEPTH_LIMIT),
                 "depth-limit") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_OUTPUT_LIMIT),
                 "output-limit") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_COUNT_LIMIT),
                 "count-limit") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR),
                 "unsupported-operator") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_UNSUPPORTED_VARIADIC),
                 "unsupported-variadic") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_GENERATION_EXHAUSTED),
                 "generation-exhausted") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_OUT_OF_MEMORY),
                 "out-of-memory") == 0);
    CHECK(strcmp(noc_macro_expansion_status_name(
                     (Noc_Macro_Expansion_Status)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_macro_expansion_token_origin_name(
                     NOC_MACRO_EXPANSION_TOKEN_INPUT),
                 "input") == 0);
    CHECK(strcmp(noc_macro_expansion_token_origin_name(
                     NOC_MACRO_EXPANSION_TOKEN_ARGUMENT),
                 "argument") == 0);
    CHECK(strcmp(noc_macro_expansion_token_origin_name(
                     NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT),
                 "replacement") == 0);
    CHECK(strcmp(noc_macro_expansion_token_origin_name(
                     NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION),
                 "stringification") == 0);
    CHECK(strcmp(noc_macro_expansion_token_origin_name(
                     (Noc_Macro_Expansion_Token_Origin)99),
                 "unknown") == 0);
    CHECK(limits.max_depth > 0);
    CHECK(limits.max_output_tokens > limits.max_depth);
    CHECK(limits.max_expansions > limits.max_depth);
}

static void test_recursive_object_expansion_and_provenance(void)
{
    static const char definitions_source[] =
        "#define ONE 1\n"
        "#define TWO ONE + ONE\n"
        "#define EMPTY\n"
        "#define SELF SELF\n"
        "#define A B\n"
        "#define B A\n"
        "#define FUNC(x) x\n"
        "#define PASTE a ## b\n";
    static const char input_source[] = "TWO EMPTY SELF A FUNC(9) MISSING\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot definitions_snapshot = {0};
    Noc_Document_Snapshot input_snapshot = {0};
    Noc_Preprocessor_Unit definitions = {0};
    Noc_Preprocessor_Unit input = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Macro_Expansion expansion = {0};
    Noc_Buffer rendered = {0};
    Diagnostic_State diagnostics = {0};
    Noc_Token_Range input_range;
    size_t argument_token_count = 0;
    size_t input_token_count = 0;
    size_t replacement_token_count = 0;
    size_t index;

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "definitions.h",
                                      definitions_source,
                                      sizeof(definitions_source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &definitions_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "input.c",
                                      input_source,
                                      sizeof(input_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &input_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &definitions_snapshot,
                                      NOC_MACROS_FULL,
                                      &definitions));
    CHECK(noc_preprocessor_unit_build(&context,
                                      &input_snapshot,
                                      NOC_MACROS_FULL,
                                      &input));
    for (index = 0; index < definitions.macro_directive_count; ++index) {
        CHECK(noc_macro_environment_apply(&environment, &definitions, index) ==
              NOC_MACRO_ENVIRONMENT_OK);
    }
    input_range.begin = 0;
    input_range.end = input.preprocessing_token_count - 1;
    CHECK(noc_macro_expansion_build(&environment,
                                    environment.count,
                                    &input,
                                    input_range,
                                    noc_macro_expansion_default_limits(),
                                    &expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_is_valid(&expansion));
    CHECK(expansion.generation == 1);
    CHECK(expansion.frame_count == 8);
    CHECK(noc_macro_expansion_render(&expansion, &rendered));
    CHECK(slice_equals((Noc_Slice){rendered.items, rendered.count},
                       "1 + 1  SELF A 9 MISSING\n"));
    CHECK(rendered.items[rendered.count] == '\0');
    CHECK(noc_macro_expansion_frame_at(&expansion, 0)->environment_entry_index ==
          1);
    CHECK(noc_macro_expansion_frame_at(&expansion, 0)->parent_frame_index ==
          NOC_TOKEN_INDEX_NONE);
    CHECK(noc_macro_expansion_frame_at(&expansion, 1)->environment_entry_index ==
          0);
    CHECK(noc_macro_expansion_frame_at(&expansion, 1)->parent_frame_index == 0);
    CHECK(noc_macro_expansion_frame_at(&expansion, 5)->environment_entry_index ==
          4);
    CHECK(noc_macro_expansion_frame_at(&expansion, 6)->environment_entry_index ==
          5);
    CHECK(noc_macro_expansion_frame_at(&expansion, 6)->parent_frame_index == 5);
    CHECK(noc_macro_expansion_frame_at(&expansion, expansion.frame_count) == NULL);
    for (index = 0; index < expansion.count; ++index) {
        const Noc_Macro_Expansion_Token *token =
            noc_macro_expansion_token_at(&expansion, index);
        CHECK(token != NULL);
        if (!token) continue;
        CHECK(token->preprocessing_token_index <
              token->unit->preprocessing_token_count);
        CHECK(noc_slice_equal(token->token.text,
                              token->unit->preprocessing_tokens[
                                  token->preprocessing_token_index].token.text));
        if (token->origin == NOC_MACRO_EXPANSION_TOKEN_INPUT) {
            CHECK(token->unit == &input);
            CHECK(token->frame_index == NOC_TOKEN_INDEX_NONE);
            input_token_count += 1;
        } else if (token->origin == NOC_MACRO_EXPANSION_TOKEN_ARGUMENT) {
            CHECK(token->unit == &input);
            CHECK(token->frame_index < expansion.frame_count);
            argument_token_count += 1;
        } else {
            CHECK(token->origin == NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT);
            CHECK(token->unit == &definitions);
            CHECK(token->frame_index < expansion.frame_count);
            replacement_token_count += 1;
        }
    }
    CHECK(input_token_count > 0);
    CHECK(argument_token_count > 0);
    CHECK(replacement_token_count > 0);
    CHECK(noc_macro_expansion_token_at(&expansion, expansion.count) == NULL);
    CHECK(diagnostics.errors == 0);

    noc_buffer_free(&rendered);
    noc_macro_expansion_free(&expansion);
    noc_macro_environment_free(&environment);
    noc_preprocessor_unit_free(&input);
    noc_preprocessor_unit_free(&definitions);
    noc_document_snapshot_free(&input_snapshot);
    noc_document_snapshot_free(&definitions_snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_limits_transactionality_and_stale_inputs(void)
{
    static const char definitions_source[] =
        "#define ONE 1\n"
        "#define TWO ONE + ONE\n"
        "#define PASTE a ## b\n"
        "#define DIGRAPH_PASTE a %:%: b\n";
    static const char input_source[] = "TWO\n";
    static const char paste_source[] = "PASTE\n";
    static const char digraph_paste_source[] = "DIGRAPH_PASTE\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot definitions_snapshot = {0};
    Noc_Document_Snapshot input_snapshot = {0};
    Noc_Document_Snapshot paste_snapshot = {0};
    Noc_Document_Snapshot digraph_paste_snapshot = {0};
    Noc_Preprocessor_Unit definitions = {0};
    Noc_Preprocessor_Unit input = {0};
    Noc_Preprocessor_Unit paste = {0};
    Noc_Preprocessor_Unit digraph_paste = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Macro_Expansion expansion = {0};
    Noc_Macro_Expansion_Limits limits = noc_macro_expansion_default_limits();
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Macro_Expansion_Frame *preserved_frames;
    size_t preserved_count;
    size_t preserved_generation;
    size_t index;

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "limits-definitions.h",
                                      definitions_source,
                                      sizeof(definitions_source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &definitions_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "limits-input.c",
                                      input_source,
                                      sizeof(input_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &input_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "paste-input.c",
                                      paste_source,
                                      sizeof(paste_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &paste_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "digraph-paste-input.c",
                                      digraph_paste_source,
                                      sizeof(digraph_paste_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &digraph_paste_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &definitions_snapshot,
                                      NOC_MACROS_FULL,
                                      &definitions));
    CHECK(noc_preprocessor_unit_build(&context,
                                      &input_snapshot,
                                      NOC_MACROS_FULL,
                                      &input));
    CHECK(noc_preprocessor_unit_build(&context,
                                      &paste_snapshot,
                                      NOC_MACROS_FULL,
                                      &paste));
    CHECK(noc_preprocessor_unit_build(&context,
                                      &digraph_paste_snapshot,
                                      NOC_MACROS_FULL,
                                      &digraph_paste));
    for (index = 0; index < definitions.macro_directive_count; ++index) {
        CHECK(noc_macro_environment_apply(&environment, &definitions, index) ==
              NOC_MACRO_ENVIRONMENT_OK);
    }
    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count,
              &input,
              (Noc_Token_Range){0, input.preprocessing_token_count - 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_OK);
    preserved_items = expansion.items;
    preserved_frames = expansion.frames;
    preserved_count = expansion.count;
    preserved_generation = expansion.generation;

    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count,
              &paste,
              (Noc_Token_Range){0, paste.preprocessing_token_count - 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR);
    CHECK(expansion.items == preserved_items && expansion.frames == preserved_frames);
    CHECK(expansion.count == preserved_count);
    CHECK(expansion.generation == preserved_generation);
    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count,
              &digraph_paste,
              (Noc_Token_Range){0, digraph_paste.preprocessing_token_count - 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR);
    CHECK(expansion.items == preserved_items && expansion.frames == preserved_frames);
    CHECK(expansion.count == preserved_count);
    CHECK(expansion.generation == preserved_generation);
    limits.max_depth = 1;
    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count,
              &input,
              (Noc_Token_Range){0, input.preprocessing_token_count - 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_DEPTH_LIMIT);
    limits = noc_macro_expansion_default_limits();
    limits.max_output_tokens = 1;
    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count,
              &input,
              (Noc_Token_Range){0, input.preprocessing_token_count - 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_OUTPUT_LIMIT);
    limits = noc_macro_expansion_default_limits();
    limits.max_expansions = 1;
    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count,
              &input,
              (Noc_Token_Range){0, input.preprocessing_token_count - 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_COUNT_LIMIT);
    limits = noc_macro_expansion_default_limits();
    limits.max_depth = 257;
    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count,
              &input,
              (Noc_Token_Range){0, input.preprocessing_token_count - 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_INVALID_ARGUMENT);
    limits = noc_macro_expansion_default_limits();
    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count + 1,
              &input,
              (Noc_Token_Range){0, input.preprocessing_token_count - 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_INVALID_ARGUMENT);
    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count,
              &input,
              (Noc_Token_Range){1, input.preprocessing_token_count + 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_INVALID_ARGUMENT);
    CHECK(expansion.items == preserved_items && expansion.frames == preserved_frames);
    CHECK(expansion.count == preserved_count);
    CHECK(expansion.generation == preserved_generation);

    CHECK(noc_macro_environment_apply(&environment, &definitions, 0) ==
          NOC_MACRO_ENVIRONMENT_OK);
    CHECK(!noc_macro_expansion_is_valid(&expansion));
    CHECK(noc_macro_expansion_token_at(&expansion, 0) == NULL);
    CHECK(noc_macro_expansion_frame_at(&expansion, 0) == NULL);

    noc_macro_expansion_free(&expansion);
    noc_macro_environment_free(&environment);
    noc_preprocessor_unit_free(&digraph_paste);
    noc_preprocessor_unit_free(&paste);
    noc_preprocessor_unit_free(&input);
    noc_preprocessor_unit_free(&definitions);
    noc_document_snapshot_free(&digraph_paste_snapshot);
    noc_document_snapshot_free(&paste_snapshot);
    noc_document_snapshot_free(&input_snapshot);
    noc_document_snapshot_free(&definitions_snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_history_prefix_and_definition_staleness(void)
{
    static const char definitions_source[] =
        "#define VALUE 1\n"
        "#undef VALUE\n"
        "#define VALUE 2\n";
    static const char input_source[] = "VALUE\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot definitions_snapshot = {0};
    Noc_Document_Snapshot input_snapshot = {0};
    Noc_Preprocessor_Unit definitions = {0};
    Noc_Preprocessor_Unit input = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Macro_Expansion expansion = {0};
    Noc_Buffer rendered = {0};
    Noc_Token_Range input_range;
    size_t index;

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "history-definitions.h",
                                      definitions_source,
                                      sizeof(definitions_source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &definitions_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "history-input.c",
                                      input_source,
                                      sizeof(input_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &input_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &definitions_snapshot,
                                      NOC_MACROS_FULL,
                                      &definitions));
    CHECK(noc_preprocessor_unit_build(&context,
                                      &input_snapshot,
                                      NOC_MACROS_FULL,
                                      &input));
    CHECK(definitions.macro_directive_count == 3);
    for (index = 0; index < definitions.macro_directive_count; ++index) {
        CHECK(noc_macro_environment_apply(&environment, &definitions, index) ==
              NOC_MACRO_ENVIRONMENT_OK);
    }
    input_range.begin = 0;
    input_range.end = input.preprocessing_token_count - 1;

    CHECK(noc_macro_expansion_build(&environment,
                                    1,
                                    &input,
                                    input_range,
                                    noc_macro_expansion_default_limits(),
                                    &expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_render(&expansion, &rendered));
    CHECK(slice_equals((Noc_Slice){rendered.items, rendered.count}, "1\n"));

    CHECK(noc_macro_expansion_build(&environment,
                                    2,
                                    &input,
                                    input_range,
                                    noc_macro_expansion_default_limits(),
                                    &expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_render(&expansion, &rendered));
    CHECK(slice_equals((Noc_Slice){rendered.items, rendered.count}, "VALUE\n"));
    CHECK(expansion.frame_count == 0);

    CHECK(noc_macro_expansion_build(&environment,
                                    3,
                                    &input,
                                    input_range,
                                    noc_macro_expansion_default_limits(),
                                    &expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_render(&expansion, &rendered));
    CHECK(slice_equals((Noc_Slice){rendered.items, rendered.count}, "2\n"));
    CHECK(noc_macro_environment_is_valid(&environment));
    CHECK(noc_macro_expansion_is_valid(&expansion));

    CHECK(noc_preprocessor_unit_build(&context,
                                      &definitions_snapshot,
                                      NOC_MACROS_FULL,
                                      &definitions));
    CHECK(!noc_macro_environment_is_valid(&environment));
    CHECK(!noc_macro_expansion_is_valid(&expansion));

    noc_buffer_free(&rendered);
    noc_macro_expansion_free(&expansion);
    noc_macro_environment_free(&environment);
    noc_preprocessor_unit_free(&input);
    noc_preprocessor_unit_free(&definitions);
    noc_document_snapshot_free(&input_snapshot);
    noc_document_snapshot_free(&definitions_snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_hide_sets_do_not_consume_output_limit(void)
{
    static const char definitions_source[] =
        "#define A B\n"
        "#define B C\n"
        "#define C 1\n";
    static const char input_source[] = "A";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot definitions_snapshot = {0};
    Noc_Document_Snapshot input_snapshot = {0};
    Noc_Preprocessor_Unit definitions = {0};
    Noc_Preprocessor_Unit input = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Macro_Expansion expansion = {0};
    Noc_Buffer rendered = {0};
    Noc_Macro_Expansion_Limits limits = noc_macro_expansion_default_limits();
    size_t index;

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "hide-set-definitions.h",
                                      definitions_source,
                                      sizeof(definitions_source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &definitions_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "hide-set-input.c",
                                      input_source,
                                      sizeof(input_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &input_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &definitions_snapshot,
                                      NOC_MACROS_FULL,
                                      &definitions));
    CHECK(noc_preprocessor_unit_build(&context,
                                      &input_snapshot,
                                      NOC_MACROS_FULL,
                                      &input));
    for (index = 0; index < definitions.macro_directive_count; ++index) {
        CHECK(noc_macro_environment_apply(&environment,
                                          &definitions,
                                          index) == NOC_MACRO_ENVIRONMENT_OK);
    }
    limits.max_depth = 8;
    limits.max_output_tokens = 1;
    limits.max_expansions = 8;
    CHECK(noc_macro_expansion_build(
              &environment,
              environment.count,
              &input,
              (Noc_Token_Range){0, input.preprocessing_token_count - 1},
              limits,
              &expansion) == NOC_MACRO_EXPANSION_OK);
    CHECK(expansion.count == 1);
    CHECK(expansion.frame_count == 3);
    CHECK(noc_macro_expansion_render(&expansion, &rendered));
    CHECK(slice_equals((Noc_Slice){rendered.items, rendered.count}, "1"));

    noc_buffer_free(&rendered);
    noc_macro_expansion_free(&expansion);
    noc_macro_environment_free(&environment);
    noc_preprocessor_unit_free(&input);
    noc_preprocessor_unit_free(&definitions);
    noc_document_snapshot_free(&input_snapshot);
    noc_document_snapshot_free(&definitions_snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

int main(void)
{
    check_expansion_names_and_defaults();
    test_recursive_object_expansion_and_provenance();
    test_limits_transactionality_and_stale_inputs();
    test_history_prefix_and_definition_staleness();
    test_hide_sets_do_not_consume_output_limit();
    return finish_suite("macro-expansion");
}
