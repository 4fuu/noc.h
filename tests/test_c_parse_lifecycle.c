#include "test_support.h"

static bool cancel_immediately(void *user_data)
{
    size_t *calls = (size_t *)user_data;
    *calls += 1;
    return true;
}

typedef struct {
    size_t calls;
    size_t cancel_after;
} Delayed_Cancel;

static bool cancel_after_delay(void *user_data)
{
    Delayed_Cancel *state = (Delayed_Cancel *)user_data;
    state->calls += 1;
    return state->calls >= state->cancel_after;
}

static void check_preserved(const Noc_C_Parse_Tree *tree,
                            Noc_C_Parse_Tree_Impl *implementation,
                            size_t generation,
                            size_t node_count)
{
    CHECK(tree->impl == implementation);
    CHECK(tree->generation == generation);
    CHECK(noc_c_parse_tree_is_valid(tree));
    CHECK(noc_c_parse_tree_node_count(tree) == node_count);
}

static void test_transactional_failures_and_rebuild(void)
{
    static const char old_source[] = "int old_value = 1;\n";
    static const char new_source[] =
        "int new_value = 2;\n"
        "int read_value(void) { return new_value; }\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot old_snapshot = {0};
    Noc_Document_Snapshot new_snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Parse_Options options = noc_c_parse_default_options();
    Noc_C_Parse_Tree_Impl *implementation;
    size_t generation;
    size_t node_count;
    size_t cancel_calls = 0;
    const char *const status_names[] = {
        "ok",
        "invalid-argument",
        "cancelled",
        "limit-exceeded",
        "generation-exhausted",
        "out-of-memory",
        "engine-failure",
    };
    size_t status_index;

    noc_workspace_init(&workspace);
    for (status_index = 0;
         status_index < sizeof(status_names) / sizeof(status_names[0]);
         ++status_index) {
        CHECK(strcmp(noc_c_parse_status_name(
                         (Noc_C_Parse_Status)status_index),
                     status_names[status_index]) == 0);
    }
    CHECK(strcmp(noc_c_parse_status_name((Noc_C_Parse_Status)999),
                 "unknown") == 0);
    CHECK(options.max_source_bytes >= sizeof(old_source) - 1);
    CHECK(options.max_nodes > 1);
    CHECK(noc_workspace_open_document(&workspace,
                                      "parse/lifecycle.c",
                                      old_source,
                                      sizeof(old_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &old_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&old_snapshot, options, &tree) ==
          NOC_C_PARSE_OK);
    implementation = tree.impl;
    generation = tree.generation;
    node_count = noc_c_parse_tree_node_count(&tree);

    options.max_source_bytes = sizeof(old_source) - 2;
    CHECK(noc_c_parse_tree_build(&old_snapshot, options, &tree) ==
          NOC_C_PARSE_LIMIT_EXCEEDED);
    check_preserved(&tree, implementation, generation, node_count);

    options = noc_c_parse_default_options();
    options.max_nodes = 1;
    CHECK(noc_c_parse_tree_build(&old_snapshot, options, &tree) ==
          NOC_C_PARSE_LIMIT_EXCEEDED);
    check_preserved(&tree, implementation, generation, node_count);

    options = noc_c_parse_default_options();
    options.should_cancel = cancel_immediately;
    options.cancel_user_data = &cancel_calls;
    CHECK(noc_c_parse_tree_build(&old_snapshot, options, &tree) ==
          NOC_C_PARSE_CANCELLED);
    CHECK(cancel_calls == 1);
    check_preserved(&tree, implementation, generation, node_count);

    memset(&options, 0, sizeof(options));
    CHECK(noc_c_parse_tree_build(&old_snapshot, options, &tree) ==
          NOC_C_PARSE_INVALID_ARGUMENT);
    check_preserved(&tree, implementation, generation, node_count);

    CHECK(noc_workspace_update_document(&workspace,
                                        &old_snapshot,
                                        new_source,
                                        sizeof(new_source) - 1,
                                        &new_snapshot) == NOC_WORKSPACE_OK);
    CHECK(slice_equals(noc_document_snapshot_source(
                           noc_c_parse_tree_snapshot(&tree)),
                       old_source));
    CHECK(noc_c_parse_tree_build(&new_snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(tree.generation == generation + 1);
    CHECK(tree.impl != implementation);
    CHECK(noc_c_parse_tree_node_count(&tree) > node_count);
    CHECK(slice_equals(noc_document_snapshot_source(
                           noc_c_parse_tree_snapshot(&tree)),
                       new_source));
    generation = tree.generation;
    CHECK(noc_c_parse_tree_build(noc_c_parse_tree_snapshot(&tree),
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(tree.generation == generation + 1);
    CHECK(slice_equals(noc_document_snapshot_source(
                           noc_c_parse_tree_snapshot(&tree)),
                       new_source));

    noc_document_snapshot_free(&old_snapshot);
    noc_document_snapshot_free(&new_snapshot);
    noc_workspace_deinit(&workspace);
    CHECK(noc_c_parse_tree_is_valid(&tree));
    CHECK(slice_equals(noc_c_parse_node_source(&tree, 0), new_source));
    noc_c_parse_tree_free(&tree);
}

static void test_generation_exhaustion_without_work(void)
{
    static const char source[] = "int value;\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree exhausted = {0};

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "parse/exhausted.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    exhausted.generation = SIZE_MAX;
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &exhausted) ==
          NOC_C_PARSE_GENERATION_EXHAUSTED);
    CHECK(exhausted.impl == NULL);
    CHECK(exhausted.generation == SIZE_MAX);
    noc_c_parse_tree_free(&exhausted);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static void test_engine_progress_cancellation_is_transactional(void)
{
    static const char baseline_source[] = "int preserved;\n";
    static const char line[] = "int repeated;\n";
    const size_t source_count = 512 * 1024;
    char *source = (char *)malloc(source_count);
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot baseline = {0};
    Noc_Document_Snapshot large = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Parse_Options options = noc_c_parse_default_options();
    Noc_C_Parse_Tree_Impl *implementation;
    Delayed_Cancel cancellation = {0, 16};
    size_t generation;
    size_t node_count;
    size_t offset;

    CHECK(source != NULL);
    if (!source) return;
    for (offset = 0; offset < source_count;) {
        size_t remaining = source_count - offset;
        size_t copy_count = remaining < sizeof(line) - 1
                                ? remaining
                                : sizeof(line) - 1;
        memcpy(source + offset, line, copy_count);
        offset += copy_count;
    }
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "parse/preserved.c",
                                      baseline_source,
                                      sizeof(baseline_source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &baseline) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "parse/cancel-large.c",
                                      source,
                                      source_count,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &large) == NOC_WORKSPACE_OK);
    free(source);
    CHECK(noc_c_parse_tree_build(&baseline,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    implementation = tree.impl;
    generation = tree.generation;
    node_count = noc_c_parse_tree_node_count(&tree);
    options.max_nodes = 1;
    options.should_cancel = cancel_after_delay;
    options.cancel_user_data = &cancellation;
    CHECK(noc_c_parse_tree_build(&large, options, &tree) ==
          NOC_C_PARSE_CANCELLED);
    /* max_nodes=1 would stop flattening before this many polls; reaching the
       threshold proves that the engine progress callback cooperated. */
    CHECK(cancellation.calls == cancellation.cancel_after);
    check_preserved(&tree, implementation, generation, node_count);

    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&baseline);
    noc_document_snapshot_free(&large);
    noc_workspace_deinit(&workspace);
}

int main(void)
{
    test_transactional_failures_and_rebuild();
    test_generation_exhaustion_without_work();
    test_engine_progress_cancellation_is_transactional();
    return finish_suite("C parse lifecycle and limits");
}
