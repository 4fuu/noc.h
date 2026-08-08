#include "test_support.h"

static size_t find_descendant_kind(const Noc_Syntax_Tree *tree,
                                   size_t root,
                                   Noc_Syntax_Kind kind)
{
    size_t node = noc_syntax_next_preorder(tree, root);
    while (node != NOC_SYNTAX_NONE) {
        const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
        if (syntax && syntax->kind == kind) return node;
        node = noc_syntax_next_preorder(tree, node);
    }
    return NOC_SYNTAX_NONE;
}

static void test_lossless_syntax_tree(void)
{
    static const char source[] =
        "int main(void) {\n"
        "    int values[2] = {1, 2};\n"
        "    return values[(1)];\n"
        "}\n";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    size_t root;
    size_t first;
    size_t second;
    size_t parameters;
    size_t body;
    size_t bracket;
    size_t bracket_value;
    size_t node;
    size_t visited = 0;
    Noc_Token_Range inner;
    Noc_Token_Range spanning;
    const Noc_Syntax_Node *bracket_syntax;
    const Noc_Token *token;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, "tree.c", source, sizeof(source) - 1, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_syntax_tree_is_valid(&tree));
    root = noc_syntax_root(&tree);
    CHECK(root == 0);
    CHECK(strcmp(noc_syntax_kind_name(NOC_SYNTAX_ROOT), "root") == 0);
    CHECK(strcmp(noc_syntax_kind_name(NOC_SYNTAX_BRACE_GROUP), "brace group") == 0);
    CHECK(slice_equals(noc_syntax_source(&tree, root), source));
    CHECK(noc_syntax_parent(&tree, root) == NOC_SYNTAX_NONE);
    CHECK(noc_syntax_child_count(&tree, root) > 4);

    first = noc_syntax_first_child(&tree, root);
    CHECK(first != NOC_SYNTAX_NONE);
    token = noc_syntax_token(&tree, first);
    CHECK(token != NULL && noc_token_is_identifier(*token, "int"));
    second = noc_syntax_next_sibling(&tree, first);
    CHECK(second != NOC_SYNTAX_NONE);
    CHECK(noc_syntax_parent(&tree, second) == root);

    parameters = noc_syntax_first_child_of_kind(&tree, root, NOC_SYNTAX_PAREN_GROUP);
    body = noc_syntax_first_child_of_kind(&tree, root, NOC_SYNTAX_BRACE_GROUP);
    CHECK(parameters != NOC_SYNTAX_NONE);
    CHECK(body != NOC_SYNTAX_NONE);
    CHECK(slice_equals(noc_syntax_source(&tree, parameters), "(void)"));
    inner = noc_syntax_inner_range(&tree, parameters);
    CHECK(slice_equals(noc_token_range_source(&stream, inner), "void"));
    CHECK(noc_syntax_location(&tree, body).line == 1);
    CHECK(noc_syntax_token(&tree, body) == NULL);
    bracket = find_descendant_kind(&tree, body, NOC_SYNTAX_BRACKET_GROUP);
    CHECK(bracket != NOC_SYNTAX_NONE);
    CHECK(slice_equals(noc_syntax_source(&tree, bracket), "[2]"));
    bracket_syntax = noc_syntax_node(&tree, bracket);
    inner = noc_syntax_inner_range(&tree, bracket);
    bracket_value = noc_syntax_node_at_token(&tree, inner.begin);
    CHECK(bracket_syntax != NULL);
    CHECK(bracket_value != NOC_SYNTAX_NONE);
    CHECK(noc_syntax_token(&tree, bracket_value) != NULL);
    CHECK(noc_syntax_parent(&tree, bracket_value) == bracket);
    CHECK(noc_syntax_node_at_token(&tree, bracket_syntax->range.begin) == bracket);
    CHECK(noc_syntax_node_at_token(&tree, bracket_syntax->range.end - 1) == bracket);
    CHECK(noc_syntax_node_covering_range(&tree, inner) == bracket_value);
    CHECK(noc_syntax_node_covering_range(&tree, bracket_syntax->range) == bracket);
    CHECK(noc_syntax_node_covering_range(&tree,
                                         noc_syntax_node(&tree, body)->range) == body);
    spanning.begin = noc_syntax_node(&tree, parameters)->range.begin;
    spanning.end = noc_syntax_node(&tree, body)->range.end;
    CHECK(noc_syntax_node_covering_range(&tree, spanning) == root);
    CHECK(noc_syntax_depth(&tree, root) == 0);
    CHECK(noc_syntax_depth(&tree, body) == 1);
    CHECK(noc_syntax_depth(&tree, bracket) == 2);
    CHECK(noc_syntax_depth(&tree, bracket_value) == 3);
    CHECK(noc_syntax_common_ancestor(&tree, bracket, bracket_value) == bracket);
    CHECK(noc_syntax_common_ancestor(&tree, parameters, body) == root);
    CHECK(noc_syntax_common_ancestor(&tree, bracket_value, bracket_value) == bracket_value);
    CHECK(noc_syntax_node_at_token(&tree, stream.count - 1) == NOC_SYNTAX_NONE);
    CHECK(noc_syntax_node_covering_range(&tree,
                                         (Noc_Token_Range){inner.begin,
                                                           inner.begin}) == NOC_SYNTAX_NONE);
    CHECK(noc_syntax_depth(&tree, tree.count) == NOC_SYNTAX_NONE);
    CHECK(noc_syntax_common_ancestor(&tree, root, tree.count) == NOC_SYNTAX_NONE);

    node = root;
    while (node != NOC_SYNTAX_NONE && visited <= tree.count) {
        visited += 1;
        node = noc_syntax_next_preorder(&tree, node);
    }
    CHECK(visited == tree.count);
    CHECK(node == NOC_SYNTAX_NONE);
    CHECK(noc_syntax_node(&tree, tree.count) == NULL);
    CHECK(noc_syntax_first_child(&tree, tree.count) == NOC_SYNTAX_NONE);
    CHECK(noc_syntax_inner_range(&tree, first).begin == NOC_TOKEN_INDEX_NONE);

    noc_syntax_tree_free(&tree);
    CHECK(!noc_syntax_tree_is_valid(&tree));
    CHECK(noc_syntax_node_at_token(&tree, 0) == NOC_SYNTAX_NONE);
    CHECK(noc_syntax_node_covering_range(&tree,
                                         (Noc_Token_Range){0, 1}) == NOC_SYNTAX_NONE);
    CHECK(noc_syntax_depth(&tree, 0) == NOC_SYNTAX_NONE);
    CHECK(noc_syntax_common_ancestor(&tree, 0, 0) == NOC_SYNTAX_NONE);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

