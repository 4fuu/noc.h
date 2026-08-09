#include "macro_expansion_test_support.h"

static bool cancel_immediately(void *user_data)
{
    size_t *calls = (size_t *)user_data;
    *calls += 1;
    return true;
}

static void test_immediate_cancellation_precedes_large_topology_work(void)
{
    enum { SPLICE_COUNT = 8192 };
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    Noc_C_Parse_Options options = noc_c_parse_default_options();
    Noc_Macro_Expansion_Token *original_items;
    Noc_Macro_Expansion_Token *items;
    size_t original_count;
    size_t original_capacity;
    size_t cancel_calls = 0;
    size_t index;

    macro_fixture_init(&fixture, "", ";\\\n+");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(fixture.expansion.count == 3);
    if (fixture.expansion.count != 3) {
        macro_fixture_deinit(&fixture);
        return;
    }
    items = (Noc_Macro_Expansion_Token *)malloc(
        (SPLICE_COUNT + 2) * sizeof(*items));
    CHECK(items != NULL);
    if (!items) {
        macro_fixture_deinit(&fixture);
        return;
    }
    original_items = fixture.expansion.items;
    original_count = fixture.expansion.count;
    original_capacity = fixture.expansion.capacity;
    items[0] = original_items[0];
    for (index = 0; index < SPLICE_COUNT; ++index) {
        items[index + 1] = original_items[1];
    }
    items[SPLICE_COUNT + 1] = original_items[2];
    fixture.expansion.items = items;
    fixture.expansion.count = SPLICE_COUNT + 2;
    fixture.expansion.capacity = SPLICE_COUNT + 2;
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    fixture.expansion.items = original_items;
    fixture.expansion.count = original_count;
    fixture.expansion.capacity = original_capacity;
    free(items);
    CHECK(noc_logical_source_text(&source).count == 3);
    CHECK(noc_logical_source_token_count(&source) > SPLICE_COUNT);

    options.should_cancel = cancel_immediately;
    options.cancel_user_data = &cancel_calls;
    CHECK(noc_logical_c_parse_tree_build(&source, options, &tree) ==
          NOC_C_PARSE_CANCELLED);
    CHECK(cancel_calls == 1);
    CHECK(tree.impl == NULL && tree.generation == 0);

    noc_logical_c_parse_tree_free(&tree);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
}

static void check_preserved(const Noc_Logical_C_Parse_Tree *tree,
                            Noc_Logical_C_Parse_Tree_Impl *implementation,
                            size_t generation,
                            size_t node_count,
                            const char *text)
{
    const Noc_Logical_Source *retained =
        noc_logical_c_parse_tree_source(tree);
    CHECK(tree->impl == implementation);
    CHECK(tree->generation == generation);
    CHECK(noc_logical_c_parse_tree_is_valid(tree));
    CHECK(noc_logical_c_parse_tree_node_count(tree) == node_count);
    CHECK(retained != NULL && slice_equals(noc_logical_source_text(retained),
                                           text));
}

static void test_clone_and_retained_revision(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Source clone = {0};
    Noc_Slice source_text;

    macro_fixture_init(&fixture, "", "int retained;\n");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    source_text = noc_logical_source_text(&source);
    CHECK(noc_logical_source_clone(&source, &clone) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_is_valid(&clone));
    CHECK(clone.generation == source.generation);
    CHECK(noc_logical_source_text(&clone).data == source_text.data);
    CHECK(noc_logical_source_clone(&clone, &clone) == NOC_LOGICAL_SOURCE_OK);

    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
    CHECK(noc_logical_source_is_valid(&clone));
    CHECK(noc_logical_source_text(&clone).data == source_text.data);
    CHECK(slice_equals(noc_logical_source_text(&clone), "int retained ;\n"));
    CHECK(noc_logical_source_clone(NULL, &clone) ==
          NOC_LOGICAL_SOURCE_INVALID_ARGUMENT);
    CHECK(noc_logical_source_is_valid(&clone));
    noc_logical_source_free(&clone);
}

