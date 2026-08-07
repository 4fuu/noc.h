#define NOC_IMPLEMENTATION
#include "../noc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n",                      \
                    __FILE__, __LINE__, #condition);                            \
            failures += 1;                                                      \
        }                                                                       \
    } while (0)

typedef struct {
    size_t errors;
    size_t last_message_count;
    Noc_Location last_location;
    char last_path[64];
    char last_message[160];
} Diagnostic_State;

static void count_diagnostics(void *user_data, const Noc_Diagnostic *diagnostic)
{
    Diagnostic_State *state = (Diagnostic_State *)user_data;
    if (diagnostic->severity == NOC_DIAGNOSTIC_ERROR) {
        state->errors += 1;
        state->last_message_count = strlen(diagnostic->message);
        state->last_location = diagnostic->location;
        (void)snprintf(state->last_message,
                       sizeof(state->last_message),
                       "%s",
                       diagnostic->message);
        if (diagnostic->location.path) {
            (void)snprintf(state->last_path,
                           sizeof(state->last_path),
                           "%s",
                           diagnostic->location.path);
        } else {
            state->last_path[0] = '\0';
        }
    }
}

static bool slice_equals(Noc_Slice slice, const char *expected)
{
    size_t expected_count = strlen(expected);
    return slice.count == expected_count &&
           (expected_count == 0 || memcmp(slice.data, expected, expected_count) == 0);
}

static void test_lexer(void)
{
    static const char source[] =
        "  #define VALUE(x) x \\\n"
        "    + 1\n"
        "@twice(21) /* comment */\n";
    Noc_Lexer lexer;
    Noc_Token token;

    noc_lexer_init(&lexer, "lexer.c", source, sizeof(source) - 1);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_WHITESPACE);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_PREPROCESSOR);
    CHECK(token.location.line == 1);
    token = noc_lexer_next(&lexer);
    CHECK(noc_token_is_punct(token, "@"));
    CHECK(token.location.line == 3);
    token = noc_lexer_next(&lexer);
    CHECK(noc_token_is_identifier(token, "twice"));
}

