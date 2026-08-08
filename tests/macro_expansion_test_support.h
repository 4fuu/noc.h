#ifndef NOC_MACRO_EXPANSION_TEST_SUPPORT_H_INCLUDED
#define NOC_MACRO_EXPANSION_TEST_SUPPORT_H_INCLUDED

#include "test_support.h"

typedef struct {
    Noc_Context context;
    Noc_Workspace workspace;
    Noc_Document_Snapshot definitions_snapshot;
    Noc_Document_Snapshot input_snapshot;
    Noc_Preprocessor_Unit definitions;
    Noc_Preprocessor_Unit input;
    Noc_Macro_Environment environment;
    Noc_Macro_Expansion expansion;
    Noc_Buffer rendered;
    Diagnostic_State diagnostics;
} Macro_Expansion_Fixture;

static inline void macro_fixture_init(Macro_Expansion_Fixture *fixture,
                                      const char *definitions,
                                      const char *input)
{
    size_t index;
    memset(fixture, 0, sizeof(*fixture));
    noc_context_init(&fixture->context);
    noc_context_set_diagnostic(&fixture->context,
                               count_diagnostics,
                               &fixture->diagnostics);
    noc_workspace_init(&fixture->workspace);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      "macro-definitions.h",
                                      definitions,
                                      strlen(definitions),
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &fixture->definitions_snapshot) ==
          NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      "macro-input.c",
                                      input,
                                      strlen(input),
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &fixture->input_snapshot) ==
          NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&fixture->context,
                                      &fixture->definitions_snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture->definitions));
    CHECK(noc_preprocessor_unit_build(&fixture->context,
                                      &fixture->input_snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture->input));
    for (index = 0;
         index < fixture->definitions.macro_directive_count;
         ++index) {
        CHECK(noc_macro_environment_apply(&fixture->environment,
                                          &fixture->definitions,
                                          index) == NOC_MACRO_ENVIRONMENT_OK);
    }
}

static inline void macro_fixture_deinit(Macro_Expansion_Fixture *fixture)
{
    noc_buffer_free(&fixture->rendered);
    noc_macro_expansion_free(&fixture->expansion);
    noc_macro_environment_free(&fixture->environment);
    noc_preprocessor_unit_free(&fixture->input);
    noc_preprocessor_unit_free(&fixture->definitions);
    noc_document_snapshot_free(&fixture->input_snapshot);
    noc_document_snapshot_free(&fixture->definitions_snapshot);
    noc_workspace_deinit(&fixture->workspace);
    noc_context_deinit(&fixture->context);
}

static inline Noc_Token_Range macro_fixture_full_input(
    const Macro_Expansion_Fixture *fixture)
{
    Noc_Token_Range range;
    range.begin = 0;
    range.end = fixture->input.preprocessing_token_count - 1;
    return range;
}

static inline Noc_Token_Range macro_fixture_input_line(
    const Macro_Expansion_Fixture *fixture,
    size_t line)
{
    Noc_Token_Range range = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    size_t index;
    for (index = 0; index < fixture->input.preprocessing_token_count; ++index) {
        Noc_Token token = fixture->input.preprocessing_tokens[index].token;
        if (token.kind == NOC_TOKEN_EOF || token.location.line > line) break;
        if (token.location.line != line) continue;
        if (range.begin == NOC_TOKEN_INDEX_NONE) range.begin = index;
        range.end = index + 1;
    }
    return range;
}

static inline Noc_Macro_Expansion_Status macro_fixture_expand(
    Macro_Expansion_Fixture *fixture,
    Noc_Token_Range range)
{
    return noc_macro_expansion_build(&fixture->environment,
                                     fixture->environment.count,
                                     &fixture->input,
                                     range,
                                     noc_macro_expansion_default_limits(),
                                     &fixture->expansion);
}

static inline bool macro_fixture_render_equals(Macro_Expansion_Fixture *fixture,
                                               const char *expected)
{
    if (!noc_macro_expansion_render(&fixture->expansion, &fixture->rendered)) {
        return false;
    }
    return slice_equals(
        (Noc_Slice){fixture->rendered.items, fixture->rendered.count},
        expected);
}

#endif /* NOC_MACRO_EXPANSION_TEST_SUPPORT_H_INCLUDED */
