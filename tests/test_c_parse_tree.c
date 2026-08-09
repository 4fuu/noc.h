#include "test_support.h"

static size_t find_kind(const Noc_C_Parse_Tree *tree, const char *kind)
{
    size_t index;
    for (index = 0; index < noc_c_parse_tree_node_count(tree); ++index) {
        const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, index);
        if (node && slice_equals(node->kind, kind)) return index;
    }
    return NOC_C_PARSE_NODE_NONE;
}

static size_t find_field_kind(const Noc_C_Parse_Tree *tree,
                              const char *field,
                              const char *kind)
{
    size_t index;
    for (index = 0; index < noc_c_parse_tree_node_count(tree); ++index) {
        const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, index);
        if (node && slice_equals(node->field, field) &&
            slice_equals(node->kind, kind)) {
            return index;
        }
    }
    return NOC_C_PARSE_NODE_NONE;
}

static void check_tree_invariants(const Noc_C_Parse_Tree *tree)
{
    const Noc_Document_Snapshot *snapshot = noc_c_parse_tree_snapshot(tree);
    Noc_Slice source = noc_document_snapshot_source(snapshot);
    size_t count = noc_c_parse_tree_node_count(tree);
    size_t index;
    for (index = 0; index < count; ++index) {
        const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, index);
        size_t child = node->first_child;
        size_t child_count = 0;
        CHECK(node != NULL);
        CHECK(node->generation == noc_c_parse_tree_generation(tree));
        CHECK(node->bytes.begin <= node->bytes.end);
        CHECK(node->bytes.end <= source.count);
        CHECK(node->kind.data != NULL);
        CHECK(node->kind.count != 0);
        if (index == 0) {
            CHECK(node->parent == NOC_C_PARSE_NODE_NONE);
        } else {
            const Noc_C_Parse_Node *parent;
            CHECK(node->parent < index);
            parent = noc_c_parse_tree_node_at(tree, node->parent);
            CHECK(parent != NULL);
            CHECK(parent->bytes.begin <= node->bytes.begin);
            CHECK(node->bytes.end <= parent->bytes.end);
        }
        while (child != NOC_C_PARSE_NODE_NONE) {
            const Noc_C_Parse_Node *child_node;
            CHECK(child < count);
            child_node = noc_c_parse_tree_node_at(tree, child);
            CHECK(child_node != NULL);
            CHECK(child_node->parent == index);
            child_count += 1;
            CHECK(child_count <= count);
            child = child_node->next_sibling;
        }
        CHECK(child_count == node->child_count);
        CHECK((node->child_count == 0) ==
              (node->first_child == NOC_C_PARSE_NODE_NONE));
        CHECK((node->child_count == 0) ==
              (node->last_child == NOC_C_PARSE_NODE_NONE));
        {
            Noc_Slice spelling = noc_c_parse_node_source(tree, index);
            CHECK(spelling.data != NULL);
            CHECK(spelling.count == node->bytes.end - node->bytes.begin);
            CHECK(spelling.count == 0 ||
                  memcmp(spelling.data,
                         source.data + node->bytes.begin,
                         spelling.count) == 0);
        }
    }
    CHECK(noc_c_parse_tree_node_at(tree, count) == NULL);
}

