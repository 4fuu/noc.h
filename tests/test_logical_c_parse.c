#include "macro_expansion_test_support.h"

static size_t find_kind(const Noc_Logical_C_Parse_Tree *tree,
                        const char *kind)
{
    size_t index;
    for (index = 0; index < noc_logical_c_parse_tree_node_count(tree); ++index) {
        const Noc_Logical_C_Parse_Node *node =
            noc_logical_c_parse_tree_node_at(tree, index);
        if (node && slice_equals(node->kind, kind)) return index;
    }
    return NOC_C_PARSE_NODE_NONE;
}

static void check_tree_invariants(const Noc_Logical_C_Parse_Tree *tree)
{
    const Noc_Logical_Source *source =
        noc_logical_c_parse_tree_source(tree);
    Noc_Slice text = noc_logical_source_text(source);
    size_t count = noc_logical_c_parse_tree_node_count(tree);
    size_t index;

    CHECK(source != NULL);
    CHECK(count != 0);
    for (index = 0; index < count; ++index) {
        const Noc_Logical_C_Parse_Node *node =
            noc_logical_c_parse_tree_node_at(tree, index);
        Noc_Slice spelling;
        size_t child;
        size_t child_count = 0;
        CHECK(node != NULL);
        if (!node) continue;
        CHECK(node->generation == noc_logical_c_parse_tree_generation(tree));
        CHECK(node->bytes.begin <= node->bytes.end);
        CHECK(node->bytes.end <= text.count);
        CHECK(node->kind.data != NULL && node->kind.count != 0);
        if (index == 0) {
            CHECK(node->parent == NOC_C_PARSE_NODE_NONE);
        } else {
            const Noc_Logical_C_Parse_Node *parent;
            CHECK(node->parent < index);
            parent = noc_logical_c_parse_tree_node_at(tree, node->parent);
            CHECK(parent != NULL);
            if (parent) {
                CHECK(parent->bytes.begin <= node->bytes.begin);
                CHECK(node->bytes.end <= parent->bytes.end);
            }
        }
        spelling = noc_logical_c_parse_node_source(tree, index);
        CHECK(spelling.data == text.data + node->bytes.begin);
        CHECK(spelling.count == node->bytes.end - node->bytes.begin);
        child = node->first_child;
        while (child != NOC_C_PARSE_NODE_NONE) {
            const Noc_Logical_C_Parse_Node *child_node =
                noc_logical_c_parse_tree_node_at(tree, child);
            CHECK(child_node != NULL);
            if (!child_node) break;
            CHECK(child_node->parent == index);
            child_count += 1;
            CHECK(child_count <= count);
            child = child_node->next_sibling;
        }
        CHECK(child_count == node->child_count);
    }
    CHECK(noc_logical_c_parse_tree_node_at(tree, count) == NULL);
}