static void validate_syntax_tree_ownership(const Noc_Syntax_Tree *tree)
{
    size_t *ownership = (size_t *)calloc(tree->stream->count, sizeof(*ownership));
    size_t i;
    size_t node;
    size_t visited = 0;
    CHECK(ownership != NULL);
    if (!ownership) return;

    for (i = 0; i < tree->count; ++i) {
        const Noc_Syntax_Node *syntax = &tree->items[i];
        size_t child;
        size_t last = NOC_SYNTAX_NONE;
        size_t children = 0;
        CHECK(noc_token_range_is_valid(tree->stream, syntax->range));
        CHECK(syntax->parent == NOC_SYNTAX_NONE || syntax->parent < tree->count);
        CHECK(syntax->first_child == NOC_SYNTAX_NONE || syntax->first_child < tree->count);
        CHECK(syntax->last_child == NOC_SYNTAX_NONE || syntax->last_child < tree->count);
        CHECK(syntax->next_sibling == NOC_SYNTAX_NONE || syntax->next_sibling < tree->count);
        if (syntax->kind == NOC_SYNTAX_TOKEN) {
            CHECK(syntax->range.end == syntax->range.begin + 1);
            ownership[syntax->range.begin] += 1;
        } else if (syntax->kind != NOC_SYNTAX_ROOT) {
            CHECK(syntax->range.end >= syntax->range.begin + 2);
            ownership[syntax->range.begin] += 1;
            ownership[syntax->range.end - 1] += 1;
        }
        child = syntax->first_child;
        while (child != NOC_SYNTAX_NONE && children <= tree->count) {
            CHECK(tree->items[child].parent == i);
            last = child;
            child = tree->items[child].next_sibling;
            children += 1;
        }
        CHECK(child == NOC_SYNTAX_NONE);
        CHECK(last == syntax->last_child);
        CHECK((syntax->first_child == NOC_SYNTAX_NONE) ==
              (syntax->last_child == NOC_SYNTAX_NONE));
    }
    for (i = 0; i + 1 < tree->stream->count; ++i) CHECK(ownership[i] == 1);
    CHECK(ownership[tree->stream->count - 1] == 0);

    node = noc_syntax_root(tree);
    while (node != NOC_SYNTAX_NONE && visited <= tree->count) {
        visited += 1;
        node = noc_syntax_next_preorder(tree, node);
    }
    CHECK(node == NOC_SYNTAX_NONE);
    CHECK(visited == tree->count);
    free(ownership);
}

