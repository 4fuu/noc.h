#include "include_graph_test_support.h"

typedef struct {
    const Noc_Document_Snapshot *snapshot;
    size_t calls;
} Cross_Workspace_Resolver;

static Noc_Include_Resolve_Status resolve_cross_workspace(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *output)
{
    Cross_Workspace_Resolver *state = (Cross_Workspace_Resolver *)user_data;
    (void)request;
    state->calls += 1;
    CHECK(noc_document_snapshot_clone(state->snapshot, output) ==
          NOC_WORKSPACE_OK);
    return NOC_INCLUDE_RESOLVE_FOUND;
}

static void test_empty_root_and_depth_first_contexts(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    const Noc_Include_Graph_Node *node;
    const Noc_Include_Graph_Edge *edge;
    size_t a;
    size_t b;
    size_t c;

    graph_fixture_init(&fixture, "int root;\n");
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_is_valid(&graph));
    CHECK(noc_include_graph_node_count(&graph) == 1);
    CHECK(noc_include_graph_edge_count(&graph) == 0);
    CHECK(fixture.call_count == 0);
    node = noc_include_graph_node_at(&graph, 0);
    CHECK(node && node->index == 0 && node->depth == 0);
    CHECK(node && node->traversal_complete && !node->may_mutate_macros);
    CHECK(node && strcmp(node->path, "root.c") == 0);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);

    graph_fixture_init(&fixture,
                       "#define FIRST \"a.h\"\n"
                       "#include FIRST\n"
                       "#include <b.h>\n"
                       "#include \"a.h\"\n");
    a = graph_fixture_add_document(&fixture,
                                   "include/a.h",
                                   "#include \"c.h\"\n",
                                   NOC_SOURCE_CLASS_SYSTEM);
    b = graph_fixture_add_document(&fixture,
                                   "include/b.h",
                                   "int b;\n",
                                   NOC_SOURCE_CLASS_SYSTEM);
    c = graph_fixture_add_document(&fixture,
                                   "include/c.h",
                                   "int c;\n",
                                   NOC_SOURCE_CLASS_SYSTEM);
    graph_fixture_bind(&fixture, "a.h", a, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture, "b.h", b, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture, "c.h", c, NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_is_valid(&graph));
    CHECK(noc_include_graph_node_count(&graph) == 6);
    CHECK(noc_include_graph_edge_count(&graph) == 5);
    CHECK(fixture.call_count == 5);
    CHECK(strcmp(fixture.call_names[0], "a.h") == 0);
    CHECK(strcmp(fixture.call_names[1], "c.h") == 0);
    CHECK(strcmp(fixture.call_names[2], "b.h") == 0);
    CHECK(strcmp(fixture.call_names[3], "a.h") == 0);
    CHECK(strcmp(fixture.call_names[4], "c.h") == 0);
    CHECK(strcmp(fixture.call_paths[1], "include/a.h") == 0);

    node = noc_include_graph_node_at(&graph, 0);
    CHECK(node && node->outgoing_edge_count == 3);
    CHECK(node && node->may_mutate_macros);
    edge = noc_include_graph_node_edge_at(&graph, 0, 0);
    CHECK(edge && edge->index == 0 && edge->target_node_index == 1);
    CHECK(edge && edge->macro_expanded && slice_equals(edge->logical_name, "a.h"));
    edge = noc_include_graph_node_edge_at(&graph, 0, 1);
    CHECK(edge && edge->index == 2 && edge->target_node_index == 3);
    CHECK(edge && !edge->macro_expanded && edge->form == NOC_INCLUDE_FORM_ANGLED);
    edge = noc_include_graph_node_edge_at(&graph, 0, 2);
    CHECK(edge && edge->index == 3 && edge->target_node_index == 4);
    CHECK(noc_include_graph_node_at(&graph, 1)->file_id ==
          noc_include_graph_node_at(&graph, 4)->file_id);
    CHECK(noc_include_graph_node_at(&graph, 1)->index !=
          noc_include_graph_node_at(&graph, 4)->index);
    CHECK(noc_include_graph_node_at(&graph, 2)->depth == 2);
    CHECK(noc_include_graph_node_at(&graph, 5)->depth == 2);

    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

