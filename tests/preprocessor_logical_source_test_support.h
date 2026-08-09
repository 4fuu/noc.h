#ifndef NOC_PREPROCESSOR_LOGICAL_SOURCE_TEST_SUPPORT_H_INCLUDED
#define NOC_PREPROCESSOR_LOGICAL_SOURCE_TEST_SUPPORT_H_INCLUDED

#include "test_support.h"

typedef struct {
    Noc_Context context;
    Noc_Workspace workspace;
    Noc_Document_Snapshot definitions_snapshot;
    Noc_Document_Snapshot input_snapshot;
    Noc_Preprocessor_Unit definitions;
    Noc_Preprocessor_Unit input;
    Noc_Macro_Environment initial_environment;
    Noc_Preprocessor_Conditional_Groups groups;
    Diagnostic_State diagnostics;
} Preprocessor_Logical_Source_Fixture;

static inline void preprocessor_logical_source_fixture_init(
    Preprocessor_Logical_Source_Fixture *fixture,
    const char *definitions,
    const char *input,
    Noc_Source_Class source_class,
    Noc_Macro_Policy macro_policy)
{
    size_t index;
    memset(fixture, 0, sizeof(*fixture));
    noc_context_init(&fixture->context);
    noc_context_set_diagnostic(&fixture->context,
                               count_diagnostics,
                               &fixture->diagnostics);
    noc_workspace_init(&fixture->workspace);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      "preprocessor-definitions.h",
                                      definitions,
                                      strlen(definitions),
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &fixture->definitions_snapshot) ==
          NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      "preprocessor-input.c",
                                      input,
                                      strlen(input),
                                      source_class,
                                      &fixture->input_snapshot) ==
          NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&fixture->context,
                                      &fixture->definitions_snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture->definitions));
    CHECK(noc_preprocessor_unit_build(&fixture->context,
                                      &fixture->input_snapshot,
                                      macro_policy,
                                      &fixture->input));
    for (index = 0;
         index < fixture->definitions.macro_directive_count;
         ++index) {
        CHECK(noc_macro_environment_apply(&fixture->initial_environment,
                                          &fixture->definitions,
                                          index) == NOC_MACRO_ENVIRONMENT_OK);
    }
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture->initial_environment,
              fixture->initial_environment.count,
              &fixture->input,
              noc_macro_expansion_default_limits(),
              &fixture->groups) == NOC_CONDITIONAL_GROUPS_OK);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture->groups));
}

static inline void preprocessor_logical_source_fixture_deinit(
    Preprocessor_Logical_Source_Fixture *fixture)
{
    noc_preprocessor_conditional_groups_free(&fixture->groups);
    noc_macro_environment_free(&fixture->initial_environment);
    noc_preprocessor_unit_free(&fixture->input);
    noc_preprocessor_unit_free(&fixture->definitions);
    noc_document_snapshot_free(&fixture->input_snapshot);
    noc_document_snapshot_free(&fixture->definitions_snapshot);
    noc_workspace_deinit(&fixture->workspace);
    noc_context_deinit(&fixture->context);
}

static inline bool preprocessor_logical_source_text_contains(
    const Noc_Logical_Source *source,
    const char *needle)
{
    Noc_Slice text = noc_logical_source_text(source);
    return text.data && strstr(text.data, needle) != NULL;
}

#endif /* NOC_PREPROCESSOR_LOGICAL_SOURCE_TEST_SUPPORT_H_INCLUDED */