static void test_translation_unit_and_traversal(void)
{
    static const char source[] =
        "typedef struct Pair { unsigned bits : 3; int value; } Pair;\n"
        "enum Color { RED, GREEN = 4 };\n"
        "/* retained as an extra syntax node */\n"
        "static int add(int left, int right) {\n"
        "    int values[2] = { [1] = right };\n"
        "    return left + values[1];\n"
        "}\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    const Noc_C_Parse_Node *root;
    const Noc_C_Parse_Node *function;
    const Noc_C_Parse_Node *comment;
    const Noc_C_Parse_Node *keyword;
    Noc_Location location;
    size_t comment_index;
    size_t function_index;
    size_t keyword_index;

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "parse/tree.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_parse_tree_is_valid(&tree));
    CHECK(noc_c_parse_tree_generation(&tree) == 1);
    CHECK(noc_c_parse_tree_root(&tree) == 0);
    CHECK(!noc_c_parse_tree_has_error(&tree));
    CHECK(noc_c_parse_tree_node_count(&tree) > 40);
    root = noc_c_parse_tree_node_at(&tree, 0);
    CHECK(root != NULL);
    CHECK(slice_equals(root->kind, "translation_unit"));
    CHECK(root->bytes.begin == 0);
    CHECK(root->bytes.end == sizeof(source) - 1);
    CHECK((root->flags & NOC_C_PARSE_NODE_NAMED) != 0);

    CHECK(find_kind(&tree, "struct_specifier") != NOC_C_PARSE_NODE_NONE);
    CHECK(find_kind(&tree, "field_declaration") != NOC_C_PARSE_NODE_NONE);
    CHECK(find_kind(&tree, "enum_specifier") != NOC_C_PARSE_NODE_NONE);
    CHECK(find_kind(&tree, "initializer_list") != NOC_C_PARSE_NODE_NONE);
    CHECK(find_kind(&tree, "subscript_designator") != NOC_C_PARSE_NODE_NONE);
    comment_index = find_kind(&tree, "comment");
    CHECK(comment_index != NOC_C_PARSE_NODE_NONE);
    comment = noc_c_parse_tree_node_at(&tree, comment_index);
    CHECK(comment != NULL);
    CHECK((comment->flags & NOC_C_PARSE_NODE_NAMED) != 0);
    CHECK((comment->flags & NOC_C_PARSE_NODE_EXTRA) != 0);
    keyword_index = find_kind(&tree, "static");
    CHECK(keyword_index != NOC_C_PARSE_NODE_NONE);
    keyword = noc_c_parse_tree_node_at(&tree, keyword_index);
    CHECK(keyword != NULL);
    CHECK((keyword->flags & NOC_C_PARSE_NODE_NAMED) == 0);
    CHECK(find_field_kind(&tree, "body", "compound_statement") !=
          NOC_C_PARSE_NODE_NONE);

    function_index = find_kind(&tree, "function_definition");
    CHECK(function_index != NOC_C_PARSE_NODE_NONE);
    function = noc_c_parse_tree_node_at(&tree, function_index);
    CHECK(function != NULL);
    CHECK(slice_equals(noc_c_parse_node_source(&tree, function_index),
                       "static int add(int left, int right) {\n"
                       "    int values[2] = { [1] = right };\n"
                       "    return left + values[1];\n"
                       "}"));
    location = noc_c_parse_node_location(&tree, function_index);
    CHECK(location.path != NULL && strcmp(location.path, "parse/tree.c") == 0);
    CHECK(location.line == 4);
    CHECK(location.column == 1);
    CHECK(location.offset == function->bytes.begin);
    check_tree_invariants(&tree);

    /* The tree owns a snapshot clone; all physical source queries survive the
       caller's handles and workspace. */
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    CHECK(noc_c_parse_tree_is_valid(&tree));
    CHECK(slice_equals(noc_c_parse_tree_node_at(&tree, 0)->kind,
                       "translation_unit"));
    CHECK(noc_c_parse_node_location(&tree, function_index).line == 4);

    noc_c_parse_tree_free(&tree);
    CHECK(!noc_c_parse_tree_is_valid(&tree));
    CHECK(noc_c_parse_tree_root(&tree) == NOC_C_PARSE_NODE_NONE);
    CHECK(noc_c_parse_tree_snapshot(&tree) == NULL);
    CHECK(noc_c_parse_node_source(&tree, 0).data == NULL);
    CHECK(noc_c_parse_node_location(&tree, 0).path == NULL);
    noc_c_parse_tree_free(&tree);
    noc_c_parse_tree_free(NULL);
}

static void test_empty_translation_unit(void)
{
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    const Noc_C_Parse_Node *root;
    Noc_Slice source;

    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "parse/empty.c",
                                      NULL,
                                      0,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(!noc_c_parse_tree_has_error(&tree));
    CHECK(noc_c_parse_tree_node_count(&tree) == 1);
    root = noc_c_parse_tree_node_at(&tree, 0);
    CHECK(root != NULL);
    CHECK(slice_equals(root->kind, "translation_unit"));
    CHECK(root->bytes.begin == 0 && root->bytes.end == 0);
    source = noc_c_parse_node_source(&tree, 0);
    CHECK(source.data != NULL);
    CHECK(source.count == 0);

    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

int main(void)
{
    test_translation_unit_and_traversal();
    test_empty_translation_unit();
    return finish_suite("recoverable C parse tree");
}