static void test_token_stream_and_cursor(void)
{
    char source[] = "  call(alpha, nested(1, 2), (Pair){3, 4})  ";
    char path[] = "tokens.c";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Token_Cursor cursor;
    Noc_Token_Cursor argument_cursor;
    Noc_Token_Range whole;
    Noc_Token_Range inside;
    Noc_Token_Range trimmed;
    Noc_Argument_List arguments = {0};
    Noc_Token token;
    Noc_Slice source_view;
    Diagnostic_State diagnostics = {0};
    char *preserved_source;
    size_t mark;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, path, source, strlen(source), &stream));
    CHECK(noc_token_stream_is_valid(&stream));
    source[2] = 'X';
    path[0] = 'X';
    source_view = noc_token_stream_source(&stream);
    CHECK(slice_equals(source_view, "  call(alpha, nested(1, 2), (Pair){3, 4})  "));
    CHECK(strcmp(stream.path, "tokens.c") == 0);
    CHECK(stream.count > 1);
    CHECK(stream.items[stream.count - 1].kind == NOC_TOKEN_EOF);

    noc_token_cursor_init(&cursor, &stream);
    CHECK(noc_token_cursor_peek_raw(&cursor, 0)->kind == NOC_TOKEN_WHITESPACE);
    CHECK(noc_token_is_identifier(*noc_token_cursor_peek(&cursor, 0), "call"));
    mark = noc_token_cursor_mark(&cursor);
    CHECK(noc_token_cursor_match_kind(&cursor, NOC_TOKEN_IDENTIFIER, &token));
    CHECK(noc_token_is_identifier(token, "call"));
    CHECK(!noc_token_cursor_match_punct(&cursor, "[", NULL));
    CHECK(noc_token_cursor_take_balanced(&cursor, "(", ")", &whole, &inside));
    CHECK(slice_equals(noc_token_range_source(&stream, whole),
                       "(alpha, nested(1, 2), (Pair){3, 4})"));
    CHECK(noc_token_range_location(&stream, whole).column == 7);
    CHECK(noc_token_cursor_at_end(&cursor));
    CHECK(noc_token_cursor_rewind(&cursor, mark));
    noc_token_cursor_skip_trivia(&cursor);
    CHECK(noc_token_cursor_mark(&cursor) > mark);
    CHECK(noc_token_cursor_rewind(&cursor, mark));
    CHECK(noc_token_cursor_match_identifier(&cursor, "call", NULL));
    CHECK(noc_token_cursor_match_punct(&cursor, "(", NULL));
    CHECK(noc_token_cursor_take_raw(&cursor, &token));
    CHECK(noc_token_is_identifier(token, "alpha"));
    CHECK(noc_token_cursor_rewind(&cursor, mark));
    CHECK(noc_token_cursor_take(&cursor, &token));
    CHECK(noc_token_is_identifier(token, "call"));

    CHECK(noc_token_range_is_valid(&stream, inside));
    CHECK(!noc_token_range_is_valid(&stream,
                                    (Noc_Token_Range){stream.count, stream.count + 1}));
    trimmed = noc_token_range_trim_trivia(&stream, (Noc_Token_Range){0, stream.count - 1});
    CHECK(slice_equals(noc_token_range_source(&stream, trimmed),
                       "call(alpha, nested(1, 2), (Pair){3, 4})"));
    CHECK(noc_parse_arguments(&stream, inside, &arguments));
    CHECK(arguments.count == 3);
    CHECK(slice_equals(noc_token_range_source(&stream, arguments.items[0]), "alpha"));
    CHECK(slice_equals(noc_token_range_source(&stream, arguments.items[1]), "nested(1, 2)"));
    CHECK(slice_equals(noc_token_range_source(&stream, arguments.items[2]), "(Pair){3, 4}"));

    CHECK(noc_token_cursor_init_range(&argument_cursor, &stream, arguments.items[1]));
    CHECK(noc_token_cursor_match_identifier(&argument_cursor, NULL, &token));
    CHECK(noc_token_is_identifier(token, "nested"));
    CHECK(!noc_token_cursor_rewind(&argument_cursor, arguments.items[1].begin - 1));
    while (noc_token_cursor_take_raw(&argument_cursor, NULL)) {}
    CHECK(noc_token_cursor_at_end(&argument_cursor));
    CHECK(noc_token_cursor_peek_raw(&argument_cursor, 0) == NULL);
    CHECK(!noc_token_cursor_take_raw(&argument_cursor, NULL));
    CHECK(noc_token_cursor_rewind(&argument_cursor, argument_cursor.end));

    noc_argument_list_free(&arguments);
    CHECK(noc_tokenize(&context, "next.c", "next", 4, &stream));
    CHECK(slice_equals(noc_token_stream_source(&stream), "next"));
    preserved_source = stream.source;
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_tokenize(&context, "bad.c", "/*", 2, &stream));
    CHECK(stream.source == preserved_source);
    CHECK(slice_equals(noc_token_stream_source(&stream), "next"));
    CHECK(!noc_tokenize(&context, "huge.c", "", SIZE_MAX, &stream));
    CHECK(stream.source == preserved_source);
    noc_token_stream_free(&stream);
    CHECK(!noc_token_stream_is_valid(&stream));
    CHECK(stream.items == NULL);
    CHECK(stream.source == NULL);
    noc_context_deinit(&context);
}

