#ifndef NOC_INCLUDE_CONTROL_TEST_SUPPORT_H_INCLUDED
#define NOC_INCLUDE_CONTROL_TEST_SUPPORT_H_INCLUDED

#include "test_support.h"

typedef struct {
    Noc_Context context;
    Noc_Workspace workspace;
    Noc_Document_Snapshot snapshot;
    Noc_Preprocessor_Unit unit;
    Noc_Macro_Environment initial_environment;
    Noc_Preprocessor_Conditional_Groups groups;
    Diagnostic_State diagnostics;
} Include_Control_Fixture;

static inline void include_control_fixture_init(
    Include_Control_Fixture *fixture,
    const char *path,
    const char *source,
    Noc_Macro_Policy macro_policy)
{
    memset(fixture, 0, sizeof(*fixture));
    noc_context_init(&fixture->context);
    noc_context_set_diagnostic(&fixture->context,
                               count_diagnostics,
                               &fixture->diagnostics);
    noc_workspace_init(&fixture->workspace);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      path,
                                      source,
                                      strlen(source),
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &fixture->snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&fixture->context,
                                      &fixture->snapshot,
                                      macro_policy,
                                      &fixture->unit));
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture->initial_environment,
              0,
              &fixture->unit,
              noc_macro_expansion_default_limits(),
              &fixture->groups) == NOC_CONDITIONAL_GROUPS_OK);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture->groups));
}

static inline void include_control_fixture_rebuild_groups(
    Include_Control_Fixture *fixture)
{
    CHECK(noc_preprocessor_conditional_groups_build(
              &fixture->initial_environment,
              0,
              &fixture->unit,
              noc_macro_expansion_default_limits(),
              &fixture->groups) == NOC_CONDITIONAL_GROUPS_OK);
    CHECK(noc_preprocessor_conditional_groups_is_valid(&fixture->groups));
}

static inline size_t include_control_fixture_directive(
    const Include_Control_Fixture *fixture,
    Noc_Preprocessor_Directive_Kind kind,
    size_t occurrence)
{
    size_t index;
    for (index = 0; index < fixture->unit.count; ++index) {
        if (fixture->unit.items[index].kind != kind) continue;
        if (occurrence == 0) return index;
        occurrence -= 1;
    }
    return NOC_TOKEN_INDEX_NONE;
}

static inline void include_control_fixture_deinit(
    Include_Control_Fixture *fixture)
{
    noc_preprocessor_conditional_groups_free(&fixture->groups);
    noc_macro_environment_free(&fixture->initial_environment);
    noc_preprocessor_unit_free(&fixture->unit);
    noc_document_snapshot_free(&fixture->snapshot);
    noc_workspace_deinit(&fixture->workspace);
    noc_context_deinit(&fixture->context);
}

#endif /* NOC_INCLUDE_CONTROL_TEST_SUPPORT_H_INCLUDED */
