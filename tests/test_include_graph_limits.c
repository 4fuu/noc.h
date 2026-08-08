#include "include_graph_test_support.h"

typedef struct {
    size_t calls;
    size_t cancel_on_call;
} Graph_Cancel_State;

static bool cancel_graph_build(void *user_data)
{
    Graph_Cancel_State *state = (Graph_Cancel_State *)user_data;
    size_t call = state->calls++;
    return call >= state->cancel_on_call;
}

typedef struct {
    Noc_Workspace *workspace;
    size_t terminal_index;
    size_t calls;
} Deep_Graph_Resolver;

static Noc_Include_Resolve_Status resolve_deep_graph(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *output)
{
    Deep_Graph_Resolver *state = (Deep_Graph_Resolver *)user_data;
    char name[64];
    char source[96];
    size_t index = 0;
    int matched;
    CHECK(request->logical_name.count < sizeof(name));
    if (request->logical_name.count >= sizeof(name)) {
        return NOC_INCLUDE_RESOLVE_FAILED;
    }
    memcpy(name, request->logical_name.data, request->logical_name.count);
    name[request->logical_name.count] = '\0';
    matched = sscanf(name, "n%zu.h", &index);
    CHECK(matched == 1 && index > 0 && index <= state->terminal_index);
    if (matched != 1 || index == 0 || index > state->terminal_index) {
        return NOC_INCLUDE_RESOLVE_FAILED;
    }
    if (index < state->terminal_index) {
        (void)snprintf(source,
                       sizeof(source),
                       "#include \"n%zu.h\"\n",
                       index + 1);
    } else {
        source[0] = '\0';
    }
    state->calls += 1;
    CHECK(noc_workspace_open_document(state->workspace,
                                      name,
                                      source,
                                      strlen(source),
                                      NOC_SOURCE_CLASS_PROJECT,
                                      output) == NOC_WORKSPACE_OK);
    return NOC_INCLUDE_RESOLVE_FOUND;
}

static void test_depth_node_and_edge_limits(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    const Noc_Include_Graph_Edge *edge;
    size_t a;
    size_t b;

    graph_fixture_init(&fixture, "#include \"a.h\"\n");
    a = graph_fixture_add_document(&fixture,
                                   "a.h",
                                   "#include \"b.h\"\n",
                                   NOC_SOURCE_CLASS_PROJECT);
    b = graph_fixture_add_document(&fixture,
                                   "b.h",
                                   "int b;\n",
                                   NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "a.h", a, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture, "b.h", b, NOC_INCLUDE_RESOLVE_FOUND);
    options.max_depth = 1;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_node_count(&graph) == 2);
    CHECK(noc_include_graph_edge_count(&graph) == 2);
    CHECK(noc_include_graph_limit_flags(&graph) == NOC_INCLUDE_GRAPH_LIMIT_DEPTH);
    edge = noc_include_graph_edge_at(&graph, 1);
    CHECK(edge && edge->status == NOC_INCLUDE_GRAPH_EDGE_DEPTH_LIMIT);
    CHECK(edge && edge->target_node_index == NOC_TOKEN_INDEX_NONE);
    CHECK(!noc_include_graph_node_at(&graph, 1)->traversal_complete);
    CHECK(noc_include_graph_node_at(&graph, 0)->may_mutate_macros);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);

    graph_fixture_init(&fixture, "#include \"a.h\"\n");
    a = graph_fixture_add_document(&fixture,
                                   "a.h",
                                   "int a;\n",
                                   NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "a.h", a, NOC_INCLUDE_RESOLVE_FOUND);
    options = graph_fixture_full_options();
    options.max_nodes = 1;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_node_count(&graph) == 1);
    CHECK(noc_include_graph_edge_count(&graph) == 1);
    CHECK(noc_include_graph_limit_flags(&graph) == NOC_INCLUDE_GRAPH_LIMIT_NODES);
    CHECK(noc_include_graph_edge_at(&graph, 0)->status ==
          NOC_INCLUDE_GRAPH_EDGE_NODE_LIMIT);
    CHECK(!noc_include_graph_node_at(&graph, 0)->traversal_complete);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);

    graph_fixture_init(&fixture,
                       "#include \"a.h\"\n"
                       "#include \"b.h\"\n");
    a = graph_fixture_add_document(&fixture,
                                   "a.h",
                                   "int a;\n",
                                   NOC_SOURCE_CLASS_PROJECT);
    b = graph_fixture_add_document(&fixture,
                                   "b.h",
                                   "int b;\n",
                                   NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "a.h", a, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture, "b.h", b, NOC_INCLUDE_RESOLVE_FOUND);
    options = graph_fixture_full_options();
    options.max_edges = 1;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_node_count(&graph) == 2);
    CHECK(noc_include_graph_edge_count(&graph) == 1);
    CHECK(fixture.call_count == 1);
    CHECK(noc_include_graph_limit_flags(&graph) == NOC_INCLUDE_GRAPH_LIMIT_EDGES);
    CHECK(!noc_include_graph_node_at(&graph, 0)->traversal_complete);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

