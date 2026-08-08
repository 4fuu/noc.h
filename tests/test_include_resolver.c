#include "include_test_support.h"

typedef struct {
    const Noc_Document_Snapshot *target;
    Noc_Include_Resolve_Status status;
    Noc_Include_Request last_request;
    char last_path[64];
    char last_name[64];
    size_t call_count;
    bool publish_on_non_found;
    bool omit_found_snapshot;
} Resolver_State;

static Noc_Include_Resolve_Status resolve_from_state(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *resolved_snapshot)
{
    Resolver_State *state = (Resolver_State *)user_data;
    CHECK(!noc_document_snapshot_is_valid(resolved_snapshot));
    CHECK(strlen(request->including_path) < sizeof(state->last_path));
    CHECK(request->logical_name.count < sizeof(state->last_name));
    strcpy(state->last_path, request->including_path);
    memcpy(state->last_name,
           request->logical_name.data,
           request->logical_name.count);
    state->last_name[request->logical_name.count] = '\0';
    state->last_request = *request;
    state->last_request.including_path = state->last_path;
    state->last_request.directive_location.path = state->last_path;
    state->last_request.logical_name.data = state->last_name;
    state->call_count += 1;
    if ((state->status == NOC_INCLUDE_RESOLVE_FOUND &&
         !state->omit_found_snapshot) ||
        (state->status != NOC_INCLUDE_RESOLVE_FOUND &&
         state->publish_on_non_found)) {
        CHECK(noc_document_snapshot_clone(state->target, resolved_snapshot) ==
              NOC_WORKSPACE_OK);
    }
    return state->status;
}

