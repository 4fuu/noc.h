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
        "#include <closed.h CLOSE\n"
        "#pragma once\n";
    static const char guard_source[] =
        "#ifndef RELEASE_RUNTIME_H\n"
        "#define RELEASE_RUNTIME_H\n"
        "int release_runtime;\n"
        "#endif\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Document_Snapshot guard_snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Noc_Preprocessor_Unit guard_unit = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Macro_Environment guard_environment = {0};
    Noc_Preprocessor_Conditional_Groups guard_groups = {0};
    Noc_Pragma_Once pragma_once = {0};
    Noc_Include_Guard guard = {0};
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
    REQUIRE(unit.count == 3);
    REQUIRE(unit.macro_directive_count == 1);
    REQUIRE(noc_pragma_once_build(&unit, 2, &pragma_once) ==
            NOC_INCLUDE_CONTROL_BUILD_OK);
    REQUIRE(noc_pragma_once_is_valid(&pragma_once));
    REQUIRE(pragma_once.status == NOC_PRAGMA_ONCE_VALID);
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

    REQUIRE(noc_workspace_open_document(&workspace,
                                        "release-runtime.h",
                                        guard_source,
                                        sizeof(guard_source) - 1,
                                        NOC_SOURCE_CLASS_PROJECT,
                                        &guard_snapshot) == NOC_WORKSPACE_OK);
    REQUIRE(noc_preprocessor_unit_build(&context,
                                        &guard_snapshot,
                                        NOC_MACROS_FULL,
                                        &guard_unit));
    REQUIRE(noc_preprocessor_conditional_groups_build(
                &guard_environment,
                0,
                &guard_unit,
                noc_macro_expansion_default_limits(),
                &guard_groups) == NOC_CONDITIONAL_GROUPS_OK);
    REQUIRE(noc_include_guard_build(&guard_unit, &guard_groups, &guard) ==
            NOC_INCLUDE_CONTROL_BUILD_OK);
    REQUIRE(noc_include_guard_is_valid(&guard));
    REQUIRE(guard.status == NOC_INCLUDE_GUARD_CANONICAL);
    REQUIRE(guard.definition_allowed);
    REQUIRE(guard.guard_name.count == sizeof("RELEASE_RUNTIME_H") - 1);
    REQUIRE(guard.guard_name.data != NULL &&
            memcmp(guard.guard_name.data,
                   "RELEASE_RUNTIME_H",
                   sizeof("RELEASE_RUNTIME_H") - 1) == 0);

    noc_preprocessor_conditional_groups_free(&guard_groups);
    noc_macro_environment_free(&guard_environment);
    noc_preprocessor_unit_free(&guard_unit);
    noc_document_snapshot_free(&guard_snapshot);
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