static void test_macro_expanded_tree_and_provenance_bridge(void)
{
    static const char definitions[] =
        "#define TYPE int\n"
        "#define NAME generated\n"
        "#define BODY(value) { return value; }\n";
    static const char input[] = "TYPE NAME(void) BODY(__LINE__)\n";
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    const Noc_Logical_Source *retained;
    const Noc_Logical_C_Parse_Node *root;
    const Noc_Logical_C_Parse_Node *function;
    Noc_Logical_Token_Range function_tokens;
    Noc_Logical_Location location;
    bool saw_name_replacement = false;
    bool saw_line_builtin = false;
    size_t function_index;
    size_t token_index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(slice_equals(noc_logical_source_text(&source),
                       "int generated ( void ) { return 1 ; }\n"));
    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    CHECK(noc_logical_c_parse_tree_is_valid(&tree));
    CHECK(noc_logical_c_parse_tree_generation(&tree) == 1);
    CHECK(noc_logical_c_parse_tree_root(&tree) == 0);
    CHECK(!noc_logical_c_parse_tree_has_error(&tree));
    retained = noc_logical_c_parse_tree_source(&tree);
    CHECK(retained != NULL);
    CHECK(retained &&
          noc_logical_source_generation(retained) == source.generation);
    CHECK(retained && noc_logical_source_text(retained).data ==
                          noc_logical_source_text(&source).data);

    root = noc_logical_c_parse_tree_node_at(&tree, 0);
    CHECK(root != NULL && slice_equals(root->kind, "translation_unit"));
    CHECK(root && root->bytes.begin == 0);
    CHECK(root && root->bytes.end == noc_logical_source_text(&source).count);
    function_index = find_kind(&tree, "function_definition");
    CHECK(function_index != NOC_C_PARSE_NODE_NONE);
    function = noc_logical_c_parse_tree_node_at(&tree, function_index);
    CHECK(function != NULL);
    CHECK(slice_equals(noc_logical_c_parse_node_source(&tree, function_index),
                       "int generated ( void ) { return 1 ; }"));
    location = noc_logical_c_parse_node_location(&tree, function_index);
    CHECK(location.offset == 0 && location.line == 1 &&
          location.byte_column == 1);
    CHECK(noc_logical_c_parse_node_token_range(&tree,
                                               function_index,
                                               &function_tokens));
    CHECK(function_tokens.begin < function_tokens.end);
    for (token_index = function_tokens.begin;
         token_index < function_tokens.end;
         ++token_index) {
        Noc_Logical_Token_Macro_Provenance provenance;
        Noc_Slice token_text = noc_logical_source_token_text(retained,
                                                             token_index);
        if (!noc_logical_source_token_macro_provenance(retained,
                                                       token_index,
                                                       &provenance)) {
            continue;
        }
        if (slice_equals(token_text, "generated") &&
            provenance.macro_origin ==
                NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT) {
            saw_name_replacement = true;
        }
        if (slice_equals(token_text, "1") &&
            provenance.macro_origin == NOC_MACRO_EXPANSION_TOKEN_BUILTIN &&
            provenance.builtin_kind == NOC_MACRO_BUILTIN_LINE) {
            saw_line_builtin = true;
        }
    }
    CHECK(saw_name_replacement);
    CHECK(saw_line_builtin);
    check_tree_invariants(&tree);

    /* The parse tree retains the exact logical revision and all token-level
       provenance after every temporary preprocessing input is gone. */
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
    CHECK(noc_logical_c_parse_tree_is_valid(&tree));
    retained = noc_logical_c_parse_tree_source(&tree);
    CHECK(retained != NULL);
    CHECK(retained && slice_equals(noc_logical_source_text(retained),
                                   "int generated ( void ) { return 1 ; }\n"));
    CHECK(noc_logical_c_parse_node_token_range(&tree,
                                               function_index,
                                               &function_tokens));
    noc_logical_c_parse_tree_free(&tree);
}

static void test_recoverable_malformed_logical_input(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    bool saw_recovery = false;
    Noc_Logical_Token_Range preserved = {91, 92};
    Noc_Logical_Token_Range range;
    size_t index;

    macro_fixture_init(&fixture, "", "int broken( {\n");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    CHECK(noc_logical_c_parse_tree_has_error(&tree));
    for (index = 0; index < noc_logical_c_parse_tree_node_count(&tree); ++index) {
        const Noc_Logical_C_Parse_Node *node =
            noc_logical_c_parse_tree_node_at(&tree, index);
        if (node && (node->flags & (NOC_C_PARSE_NODE_ERROR |
                                    NOC_C_PARSE_NODE_MISSING |
                                    NOC_C_PARSE_NODE_HAS_ERROR)) != 0) {
            saw_recovery = true;
        }
    }
    CHECK(saw_recovery);
    check_tree_invariants(&tree);
    range = preserved;
    CHECK(!noc_logical_c_parse_node_token_range(
        &tree, noc_logical_c_parse_tree_node_count(&tree), &range));
    CHECK(range.begin == preserved.begin && range.end == preserved.end);

    noc_logical_c_parse_tree_free(&tree);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_macro_expanded_tree_and_provenance_bridge();
    test_recoverable_malformed_logical_input();
    return finish_suite("logical C parse tree");
}