static void test_status_names_and_request_contract(void)
{
    static const Noc_Include_Resolve_Status statuses[] = {
        NOC_INCLUDE_RESOLVE_FOUND,
        NOC_INCLUDE_RESOLVE_NOT_FOUND,
        NOC_INCLUDE_RESOLVE_AMBIGUOUS,
        NOC_INCLUDE_RESOLVE_DENIED,
        NOC_INCLUDE_RESOLVE_CANCELLED,
        NOC_INCLUDE_RESOLVE_FAILED,
        NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT,
        NOC_INCLUDE_RESOLVE_STALE,
        NOC_INCLUDE_RESOLVE_OUT_OF_MEMORY,
        NOC_INCLUDE_RESOLVE_INVALID_RESULT,
    };
    static const char *const names[] = {
        "found", "not-found", "ambiguous", "denied", "cancelled", "failed",
        "invalid-argument", "stale", "out-of-memory", "invalid-result",
    };
    Include_Fixture fixture;
    Noc_Document_Snapshot target = {0};
    Noc_Document_Snapshot output = {0};
    Noc_Include_Operand operand = {0};
    Resolver_State state = {0};
    Noc_Include_Resolver resolver;
    size_t index;
    Noc_Document_Snapshot_Impl *previous_output;

    for (index = 0; index < sizeof(statuses) / sizeof(statuses[0]); ++index) {
        CHECK(strcmp(noc_include_resolve_status_name(statuses[index]),
                     names[index]) == 0);
    }
    CHECK(strcmp(noc_include_resolve_status_name(
                     (Noc_Include_Resolve_Status)99),
                 "unknown") == 0);

    include_fixture_init(&fixture,
                         "src/main.c",
                         "#include \"library/api.h\"\n");
    CHECK(noc_workspace_open_document(&fixture.workspace,
                                      "include/library/api.h",
                                      "int api(void);\n",
                                      15,
                                      NOC_SOURCE_CLASS_SYSTEM,
                                      &target) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&fixture.workspace,
                                      "previous.h",
                                      "old\n",
                                      4,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &output) == NOC_WORKSPACE_OK);
    previous_output = output.impl;
    CHECK(noc_include_operand_build(&fixture.unit, 0, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    state.target = &target;
    state.status = NOC_INCLUDE_RESOLVE_FOUND;
    resolver.resolve = resolve_from_state;
    resolver.user_data = &state;
    CHECK(noc_include_resolve(resolver, &operand, &output) ==
          NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(state.call_count == 1);
    CHECK(strcmp(state.last_request.including_path, "src/main.c") == 0);
    CHECK(state.last_request.including_file_id == fixture.unit.file_id);
    CHECK(state.last_request.including_document_generation ==
          fixture.unit.document_generation);
    CHECK(state.last_request.including_source_class ==
          NOC_SOURCE_CLASS_PROJECT);
    CHECK(state.last_request.directive_index == 0);
    CHECK(state.last_request.directive_location.line == 1);
    CHECK(state.last_request.form == NOC_INCLUDE_FORM_QUOTED);
    CHECK(slice_equals(state.last_request.logical_name, "library/api.h"));
    CHECK(noc_document_snapshot_is_valid(&output));
    CHECK(output.impl != previous_output);
    CHECK(strcmp(noc_document_snapshot_path(&output),
                 "include/library/api.h") == 0);
    CHECK(noc_document_snapshot_source_class(&output) ==
          NOC_SOURCE_CLASS_SYSTEM);

    noc_include_operand_free(&operand);
    noc_document_snapshot_free(&output);
    noc_document_snapshot_free(&target);
    include_fixture_deinit(&fixture);
}

static void test_outcomes_preserve_output_and_validate_callback(void)
{
    static const Noc_Include_Resolve_Status recoverable[] = {
        NOC_INCLUDE_RESOLVE_NOT_FOUND,
        NOC_INCLUDE_RESOLVE_AMBIGUOUS,
        NOC_INCLUDE_RESOLVE_DENIED,
        NOC_INCLUDE_RESOLVE_CANCELLED,
        NOC_INCLUDE_RESOLVE_FAILED,
        NOC_INCLUDE_RESOLVE_OUT_OF_MEMORY,
    };
    Include_Fixture fixture;
    Noc_Document_Snapshot target = {0};
    Noc_Document_Snapshot output = {0};
    Noc_Include_Operand operand = {0};
    Resolver_State state = {0};
    Noc_Include_Resolver resolver = {resolve_from_state, &state};
    Noc_Document_Snapshot_Impl *preserved_impl;
    size_t index;

    include_fixture_init(&fixture,
                         "root.c",
                         "#include <target.h>\n#include <>\n");
    CHECK(noc_workspace_open_document(&fixture.workspace,
                                      "target.h",
                                      "target\n",
                                      7,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &target) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&fixture.workspace,
                                      "preserved.h",
                                      "preserved\n",
                                      10,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &output) == NOC_WORKSPACE_OK);
    preserved_impl = output.impl;
    CHECK(noc_include_operand_build(&fixture.unit, 0, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    state.target = &target;
    for (index = 0; index < sizeof(recoverable) / sizeof(recoverable[0]); ++index) {
        state.status = recoverable[index];
        CHECK(noc_include_resolve(resolver, &operand, &output) ==
              recoverable[index]);
        CHECK(output.impl == preserved_impl);
    }

    state.status = NOC_INCLUDE_RESOLVE_FOUND;
    state.omit_found_snapshot = true;
    CHECK(noc_include_resolve(resolver, &operand, &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_RESULT);
    CHECK(output.impl == preserved_impl);
    state.omit_found_snapshot = false;
    state.status = NOC_INCLUDE_RESOLVE_NOT_FOUND;
    state.publish_on_non_found = true;
    CHECK(noc_include_resolve(resolver, &operand, &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_RESULT);
    CHECK(output.impl == preserved_impl);
    state.publish_on_non_found = false;
    state.status = NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT;
    CHECK(noc_include_resolve(resolver, &operand, &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_RESULT);
    CHECK(output.impl == preserved_impl);

    CHECK(noc_include_resolve((Noc_Include_Resolver){0}, &operand, &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT);
    CHECK(noc_include_resolve(resolver, NULL, &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT);
    CHECK(noc_include_resolve(resolver, &operand, NULL) ==
          NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT);
    CHECK(output.impl == preserved_impl);

    CHECK(noc_include_operand_build(&fixture.unit, 1, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(operand.status == NOC_INCLUDE_OPERAND_EMPTY);
    CHECK(noc_include_resolve(resolver, &operand, &output) ==
          NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT);
    CHECK(output.impl == preserved_impl);

    noc_include_operand_free(&operand);
    noc_document_snapshot_free(&output);
    noc_document_snapshot_free(&target);
    include_fixture_deinit(&fixture);
}

static void test_stale_operand_rejected_without_callback(void)
{
    Include_Fixture fixture;
    Noc_Document_Snapshot target = {0};
    Noc_Document_Snapshot output = {0};
    Noc_Include_Operand operand = {0};
    Resolver_State state = {0};
    Noc_Include_Resolver resolver = {resolve_from_state, &state};

    include_fixture_init(&fixture, "root.c", "#include \"target.h\"\n");
    CHECK(noc_workspace_open_document(&fixture.workspace,
                                      "target.h",
                                      "",
                                      0,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &target) == NOC_WORKSPACE_OK);
    CHECK(noc_include_operand_build(&fixture.unit, 0, &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    state.target = &target;
    state.status = NOC_INCLUDE_RESOLVE_FOUND;
    noc_preprocessor_unit_free(&fixture.unit);
    CHECK(!noc_include_operand_is_valid(&operand));
    CHECK(noc_include_resolve(resolver, &operand, &output) ==
          NOC_INCLUDE_RESOLVE_STALE);
    CHECK(state.call_count == 0);
    CHECK(!noc_document_snapshot_is_valid(&output));

    noc_include_operand_free(&operand);
    noc_document_snapshot_free(&target);
    include_fixture_deinit(&fixture);
}

int main(void)
{
    test_status_names_and_request_contract();
    test_outcomes_preserve_output_and_validate_callback();
    test_stale_operand_rejected_without_callback();
    return finish_suite("include-resolver");
}
