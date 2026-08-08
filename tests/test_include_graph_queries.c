#include "include_graph_test_support.h"

static void test_names_defaults_and_invalid_queries(void)
{
    static const Noc_Include_Graph_Status graph_statuses[] = {
        NOC_INCLUDE_GRAPH_OK,
        NOC_INCLUDE_GRAPH_INVALID_ARGUMENT,
        NOC_INCLUDE_GRAPH_STALE,
        NOC_INCLUDE_GRAPH_CANCELLED,
        NOC_INCLUDE_GRAPH_GENERATION_EXHAUSTED,
        NOC_INCLUDE_GRAPH_OUT_OF_MEMORY,
        NOC_INCLUDE_GRAPH_PREPROCESSOR_FAILED,
        NOC_INCLUDE_GRAPH_INVALID_RESULT,
    };
    static const char *const graph_names[] = {
        "ok", "invalid-argument", "stale", "cancelled",
        "generation-exhausted", "out-of-memory", "preprocessor-failed",
        "invalid-result",
    };
    static const Noc_Include_Graph_Edge_Status edge_statuses[] = {
        NOC_INCLUDE_GRAPH_EDGE_RESOLVED,
        NOC_INCLUDE_GRAPH_EDGE_CYCLE,
        NOC_INCLUDE_GRAPH_EDGE_INACTIVE,
        NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_ACTIVITY,
        NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE,
        NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND,
        NOC_INCLUDE_GRAPH_EDGE_EXPANSION_FAILED,
        NOC_INCLUDE_GRAPH_EDGE_NOT_FOUND,
        NOC_INCLUDE_GRAPH_EDGE_AMBIGUOUS,
        NOC_INCLUDE_GRAPH_EDGE_DENIED,
        NOC_INCLUDE_GRAPH_EDGE_RESOLVER_FAILED,
        NOC_INCLUDE_GRAPH_EDGE_DEPTH_LIMIT,
        NOC_INCLUDE_GRAPH_EDGE_NODE_LIMIT,
    };
    static const char *const edge_names[] = {
        "resolved", "cycle", "inactive", "unknown-activity",
        "unknown-macro-state", "invalid-operand", "expansion-failed",
        "not-found", "ambiguous", "denied", "resolver-failed",
        "depth-limit", "node-limit",
    };
    Noc_Include_Graph empty = {0};
    Noc_Include_Graph_Options options = noc_include_graph_default_options();
    size_t index;

    for (index = 0;
         index < sizeof(graph_statuses) / sizeof(graph_statuses[0]);
         ++index) {
        CHECK(strcmp(noc_include_graph_status_name(graph_statuses[index]),
                     graph_names[index]) == 0);
    }
    for (index = 0;
         index < sizeof(edge_statuses) / sizeof(edge_statuses[0]);
         ++index) {
        CHECK(strcmp(noc_include_graph_edge_status_name(edge_statuses[index]),
                     edge_names[index]) == 0);
    }
    CHECK(strcmp(noc_include_graph_status_name((Noc_Include_Graph_Status)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_include_graph_edge_status_name(
                     (Noc_Include_Graph_Edge_Status)99),
                 "unknown") == 0);
    CHECK(options.macro_policy == NOC_MACROS_TRUSTED_ONLY);
    CHECK(options.max_depth == 128);
    CHECK(options.max_nodes == 1024);
    CHECK(options.max_edges == 8192);
    CHECK(options.should_cancel == NULL && options.cancel_user_data == NULL);
    CHECK(!noc_include_graph_is_valid(&empty));
    CHECK(noc_include_graph_node_count(&empty) == 0);
    CHECK(noc_include_graph_edge_count(&empty) == 0);
    CHECK(noc_include_graph_limit_flags(&empty) == NOC_INCLUDE_GRAPH_LIMIT_NONE);
    CHECK(noc_include_graph_node_at(&empty, 0) == NULL);
    CHECK(noc_include_graph_edge_at(&empty, 0) == NULL);
    CHECK(noc_include_graph_node_edge_at(&empty, 0, 0) == NULL);
    CHECK(noc_include_graph_node_snapshot(&empty, 0) == NULL);
    CHECK(noc_include_graph_node_preprocessor_unit(&empty, 0) == NULL);
    CHECK(noc_include_graph_node_conditional_groups(&empty, 0) == NULL);
    CHECK(noc_include_graph_edge_operand(&empty, 0) == NULL);
    CHECK(noc_include_graph_edge_expansion(&empty, 0) == NULL);
    noc_include_graph_free(&empty);
    noc_include_graph_free(&empty);
}

static void test_phase_queries_provenance_and_owned_lifetime(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    const Noc_Include_Graph_Node *root;
    const Noc_Include_Graph_Edge *expanded_edge;
    const Noc_Include_Graph_Edge *missing_edge;
    const Noc_Document_Snapshot *snapshot;
    const Noc_Preprocessor_Unit *unit;
    const Noc_Preprocessor_Conditional_Groups *groups;
    const Noc_Include_Operand *operand;
    const Noc_Include_Expansion *expansion;
    const Noc_Macro_Expansion_Token *token;
    const Noc_Macro_Expansion_Frame *frame;
    size_t api;
    size_t generation;

    graph_fixture_init(&fixture,
                       "#define HEADER <dir/api.h>\n"
                       "#include HEADER\n"
                       "#include \"missing.h\"\n");
    api = graph_fixture_add_document(&fixture,
                                     "sdk/dir/api.h",
                                     "int api(void);\n",
                                     NOC_SOURCE_CLASS_SYSTEM);
    graph_fixture_bind(&fixture, "dir/api.h", api, NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_is_valid(&graph));
    CHECK(noc_include_graph_node_count(&graph) == 2);
    CHECK(noc_include_graph_edge_count(&graph) == 2);

    root = noc_include_graph_node_at(&graph, 0);
    expanded_edge = noc_include_graph_edge_at(&graph, 0);
    missing_edge = noc_include_graph_edge_at(&graph, 1);
    snapshot = noc_include_graph_node_snapshot(&graph, 0);
    unit = noc_include_graph_node_preprocessor_unit(&graph, 0);
    groups = noc_include_graph_node_conditional_groups(&graph, 0);
    operand = noc_include_graph_edge_operand(&graph, 0);
    expansion = noc_include_graph_edge_expansion(&graph, 0);
    CHECK(root && expanded_edge && missing_edge && snapshot && unit && groups);
    CHECK(operand && expansion);
    CHECK(root == noc_include_graph_node_at(&graph, 0));
    CHECK(expanded_edge == noc_include_graph_node_edge_at(&graph, 0, 0));
    CHECK(snapshot == noc_include_graph_node_snapshot(&graph, 0));
    CHECK(unit == groups->unit);
    CHECK(noc_document_snapshot_file_id(snapshot) == root->file_id);
    CHECK(unit->file_id == root->file_id);
    CHECK(operand->unit == unit);
    CHECK(operand->status == NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED);
    CHECK(operand->body_tokens.begin < operand->body_tokens.end);
    CHECK(operand->body_tokens.end <= unit->preprocessing_token_count);
    CHECK(expanded_edge->status == NOC_INCLUDE_GRAPH_EDGE_RESOLVED);
    CHECK(expanded_edge->target_node_index == 1);
    CHECK(expanded_edge->macro_expanded);
    CHECK(expanded_edge->form == NOC_INCLUDE_FORM_ANGLED);
    CHECK(slice_equals(expanded_edge->logical_name, "dir/api.h"));
    CHECK(expansion->logical_name.data == expanded_edge->logical_name.data);
    CHECK(expansion->logical_name.data[expansion->logical_name.count] == '\0');
    CHECK(expansion->macro_expansion.count > 0);
    token = noc_macro_expansion_token_at(&expansion->macro_expansion, 0);
    CHECK(token && token->origin == NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT);
    CHECK(token && token->unit == unit);
    frame = token ? noc_macro_expansion_frame_at(&expansion->macro_expansion,
                                                  token->frame_index) : NULL;
    CHECK(frame && frame->invocation_unit == unit);
    CHECK(missing_edge->status == NOC_INCLUDE_GRAPH_EDGE_NOT_FOUND);
    CHECK(!missing_edge->macro_expanded);
    CHECK(noc_include_graph_edge_expansion(&graph, 1) == NULL);
    CHECK(noc_include_graph_node_at(&graph, 2) == NULL);
    CHECK(noc_include_graph_edge_at(&graph, 2) == NULL);
    CHECK(noc_include_graph_node_edge_at(&graph, 0, 2) == NULL);
    CHECK(noc_include_graph_node_snapshot(&graph, 2) == NULL);
    CHECK(noc_include_graph_node_preprocessor_unit(&graph, 2) == NULL);
    CHECK(noc_include_graph_node_conditional_groups(&graph, 2) == NULL);
    CHECK(noc_include_graph_edge_operand(&graph, 2) == NULL);
    CHECK(noc_include_graph_edge_expansion(&graph, 2) == NULL);

    generation = graph.generation;
    graph_fixture_deinit(&fixture);
    CHECK(noc_include_graph_is_valid(&graph));
    CHECK(graph.generation == generation);
    CHECK(strcmp(noc_include_graph_node_at(&graph, 1)->path,
                 "sdk/dir/api.h") == 0);
    CHECK(slice_equals(noc_document_snapshot_source(
                           noc_include_graph_node_snapshot(&graph, 1)),
                       "int api(void);\n"));
    noc_include_graph_free(&graph);
    CHECK(graph.impl == NULL && graph.generation == generation);
    CHECK(noc_include_graph_node_count(&graph) == 0);
    noc_include_graph_free(&graph);
}

static void test_initial_environment_and_successful_rebuild_generation(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Preprocessor_Unit definitions = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Include_Resolver resolver = {graph_fixture_resolve, &fixture};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    size_t definitions_index;
    size_t target;
    size_t generation;
    Noc_Include_Graph_Impl *implementation;

    graph_fixture_init(&fixture, "#include EXTERNAL_HEADER\n");
    definitions_index = graph_fixture_add_document(
        &fixture,
        "prelude/noc.h",
        "#define EXTERNAL_HEADER \"external.h\"\n",
        NOC_SOURCE_CLASS_TRUSTED);
    target = graph_fixture_add_document(&fixture,
                                        "external.h",
                                        "int external;\n",
                                        NOC_SOURCE_CLASS_PROJECT);
    graph_fixture_bind(&fixture,
                       "external.h",
                       target,
                       NOC_INCLUDE_RESOLVE_FOUND);
    CHECK(noc_preprocessor_unit_build(&fixture.context,
                                      &fixture.documents[definitions_index],
                                      NOC_MACROS_FULL,
                                      &definitions));
    CHECK(noc_macro_environment_apply(&environment, &definitions, 0) ==
          NOC_MACRO_ENVIRONMENT_OK);
    CHECK(noc_include_graph_build(&fixture.context,
                                  &fixture.documents[0],
                                  &environment,
                                  environment.count,
                                  resolver,
                                  options,
                                  &graph) == NOC_INCLUDE_GRAPH_OK);
    CHECK(noc_include_graph_edge_at(&graph, 0)->macro_expanded);
    CHECK(slice_equals(noc_include_graph_edge_at(&graph, 0)->logical_name,
                       "external.h"));
    generation = graph.generation;
    implementation = graph.impl;
    CHECK(noc_include_graph_build(&fixture.context,
                                  &fixture.documents[0],
                                  &environment,
                                  environment.count,
                                  resolver,
                                  options,
                                  &graph) == NOC_INCLUDE_GRAPH_OK);
    CHECK(graph.generation == generation + 1);
    CHECK(graph.impl != implementation);
    CHECK(noc_include_graph_is_valid(&graph));

    noc_include_graph_free(&graph);
    noc_macro_environment_free(&environment);
    noc_preprocessor_unit_free(&definitions);
    graph_fixture_deinit(&fixture);
}

static void test_self_borrow_and_stale_initial_prefix_are_rejected(void)
{
    Include_Graph_Fixture fixture;
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options options = graph_fixture_full_options();
    Noc_Include_Resolver resolver = {graph_fixture_resolve, &fixture};
    const Noc_Preprocessor_Conditional_Groups *groups;
    Noc_Include_Graph_Impl *preserved_impl;
    size_t preserved_generation;

    graph_fixture_init(&fixture, "#define OWNED 1\n");
    CHECK(graph_fixture_build(&fixture, options, &graph) ==
          NOC_INCLUDE_GRAPH_OK);
    groups = noc_include_graph_node_conditional_groups(&graph, 0);
    CHECK(groups && groups->environment.count == 1);
    preserved_impl = graph.impl;
    preserved_generation = graph.generation;
    CHECK(noc_include_graph_build(&fixture.context,
                                  &fixture.documents[0],
                                  &groups->environment,
                                  groups->environment.count,
                                  resolver,
                                  options,
                                  &graph) == NOC_INCLUDE_GRAPH_INVALID_ARGUMENT);
    CHECK(graph.impl == preserved_impl && graph.generation == preserved_generation);
    CHECK(noc_include_graph_is_valid(&graph));
    noc_include_graph_free(&graph);
    graph_fixture_deinit(&fixture);

    {
        Noc_Preprocessor_Unit definitions = {0};
        Noc_Macro_Environment stale_environment = {0};
        size_t definitions_index;

        graph_fixture_init(&fixture, "int root;\n");
        definitions_index = graph_fixture_add_document(
            &fixture,
            "prelude.h",
            "#define EXTERNAL 1\n",
            NOC_SOURCE_CLASS_TRUSTED);
        CHECK(graph_fixture_build(&fixture, options, &graph) ==
              NOC_INCLUDE_GRAPH_OK);
        preserved_impl = graph.impl;
        preserved_generation = graph.generation;
        CHECK(noc_preprocessor_unit_build(&fixture.context,
                                          &fixture.documents[definitions_index],
                                          NOC_MACROS_FULL,
                                          &definitions));
        CHECK(noc_macro_environment_apply(&stale_environment,
                                          &definitions,
                                          0) == NOC_MACRO_ENVIRONMENT_OK);
        CHECK(noc_preprocessor_unit_build(&fixture.context,
                                          &fixture.documents[definitions_index],
                                          NOC_MACROS_FULL,
                                          &definitions));
        CHECK(!noc_macro_environment_is_valid(&stale_environment));
        CHECK(noc_include_graph_build(&fixture.context,
                                      &fixture.documents[0],
                                      &stale_environment,
                                      stale_environment.count,
                                      resolver,
                                      options,
                                      &graph) == NOC_INCLUDE_GRAPH_STALE);
        CHECK(graph.impl == preserved_impl &&
              graph.generation == preserved_generation);
        CHECK(noc_include_graph_is_valid(&graph));

        noc_include_graph_free(&graph);
        noc_macro_environment_free(&stale_environment);
        noc_preprocessor_unit_free(&definitions);
        graph_fixture_deinit(&fixture);
    }
}

int main(void)
{
    test_names_defaults_and_invalid_queries();
    test_phase_queries_provenance_and_owned_lifetime();
    test_initial_environment_and_successful_rebuild_generation();
    test_self_borrow_and_stale_initial_prefix_are_rejected();
    return finish_suite("include-graph-queries");
}