static void test_argument_and_balance_edges(void)
{
    static const char sparse[] = "( , value, )";
    static const char malformed[] = "([)]";
    static const char mixed[] = "({[value]})";
    static const char unterminated[] = "(value";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Token_Stream zero_stream = {0};
    Noc_Token_Cursor cursor;
    Noc_Token_Range inside;
    Noc_Token_Range invalid;
    Noc_Argument_List arguments = {0};
    Noc_Token_Range *preserved_arguments;
    size_t preserved_count;
    size_t mark;

    noc_context_init(&context);
    CHECK(!noc_token_range_is_valid(&zero_stream, (Noc_Token_Range){0, 0}));
    invalid = noc_token_range_trim_trivia(&zero_stream, (Noc_Token_Range){0, 0});
    CHECK(invalid.begin == SIZE_MAX && invalid.end == SIZE_MAX);
    CHECK(noc_token_stream_source(&zero_stream).data == NULL);
    noc_token_cursor_init(&cursor, &zero_stream);
    CHECK(noc_token_cursor_at_end(&cursor));

    CHECK(noc_tokenize(&context, "sparse.c", sparse, sizeof(sparse) - 1, &stream));
    noc_token_cursor_init(&cursor, &stream);
    CHECK(noc_token_cursor_take_balanced(&cursor, "(", ")", NULL, &inside));
    CHECK(noc_parse_arguments(&stream, inside, &arguments));
    CHECK(arguments.count == 3);
    CHECK(arguments.items[0].begin == arguments.items[0].end);
    CHECK(slice_equals(noc_token_range_source(&stream, arguments.items[1]), "value"));
    CHECK(arguments.items[2].begin == arguments.items[2].end);
    preserved_arguments = arguments.items;
    preserved_count = arguments.count;
    noc_token_stream_free(&stream);

    CHECK(noc_tokenize(&context,
                       "malformed.c",
                       malformed,
                       sizeof(malformed) - 1,
                       &stream));
    CHECK(!noc_parse_arguments(&stream,
                               (Noc_Token_Range){0, stream.count - 1},
                               &arguments));
    CHECK(arguments.items == preserved_arguments);
    CHECK(arguments.count == preserved_count);
    noc_token_cursor_init(&cursor, &stream);
    mark = noc_token_cursor_mark(&cursor);
    CHECK(!noc_token_cursor_take_balanced(&cursor, "(", ")", NULL, NULL));
    CHECK(noc_token_cursor_mark(&cursor) == mark);
    noc_token_stream_free(&stream);

    CHECK(noc_tokenize(&context, "mixed.c", mixed, sizeof(mixed) - 1, &stream));
    noc_token_cursor_init(&cursor, &stream);
    CHECK(noc_token_cursor_take_balanced(&cursor, "(", ")", NULL, &inside));
    CHECK(slice_equals(noc_token_range_source(&stream, inside), "{[value]}"));
    noc_token_stream_free(&stream);

    CHECK(noc_tokenize(&context,
                       "unterminated.c",
                       unterminated,
                       sizeof(unterminated) - 1,
                       &stream));
    noc_token_cursor_init(&cursor, &stream);
    mark = noc_token_cursor_mark(&cursor);
    CHECK(!noc_token_cursor_take_balanced(&cursor, "(", ")", NULL, NULL));
    CHECK(noc_token_cursor_mark(&cursor) == mark);
    CHECK(!noc_token_cursor_take_balanced(&cursor, "|", "|", NULL, NULL));
    noc_token_stream_free(&stream);

    CHECK(noc_tokenize(&context, "empty.c", "", 0, &stream));
    CHECK(noc_parse_arguments(&stream,
                              (Noc_Token_Range){0, stream.count},
                              &arguments));
    CHECK(arguments.count == 0);
    noc_token_stream_free(&stream);

    CHECK(noc_tokenize(&context, "comma.c", ",", 1, &stream));
    CHECK(noc_parse_arguments(&stream,
                              (Noc_Token_Range){0, stream.count},
                              &arguments));
    CHECK(arguments.count == 2);
    CHECK(arguments.items[0].begin == arguments.items[0].end);
    CHECK(arguments.items[1].begin == arguments.items[1].end);
    CHECK(arguments.items[1].end < stream.count);
    noc_argument_list_free(&arguments);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

static void test_tokenize_error(void)
{
    char source[256];
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Diagnostic_State diagnostics = {0};
    memset(source, 'x', sizeof(source));
    source[0] = '/';
    source[1] = '*';
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_tokenize(&context, "invalid.c", source, sizeof(source), &stream));
    CHECK(diagnostics.errors == 1);
    CHECK(diagnostics.last_message_count < 160);
    CHECK(diagnostics.last_location.line == 1);
    CHECK(strcmp(diagnostics.last_path, "invalid.c") == 0);
    CHECK(stream.items == NULL);
    CHECK(stream.source == NULL);
    noc_context_deinit(&context);
}

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
    size_t node;
    size_t visited = 0;
    Noc_Token_Range inner;
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

static bool external_name_equals(const Noc_C_Translation_Unit *unit,
                                 const Noc_C_External_Item *item,
                                 const char *expected)
{
    if (item->name_token == NOC_TOKEN_INDEX_NONE ||
        item->name_token >= unit->stream->count) {
        return false;
    }
    return noc_token_is_identifier(unit->stream->items[item->name_token], expected);
}

