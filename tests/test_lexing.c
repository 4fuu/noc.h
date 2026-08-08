#include "test_support.h"

static bool expand_expected_identifier(Noc_Rewriter *rewriter,
                                       const Noc_Rule *rule,
                                       void *user_data)
{
    Noc_Token token;
    (void)rule;
    (void)user_data;
    return noc_rw_expect_identifier(rewriter, "value", &token) &&
           noc_token_is_identifier(token, "value") &&
           noc_rw_emit_cstr(rewriter, "value");
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

static void test_phase2_splices(void)
{
    static const char source[] =
        "fo\\" "\n" "\\" "\r\n" "\\" "\r" "o "
        "%\\" "\r\n" ":\\" "\n" "%\\" "\r" ": "
        ">\\" "\n" "\\" "\r\n" ">\\" "\r" "==";
    static const char directive_source[] =
        "#\\" "\n" "#\n"
        "%\\" "\r\n" ":%\\" "\r" ":\n"
        "%\\" "\n" ":define VALUE 1\n";
    static const char literal_source[] =
        "1\\" "\n" "e2 "
        "0x1p\\" "\r\n" "-2 "
        ".\\" "\r" "5 "
        "u\\" "\n" "8\"text\" "
        "L\\" "\n" "'x'";
    Noc_Lexer lexer;
    Noc_Token token;
    Noc_Buffer logical = {0};
    Noc_Buffer preserved = {0};
    Noc_Token invalid = {0};
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Noc_Rule rule = {
        "expect",
        NOC_RULE_TOKEN,
        "@expect identifier",
        "Expect an identifier with a phase-2 splice.",
        expand_expected_identifier,
        NULL,
    };

    noc_lexer_init(&lexer, "splices.c", source, sizeof(source) - 1);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_IDENTIFIER);
    CHECK(slice_equals(token.text,
                       "fo\\" "\n" "\\" "\r\n" "\\" "\r" "o"));
    CHECK(noc_token_is_identifier(token, "foo"));
    CHECK(!noc_token_is_identifier(token, "fo"));
    CHECK(noc_token_logical_text(token, &logical));
    CHECK(logical.count == 3 && strcmp(logical.items, "foo") == 0);

    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_WHITESPACE);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_PUNCTUATOR);
    CHECK(noc_token_is_punct(token, "%:%:"));
    CHECK(token.location.line == 4);
    CHECK(noc_token_logical_text(token, &logical));
    CHECK(logical.count == 4 && strcmp(logical.items, "%:%:") == 0);

    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_WHITESPACE);
    token = noc_lexer_next(&lexer);
    CHECK(noc_token_is_punct(token, ">>="));
    CHECK(!noc_token_is_punct(token, ">>"));
    CHECK(token.location.line == 7);
    CHECK(noc_token_logical_text(token, &logical));
    CHECK(logical.count == 3 && strcmp(logical.items, ">>=") == 0);
    token = noc_lexer_next(&lexer);
    CHECK(noc_token_is_punct(token, "="));
    CHECK(token.location.line == 10);
    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_EOF);

    noc_lexer_init(&lexer,
                   "directive-splices.c",
                   directive_source,
                   sizeof(directive_source) - 1);
    token = noc_lexer_next(&lexer);
    CHECK(noc_token_is_punct(token, "##"));
    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_NEWLINE);
    token = noc_lexer_next(&lexer);
    CHECK(noc_token_is_punct(token, "%:%:"));
    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_NEWLINE);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_PREPROCESSOR);
    CHECK(token.location.line == 6);
    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_EOF);

    noc_lexer_init(&lexer,
                   "literal-splices.c",
                   literal_source,
                   sizeof(literal_source) - 1);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_NUMBER);
    CHECK(noc_token_logical_text(token, &logical));
    CHECK(strcmp(logical.items, "1e2") == 0);
    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_WHITESPACE);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_NUMBER);
    CHECK(noc_token_logical_text(token, &logical));
    CHECK(strcmp(logical.items, "0x1p-2") == 0);
    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_WHITESPACE);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_NUMBER);
    CHECK(noc_token_logical_text(token, &logical));
    CHECK(strcmp(logical.items, ".5") == 0);
    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_WHITESPACE);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_STRING);
    CHECK(noc_token_logical_text(token, &logical));
    CHECK(strcmp(logical.items, "u8\"text\"") == 0);
    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_WHITESPACE);
    token = noc_lexer_next(&lexer);
    CHECK(token.kind == NOC_TOKEN_CHARACTER);
    CHECK(noc_token_logical_text(token, &logical));
    CHECK(strcmp(logical.items, "L'x'") == 0);
    CHECK(noc_lexer_next(&lexer).kind == NOC_TOKEN_EOF);

    CHECK(noc_buffer_append_cstr(&preserved, "preserved"));
    CHECK(noc_buffer_terminate(&preserved));
    invalid.text.data = NULL;
    invalid.text.count = 1;
    CHECK(!noc_token_logical_text(invalid, &preserved));
    CHECK(preserved.count == 9 && strcmp(preserved.items, "preserved") == 0);
    token.text = noc_slice_from_cstr("plain");
    CHECK(noc_token_logical_text(token, &preserved));
    CHECK(preserved.count == 5 && strcmp(preserved.items, "plain") == 0);
    token.text.data = NULL;
    token.text.count = 0;
    CHECK(noc_token_logical_text(token, &preserved));
    CHECK(preserved.count == 0 && preserved.items[0] == '\0');
    noc_buffer_free(&preserved);
    noc_buffer_free(&logical);

    noc_context_init(&context);
    context.options.emit_line_directives = false;
    CHECK(noc_register_rule(&context, rule));
    CHECK(noc_transform_source(&context,
                               "spliced-operand.c",
                               "@expect va\\\nlue",
                               sizeof("@expect va\\\nlue") - 1,
                               &result));
    CHECK(result.output != NULL && strcmp(result.output, "value") == 0);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
}

