#include "test_support.h"

static void check_status_names(void)
{
    CHECK(strcmp(noc_macro_environment_status_name(NOC_MACRO_ENVIRONMENT_OK),
                 "ok") == 0);
    CHECK(strcmp(noc_macro_environment_status_name(
                     NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT),
                 "invalid-argument") == 0);
    CHECK(strcmp(noc_macro_environment_status_name(
                     NOC_MACRO_ENVIRONMENT_INVALID_DIRECTIVE),
                 "invalid-directive") == 0);
    CHECK(strcmp(noc_macro_environment_status_name(NOC_MACRO_ENVIRONMENT_DISABLED),
                 "disabled") == 0);
    CHECK(strcmp(noc_macro_environment_status_name(NOC_MACRO_ENVIRONMENT_STALE),
                 "stale") == 0);
    CHECK(strcmp(noc_macro_environment_status_name(
                     NOC_MACRO_ENVIRONMENT_GENERATION_EXHAUSTED),
                 "generation-exhausted") == 0);
    CHECK(strcmp(noc_macro_environment_status_name(
                     NOC_MACRO_ENVIRONMENT_OUT_OF_MEMORY),
                 "out-of-memory") == 0);
    CHECK(strcmp(noc_macro_environment_status_name(
                     (Noc_Macro_Environment_Status)99),
                 "unknown") == 0);
}

