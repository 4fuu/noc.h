#include "test_support.h"

static size_t find_flag(const Noc_C_Parse_Tree *tree, unsigned int flag)
{
    size_t index;
    for (index = 0; index < noc_c_parse_tree_node_count(tree); ++index) {
        const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, index);
        if (node && (node->flags & flag) != 0) return index;
    }
    return NOC_C_PARSE_NODE_NONE;
}

static size_t find_kind(const Noc_C_Parse_Tree *tree, const char *kind)
{
    size_t index;
    for (index = 0; index < noc_c_parse_tree_node_count(tree); ++index) {
        const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, index);
        if (node && slice_equals(node->kind, kind)) return index;
    }
    return NOC_C_PARSE_NODE_NONE;
}

static void test_missing_token_is_zero_width(void)
{
    static const char source[] = "int f(void) { return 1 }\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    const Noc_C_Parse_Node *missing;
    Noc_Slice insertion;
    Noc_Location location;
    size_t missing_index;

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "parse/missing.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_parse_tree_is_valid(&tree));
    CHECK(noc_c_parse_tree_has_error(&tree));
    missing_index = find_flag(&tree, NOC_C_PARSE_NODE_MISSING);
    CHECK(missing_index != NOC_C_PARSE_NODE_NONE);
    missing = noc_c_parse_tree_node_at(&tree, missing_index);
    CHECK(missing != NULL);
    CHECK(slice_equals(missing->kind, ";"));
    CHECK((missing->flags & NOC_C_PARSE_NODE_HAS_ERROR) != 0);
    CHECK(missing->bytes.begin == missing->bytes.end);
    insertion = noc_c_parse_node_source(&tree, missing_index);
    CHECK(insertion.data != NULL);
    CHECK(insertion.count == 0);
    CHECK(insertion.data ==
          noc_document_snapshot_source(noc_c_parse_tree_snapshot(&tree)).data +
              missing->bytes.begin);
    location = noc_c_parse_node_location(&tree, missing_index);
    CHECK(location.line == 1);
    CHECK(location.column == 23);

    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static void test_incomplete_editor_input_keeps_structure(void)
{
    static const char source[] =
        "#define VALUE 3\n"
        "int unfinished(int x) {\n"
        "    if (x > VALUE) {\n"
        "        return x + ;\n"
        "    }\n"
        "}\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    size_t error_index;

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "parse/incomplete.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_parse_tree_is_valid(&tree));
    CHECK(noc_c_parse_tree_has_error(&tree));
    CHECK(find_kind(&tree, "preproc_def") != NOC_C_PARSE_NODE_NONE);
    CHECK(find_kind(&tree, "function_definition") != NOC_C_PARSE_NODE_NONE);
    CHECK(find_kind(&tree, "if_statement") != NOC_C_PARSE_NODE_NONE);
    error_index = find_flag(&tree, NOC_C_PARSE_NODE_ERROR);
    CHECK(error_index != NOC_C_PARSE_NODE_NONE ||
          find_flag(&tree, NOC_C_PARSE_NODE_MISSING) !=
              NOC_C_PARSE_NODE_NONE);
    CHECK(noc_c_parse_tree_node_at(&tree, 0)->bytes.end ==
          sizeof(source) - 1);

    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static void test_skipped_edge_byte_remains_in_document_root(void)
{
    static const char source[] = "\vint value;\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    const Noc_C_Parse_Node *root;

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "parse/skipped-edge.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_parse_tree_is_valid(&tree));
    CHECK(noc_c_parse_tree_has_error(&tree));
    root = noc_c_parse_tree_node_at(&tree, 0);
    CHECK(root != NULL);
    CHECK(root->bytes.begin == 0);
    CHECK(root->bytes.end == sizeof(source) - 1);
    CHECK((root->flags & NOC_C_PARSE_NODE_SKIPPED_SOURCE) != 0);
    CHECK(slice_equals(noc_c_parse_node_source(&tree, 0), source));

    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

int main(void)
{
    test_missing_token_is_zero_width();
    test_incomplete_editor_input_keeps_structure();
    test_skipped_edge_byte_remains_in_document_root();
    return finish_suite("recoverable C parse errors");
}