static void test_nested_edge_limit_resumes_parent_frame(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    size_t child;
    size_t leaf;
    size_t omitted_child;
    size_t omitted_parent;

    graph_fixture_init(&fixture,
                       "#include \"child.h\"\n"
                       "#include \"omitted-parent.h\"\n");
    child = graph_fixture_add_document(
        &fixture,
        "child.h",
        "#include \"leaf.h\"\n#include \"omitted-child.h\"\n",
        NOC_SOURCE_CLASS_PROJECT);
    leaf = graph_fixture_add_document(&fixture,
                                      "leaf.h",
                                      "int leaf;\n",
                                      NOC_SOURCE_CLASS_PROJECT);
    omitted_child = graph_fixture_add_document(&fixture,
                                                "omitted-child.h",
                                                "int omitted_child;\n",
                                                NOC_SOURCE_CLASS_PROJECT);
    omitted_parent = graph_fixture_add_document(&fixture,
                                                 "omitted-parent.h",
                                                 "int omitted_parent;\n",
                                                 NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "child.h", child, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture, "leaf.h", leaf, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture,
                       "omitted-child.h",
                       omitted_child,
                       NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture,
                       "omitted-parent.h",
                       omitted_parent,
                       NOC_INCLUDE_RESOLVE_FOUND);
    options.max_edges = 2;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(fixture.call_count == 2);
    CHECK(strcmp(fixture.call_names[0], "child.h") == 0);
    CHECK(strcmp(fixture.call_names[1], "leaf.h") == 0);
    CHECK(noc_include_graph_node_count(&graph) == 3);
    CHECK(noc_include_graph_edge_count(&graph) == 2);
    CHECK(noc_include_graph_edge_at(&graph, 0)->source_node_index == 0);
    CHECK(noc_include_graph_edge_at(&graph, 0)->target_node_index == 1);
    CHECK(noc_include_graph_edge_at(&graph, 1)->source_node_index == 1);
    CHECK(noc_include_graph_edge_at(&graph, 1)->target_node_index == 2);
    CHECK(!noc_include_graph_node_at(&graph, 0)->traversal_complete);
    CHECK(!noc_include_graph_node_at(&graph, 1)->traversal_complete);
    CHECK(noc_include_graph_node_at(&graph, 0)->may_mutate_macros);
    CHECK(noc_include_graph_node_at(&graph, 1)->may_mutate_macros);
    CHECK(noc_include_graph_limit_flags(&graph) == NOC_INCLUDE_GRAPH_LIMIT_EDGES);

    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

static void test_invalid_limits_and_generation_exhaustion(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();

    graph_fixture_init(&fixture, "int root;\n");
    options.max_depth = 0;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_INVALID_ARGUMENT);
    options = graph_fixture_full_options();
    options.max_nodes = 0;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_INVALID_ARGUMENT);
    options = graph_fixture_full_options();
    options.max_edges = 0;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_INVALID_ARGUMENT);
    options = graph_fixture_full_options();
    options.max_depth = SIZE_MAX;
    options.max_nodes = SIZE_MAX;
    options.max_edges = SIZE_MAX;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    noc_include_graph_free(&graph);

    graph.generation = SIZE_MAX;
    CHECK(graph_fixture_build(&fixture, graph_fixture_full_options(), &graph) ==
          NOC_INCLUDE_GRAPH_GENERATION_EXHAUSTED);
    CHECK(graph.impl == NULL && graph.generation == SIZE_MAX);
    graph.generation = 0;
    graph_fixture_deinit(&fixture);
}