static void test_active_order_and_history(void)
{
    static const char left_source[] =
        "#define A 1\n"
        "#define F(x) x\n"
        "#undef A\n"
        "#define A 2\n"
        "#define SPLI\\\nCE 5\n";
    static const char right_source[] =
        "#define B A\n"
        "#undef F\n"
        "#define A 3\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot left_snapshot = {0};
    Noc_Document_Snapshot right_snapshot = {0};
    Noc_Preprocessor_Unit left = {0};
    Noc_Preprocessor_Unit right = {0};
    Noc_Macro_Environment environment = {0};
    const Noc_Macro_Environment_Entry *entry;
    Diagnostic_State diagnostics = {0};
    size_t index;

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    noc_workspace_init(&workspace);
    CHECK(noc_macro_environment_is_valid(&environment));
    CHECK(noc_macro_environment_lookup(&environment,
                                       noc_slice_from_cstr("A")) == NULL);
    CHECK(noc_workspace_open_document(&workspace,
                                      "left.h",
                                      left_source,
                                      sizeof(left_source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &left_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "right.h",
                                      right_source,
                                      sizeof(right_source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &right_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &left_snapshot,
                                      NOC_MACROS_FULL,
                                      &left));
    CHECK(noc_preprocessor_unit_build(&context,
                                      &right_snapshot,
                                      NOC_MACROS_FULL,
                                      &right));
    CHECK(left.macro_directive_count == 5);
    CHECK(right.macro_directive_count == 3);

    for (index = 0; index < left.macro_directive_count; ++index) {
        CHECK(noc_macro_environment_apply(&environment, &left, index) ==
              NOC_MACRO_ENVIRONMENT_OK);
    }
    for (index = 0; index < right.macro_directive_count; ++index) {
        CHECK(noc_macro_environment_apply(&environment, &right, index) ==
              NOC_MACRO_ENVIRONMENT_OK);
    }
    CHECK(noc_macro_environment_is_valid(&environment));
    CHECK(environment.count == 8);
    CHECK(environment.generation == 8);
    CHECK(environment.items[0].previous_entry_index == NOC_TOKEN_INDEX_NONE);
    CHECK(environment.items[1].previous_entry_index == NOC_TOKEN_INDEX_NONE);
    CHECK(environment.items[2].previous_entry_index == 0);
    CHECK(environment.items[3].previous_entry_index == 2);
    CHECK(environment.items[4].previous_entry_index == NOC_TOKEN_INDEX_NONE);
    CHECK(environment.items[5].previous_entry_index == NOC_TOKEN_INDEX_NONE);
    CHECK(environment.items[6].previous_entry_index == 1);
    CHECK(environment.items[7].previous_entry_index == 3);

    CHECK(noc_macro_environment_lookup_before(&environment,
                                              noc_slice_from_cstr("A"),
                                              0) == NULL);
    entry = noc_macro_environment_lookup_before(&environment,
                                                noc_slice_from_cstr("A"),
                                                1);
    CHECK(entry == &environment.items[0]);
    CHECK(noc_macro_environment_lookup_before(&environment,
                                              noc_slice_from_cstr("A"),
                                              3) == NULL);
    entry = noc_macro_environment_lookup_before(&environment,
                                                noc_slice_from_cstr("A"),
                                                4);
    CHECK(entry == &environment.items[3]);
    entry = noc_macro_environment_lookup(&environment, noc_slice_from_cstr("A"));
    CHECK(entry == &environment.items[7]);
    CHECK(entry && entry->unit == &right);
    CHECK(noc_macro_environment_lookup_before(&environment,
                                              noc_slice_from_cstr("F"),
                                              2) == &environment.items[1]);
    CHECK(noc_macro_environment_lookup(&environment,
                                       noc_slice_from_cstr("F")) == NULL);
    CHECK(noc_macro_environment_lookup(&environment,
                                       noc_slice_from_cstr("SPLICE")) ==
          &environment.items[4]);
    CHECK(noc_macro_environment_lookup(&environment,
                                       noc_slice_from_cstr("MISSING")) == NULL);
    CHECK(noc_macro_environment_lookup_before(&environment,
                                              noc_slice_from_cstr("A"),
                                              environment.count + 1) == NULL);
    CHECK(noc_macro_environment_lookup(&environment, (Noc_Slice){0}) == NULL);
    CHECK(noc_macro_environment_entry_at(&environment, 0) ==
          &environment.items[0]);
    CHECK(noc_macro_environment_entry_at(&environment, environment.count) == NULL);
    CHECK(noc_macro_environment_entry_directive(&environment, 2) ==
          &left.macro_directives[2]);
    CHECK(slice_equals(left.preprocessing_tokens[
                           left.macro_directives[2].name_token_index].token.text,
                       "A"));
    CHECK(noc_macro_environment_entry_directive(&environment,
                                                environment.count) == NULL);
    CHECK(diagnostics.errors == 0);

    noc_macro_environment_free(&environment);
    CHECK(noc_macro_environment_is_valid(&environment));
    CHECK(environment.items == NULL && environment.count == 0);
    CHECK(environment.generation == 8);
    noc_preprocessor_unit_free(&right);
    noc_preprocessor_unit_free(&left);
    noc_document_snapshot_free(&right_snapshot);
    noc_document_snapshot_free(&left_snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_rejected_apply_and_stale_units(void)
{
    static const char source[] =
        "#define GOOD 1\n"
        "#define BAD(\n";
    static const char blocked_source[] = "#define BLOCKED 1\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Document_Snapshot blocked_snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Noc_Preprocessor_Unit blocked = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Macro_Environment exhausted = {0};
    Noc_Macro_Environment corrupt = {0};
    Noc_Macro_Environment_Entry *preserved_items;
    size_t preserved_count;
    size_t preserved_generation;

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "environment.h",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "blocked.h",
                                      blocked_source,
                                      sizeof(blocked_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &blocked_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(noc_preprocessor_unit_build(&context,
                                      &blocked_snapshot,
                                      NOC_MACROS_DISABLED,
                                      &blocked));
    CHECK(noc_macro_environment_apply(&environment, &unit, 0) ==
          NOC_MACRO_ENVIRONMENT_OK);
    preserved_items = environment.items;
    preserved_count = environment.count;
    preserved_generation = environment.generation;
    CHECK(noc_macro_environment_apply(&environment, &unit, 1) ==
          NOC_MACRO_ENVIRONMENT_INVALID_DIRECTIVE);
    CHECK(noc_macro_environment_apply(&environment,
                                      &unit,
                                      unit.macro_directive_count) ==
          NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT);
    CHECK(noc_macro_environment_apply(&environment, &blocked, 0) ==
          NOC_MACRO_ENVIRONMENT_DISABLED);
    CHECK(noc_macro_environment_apply(NULL, &unit, 0) ==
          NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT);
    CHECK(noc_macro_environment_apply(&environment, NULL, 0) ==
          NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT);
    CHECK(environment.items == preserved_items);
    CHECK(environment.count == preserved_count);
    CHECK(environment.generation == preserved_generation);

    exhausted.generation = SIZE_MAX;
    CHECK(noc_macro_environment_apply(&exhausted, &unit, 0) ==
          NOC_MACRO_ENVIRONMENT_GENERATION_EXHAUSTED);
    CHECK(exhausted.items == NULL && exhausted.count == 0);
    corrupt.capacity = 1;
    CHECK(!noc_macro_environment_is_valid(&corrupt));
    CHECK(noc_macro_environment_apply(&corrupt, &unit, 0) ==
          NOC_MACRO_ENVIRONMENT_STALE);

    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(!noc_macro_environment_is_valid(&environment));
    CHECK(noc_macro_environment_entry_at(&environment, 0) == NULL);
    CHECK(noc_macro_environment_entry_directive(&environment, 0) == NULL);
    CHECK(noc_macro_environment_lookup(&environment,
                                       noc_slice_from_cstr("GOOD")) == NULL);
    CHECK(noc_macro_environment_apply(&environment, &unit, 0) ==
          NOC_MACRO_ENVIRONMENT_STALE);
    CHECK(environment.items == preserved_items);
    CHECK(environment.count == preserved_count);
    CHECK(environment.generation == preserved_generation);

    noc_macro_environment_free(&corrupt);
    noc_macro_environment_free(&exhausted);
    noc_macro_environment_free(&environment);
    noc_preprocessor_unit_free(&blocked);
    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&blocked_snapshot);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

int main(void)
{
    check_status_names();
    test_active_order_and_history();
    test_rejected_apply_and_stale_units();
    return finish_suite("macro-environment");
}