static Noc_Preprocessor_Activity activity_for_identifier(
    const Noc_Token_Stream *stream,
    const Noc_Preprocessor_Map *map,
    const char *identifier)
{
    size_t i;
    for (i = 0; i < stream->count; ++i) {
        if (noc_token_is_identifier(stream->items[i], identifier)) {
            return noc_preprocessor_activity_at(map, i);
        }
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

static void test_preprocessor_activity_map(void)
{
    static const char source[] =
        "top\n"
        "#if 0\n"
        "dead_zero\n"
        "#if FLAG\n"
        "nested_dead\n"
        "#endif\n"
        "#elif 1\n"
        "live_elif\n"
        "#else\n"
        "dead_else\n"
        "#endif\n"
        "%:if FLAG\n"
        "maybe_if\n"
        "%:else\n"
        "maybe_else\n"
        "%:endif\n"
        "#if 1\n"
        "live_one\n"
        "#elif 1\n"
        "dead_elif\n"
        "#else\n"
        "dead_final\n"
        "#endif\n"
        "#if 0\n"
        "dead_before_elifdef\n"
        "#elifdef FLAG\n"
        "maybe_elifdef\n"
        "#endif\n"
        "#if 0 /*\r\n"
        "comment */\r\n"
        "dead_commented\r\n"
        "#elif 1 /*\r"
        "comment */\r"
        "live_commented\r"
        "#endif\r"
        "bottom\n";
    static const char malformed[] = "#else\nvalue\n";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Token_Stream bad_stream = {0};
    Noc_Preprocessor_Map map = {0};
    Diagnostic_State diagnostics = {0};
    Noc_Preprocessor_Activity *preserved_items;

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_tokenize(&context, "activity.c", source, sizeof(source) - 1, &stream));
    CHECK(noc_preprocessor_map_build(&context, &stream, &map));
    CHECK(noc_preprocessor_map_is_valid(&map));
    CHECK(map.count == stream.count);
    CHECK(activity_for_identifier(&stream, &map, "top") ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "dead_zero") ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "nested_dead") ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "live_elif") ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "dead_else") ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "maybe_if") ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(activity_for_identifier(&stream, &map, "maybe_else") ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(activity_for_identifier(&stream, &map, "live_one") ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "dead_elif") ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "dead_final") ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "dead_before_elifdef") ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "maybe_elifdef") ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);
    CHECK(activity_for_identifier(&stream, &map, "dead_commented") ==
          NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "live_commented") ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(activity_for_identifier(&stream, &map, "bottom") ==
          NOC_PREPROCESSOR_ACTIVITY_ACTIVE);
    CHECK(noc_preprocessor_activity_at(&map, stream.count) ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);

    preserved_items = map.items;
    CHECK(!noc_tokenize(&context, "failed-replacement.c", "/*", 2, &stream));
    CHECK(diagnostics.errors == 1);
    CHECK(map.items == preserved_items);
    CHECK(noc_preprocessor_map_is_valid(&map));
    CHECK(noc_tokenize(&context,
                       "malformed-activity.c",
                       malformed,
                       sizeof(malformed) - 1,
                       &bad_stream));
    CHECK(!noc_preprocessor_map_build(&context, &bad_stream, &map));
    CHECK(diagnostics.errors == 2);
    CHECK(map.items == preserved_items);
    CHECK(noc_preprocessor_map_is_valid(&map));
    CHECK(noc_tokenize(&context, "replacement.c", "replacement", 11, &stream));
    CHECK(!noc_preprocessor_map_is_valid(&map));
    CHECK(noc_preprocessor_activity_at(&map, 0) ==
          NOC_PREPROCESSOR_ACTIVITY_UNKNOWN);

    noc_preprocessor_map_free(&map);
    CHECK(!noc_preprocessor_map_is_valid(&map));
    noc_token_stream_free(&bad_stream);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
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
    test_phase2_splices();
    test_preprocessor_activity_map();
    test_token_stream_and_cursor();
    test_argument_and_balance_edges();
    test_tokenize_error();
    test_buffer_self_append();
    test_string_codec();
    return finish_suite("lexing");
}