static void test_deep_traversal_uses_a_bounded_heap_stack(void)
{
    enum { TERMINAL_INDEX = 4096 };
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot root = {0};
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = noc_include_graph_default_options();
    Deep_Graph_Resolver state = {&workspace, TERMINAL_INDEX, 0};
    Noc_Include_Resolver resolver = {resolve_deep_graph, &state};

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "root.c",
                                      "#include \"n1.h\"\n",
                                      sizeof("#include \"n1.h\"\n") - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &root) == NOC_WORKSPACE_OK);
    options.max_depth = SIZE_MAX;
    options.max_nodes = SIZE_MAX;
    options.max_edges = SIZE_MAX;
    CHECK(noc_include_graph_build(&context,
                                  &root,
                                  NULL,
                                  0,
                                  resolver,
                                  options,
                                  &graph) == NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_is_valid(&graph));
    CHECK(state.calls == TERMINAL_INDEX);
    CHECK(noc_include_graph_node_count(&graph) == TERMINAL_INDEX + 1);
    CHECK(noc_include_graph_edge_count(&graph) == TERMINAL_INDEX);
    CHECK(noc_include_graph_node_at(&graph,
                                    TERMINAL_INDEX)->depth == TERMINAL_INDEX);
    CHECK(noc_include_graph_limit_flags(&graph) == NOC_INCLUDE_GRAPH_LIMIT_NONE);

    noc_include_graph_free(&graph);
    noc_document_snapshot_free(&root);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_cancellation_and_callback_failures_are_transactional(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    Noc_Include_Graph_Impl *preserved_impl;
    size_t preserved_generation;
    size_t target;
    Graph_Cancel_State cancel = {0, 0};

    graph_fixture_init(&fixture, "#include \"target.h\"\n");
    target = graph_fixture_add_document(&fixture,
                                        "target.h",
                                        "int target;\n",
                                        NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture,
                       "target.h",
                       target,
                       NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    preserved_impl = graph.impl;
    preserved_generation = graph.generation;

    options.should_cancel = cancel_graph_build;
    options.cancel_user_data = &cancel;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_CANCELLED);
    CHECK(graph.impl == preserved_impl && graph.generation == preserved_generation);
    CHECK(noc_include_graph_is_valid(&graph));

    options.should_cancel = NULL;
    options.cancel_user_data = NULL;
    fixture.bindings[0].status = NOC_INCLUDE_RESOLVE_CANCELLED;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_CANCELLED);
    CHECK(graph.impl == preserved_impl && graph.generation == preserved_generation);

    fixture.bindings[0].status = NOC_INCLUDE_RESOLVE_OUT_OF_MEMORY;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OUT_OF_MEMORY);
    CHECK(graph.impl == preserved_impl && graph.generation == preserved_generation);

    fixture.bindings[0].status = NOC_INCLUDE_RESOLVE_FOUND;
    fixture.omit_found_snapshot = true;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_INVALID_RESULT);
    CHECK(graph.impl == preserved_impl && graph.generation == preserved_generation);
    fixture.omit_found_snapshot = false;

    fixture.bindings[0].status = NOC_INCLUDE_RESOLVE_NOT_FOUND;
    fixture.publish_snapshot_on_failure = true;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_INVALID_RESULT);
    CHECK(graph.impl == preserved_impl && graph.generation == preserved_generation);
    CHECK(noc_include_graph_node_count(&graph) == 2);

    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

static void test_cancel_during_traversal(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    Graph_Cancel_State cancel = {0, 2};
    Noc_Include_Graph_Impl *preserved_impl;
    size_t preserved_generation;
    size_t child;
    size_t nested;

    graph_fixture_init(&fixture, "#include \"child.h\"\n");
    child = graph_fixture_add_document(&fixture,
                                       "child.h",
                                       "#include \"nested.h\"\n",
                                       NOC_SOURCE_CLASS_PROJECT);
    nested = graph_fixture_add_document(&fixture,
                                        "nested.h",
                                        "int nested;\n",
                                        NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "child.h", child, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture, "nested.h", nested, NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    preserved_impl = graph.impl;
    preserved_generation = graph.generation;
    fixture.call_count = 0;
    options.should_cancel = cancel_graph_build;
    options.cancel_user_data = &cancel;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_CANCELLED);
    CHECK(cancel.calls == 3);
    CHECK(fixture.call_count == 1);
    CHECK(graph.impl == preserved_impl && graph.generation == preserved_generation);
    CHECK(noc_include_graph_is_valid(&graph));
    CHECK(noc_include_graph_node_count(&graph) == 3);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

int main(void)
{
    test_depth_node_and_edge_limits();
    test_nested_edge_limit_resumes_parent_frame();
    test_invalid_limits_and_generation_exhaustion();
    test_deep_traversal_uses_a_bounded_heap_stack();
    test_cancellation_and_callback_failures_are_transactional();
    test_cancel_during_traversal();
    return finish_suite("include-graph-limits");
}
