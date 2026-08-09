#include "macro_expansion_test_support.h"

static size_t find_kind(const Noc_Logical_C_Ast *ast, Noc_C_Ast_Kind kind)
{
    size_t index;
    for (index = 0; index < noc_logical_c_ast_node_count(ast); ++index) {
        const Noc_Logical_C_Ast_Node *node =
            noc_logical_c_ast_node_at(ast, index);
        if (node && node->kind == kind) return index;
    }
    return NOC_C_AST_NODE_NONE;
}

static size_t find_source(const Noc_Logical_C_Ast *ast, const char *spelling)
{
    size_t index;
    for (index = 0; index < noc_logical_c_ast_node_count(ast); ++index) {
        if (slice_equals(noc_logical_c_ast_node_source(ast, index), spelling)) {
            return index;
        }
    }
    return NOC_C_AST_NODE_NONE;
}

static void check_topology(const Noc_Logical_C_Ast *ast)
{
    Noc_Slice text = noc_logical_source_text(noc_logical_c_ast_source(ast));
    size_t index;
    for (index = 0; index < noc_logical_c_ast_node_count(ast); ++index) {
        const Noc_Logical_C_Ast_Node *node =
            noc_logical_c_ast_node_at(ast, index);
        size_t child;
        size_t children = 0;
        CHECK(node != NULL);
        if (!node) continue;
        CHECK(node->generation == noc_logical_c_ast_generation(ast));
        CHECK(node->kind != NOC_C_AST_KIND_UNKNOWN);
        CHECK(node->field != NOC_C_AST_FIELD_UNKNOWN);
        CHECK(node->bytes.begin <= node->bytes.end);
        CHECK(node->bytes.end <= text.count);
        CHECK(index == 0 ? node->parent == NOC_C_AST_NODE_NONE
                         : node->parent < index);
        CHECK(noc_logical_c_ast_node_source(ast, index).count ==
              node->bytes.end - node->bytes.begin);
        child = node->first_child;
        while (child != NOC_C_AST_NODE_NONE) {
            const Noc_Logical_C_Ast_Node *child_node =
                noc_logical_c_ast_node_at(ast, child);
            CHECK(child_node != NULL);
            if (!child_node) break;
            CHECK(child_node->parent == index);
            CHECK(node->bytes.begin <= child_node->bytes.begin);
            CHECK(child_node->bytes.end <= node->bytes.end);
            children += 1;
            CHECK(children <= node->child_count);
            child = child_node->next_sibling;
        }
        CHECK(children == node->child_count);
    }
}

