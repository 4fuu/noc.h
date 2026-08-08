#ifndef NOC_INCLUDE_TEST_SUPPORT_H_INCLUDED
#define NOC_INCLUDE_TEST_SUPPORT_H_INCLUDED

#include "test_support.h"

typedef struct {
    Noc_Context context;
    Noc_Workspace workspace;
    Noc_Document_Snapshot snapshot;
    Noc_Preprocessor_Unit unit;
    Diagnostic_State diagnostics;
} Include_Fixture;

static inline void include_fixture_init(Include_Fixture *fixture,
                                        const char *path,
                                        const char *source)
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
                                      NOC_MACROS_FULL,
                                      &fixture->unit));
}

static inline void include_fixture_deinit(Include_Fixture *fixture)
{
    noc_preprocessor_unit_free(&fixture->unit);
    noc_document_snapshot_free(&fixture->snapshot);
    noc_workspace_deinit(&fixture->workspace);
    noc_context_deinit(&fixture->context);
}

#endif /* NOC_INCLUDE_TEST_SUPPORT_H_INCLUDED */