static void test_c_translation_unit_analysis(void)
{
    static const char source[] =
        "#define API extern\n"
        "typedef struct Pair { int x; int y; } Pair;\n"
        "struct Forward;\n"
        "enum Kind { KIND_A, KIND_B };\n"
        "static int global = make_value(1);\n"
        "int prototype(const char *name, int values[4]);\n"
        "int (*callback)(int);\n"
        "int decorated(void) __attribute__((unused));\n"
        "int log_message(const char *format, ...);\n"
        "int add(int left, int right) { return left + right; }\n"
        "int first, second;\n"
        "_Static_assert(1, \"ok\");\n"
        "unfinished";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_Syntax_Tree invalid_tree = {0};
    Noc_C_Translation_Unit unit = {0};
    Noc_C_Parameter_List parameters = {0};
    Noc_C_External_Item *preserved_items;
    Noc_C_Parameter *preserved_parameters;
    Diagnostic_State diagnostics = {0};
    const Noc_C_External_Item *item;

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_tokenize(&context, "analysis.c", source, sizeof(source) - 1, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(unit.count == 12);
    preserved_items = unit.items;

    item = noc_c_external_item(&unit, 0);
    CHECK(item != NULL);
    CHECK(item->kind == NOC_C_EXTERNAL_DECLARATION);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TYPEDEF);
    CHECK(external_name_equals(&unit, item, "Pair"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->range),
                       "typedef struct Pair { int x; int y; } Pair;"));

    item = noc_c_external_item(&unit, 1);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "Forward"));
    item = noc_c_external_item(&unit, 2);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "Kind"));

    item = noc_c_external_item(&unit, 3);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "global"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->signature),
                       "static int global = make_value(1)"));

    item = noc_c_external_item(&unit, 4);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "prototype"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->parameters),
                       "(const char *name, int values[4])"));
    CHECK(item->body.begin == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_c_parse_parameters(unit.stream, item->parameters, &parameters));
    CHECK(parameters.count == 2);
    CHECK(slice_equals(noc_token_range_source(unit.stream, parameters.items[0].range),
                       "const char *name"));
    CHECK(external_name_equals(&unit,
                               &(Noc_C_External_Item){.name_token =
                                                          parameters.items[0].name_token},
                               "name"));
    CHECK(external_name_equals(&unit,
                               &(Noc_C_External_Item){.name_token =
                                                          parameters.items[1].name_token},
                               "values"));
    CHECK(!parameters.items[0].is_variadic);
    preserved_parameters = parameters.items;
    CHECK(!noc_c_parse_parameters(unit.stream, item->body, &parameters));
    CHECK(parameters.items == preserved_parameters);

    item = noc_c_external_item(&unit, 5);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "callback"));

    item = noc_c_external_item(&unit, 6);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "decorated"));

    item = noc_c_external_item(&unit, 7);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "log_message"));
    CHECK(noc_c_parse_parameters(unit.stream, item->parameters, &parameters));
    CHECK(parameters.count == 2);
    CHECK(external_name_equals(&unit,
                               &(Noc_C_External_Item){.name_token =
                                                          parameters.items[0].name_token},
                               "format"));
    CHECK(parameters.items[1].is_variadic);
    CHECK(parameters.items[1].name_token == NOC_TOKEN_INDEX_NONE);

    item = noc_c_external_item(&unit, 8);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "add"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->signature),
                       "int add(int left, int right)"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->body),
                       "{ return left + right; }"));
    CHECK(noc_token_range_location(unit.stream, item->range).line == 10);
    CHECK(noc_c_compound_statement_is_valid(unit.stream, item->body));
    CHECK(slice_equals(noc_token_range_source(
                           unit.stream,
                           noc_c_compound_statement_inner(unit.stream, item->body)),
                       " return left + right; "));
    CHECK(!noc_c_compound_statement_is_valid(unit.stream, item->signature));
    CHECK(noc_c_compound_statement_inner(unit.stream, item->signature).begin ==
          NOC_TOKEN_INDEX_NONE);

    item = noc_c_external_item(&unit, 9);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_UNKNOWN);
    CHECK(item->name_token == NOC_TOKEN_INDEX_NONE);
    item = noc_c_external_item(&unit, 10);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_UNKNOWN);
    CHECK(item->name_token == NOC_TOKEN_INDEX_NONE);
    item = noc_c_external_item(&unit, 11);
    CHECK(item->kind == NOC_C_EXTERNAL_UNKNOWN);
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->range), "unfinished"));

    CHECK(strcmp(noc_c_external_kind_name(NOC_C_EXTERNAL_FUNCTION_DEFINITION),
                 "function definition") == 0);
    CHECK(strcmp(noc_c_external_kind_name((Noc_C_External_Kind)99),
                 "unknown external item") == 0);
    CHECK(strcmp(noc_c_declaration_kind_name(NOC_C_DECLARATION_TYPEDEF),
                 "typedef declaration") == 0);
    CHECK(strcmp(noc_c_declaration_kind_name((Noc_C_Declaration_Kind)99),
                 "unknown declaration") == 0);
    CHECK(noc_c_external_item(&unit, unit.count) == NULL);

    noc_syntax_tree_free(&tree);
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(!noc_c_translation_unit_build(&context, &invalid_tree, &unit));
    CHECK(unit.items == preserved_items);
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(diagnostics.errors == 1);

    CHECK(!noc_tokenize(&context, "bad-replacement.c", "/*", 2, &stream));
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(unit.items == preserved_items);
    CHECK(diagnostics.errors == 2);
    CHECK(noc_tokenize(&context, "replacement.c", "int replacement;", 16, &stream));
    CHECK(!noc_c_translation_unit_is_valid(&unit));
    CHECK(noc_c_external_item(&unit, 0) == NULL);
    noc_c_parameter_list_free(&parameters);
    CHECK(parameters.items == NULL && parameters.count == 0);
    noc_c_translation_unit_free(&unit);
    CHECK(!noc_c_translation_unit_is_valid(&unit));
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

