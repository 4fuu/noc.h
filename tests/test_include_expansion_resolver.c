#include "include_expansion_test_support.h"

typedef struct {
    const Noc_Document_Snapshot *target;
    Noc_Include_Resolve_Status status;
    char path[64];
    char name[64];
    Noc_Include_Form form;
    size_t directive_index;
    size_t call_count;
    bool publish_on_failure;
} Expanded_Resolver_State;

static Noc_Include_Resolve_Status resolve_expanded(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *output)
{
    Expanded_Resolver_State *state = (Expanded_Resolver_State *)user_data;
    CHECK(!noc_document_snapshot_is_valid(output));
    CHECK(strlen(request->including_path) < sizeof(state->path));
    CHECK(request->logical_name.count < sizeof(state->name));
    strcpy(state->path, request->including_path);
    memcpy(state->name, request->logical_name.data, request->logical_name.count);
    state->name[request->logical_name.count] = '\0';
    state->form = request->form;
    state->directive_index = request->directive_index;
    state->call_count += 1;
    if (state->status == NOC_INCLUDE_RESOLVE_FOUND || state->publish_on_failure) {
        CHECK(noc_document_snapshot_clone(state->target, output) ==
              NOC_WORKSPACE_OK);
    }
    return state->status;
}

static void test_expanded_resolution_and_failure_transactionality(void)
{
    static const char definitions[] =
        "#define HEADER \"generated/api.h\"\n"
        "#define EMPTY\n";
    static const char input[] =
        "#include HEADER\n"
        "#include EMPTY\n";
    Macro_Expansion_Fixture fixture;
    Noc_Document_Snapshot target = {0};
    Noc_Document_Snapshot output = {0};
    Noc_Include_Expansion expansion = {0};
    Expanded_Resolver_State state = {0};
    Noc_Include_Resolver resolver = {resolve_expanded, &state};
    Noc_Document_Snapshot_Impl *preserved;
    size_t saved_environment_generation;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(noc_workspace_open_document(&fixture.workspace,
                                      "include/generated/api.h",
                                      "api\n",
                                      4,
                                      NOC_SOURCE_CLASS_GENERATED,
                                      &target) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&fixture.workspace,
                                      "preserved.h",
                                      "old\n",
                                      4,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &output) == NOC_WORKSPACE_OK);
    CHECK(include_expansion_build_at(&fixture, 0, &expansion) ==
          NOC_MACRO_EXPANSION_OK);
    state.target = &target;
    state.status = NOC_INCLUDE_RESOLVE_FOUND;
    CHECK(noc_include_expansion_resolve(resolver, &expansion, &output) ==
          NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(state.call_count == 1);
    CHECK(strcmp(state.path, "macro-input.c") == 0);
    CHECK(strcmp(state.name, "generated/api.h") == 0);
    CHECK(state.form == NOC_INCLUDE_FORM_QUOTED);
    CHECK(state.directive_index == 0);
    CHECK(strcmp(noc_document_snapshot_path(&output),
                 "include/generated/api.h") == 0);
    CHECK(noc_document_snapshot_source_class(&output) ==
          NOC_SOURCE_CLASS_GENERATED);

    preserved = output.impl;
    state.status = NOC_INCLUDE_RESOLVE_NOT_FOUND;
    CHECK(noc_include_expansion_resolve(resolver, &expansion, &output) ==
          NOC_INCLUDE_RESOLVE_NOT_FOUND);
    CHECK(output.impl == preserved);
    state.publish_on_failure = true;
    CHECK(noc_include_expansion_resolve(resolver, &expansion, &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_RESULT);
    CHECK(output.impl == preserved);
    state.publish_on_failure = false;

    CHECK(include_expansion_build_at(&fixture, 1, &expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(expansion.status == NOC_INCLUDE_OPERAND_EMPTY);
    state.call_count = 0;
    CHECK(noc_include_expansion_resolve(resolver, &expansion, &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT);
    CHECK(state.call_count == 0);
    CHECK(output.impl == preserved);

    CHECK(include_expansion_build_at(&fixture, 0, &expansion) ==
          NOC_MACRO_EXPANSION_OK);
    saved_environment_generation = fixture.environment.generation;
    fixture.environment.generation += 1;
    CHECK(!noc_include_expansion_is_valid(&expansion));
    CHECK(noc_include_expansion_resolve(resolver, &expansion, &output) ==
          NOC_INCLUDE_RESOLVE_STALE);
    CHECK(state.call_count == 0);
    CHECK(output.impl == preserved);
    fixture.environment.generation = saved_environment_generation;
    CHECK(noc_include_expansion_is_valid(&expansion));

    CHECK(noc_include_expansion_resolve((Noc_Include_Resolver){0},
                                        &expansion,
                                        &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT);
    CHECK(noc_include_expansion_resolve(resolver, NULL, &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT);
    CHECK(noc_include_expansion_resolve(resolver, &expansion, NULL) ==
          NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT);
    CHECK(output.impl == preserved);

    noc_include_expansion_free(&expansion);
    noc_document_snapshot_free(&output);
    noc_document_snapshot_free(&target);
    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_expanded_resolution_and_failure_transactionality();
    return finish_suite("include-expansion-resolver");
}