static void test_cycles_and_conditional_activity(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    const Noc_Include_Graph_Edge *edge;
    size_t a;

    graph_fixture_init(&fixture, "#include \"a.h\"\n");
    a = graph_fixture_add_document(&fixture,
                                   "a.h",
                                   "#include \"root.c\"\n",
                                   NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "a.h", a, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture, "root.c", 0, NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_node_count(&graph) == 2);
    CHECK(noc_include_graph_edge_count(&graph) == 2);
    edge = noc_include_graph_edge_at(&graph, 1);
    CHECK(edge && edge->status == NOC_INCLUDE_GRAPH_EDGE_CYCLE);
    CHECK(edge && edge->source_node_index == 1 && edge->target_node_index == 0);
    CHECK(noc_include_graph_node_at(&graph, 0)->may_mutate_macros);
    CHECK(noc_include_graph_node_at(&graph, 1)->may_mutate_macros);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);

    graph_fixture_init(&fixture,
                       "#if 0\n"
                       "#include \"inactive.h\"\n"
                       "#endif\n"
                       "#if 'A'\n"
                       "#include \"unknown.h\"\n"
                       "#endif\n"
                       "#include \"after.h\"\n");
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_node_count(&graph) == 1);
    CHECK(noc_include_graph_edge_count(&graph) == 3);
    CHECK(fixture.call_count == 0);
    edge = noc_include_graph_edge_at(&graph, 0);
    CHECK(edge && edge->conditional_activity == NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(edge && edge->status == NOC_INCLUDE_GRAPH_EDGE_INACTIVE);
    edge = noc_include_graph_edge_at(&graph, 1);
    CHECK(edge && edge->conditional_activity == NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(edge && edge->status == NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_ACTIVITY);
    edge = noc_include_graph_edge_at(&graph, 2);
    CHECK(edge && edge->status == NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE);
    CHECK(noc_include_graph_node_at(&graph, 0)->may_mutate_macros);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

static void test_cycle_identity_is_snapshot_revision(void)
{
    Noc_Context context;
    Noc_Workspace root_workspace = {0};
    Noc_Workspace foreign_workspace = {0};
    Noc_Document_Snapshot root = {0};
    Noc_Document_Snapshot foreign = {0};
    Noc_Include_Graph graph = {0};
    Cross_Workspace_Resolver state = {&foreign, 0};
    Noc_Include_Resolver resolver = {resolve_cross_workspace, &state};

    noc_context_init(&context);
    noc_workspace_init(&root_workspace);
    noc_workspace_init(&foreign_workspace);
    CHECK(noc_workspace_open_document(&root_workspace,
                                      "same.c",
                                      "#include \"same.c\"\n",
                                      sizeof("#include \"same.c\"\n") - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &root) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&foreign_workspace,
                                      "same.c",
                                      "int foreign;\n",
                                      sizeof("int foreign;\n") - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &foreign) == NOC_WORKSPACE_OK);
    CHECK(noc_document_snapshot_file_id(&root) ==
          noc_document_snapshot_file_id(&foreign));
    CHECK(noc_document_snapshot_generation(&root) ==
          noc_document_snapshot_generation(&foreign));
    CHECK(strcmp(noc_document_snapshot_path(&root),
                 noc_document_snapshot_path(&foreign)) == 0);
    CHECK(root.impl != foreign.impl);
    CHECK(noc_include_graph_build(&context,
                                  &root,
                                  NULL,
                                  0,
                                  resolver,
                                  noc_include_graph_default_options(),
                                  &graph) == NOC_INCLUDE_GRAPH_OK);
    CHECK(state.calls == 1);
    CHECK(noc_include_graph_node_count(&graph) == 2);
    CHECK(noc_include_graph_edge_at(&graph, 0)->status ==
          NOC_INCLUDE_GRAPH_EDGE_RESOLVED);
    CHECK(noc_include_graph_edge_at(&graph, 0)->target_node_index == 1);

    noc_include_graph_free(&graph);
    noc_document_snapshot_free(&foreign);
    noc_document_snapshot_free(&root);
    noc_workspace_deinit(&foreign_workspace);
    noc_workspace_deinit(&root_workspace);
    noc_context_deinit(&context);
}

static void test_exact_child_prefix_and_unknown_local_macro_event(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    size_t child;
    size_t visible;

    graph_fixture_init(&fixture,
                       "#define BEFORE \"visible.h\"\n"
                       "#include \"child.h\"\n"
                       "#undef BEFORE\n"
                       "#define BEFORE \"wrong.h\"\n");
    child = graph_fixture_add_document(&fixture,
                                       "child.h",
                                       "#include BEFORE\n",
                                       NOC_SOURCE_CLASS_PROJECT);
    visible = graph_fixture_add_document(&fixture,
                                         "visible.h",
                                         "int visible;\n",
                                         NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "child.h", child, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture,
                       "visible.h",
                       visible,
                       NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(fixture.call_count == 2);
    CHECK(strcmp(fixture.call_names[1], "visible.h") == 0);
    CHECK(noc_include_graph_edge_at(&graph, 1)->macro_expanded);
    CHECK(noc_include_graph_edge_at(&graph, 1)->status ==
          NOC_INCLUDE_GRAPH_EDGE_RESOLVED);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);

    graph_fixture_init(&fixture,
                       "#if 'A'\n"
                       "#define MAYBE \"maybe.h\"\n"
                       "#endif\n"
                       "#include MAYBE\n");
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(fixture.call_count == 0);
    CHECK(noc_include_graph_edge_count(&graph) == 1);
    CHECK(noc_include_graph_edge_at(&graph, 0)->status ==
          NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

static void test_cross_file_macro_conservatism(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    size_t wrapper;
    size_t mutator;
    size_t clean;

    graph_fixture_init(&fixture,
                       "#include \"wrapper.h\"\n"
                       "#include LATER\n");
    wrapper = graph_fixture_add_document(&fixture,
                                         "wrapper.h",
                                         "#include \"mutator.h\"\n",
                                         NOC_SOURCE_CLASS_PROJECT);
    mutator = graph_fixture_add_document(&fixture,
                                         "mutator.h",
                                         "#define LATER \"later.h\"\n",
                                         NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "wrapper.h", wrapper, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture, "mutator.h", mutator, NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(fixture.call_count == 2);
    CHECK(noc_include_graph_node_count(&graph) == 3);
    CHECK(noc_include_graph_edge_count(&graph) == 3);
    CHECK(noc_include_graph_edge_at(&graph, 2)->status ==
          NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE);
    CHECK(noc_include_graph_edge_at(&graph, 2)->conditional_activity ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(noc_include_graph_node_at(&graph, 0)->may_mutate_macros);
    CHECK(noc_include_graph_node_at(&graph, 1)->may_mutate_macros);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);

    graph_fixture_init(&fixture,
                       "#include \"clean.h\"\n"
                       "#include \"later.h\"\n");
    clean = graph_fixture_add_document(&fixture,
                                       "clean.h",
                                       "#if 0\n#define HIDDEN 1\n#endif\n",
                                       NOC_SOURCE_CLASS_PROJECT);
    mutator = graph_fixture_add_document(&fixture,
                                         "later.h",
                                         "int later;\n",
                                         NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "clean.h", clean, NOC_INCLUDE_RESOLVE_FOUND);
    graph_fixture_bind(&fixture, "later.h", mutator, NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(fixture.call_count == 2);
    CHECK(noc_include_graph_edge_count(&graph) == 2);
    CHECK(noc_include_graph_edge_at(&graph, 1)->status ==
          NOC_INCLUDE_GRAPH_EDGE_RESOLVED);
    CHECK(!noc_include_graph_node_at(&graph, 1)->may_mutate_macros);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

static void test_child_effect_invalidates_parent_conditional_predictions(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    const Noc_Include_Graph_Edge *edge;
    size_t child;

    graph_fixture_init(&fixture,
                       "#include \"define.h\"\n"
                       "#ifdef X\n"
                       "#include \"activated.h\"\n"
                       "#endif\n");
    child = graph_fixture_add_document(&fixture,
                                       "define.h",
                                       "#define X 1\n",
                                       NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "define.h", child, NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(fixture.call_count == 1);
    CHECK(noc_include_graph_edge_count(&graph) == 2);
    edge = noc_include_graph_edge_at(&graph, 1);
    CHECK(edge && edge->conditional_activity == NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(edge && edge->status == NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);

    graph_fixture_init(&fixture,
                       "#define X 1\n"
                       "#include \"undef.h\"\n"
                       "#ifdef X\n"
                       "#include \"deactivated.h\"\n"
                       "#endif\n");
    child = graph_fixture_add_document(&fixture,
                                       "undef.h",
                                       "#undef X\n",
                                       NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture, "undef.h", child, NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(fixture.call_count == 1);
    CHECK(noc_include_graph_edge_count(&graph) == 2);
    edge = noc_include_graph_edge_at(&graph, 1);
    CHECK(edge && edge->conditional_activity == NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(edge && edge->status == NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

static void test_graph_macro_policy_and_expansion_limits(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = noc_include_graph_default_options();
    size_t target;

    graph_fixture_init(&fixture,
                       "#define HEADER <target.h>\n"
                       "#include HEADER\n");
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
    CHECK(fixture.call_count == 0);
    CHECK(noc_include_graph_edge_at(&graph, 0)->status ==
          NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND);
    noc_include_graph_free(&graph);

    fixture.call_count = 0;
    options = graph_fixture_full_options();
    options.macro_expansion_options.limits.max_output_tokens = 1;
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(fixture.call_count == 0);
    CHECK(noc_include_graph_edge_at(&graph, 0)->status ==
          NOC_INCLUDE_GRAPH_EDGE_EXPANSION_FAILED);
    CHECK(noc_include_graph_edge_at(&graph, 0)->expansion_status ==
          NOC_MACRO_EXPANSION_OUTPUT_LIMIT);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

static void test_recoverable_operands_and_resolver_outcomes(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    static const Noc_Include_Graph_Edge_Status expected[] = {
        NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND,
        NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND,
        NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND,
        NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND,
        NOC_INCLUDE_GRAPH_EDGE_NOT_FOUND,
        NOC_INCLUDE_GRAPH_EDGE_AMBIGUOUS,
        NOC_INCLUDE_GRAPH_EDGE_DENIED,
        NOC_INCLUDE_GRAPH_EDGE_RESOLVER_FAILED,
    };
    size_t index;

    graph_fixture_init(&fixture,
                       "#define EMPTY\n"
                       "#define BAD value\n"
                       "#include\n"
                       "#include <>\n"
                       "#include EMPTY\n"
                       "#include BAD\n"
                       "#include \"missing.h\"\n"
                       "#include \"ambiguous.h\"\n"
                       "#include \"denied.h\"\n"
                       "#include \"failed.h\"\n");
    graph_fixture_bind(&fixture,
                       "ambiguous.h",
                       NOC_TOKEN_INDEX_NONE,
                       NOC_INCLUDE_RESOLVE_AMBIGUOUS);
    graph_fixture_bind(&fixture,
                       "denied.h",
                       NOC_TOKEN_INDEX_NONE,
                       NOC_INCLUDE_RESOLVE_DENIED);
    graph_fixture_bind(&fixture,
                       "failed.h",
                       NOC_TOKEN_INDEX_NONE,
                       NOC_INCLUDE_RESOLVE_FAILED);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_edge_count(&graph) == 8);
    CHECK(fixture.call_count == 4);
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        CHECK(noc_include_graph_edge_at(&graph, index)->status == expected[index]);
    }
    CHECK(!noc_include_graph_edge_at(&graph, 0)->macro_expanded);
    CHECK(noc_include_graph_edge_at(&graph, 2)->macro_expanded);
    CHECK(noc_include_graph_edge_at(&graph, 3)->macro_expanded);
    CHECK(noc_include_graph_edge_at(&graph, 2)->expansion_status ==
          NOC_MACRO_EXPANSION_OK);
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);
}

int main(void)
{
    test_empty_root_and_depth_first_contexts();
    test_cycles_and_conditional_activity();
    test_cycle_identity_is_snapshot_revision();
    test_exact_child_prefix_and_unknown_local_macro_event();
    test_cross_file_macro_conservatism();
    test_child_effect_invalidates_parent_conditional_predictions();
    test_graph_macro_policy_and_expansion_limits();
    test_recoverable_operands_and_resolver_outcomes();
    return finish_suite("include-graph");
}
