#ifndef NOC_INCLUDE_GRAPH_TEST_SUPPORT_H_INCLUDED
#define NOC_INCLUDE_GRAPH_TEST_SUPPORT_H_INCLUDED

#include "test_support.h"

enum {
    GRAPH_FIXTURE_MAX_DOCUMENTS = 32,
    GRAPH_FIXTURE_MAX_BINDINGS = 64,
    GRAPH_FIXTURE_MAX_CALLS = 64,
    GRAPH_FIXTURE_NAME_CAPACITY = 96,
};

typedef struct {
    const char *logical_name;
    size_t document_index;
    Noc_Include_Resolve_Status status;
} Graph_Resolver_Binding;

typedef struct {
    Noc_Context context;
    Noc_Workspace workspace;
    Diagnostic_State diagnostics;
    Noc_Document_Snapshot documents[GRAPH_FIXTURE_MAX_DOCUMENTS];
    size_t document_count;
    Graph_Resolver_Binding bindings[GRAPH_FIXTURE_MAX_BINDINGS];
    size_t binding_count;
    char call_names[GRAPH_FIXTURE_MAX_CALLS][GRAPH_FIXTURE_NAME_CAPACITY];
    char call_paths[GRAPH_FIXTURE_MAX_CALLS][GRAPH_FIXTURE_NAME_CAPACITY];
    size_t call_count;
    bool omit_found_snapshot;
    bool publish_snapshot_on_failure;
} Include_Graph_Fixture;

static inline size_t graph_fixture_add_document(
    Include_Graph_Fixture *fixture,
    const char *path,
    const char *source,
    Noc_Source_Class source_class)
{
    size_t index = fixture->document_count;
    CHECK(index < GRAPH_FIXTURE_MAX_DOCUMENTS);
    if (index >= GRAPH_FIXTURE_MAX_DOCUMENTS) return NOC_TOKEN_INDEX_NONE;
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      path,
                                      source,
                                      strlen(source),
                                      source_class,
                                      &fixture->documents[index]) ==
          NOC_WORKSPACE_OK);
    fixture->document_count += 1;
    return index;
}

static inline void graph_fixture_init(Include_Graph_Fixture *fixture,
                                      const char *root_source)
{
    memset(fixture, 0, sizeof(*fixture));
    noc_context_init(&fixture->context);
    noc_context_set_diagnostic(&fixture->context,
                               count_diagnostics,
                               &fixture->diagnostics);
    noc_workspace_init(&fixture->workspace);
    CHECK(graph_fixture_add_document(fixture,
                                     "root.c",
                                     root_source,
                                     NOC_SOURCE_CLASS_PROJECT) == 0);
}

static inline void graph_fixture_bind(Include_Graph_Fixture *fixture,
                                      const char *logical_name,
                                      size_t document_index,
                                      Noc_Include_Resolve_Status status)
{
    size_t index = fixture->binding_count;
    CHECK(index < GRAPH_FIXTURE_MAX_BINDINGS);
    CHECK(document_index == NOC_TOKEN_INDEX_NONE ||
          document_index < fixture->document_count);
    if (index >= GRAPH_FIXTURE_MAX_BINDINGS) return;
    fixture->bindings[index].logical_name = logical_name;
    fixture->bindings[index].document_index = document_index;
    fixture->bindings[index].status = status;
    fixture->binding_count += 1;
}

static inline const Graph_Resolver_Binding *graph_fixture_find_binding(
    const Include_Graph_Fixture *fixture,
    Noc_Slice logical_name)
{
    size_t index;
    for (index = 0; index < fixture->binding_count; ++index) {
        const Graph_Resolver_Binding *binding = &fixture->bindings[index];
        if (slice_equals(logical_name, binding->logical_name)) {
            return binding;
        }
    }
    return NULL;
}

static inline Noc_Include_Resolve_Status graph_fixture_resolve(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *output)
{
    Include_Graph_Fixture *fixture = (Include_Graph_Fixture *)user_data;
    const Graph_Resolver_Binding *binding =
        graph_fixture_find_binding(fixture, request->logical_name);
    Noc_Include_Resolve_Status status = binding ?
        binding->status : NOC_INCLUDE_RESOLVE_NOT_FOUND;
    size_t call_index = fixture->call_count;
    CHECK(!noc_document_snapshot_is_valid(output));
    CHECK(call_index < GRAPH_FIXTURE_MAX_CALLS);
    CHECK(request->logical_name.count < GRAPH_FIXTURE_NAME_CAPACITY);
    CHECK(strlen(request->including_path) < GRAPH_FIXTURE_NAME_CAPACITY);
    if (call_index < GRAPH_FIXTURE_MAX_CALLS) {
        size_t name_count = request->logical_name.count;
        if (name_count >= GRAPH_FIXTURE_NAME_CAPACITY) {
            name_count = GRAPH_FIXTURE_NAME_CAPACITY - 1;
        }
        memcpy(fixture->call_names[call_index],
               request->logical_name.data,
               name_count);
        fixture->call_names[call_index][name_count] = '\0';
        (void)snprintf(fixture->call_paths[call_index],
                       GRAPH_FIXTURE_NAME_CAPACITY,
                       "%s",
                       request->including_path);
    }
    fixture->call_count += 1;
    if (binding && binding->document_index != NOC_TOKEN_INDEX_NONE &&
        ((status == NOC_INCLUDE_RESOLVE_FOUND &&
          !fixture->omit_found_snapshot) ||
         (status != NOC_INCLUDE_RESOLVE_FOUND &&
          fixture->publish_snapshot_on_failure))) {
        CHECK(noc_document_snapshot_clone(
                  &fixture->documents[binding->document_index], output) ==
              NOC_WORKSPACE_OK);
    }
    return status;
}

static inline Noc_Include_Graph_Options graph_fixture_full_options(void)
{
    Noc_Include_Graph_Options options = noc_include_graph_default_options();
    options.macro_policy = NOC_MACROS_FULL;
    return options;
}

static inline Noc_Include_Graph_Status graph_fixture_build(
    Include_Graph_Fixture *fixture,
    Noc_Include_Graph_Options options,
    Noc_Include_Graph *output)
{
    Noc_Include_Resolver resolver = {
        graph_fixture_resolve,
        fixture,
    };
    return noc_include_graph_build(&fixture->context,
                                   &fixture->documents[0],
                                   NULL,
                                   0,
                                   resolver,
                                   options,
                                   output);
}

static inline void graph_fixture_deinit(Include_Graph_Fixture *fixture)
{
    size_t index = fixture->document_count;
    while (index > 0) {
        noc_document_snapshot_free(&fixture->documents[--index]);
    }
    noc_workspace_deinit(&fixture->workspace);
    noc_context_deinit(&fixture->context);
}

#endif /* NOC_INCLUDE_GRAPH_TEST_SUPPORT_H_INCLUDED */