static void test_normalized_details_queries_and_macro_provenance(void)
{
    static const char definitions[] =
        "#define TYPE unsigned long\n"
        "#define VALUE 3\n"
        "#define ADD(left, right) ((left) + (right))\n";
    static const char input[] =
        "__attribute__((unused)) int decorated;\n"
        "static const TYPE data[4];\n"
        "int generated(void) { return ADD(data[0], VALUE); }\n";
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    Noc_Logical_C_Ast ast = {0};
    const Noc_Logical_C_Ast_Node *root;
    const Noc_Logical_Source *retained;
    Noc_C_Ast_Type_Spelling type_spelling;
    Noc_C_Ast_Array_Detail array_detail;
    Noc_Logical_Token_Range token_range;
    Noc_Logical_Byte_Range identifier_range;
    Noc_Logical_Location location;
    size_t storage;
    size_t qualifier;
    size_t primitive;
    size_t sized_type;
    size_t array;
    size_t binary;
    size_t extension;
    size_t identifier;
    size_t function;
    size_t token_index;
    bool saw_value_replacement = false;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    CHECK(noc_logical_c_ast_build(&tree,
                                  noc_c_ast_default_options(),
                                  &ast) == NOC_C_AST_OK);
    CHECK(noc_logical_c_ast_is_valid(&ast));
    CHECK(noc_logical_c_ast_is_syntax_complete(&ast));
    CHECK(noc_logical_c_ast_issues(&ast) == 0);
    CHECK(noc_logical_c_ast_generation(&ast) == 1);
    CHECK(noc_logical_c_ast_source_generation(&ast) == source.generation);
    CHECK(noc_logical_c_ast_root(&ast) == 0);
    retained = noc_logical_c_ast_source(&ast);
    CHECK(retained != NULL);
    CHECK(retained && noc_logical_source_text(retained).data ==
                          noc_logical_source_text(&source).data);

    root = noc_logical_c_ast_node_at(&ast, 0);
    CHECK(root != NULL && root->kind == NOC_C_AST_KIND_TRANSLATION_UNIT);
    CHECK(root && root->bytes.begin == 0);
    CHECK(root && root->bytes.end == noc_logical_source_text(&source).count);
    CHECK(noc_logical_c_ast_node_source(&ast, 0).data ==
          noc_logical_source_text(retained).data);
    location = noc_logical_c_ast_node_location(&ast, 0);
    CHECK(location.offset == 0 && location.line == 1 &&
          location.byte_column == 1);

    storage = find_kind(&ast, NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER);
    qualifier = find_kind(&ast, NOC_C_AST_KIND_TYPE_QUALIFIER);
    primitive = find_kind(&ast, NOC_C_AST_KIND_PRIMITIVE_TYPE);
    sized_type = find_kind(&ast, NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER);
    array = find_kind(&ast, NOC_C_AST_KIND_ARRAY_DECLARATOR);
    binary = find_kind(&ast, NOC_C_AST_KIND_BINARY_EXPRESSION);
    extension = find_kind(&ast, NOC_C_AST_KIND_ATTRIBUTE_SPECIFIER);
    function = find_kind(&ast, NOC_C_AST_KIND_FUNCTION_DEFINITION);
    identifier = find_source(&ast, "generated");
    CHECK(storage != NOC_C_AST_NODE_NONE);
    CHECK(qualifier != NOC_C_AST_NODE_NONE);
    CHECK(primitive != NOC_C_AST_NODE_NONE);
    CHECK(sized_type != NOC_C_AST_NODE_NONE);
    CHECK(array != NOC_C_AST_NODE_NONE);
    CHECK(binary != NOC_C_AST_NODE_NONE);
    CHECK(extension != NOC_C_AST_NODE_NONE);
    CHECK(function != NOC_C_AST_NODE_NONE);
    CHECK(identifier != NOC_C_AST_NODE_NONE);
    CHECK(noc_logical_c_ast_node_specifier(&ast, storage) ==
          NOC_C_AST_SPECIFIER_STATIC);
    CHECK(noc_logical_c_ast_node_qualifier(&ast, qualifier) ==
          NOC_C_AST_QUALIFIER_CONST);
    CHECK(noc_logical_c_ast_node_type_spelling(&ast,
                                               primitive,
                                               &type_spelling));
    CHECK(type_spelling.primitive == NOC_C_AST_PRIMITIVE_INT);
    CHECK(noc_logical_c_ast_node_type_spelling(&ast,
                                               sized_type,
                                               &type_spelling));
    CHECK((type_spelling.flags & NOC_C_AST_TYPE_UNSIGNED) != 0);
    CHECK(type_spelling.long_count == 1);
    CHECK(noc_logical_c_ast_node_array_detail(&ast, array, &array_detail));
    CHECK(array_detail.size == NOC_C_AST_ARRAY_SIZE_EXPRESSION);
    CHECK(!array_detail.has_static_minimum);
    CHECK(noc_logical_c_ast_node_operator(&ast, binary) ==
          NOC_C_AST_OPERATOR_ADD);
    CHECK(noc_logical_c_ast_node_extension(&ast, extension) ==
          NOC_C_AST_EXTENSION_GNU_ATTRIBUTE);
    CHECK(noc_logical_c_ast_node_expected(&ast, binary).kind ==
          NOC_C_AST_EXPECTED_NONE);

    identifier_range = noc_logical_c_ast_node_at(&ast, identifier)->bytes;
    CHECK(noc_logical_c_ast_node_at_offset(&ast, identifier_range.begin) ==
          identifier);
    CHECK(noc_logical_c_ast_node_covering_range(&ast, identifier_range) ==
          identifier);
    CHECK(noc_logical_c_ast_depth(&ast, identifier) > 0);
    CHECK(noc_logical_c_ast_common_ancestor(&ast, identifier, binary) ==
          function);
    CHECK(noc_logical_c_ast_node_at_offset(
              &ast, noc_logical_source_text(retained).count) ==
          NOC_C_AST_NODE_NONE);

    CHECK(noc_logical_c_ast_node_token_range(&ast, function, &token_range));
    for (token_index = token_range.begin;
         token_index < token_range.end;
         ++token_index) {
        Noc_Logical_Token_Macro_Provenance provenance;
        if (slice_equals(noc_logical_source_token_text(retained, token_index),
                         "3") &&
            noc_logical_source_token_macro_provenance(retained,
                                                       token_index,
                                                       &provenance) &&
            provenance.macro_origin ==
                NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT &&
            provenance.macro_frame_index != NOC_TOKEN_INDEX_NONE) {
            saw_value_replacement = true;
        }
    }
    CHECK(saw_value_replacement);
    check_topology(&ast);

    noc_logical_c_ast_free(&ast);
    noc_logical_c_parse_tree_free(&tree);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
}

static void test_recovery_keeps_logical_missing_expectation(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    Noc_Logical_C_Ast ast = {0};
    Noc_C_Ast_Expected expected = {0};
    Noc_Logical_Token_Range range = {0};
    size_t index;
    size_t missing = NOC_C_AST_NODE_NONE;

    macro_fixture_init(&fixture, "", "int broken(void) { return 1 }\n");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_c_parse_tree_build(&source,
                                         noc_c_parse_default_options(),
                                         &tree) == NOC_C_PARSE_OK);
    CHECK(noc_logical_c_ast_build(&tree,
                                  noc_c_ast_default_options(),
                                  &ast) == NOC_C_AST_OK);
    CHECK(!noc_logical_c_ast_is_syntax_complete(&ast));
    CHECK((noc_logical_c_ast_issues(&ast) & NOC_C_AST_ISSUE_MISSING) != 0);
    for (index = 0; index < noc_logical_c_ast_node_count(&ast); ++index) {
        const Noc_Logical_C_Ast_Node *node =
            noc_logical_c_ast_node_at(&ast, index);
        if (node && (node->flags & NOC_C_AST_NODE_MISSING) != 0) {
            missing = index;
            break;
        }
    }
    CHECK(missing != NOC_C_AST_NODE_NONE);
    expected = noc_logical_c_ast_node_expected(&ast, missing);
    CHECK(expected.kind == NOC_C_AST_EXPECTED_PUNCTUATOR);
    CHECK(slice_equals(expected.spelling, ";"));
    CHECK(noc_logical_c_ast_node_at(&ast, missing)->bytes.begin ==
          noc_logical_c_ast_node_at(&ast, missing)->bytes.end);
    CHECK(noc_logical_c_ast_node_source(&ast, missing).count == 0);
    CHECK(noc_logical_c_ast_node_token_range(&ast, missing, &range));
    CHECK(range.begin == range.end);
    check_topology(&ast);

    noc_logical_c_ast_free(&ast);
    noc_logical_c_parse_tree_free(&tree);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_normalized_details_queries_and_macro_provenance();
    test_recovery_keeps_logical_missing_expectation();
    return finish_suite("logical normalized C AST");
}
