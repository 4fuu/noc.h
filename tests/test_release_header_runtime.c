#define NOC_IMPLEMENTATION
#include "../release/noc.h"

#include <stdio.h>
#include <string.h>

static int failed = 0;

#define REQUIRE(condition)                                                      \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: requirement failed: %s\n",                \
                    __FILE__, __LINE__, #condition);                            \
            failed = 1;                                                         \
        }                                                                       \
    } while (0)

static Noc_Include_Resolve_Status release_not_found(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *output)
{
    (void)user_data;
    (void)request;
    (void)output;
    return NOC_INCLUDE_RESOLVE_NOT_FOUND;
}

int main(void)
{
    static const char source[] =
        "#define CLOSE >\n"
        "#include <closed.h CLOSE\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Include_Operand operand = {0};
    Noc_Include_Expansion expansion = {0};
    Noc_Include_Graph graph = {0};
    Noc_Include_Graph_Options graph_options =
        noc_include_graph_default_options();
    Noc_Include_Resolver resolver = {release_not_found, NULL};

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    REQUIRE(noc_workspace_open_document(&workspace,
                                        "release-runtime.c",
                                        source,
                                        sizeof(source) - 1,
                                        NOC_SOURCE_CLASS_PROJECT,
                                        &snapshot) == NOC_WORKSPACE_OK);
    REQUIRE(noc_preprocessor_unit_build(&context,
                                        &snapshot,
                                        NOC_MACROS_FULL,
                                        &unit));
    REQUIRE(unit.count == 2);
    REQUIRE(unit.macro_directive_count == 1);
    REQUIRE(noc_macro_environment_apply(&environment, &unit, 0) ==
            NOC_MACRO_ENVIRONMENT_OK);
    REQUIRE(noc_include_operand_build(&unit, 1, &operand) ==
            NOC_INCLUDE_OPERAND_BUILD_OK);
    REQUIRE(operand.status == NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED);
    REQUIRE(noc_include_expansion_build(&environment,
                                        environment.count,
                                        &operand,
                                        noc_macro_expansion_default_limits(),
                                        &expansion) == NOC_MACRO_EXPANSION_OK);
    REQUIRE(noc_include_expansion_is_valid(&expansion));
    REQUIRE(expansion.status == NOC_INCLUDE_OPERAND_DIRECT);
    REQUIRE(expansion.form == NOC_INCLUDE_FORM_ANGLED);
    REQUIRE(expansion.logical_name.count == sizeof("closed.h") - 1);
    REQUIRE(expansion.logical_name.data != NULL &&
            memcmp(expansion.logical_name.data,
                   "closed.h",
                   sizeof("closed.h") - 1) == 0);
    graph_options.macro_policy = NOC_MACROS_FULL;
    REQUIRE(noc_include_graph_build(&context,
                                    &snapshot,
                                    NULL,
                                    0,
                                    resolver,
                                    graph_options,
                                    &graph) == NOC_INCLUDE_GRAPH_OK);
    REQUIRE(noc_include_graph_is_valid(&graph));
    REQUIRE(noc_include_graph_node_count(&graph) == 1);
    REQUIRE(noc_include_graph_edge_count(&graph) == 1);
    REQUIRE(noc_include_graph_edge_at(&graph, 0)->macro_expanded);
    REQUIRE(noc_include_graph_edge_at(&graph, 0)->status ==
            NOC_INCLUDE_GRAPH_EDGE_NOT_FOUND);

    noc_include_graph_free(&graph);
    noc_include_expansion_free(&expansion);
    noc_include_operand_free(&operand);
    noc_macro_environment_free(&environment);
    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
    return failed;
}