static void test_c_analysis_rebuild_and_preprocessor(void)
{
    static const char conditional[] =
        "int\n"
        "#if FLAG\n"
        "const\n"
        "#endif\n"
        "conditional;";
    static const char directives[] = "#define M(x) { x; }\n\n";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_C_Translation_Unit unit = {0};
    const Noc_C_External_Item *item;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, "old.c", "int old;", 8, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(unit.count == 1);

    CHECK(noc_tokenize(&context,
                       "conditional.c",
                       conditional,
                       sizeof(conditional) - 1,
                       &stream));
    CHECK(!noc_c_translation_unit_is_valid(&unit));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(unit.count == 1);
    item = noc_c_external_item(&unit, 0);
    CHECK(external_name_equals(&unit, item, "conditional"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->range), conditional));

    CHECK(noc_tokenize(&context,
                       "directives.c",
                       directives,
                       sizeof(directives) - 1,
                       &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(unit.count == 0);
    CHECK(unit.items == NULL);

    noc_c_translation_unit_free(&unit);
    noc_syntax_tree_free(&tree);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

static void test_c_declarator_boundaries(void)
{
    static const char source[] =
        "int __attribute__((noinline)) prefixed(void) { return 0; }\n"
        "int after_prefixed;\n"
        "struct Result { int x; } make_result(void) { return (struct Result){0}; }\n"
        "int after_result;\n"
        "int (*factory(void))(int);\n"
        "int (*callback)(int);\n"
        "int (*factory_definition(void))(int) { return callback; }\n"
        "int after_factory;\n"
        "struct { int x; } point;\n"
        "enum { OFF, ON } state;\n"
        "struct __attribute__((packed)) Packed { int x; };\n"
        "int f(void), g(void);\n"
        "int values[] = {1, 2};\n"
        "struct Result value = { .x = 1 };\n"
        "int trailing(void) __attribute__((noinline)) { return 0; }\n"
        "int (parenthesized);\n"
        "int (* const qualified_callback)(int);\n"
        "int old_style(a)\n"
        "int a;\n"
        "{ return a; }\n"
        "int after_old_style;\n"
        "int value_before, callback_after(void);\n"
        "static struct { int x; } static_point;\n"
        "static struct StaticResult { int x; } static_make(void) "
        "{ return (struct StaticResult){0}; }\n"
        "int after_static;\n"
        "[[deprecated]] struct Tagged { int x; };\n"
        "union { int integer; } union_value;\n"
        "static enum State { STATE_OFF, STATE_ON } static_state;\n"
        "struct [[deprecated]] TagPositioned;\n";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_C_Translation_Unit unit = {0};
    const Noc_C_External_Item *item;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, "declarators.c", source, sizeof(source) - 1, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(unit.count == 28);

    item = noc_c_external_item(&unit, 0);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "prefixed"));
    item = noc_c_external_item(&unit, 1);
    CHECK(external_name_equals(&unit, item, "after_prefixed"));

    item = noc_c_external_item(&unit, 2);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "make_result"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->body),
                       "{ return (struct Result){0}; }"));
    item = noc_c_external_item(&unit, 3);
    CHECK(external_name_equals(&unit, item, "after_result"));

    item = noc_c_external_item(&unit, 4);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "factory"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->parameters), "(void)"));
    item = noc_c_external_item(&unit, 5);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "callback"));
    item = noc_c_external_item(&unit, 6);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "factory_definition"));
    item = noc_c_external_item(&unit, 7);
    CHECK(external_name_equals(&unit, item, "after_factory"));

    item = noc_c_external_item(&unit, 8);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "point"));
    item = noc_c_external_item(&unit, 9);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "state"));
    item = noc_c_external_item(&unit, 10);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "Packed"));
    item = noc_c_external_item(&unit, 11);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_UNKNOWN);
    CHECK(item->name_token == NOC_TOKEN_INDEX_NONE);
    item = noc_c_external_item(&unit, 12);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "values"));
    item = noc_c_external_item(&unit, 13);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "value"));
    item = noc_c_external_item(&unit, 14);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "trailing"));
    item = noc_c_external_item(&unit, 15);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "parenthesized"));
    item = noc_c_external_item(&unit, 16);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "qualified_callback"));
    item = noc_c_external_item(&unit, 18);
    CHECK(item->kind == NOC_C_EXTERNAL_UNKNOWN);
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->range), "{ return a; }"));
    item = noc_c_external_item(&unit, 19);
    CHECK(item->kind == NOC_C_EXTERNAL_DECLARATION);
    CHECK(external_name_equals(&unit, item, "after_old_style"));
    item = noc_c_external_item(&unit, 20);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_UNKNOWN);
    CHECK(item->name_token == NOC_TOKEN_INDEX_NONE);
    item = noc_c_external_item(&unit, 21);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "static_point"));
    item = noc_c_external_item(&unit, 22);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "static_make"));
    item = noc_c_external_item(&unit, 23);
    CHECK(external_name_equals(&unit, item, "after_static"));
    item = noc_c_external_item(&unit, 24);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "Tagged"));
    item = noc_c_external_item(&unit, 25);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "union_value"));
    item = noc_c_external_item(&unit, 26);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "static_state"));
    item = noc_c_external_item(&unit, 27);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "TagPositioned"));

    noc_c_translation_unit_free(&unit);
    noc_syntax_tree_free(&tree);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