static void test_large_syntax_tree(void)
{
    Noc_Context context;
    Noc_Buffer source = {0};
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    size_t i;
    for (i = 0; i < 20; ++i) CHECK(noc_buffer_append_cstr(&source, "("));
    for (i = 0; i < 300; ++i) CHECK(noc_buffer_append_cstr(&source, "value "));
    for (i = 0; i < 20; ++i) CHECK(noc_buffer_append_cstr(&source, ")"));
    CHECK(noc_buffer_append_cstr(&source, ";"));
    CHECK(noc_buffer_terminate(&source));

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, "large.c", source.items, source.count, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(tree.count > 256);
    CHECK(slice_equals(noc_syntax_source(&tree, noc_syntax_root(&tree)), source.items));
    validate_syntax_tree_ownership(&tree);

    noc_syntax_tree_free(&tree);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
    noc_buffer_free(&source);
}

static void test_syntax_tree_errors_and_lifetime(void)
{
    static const char good_source[] = "call({value[0]});\n";
    static const char mismatched[] = "([)]";
    static const char unclosed[] = "function({value";
    static const char unexpected[] = "value];";
    Noc_Context context;
    Noc_Token_Stream good = {0};
    Noc_Token_Stream bad = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_Syntax_Node *preserved_nodes;
    Diagnostic_State diagnostics = {0};
    char *preserved_source;
    size_t preserved_generation;

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_tokenize(&context,
                       "good.c",
                       good_source,
                       sizeof(good_source) - 1,
                       &good));
    CHECK(noc_syntax_tree_build(&context, &good, &tree));
    preserved_nodes = tree.items;
    preserved_source = good.source;
    preserved_generation = good.generation;

    CHECK(!noc_tokenize(&context, "bad-comment.c", "/*", 2, &good));
    CHECK(good.source == preserved_source);
    CHECK(good.generation == preserved_generation);
    CHECK(noc_syntax_tree_is_valid(&tree));
    CHECK(slice_equals(noc_syntax_source(&tree, noc_syntax_root(&tree)), good_source));
    CHECK(diagnostics.errors == 1);
    CHECK(strstr(diagnostics.last_message, "unterminated or invalid token") != NULL);
    memset(&diagnostics, 0, sizeof(diagnostics));

    CHECK(noc_tokenize(&context, "mismatch.c", mismatched, sizeof(mismatched) - 1, &bad));
    CHECK(!noc_syntax_tree_build(&context, &bad, &tree));
    CHECK(tree.items == preserved_nodes);
    CHECK(strstr(diagnostics.last_message, "expected closing delimiter") != NULL);
    CHECK(tree.stream == &good);
    CHECK(noc_syntax_tree_is_valid(&tree));
    CHECK(slice_equals(noc_syntax_source(&tree, noc_syntax_root(&tree)), good_source));
    noc_token_stream_free(&bad);

    CHECK(noc_tokenize(&context, "unclosed.c", unclosed, sizeof(unclosed) - 1, &bad));
    CHECK(!noc_syntax_tree_build(&context, &bad, &tree));
    CHECK(tree.items == preserved_nodes);
    CHECK(strstr(diagnostics.last_message, "unclosed") != NULL);
    CHECK(strstr(diagnostics.last_message, "'}'") != NULL);
    CHECK(diagnostics.last_location.line == 1);
    CHECK(diagnostics.last_location.column == 10);
    CHECK(strcmp(diagnostics.last_path, "unclosed.c") == 0);
    noc_token_stream_free(&bad);

    CHECK(noc_tokenize(&context,
                       "unexpected.c",
                       unexpected,
                       sizeof(unexpected) - 1,
                       &bad));
    CHECK(!noc_syntax_tree_build(&context, &bad, &tree));
    CHECK(tree.items == preserved_nodes);
    noc_token_stream_free(&bad);
    CHECK(diagnostics.errors == 3);

    good.generation = SIZE_MAX;
    tree.stream_generation = SIZE_MAX;
    CHECK(noc_syntax_tree_is_valid(&tree));
    CHECK(!noc_tokenize(&context, "exhausted.c", "new", 3, &good));
    CHECK(good.generation == SIZE_MAX);
    CHECK(tree.stream_generation == SIZE_MAX);
    CHECK(noc_syntax_tree_is_valid(&tree));
    CHECK(diagnostics.errors == 4);
    good.generation = 1;
    tree.stream_generation = 1;
    CHECK(noc_tokenize(&context, "empty.c", "", 0, &good));
    CHECK(!noc_syntax_tree_is_valid(&tree));
    CHECK(noc_syntax_root(&tree) == NOC_SYNTAX_NONE);
    noc_syntax_tree_free(&tree);
    CHECK(noc_syntax_tree_build(&context, &good, &tree));
    CHECK(noc_syntax_child_count(&tree, noc_syntax_root(&tree)) == 0);
    CHECK(noc_syntax_source(&tree, noc_syntax_root(&tree)).count == 0);

    noc_syntax_tree_free(&tree);
    noc_token_stream_free(&good);
    noc_context_deinit(&context);
}