static void test_transactional_limits_cancellation_and_rebuild(void)
{
    Macro_Expansion_Fixture old_fixture;
    Macro_Expansion_Fixture new_fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    Noc_Logical_C_Parse_Tree exhausted = {0};
    Noc_C_Parse_Options options = noc_c_parse_default_options();
    Noc_Logical_C_Parse_Tree_Impl *implementation;
    size_t generation;
    size_t node_count;
    size_t cancel_calls = 0;

    macro_fixture_init(&old_fixture, "", "int old_value;\n");
    CHECK(macro_fixture_expand(&old_fixture,
                               macro_fixture_full_input(&old_fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &old_fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_c_parse_tree_build(&source, options, &tree) ==
          NOC_C_PARSE_OK);
    implementation = tree.impl;
    generation = tree.generation;
    node_count = noc_logical_c_parse_tree_node_count(&tree);

    options.max_source_bytes = strlen("int old_value;\n") - 1;
    CHECK(noc_logical_c_parse_tree_build(&source, options, &tree) ==
          NOC_C_PARSE_LIMIT_EXCEEDED);
    check_preserved(&tree, implementation, generation, node_count,
                    "int old_value ;\n");

    options = noc_c_parse_default_options();
    options.max_nodes = 1;
    CHECK(noc_logical_c_parse_tree_build(&source, options, &tree) ==
          NOC_C_PARSE_LIMIT_EXCEEDED);
    check_preserved(&tree, implementation, generation, node_count,
                    "int old_value ;\n");

    options = noc_c_parse_default_options();
    options.should_cancel = cancel_immediately;
    options.cancel_user_data = &cancel_calls;
    CHECK(noc_logical_c_parse_tree_build(&source, options, &tree) ==
          NOC_C_PARSE_CANCELLED);
    CHECK(cancel_calls == 1);
    check_preserved(&tree, implementation, generation, node_count,
                    "int old_value ;\n");

    memset(&options, 0, sizeof(options));
    CHECK(noc_logical_c_parse_tree_build(&source, options, &tree) ==
          NOC_C_PARSE_INVALID_ARGUMENT);
    CHECK(noc_logical_c_parse_tree_build(NULL,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_INVALID_ARGUMENT);
    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         NULL) == NOC_C_PARSE_INVALID_ARGUMENT);
    check_preserved(&tree, implementation, generation, node_count,
                    "int old_value ;\n");

    exhausted.generation = SIZE_MAX;
    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &exhausted) ==
          NOC_C_PARSE_GENERATION_EXHAUSTED);
    CHECK(exhausted.impl == NULL && exhausted.generation == SIZE_MAX);
    noc_logical_c_parse_tree_free(&exhausted);

    /* Rebuilding the caller's logical handle detaches from the revision retained
       by the old tree; only a successful tree rebuild changes tree queries. */
    macro_fixture_init(&new_fixture,
                       "#define VALUE 7\n",
                       "int new_value = VALUE;\n");
    CHECK(macro_fixture_expand(&new_fixture,
                               macro_fixture_full_input(&new_fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &new_fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    check_preserved(&tree, implementation, generation, node_count,
                    "int old_value ;\n");
    CHECK(slice_equals(noc_logical_source_text(&source),
                       "int new_value = 7 ;\n"));

    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    CHECK(tree.impl != implementation);
    CHECK(tree.generation == generation + 1);
    CHECK(slice_equals(noc_logical_source_text(
                           noc_logical_c_parse_tree_source(&tree)),
                       "int new_value = 7 ;\n"));

    noc_logical_source_free(&source);
    macro_fixture_deinit(&old_fixture);
    macro_fixture_deinit(&new_fixture);
    CHECK(noc_logical_c_parse_tree_is_valid(&tree));
    CHECK(slice_equals(noc_logical_c_parse_node_source(&tree, 0),
                       "int new_value = 7 ;\n"));

    noc_logical_c_parse_tree_free(&tree);
    CHECK(!noc_logical_c_parse_tree_is_valid(&tree));
    CHECK(noc_logical_c_parse_tree_generation(&tree) == 0);
    CHECK(noc_logical_c_parse_tree_source(&tree) == NULL);
    CHECK(noc_logical_c_parse_tree_node_count(&tree) == 0);
    CHECK(noc_logical_c_parse_tree_root(&tree) == NOC_C_PARSE_NODE_NONE);
    CHECK(noc_logical_c_parse_tree_node_at(&tree, 0) == NULL);
    CHECK(!noc_logical_c_parse_tree_has_error(&tree));
    CHECK(noc_logical_c_parse_node_source(&tree, 0).data == NULL);
    CHECK(noc_logical_c_parse_node_location(&tree, 0).line == 0);
    CHECK(!noc_logical_c_parse_node_token_range(&tree, 0, NULL));
    noc_logical_c_parse_tree_free(&tree);
    noc_logical_c_parse_tree_free(NULL);
}

int main(void)
{
    test_clone_and_retained_revision();
    test_immediate_cancellation_precedes_large_topology_work();
    test_transactional_limits_cancellation_and_rebuild();
    return finish_suite("logical C parse lifecycle");
}