static void test_c_parameter_name_boundaries(void)
{
    static const char source[] =
        "void parameter_cases(int [N], int (*)(Event *), typeof(expression), "
        "struct Local { int *member; } value, int values[N], "
        "int (*callback)(Event *), int (* const qualified)(void), size_t count, "
        "struct __attribute__((packed)) Packed, "
        "struct __attribute__((packed)) Packed packed_value, "
        "int array_values[sizeof(*p)], int [sizeof(*p)], "
        "typeof((*expression)) typed_value, "
        "int (* __attribute__((unused)) attributed_callback)(void), "
        "struct [[deprecated]] Tagged);";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_C_Translation_Unit unit = {0};
    Noc_C_Parameter_List parameters = {0};
    const Noc_C_External_Item *function;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, "parameters.c", source, sizeof(source) - 1, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(unit.count == 1);
    function = noc_c_external_item(&unit, 0);
    CHECK(function->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(noc_c_parse_parameters(unit.stream, function->parameters, &parameters));
    CHECK(parameters.count == 15);
    CHECK(parameters.items[0].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(parameters.items[1].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(parameters.items[2].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[3].name_token],
                                  "value"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[4].name_token],
                                  "values"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[5].name_token],
                                  "callback"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[6].name_token],
                                  "qualified"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[7].name_token],
                                  "count"));
    CHECK(parameters.items[8].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[9].name_token],
                                  "packed_value"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[10].name_token],
                                  "array_values"));
    CHECK(parameters.items[11].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[12].name_token],
                                  "typed_value"));
    CHECK(parameters.items[13].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(parameters.items[14].name_token == NOC_TOKEN_INDEX_NONE);

    noc_c_parameter_list_free(&parameters);
    noc_c_translation_unit_free(&unit);
    noc_syntax_tree_free(&tree);
    noc_token_stream_free(&stream);
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

static bool expand_twice(Noc_Rewriter *rewriter,
                         const Noc_Rule *rule,
                         void *user_data)
{
    Noc_Slice expression;
    (void)rule;
    (void)user_data;
    if (!noc_rw_capture_balanced(rewriter, "(", ")", &expression)) return false;
    return noc_rw_emit_cstr(rewriter, "((") &&
           noc_rw_emit_slice(rewriter, expression) &&
           noc_rw_emit_cstr(rewriter, ") + (") &&
           noc_rw_emit_slice(rewriter, expression) &&
           noc_rw_emit_cstr(rewriter, "))");
}

static bool expand_optional(Noc_Rewriter *rewriter,
                            const Noc_Rule *rule,
                            void *user_data)
{
    (void)rule;
    (void)user_data;
    CHECK(!noc_rw_match_punct(rewriter, "("));
    return noc_rw_emit_cstr(rewriter, "long");
}

static bool expand_failure(Noc_Rewriter *rewriter,
                           const Noc_Rule *rule,
                           void *user_data)
{
    (void)rewriter;
    (void)rule;
    (void)user_data;
    return false;
}

static bool expand_question_bytes(Noc_Rewriter *rewriter,
                                  const Noc_Rule *rule,
                                  void *user_data)
{
    static const char bytes[] = {'?', '?', '/'};
    (void)rule;
    (void)user_data;
    return noc_rw_expect_punct(rewriter, "(") &&
           noc_rw_expect_punct(rewriter, ")") &&
           noc_rw_emit_c_string(rewriter, bytes, sizeof(bytes));
}

typedef struct {
    char first[32];
    char second[32];
} Dependency_Paths;

static bool expand_dependencies(Noc_Rewriter *rewriter,
                                const Noc_Rule *rule,
                                void *user_data)
{
    Dependency_Paths *paths = (Dependency_Paths *)user_data;
    (void)rule;
    return noc_rw_expect_punct(rewriter, "(") &&
           noc_rw_expect_punct(rewriter, ")") &&
           noc_rw_add_dependency(rewriter, paths->first) &&
           noc_rw_add_dependency(rewriter, paths->second) &&
           noc_rw_add_dependency(rewriter, paths->first) &&
           noc_rw_emit_cstr(rewriter, "0");
}

static bool expand_dependency_then_error(Noc_Rewriter *rewriter,
                                         const Noc_Rule *rule,
                                         void *user_data)
{
    Noc_Context *context = (Noc_Context *)user_data;
    Noc_Token_Stream invalid = {0};
    (void)rule;
    if (!noc_rw_add_dependency(rewriter, "partial/dependency.h")) return false;
    CHECK(!noc_tokenize(context, "dependency-error.c", "/*", 2, &invalid));
    noc_token_stream_free(&invalid);
    return true;
}

static void test_transform_dependencies(void)
{
    static const char source[] = "int first = @depends(); int second = @depends();\n";
    static const char embed_source[] = "static const char text[] = @embed(\"message.txt\");\n";
    Dependency_Paths paths = {{0}, {0}};
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};
    Noc_Rule rule = {
        "depends",
        NOC_RULE_EXPRESSION,
        "@depends()",
        "Record deterministic test dependencies.",
        expand_dependencies,
        &paths,
    };
    Noc_Rule failing_rule = {
        "dependency_error",
        NOC_RULE_TOKEN,
        "@dependency_error",
        "Record a dependency and then report an error.",
        expand_dependency_then_error,
        &context,
    };
    (void)snprintf(paths.first, sizeof(paths.first), "%s", "include/first.h");
    (void)snprintf(paths.second, sizeof(paths.second), "%s", "assets/second.bin");

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_transform_source(&context,
                               "dependencies.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strcmp(result.output, "int first = 0; int second = 0;\n") == 0);
    CHECK(result.dependency_count == 2);
    CHECK(strcmp(result.dependencies[0], "include/first.h") == 0);
    CHECK(strcmp(result.dependencies[1], "assets/second.bin") == 0);
    paths.first[0] = 'X';
    paths.second[0] = 'X';
    CHECK(strcmp(result.dependencies[0], "include/first.h") == 0);
    CHECK(strcmp(result.dependencies[1], "assets/second.bin") == 0);
    noc_transform_result_free(&result);
    CHECK(result.dependencies == NULL && result.dependency_count == 0);

    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule(&context, failing_rule));
    CHECK(!noc_transform_source(&context,
                                "dependency-error.c",
                                "@dependency_error",
                                sizeof("@dependency_error") - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(result.output == NULL);
    CHECK(result.dependencies == NULL);
    CHECK(result.dependency_count == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_embed_rule(&context, "embed"));
    CHECK(noc_transform_source(&context,
                               "examples/embed/dependency-test.c",
                               embed_source,
                               sizeof(embed_source) - 1,
                               &result));
    CHECK(result.dependency_count == 1);
    CHECK(strcmp(result.dependencies[0], "examples/embed/message.txt") == 0);
    CHECK(strstr(result.output, "Hello from a normal .c file") != NULL);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_custom_rule(void)
{
    static const char source[] = "int answer = @twice(20 + 1);\n";
    static const char expected[] = "int answer = ((20 + 1) + (20 + 1));\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Noc_Rule rule = {
        "twice",
        NOC_RULE_EXPRESSION,
        "@twice(expression)",
        "Evaluate the textual expression twice and add the results.",
        expand_twice,
        NULL,
    };

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_transform_source(&context,
                               "custom.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(result.output != NULL);
    CHECK(strcmp(result.output, expected) == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_transactional_match(void)
{
    static const char source[] = "@optional value;\n";
    static const char expected[] = "long value;\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Noc_Rule rule = {
        "optional",
        NOC_RULE_TOKEN,
        "@optional",
        "Test optional token matching.",
        expand_optional,
        NULL,
    };

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_transform_source(&context,
                               "optional.c",
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strcmp(result.output, expected) == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_unknown_rule(void)
{
    static const char source[] = "int value = @missing(1);\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_transform_source(&context,
                                "unknown.c",
                                source,
                                sizeof(source) - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(result.error_count == 1);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_silent_callback_failure(void)
{
    static const char source[] = "@failure\n";
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};
    Noc_Rule rule = {
        "failure",
        NOC_RULE_TOKEN,
        "@failure",
        "Always fail without a custom diagnostic.",
        expand_failure,
        NULL,
    };

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_register_rule(&context, rule));
    CHECK(!noc_transform_source(&context,
                                "failure.c",
                                source,
                                sizeof(source) - 1,
                                &result));
    CHECK(diagnostics.errors == 1);
    CHECK(result.error_count == 1);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void check_passthrough(const char *source)
{
    Noc_Context context;
    Noc_Transform_Result result;
    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_transform_source(&context, "passthrough.c", source, strlen(source), &result));
    CHECK(result.output != NULL);
    CHECK(strcmp(result.output, source) == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_comment_and_preprocessor_opacity(void)
{
    static const char continued_line_comment[] =
        "// hidden " "\\" "\n"
        "@missing(1)\n"
        "int visible;\n";
    static const char split_comment_opener[] =
        "/" "\\" "\n" "/ @missing(1)\n"
        "int visible;\n";
    static const char directive_block_comment[] =
        "#define VALUE /*\n"
        "@missing(1)\n"
        "*/\n"
        "int visible;\n";
    static const char digraph_directive[] =
        "%:define VALUE @missing(1)\n"
        "int visible;\n";

    check_passthrough(continued_line_comment);
    check_passthrough(split_comment_opener);
    check_passthrough(directive_block_comment);
    check_passthrough(digraph_directive);
}

static void test_trigraph_rejection(void)
{
    static const char source[] = {'?', '?', '/', '\0'};
    Noc_Context context;
    Noc_Transform_Result result;
    Diagnostic_State diagnostics = {0};
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_transform_source(&context, "trigraph.c", source, 3, &result));
    CHECK(diagnostics.errors == 1);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_safe_c_string_and_line_path(void)
{
    static const char source[] = "@bytes()";
    static const char path[] = {'a', '?', '?', '/', 'b', '.', 'c', '\0'};
    Noc_Context context;
    Noc_Transform_Result result;
    Noc_Rule rule = {
        "bytes",
        NOC_RULE_EXPRESSION,
        "@bytes()",
        "Emit bytes that could otherwise form a trigraph.",
        expand_question_bytes,
        NULL,
    };

    noc_context_init(&context);
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_transform_source(&context,
                               path,
                               source,
                               sizeof(source) - 1,
                               &result));
    CHECK(strstr(result.output, "\\?\\?/") != NULL);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static void test_buffer_self_append(void)
{
    char data[200];
    Noc_Buffer buffer = {0};
    memset(data, 'x', sizeof(data));
    CHECK(noc_buffer_append(&buffer, data, sizeof(data)));
    CHECK(noc_buffer_append(&buffer, buffer.items, buffer.count));
    CHECK(buffer.count == sizeof(data) * 2);
    CHECK(memcmp(buffer.items, buffer.items + sizeof(data), sizeof(data)) == 0);
    noc_buffer_free(&buffer);
}

static void test_file_alias_rejection(void)
{
    static const char contents[] = "int untouched;\n";
    const char *path = "build/noc-alias-source.c";
    FILE *file = fopen(path, "wb");
    char actual[sizeof(contents)] = {0};
    Noc_Context context;
    Diagnostic_State diagnostics = {0};
    CHECK(file != NULL);
    if (!file) return;
    CHECK(fwrite(contents, 1, sizeof(contents) - 1, file) == sizeof(contents) - 1);
    CHECK(fclose(file) == 0);

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(!noc_transform_file(&context, path, "./build/noc-alias-source.c"));
    CHECK(diagnostics.errors == 1);
    noc_context_deinit(&context);

    file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file) {
        CHECK(fread(actual, 1, sizeof(contents) - 1, file) == sizeof(contents) - 1);
        CHECK(fclose(file) == 0);
        CHECK(memcmp(actual, contents, sizeof(contents) - 1) == 0);
    }
    CHECK(remove(path) == 0);
}

static void test_string_codec(void)
{
    static const char text[] = "\"line\\nvalue\\x21\"";
    Noc_Token token;
    Noc_Buffer decoded = {0};
    token.kind = NOC_TOKEN_STRING;
    token.text.data = text;
    token.text.count = sizeof(text) - 1;
    token.location = (Noc_Location){0};
    CHECK(noc_decode_string_token(token, &decoded));
    CHECK(decoded.count == 11);
    CHECK(memcmp(decoded.items, "line\nvalue!", 11) == 0);
    noc_buffer_free(&decoded);
}

int main(void)
{
    test_lexer();
    test_token_stream_and_cursor();
    test_argument_and_balance_edges();
    test_tokenize_error();
    test_lossless_syntax_tree();
    test_large_syntax_tree();
    test_syntax_tree_errors_and_lifetime();
    test_c_translation_unit_analysis();
    test_c_analysis_rebuild_and_preprocessor();
    test_c_declarator_boundaries();
    test_c_parameter_name_boundaries();
    test_syntax_edit_set();
    test_transform_dependencies();
    test_custom_rule();
    test_transactional_match();
    test_unknown_rule();
    test_silent_callback_failure();
    test_comment_and_preprocessor_opacity();
    test_trigraph_rejection();
    test_safe_c_string_and_line_path();
    test_buffer_self_append();
    test_file_alias_rejection();
    test_string_codec();
    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    puts("noc tests passed");
    return 0;
}