static size_t find_syntax_node_for_token(const Noc_Syntax_Tree *tree,
                                         size_t token_index)
{
    size_t node = noc_syntax_root(tree);
    while (node != NOC_SYNTAX_NONE) {
        const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
        if (syntax && syntax->kind == NOC_SYNTAX_TOKEN &&
            syntax->range.begin == token_index) {
            return node;
        }
        node = noc_syntax_next_preorder(tree, node);
    }
    return NOC_SYNTAX_NONE;
}

static void test_syntax_edit_set(void)
{
    static const char source[] =
        "int add(int left, int right) { return left + right; }\n";
    static const char expected[] =
        "long sum(int left, int right) { return left - right; } /* boundary */\n"
        "/* generated */\n";
    static const char replacement_source[] = "int replacement;\n";
    static const char reused_source[] = "long reused;\n";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_C_Translation_Unit unit = {0};
    Noc_Edit_Set edits = {0};
    Noc_Edit_Set empty = {0};
    Noc_Edit_Set adjacent = {0};
    Noc_Edit_Set deletion = {0};
    Noc_Buffer output = {0};
    const Noc_C_External_Item *function;
    Noc_Token_Range name_range;
    Noc_Token_Range eof_insertion;
    size_t type_node;
    size_t old_count;
    char replacement_type[] = "long";
    char *preserved_output;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, "edits.c", source, sizeof(source) - 1, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    function = noc_c_external_item(&unit, 0);
    CHECK(function != NULL && function->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    name_range.begin = function->name_token;
    name_range.end = function->name_token + 1;
    eof_insertion.begin = stream.count - 1;
    eof_insertion.end = stream.count - 1;
    type_node = find_syntax_node_for_token(&tree, 0);

    CHECK(noc_edit_set_add_cstr(&edits, &stream, name_range, "sum"));
    CHECK(noc_edit_set_add(&edits,
                           &stream,
                           eof_insertion,
                           (Noc_Slice){"/* generated */\n",
                                       sizeof("/* generated */\n") - 1}));
    CHECK(noc_edit_set_add_syntax(&edits,
                                  &tree,
                                  type_node,
                                  (Noc_Slice){replacement_type,
                                              sizeof(replacement_type) - 1}));
    replacement_type[0] = 'X';
    CHECK(noc_edit_set_add_cstr(&edits,
                                &stream,
                                function->body,
                                "{ return left - right; }"));
    CHECK(noc_edit_set_add_cstr(&edits,
                                &stream,
                                (Noc_Token_Range){function->body.end,
                                                  function->body.end},
                                " /* boundary */"));
    CHECK(noc_edit_set_is_valid(&edits, &stream));
    CHECK(edits.count == 5);
    CHECK(edits.items[0].range.begin == 0);
    old_count = edits.count;
    CHECK(!noc_edit_set_add_cstr(&edits, &stream, function->signature, "overlap"));
    CHECK(!noc_edit_set_add_cstr(&edits,
                                 &stream,
                                 (Noc_Token_Range){function->body.end,
                                                   function->body.end},
                                 "duplicate boundary"));
    CHECK(!noc_edit_set_add_cstr(&edits,
                                 &stream,
                                 (Noc_Token_Range){function->body.begin + 1,
                                                   function->body.begin + 1},
                                 "interior"));
    CHECK(!noc_edit_set_add_cstr(&edits,
                                 &stream,
                                 (Noc_Token_Range){stream.count - 1, stream.count},
                                 "eof"));
    CHECK(!noc_edit_set_add_syntax(&edits,
                                   &tree,
                                   tree.count,
                                   (Noc_Slice){"invalid", 7}));
    CHECK(edits.count == old_count);

    CHECK(noc_buffer_append_cstr(&output, "old output"));
    CHECK(noc_edit_set_apply(&edits, &stream, &output));
    CHECK(slice_equals((Noc_Slice){output.items, output.count}, expected));
    preserved_output = output.items;

    CHECK(noc_tokenize(&context,
                       "replacement.c",
                       replacement_source,
                       sizeof(replacement_source) - 1,
                       &stream));
    CHECK(!noc_edit_set_is_valid(&edits, &stream));
    CHECK(!noc_edit_set_apply(&edits, &stream, &output));
    CHECK(output.items == preserved_output);
    CHECK(slice_equals((Noc_Slice){output.items, output.count}, expected));
    CHECK(!noc_edit_set_add_syntax(&edits,
                                   &tree,
                                   type_node,
                                   (Noc_Slice){"short", 5}));

    noc_buffer_free(&output);
    CHECK(noc_edit_set_add_cstr(&adjacent,
                                &stream,
                                (Noc_Token_Range){1, 2},
                                " "));
    CHECK(noc_edit_set_add_cstr(&adjacent,
                                &stream,
                                (Noc_Token_Range){0, 1},
                                "long"));
    CHECK(noc_edit_set_add_cstr(&adjacent,
                                &stream,
                                (Noc_Token_Range){1, 1},
                                "/* adjacent */"));
    CHECK(!noc_edit_set_add_cstr(&adjacent,
                                 &stream,
                                 (Noc_Token_Range){1, 1},
                                 "duplicate"));
    CHECK(noc_edit_set_is_valid(&adjacent, &stream));
    CHECK(noc_edit_set_apply(&adjacent, &stream, &output));
    CHECK(slice_equals((Noc_Slice){output.items, output.count},
                       "long/* adjacent */ replacement;\n"));
    noc_buffer_free(&output);

    CHECK(noc_edit_set_add(&deletion,
                           &stream,
                           (Noc_Token_Range){2, 3},
                           (Noc_Slice){NULL, 0}));
    CHECK(noc_edit_set_apply(&deletion, &stream, &output));
    CHECK(slice_equals((Noc_Slice){output.items, output.count}, "int ;\n"));
    noc_buffer_free(&output);

    {
        size_t generation = stream.generation;
        noc_token_stream_free(&stream);
        CHECK(stream.generation == generation);
        CHECK(noc_tokenize(&context,
                           "reused.c",
                           reused_source,
                           sizeof(reused_source) - 1,
                           &stream));
        CHECK(stream.generation == generation + 1);
        CHECK(!noc_edit_set_is_valid(&edits, &stream));
    }
    CHECK(noc_edit_set_is_valid(&empty, &stream));
    CHECK(noc_edit_set_apply(&empty, &stream, &output));
    CHECK(slice_equals((Noc_Slice){output.items, output.count}, reused_source));
    noc_edit_set_free(&empty);
    noc_edit_set_free(&deletion);
    noc_edit_set_free(&adjacent);
    noc_edit_set_free(&edits);
    CHECK(edits.items == NULL && edits.count == 0);

    noc_buffer_free(&output);
    noc_c_translation_unit_free(&unit);
    noc_syntax_tree_free(&tree);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

int main(void)
{
    test_lossless_syntax_tree();
    test_large_syntax_tree();
    test_syntax_tree_errors_and_lifetime();
    test_syntax_edit_set();
    return finish_suite("syntax");
}
