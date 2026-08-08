#define NOC_IMPLEMENTATION
#include "noc.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_CHECK(condition) do { if (!(condition)) abort(); } while (0)

typedef struct {
    unsigned int selector;
} Fuzz_State;

static void ignore_diagnostic(void *user_data, const Noc_Diagnostic *diagnostic)
{
    (void)user_data;
    (void)diagnostic;
}

static bool expand_fuzz(Noc_Rewriter *rewriter,
                        const Noc_Rule *rule,
                        void *user_data)
{
    Fuzz_State *state = (Fuzz_State *)user_data;
    const Noc_Token_Stream *stream = noc_rw_token_stream(rewriter);
    Noc_Token_Range remaining = noc_rw_remaining_range(rewriter);
    const Noc_Token *next;
    (void)rule;
    FUZZ_CHECK(stream != NULL);
    FUZZ_CHECK(noc_token_range_is_valid(stream, remaining));
    (void)noc_rw_source_path(rewriter);
    (void)noc_rw_trigger_location(rewriter);
    (void)noc_rw_peek_raw(rewriter, state->selector % 4u);
    next = noc_rw_peek(rewriter, 0);

    switch (state->selector % 6u) {
    case 0: {
        Noc_Token token;
        (void)noc_rw_take_raw(rewriter, &token);
        break;
    }
    case 1:
        (void)noc_rw_match_punct(rewriter, "(");
        (void)noc_rw_match_identifier(rewriter, "value");
        break;
    case 2:
        if (next && noc_token_is_punct(*next, "(")) {
            Noc_Slice inside;
            if (!noc_rw_capture_balanced(rewriter, "(", ")", &inside)) return false;
            return noc_rw_emit_slice(rewriter, inside);
        }
        break;
    case 3: {
        const Noc_Syntax_Tree *tree = noc_rw_syntax_tree(rewriter);
        if (tree) {
            size_t cursor = remaining.begin;
            while (cursor < remaining.end &&
                   noc_token_is_trivia(stream->items[cursor])) {
                cursor += 1;
            }
            if (cursor < remaining.end) {
                size_t node = noc_syntax_node_at_token(tree, cursor);
                const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
                if (syntax && syntax->range.begin == cursor) {
                    size_t taken = NOC_SYNTAX_NONE;
                    FUZZ_CHECK(noc_rw_take_syntax(rewriter, syntax->kind, &taken));
                    FUZZ_CHECK(taken == node);
                }
            }
        }
        break;
    }
    case 4:
        FUZZ_CHECK(noc_rw_consume_range(
            rewriter,
            (Noc_Token_Range){remaining.begin, remaining.begin}));
        break;
    case 5:
        if (next && next->kind == NOC_TOKEN_IDENTIFIER) {
            Noc_Buffer logical = {0};
            Noc_Token token;
            FUZZ_CHECK(noc_token_logical_text(*next, &logical));
            FUZZ_CHECK(noc_rw_expect_identifier(rewriter, logical.items, &token));
            FUZZ_CHECK(noc_token_is_identifier(token, logical.items));
            noc_buffer_free(&logical);
        }
        break;
    }
    return noc_rw_emit_cstr(rewriter, "0");
}

static void fuzz_standalone_lexer(const uint8_t *data, size_t size)
{
    const char *source = (const char *)data;
    Noc_Lexer lexer;
    size_t offset = 0;
    size_t steps = 0;
    noc_lexer_init(&lexer, "fuzz.c", source, size);
    for (;;) {
        Noc_Token token = noc_lexer_next(&lexer);
        FUZZ_CHECK(token.location.offset == offset);
        FUZZ_CHECK(token.text.data == source + offset);
        FUZZ_CHECK(token.text.count <= size - offset);
        offset += token.text.count;
        steps += 1;
        FUZZ_CHECK(steps <= size + 3);
        if (token.kind == NOC_TOKEN_EOF) {
            FUZZ_CHECK(offset == size);
            break;
        }
    }
}

static void fuzz_token_apis(Noc_Context *context,
                            const uint8_t *data,
                            size_t size,
                            unsigned int selector)
{
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_C_Translation_Unit unit = {0};
    Noc_Preprocessor_Map preprocessor = {0};
    Noc_Argument_List arguments = {0};
    Noc_Token_Cursor cursor;
    size_t i;
    size_t offset = 0;
    if (!noc_tokenize(context, "fuzz.c", (const char *)data, size, &stream)) return;
    FUZZ_CHECK(noc_token_stream_is_valid(&stream));
    FUZZ_CHECK(stream.source_count == size);
    FUZZ_CHECK(size == 0 || memcmp(stream.source, data, size) == 0);
    for (i = 0; i < stream.count; ++i) {
        Noc_Token token = stream.items[i];
        FUZZ_CHECK(token.location.offset == offset);
        FUZZ_CHECK(token.text.data == stream.source + offset);
        FUZZ_CHECK(token.text.count <= size - offset);
        offset += token.text.count;
    }
    FUZZ_CHECK(offset == size);

    noc_token_cursor_init(&cursor, &stream);
    FUZZ_CHECK(noc_token_cursor_rewind(&cursor, selector % stream.count));
    (void)noc_token_cursor_peek_raw(&cursor, selector % 8u);
    (void)noc_token_cursor_peek(&cursor, selector % 8u);
    (void)noc_token_cursor_match_identifier(&cursor, "value", NULL);
    (void)noc_token_cursor_match_punct(&cursor, "(", NULL);
    (void)noc_token_cursor_take_balanced(&cursor, "(", ")", NULL, NULL);
    (void)noc_parse_arguments(&stream,
                              (Noc_Token_Range){0, stream.count - 1},
                              &arguments);

    if (noc_syntax_tree_build(context, &stream, &tree)) {
        Noc_Slice root_source;
        size_t node = noc_syntax_root(&tree);
        size_t visited = 0;
        FUZZ_CHECK(noc_syntax_tree_is_valid(&tree));
        root_source = noc_syntax_source(&tree, node);
        FUZZ_CHECK(root_source.count == size);
        FUZZ_CHECK(size == 0 || memcmp(root_source.data, data, size) == 0);
        while (node != NOC_SYNTAX_NONE) {
            FUZZ_CHECK(noc_syntax_node(&tree, node) != NULL);
            FUZZ_CHECK(noc_syntax_depth(&tree, node) != NOC_SYNTAX_NONE);
            visited += 1;
            FUZZ_CHECK(visited <= tree.count);
            node = noc_syntax_next_preorder(&tree, node);
        }
        FUZZ_CHECK(visited == tree.count);
        for (i = 0; i + 1 < stream.count; ++i) {
            size_t owner = noc_syntax_node_at_token(&tree, i);
            const Noc_Syntax_Node *syntax = noc_syntax_node(&tree, owner);
            FUZZ_CHECK(syntax != NULL);
            FUZZ_CHECK(syntax->range.begin <= i && i < syntax->range.end);
            FUZZ_CHECK(noc_syntax_node_covering_range(
                           &tree,
                           (Noc_Token_Range){i, i + 1}) == owner);
            FUZZ_CHECK(noc_syntax_common_ancestor(&tree, owner, owner) == owner);
        }
        (void)noc_c_translation_unit_build(context, &tree, &unit);
    }
    (void)noc_preprocessor_map_build(context, &stream, &preprocessor);

    noc_preprocessor_map_free(&preprocessor);
    noc_c_translation_unit_free(&unit);
    noc_syntax_tree_free(&tree);
    noc_argument_list_free(&arguments);
    noc_token_stream_free(&stream);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Fuzz_State state;
    Noc_Rule rule;
    fuzz_standalone_lexer(data, size);
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, ignore_diagnostic, NULL);
    state.selector = size > 0 ? data[0] : 0;
    memset(&rule, 0, sizeof(rule));
    rule.name = "fuzz";
    rule.scope = NOC_RULE_TOKEN;
    rule.syntax = "@fuzz ...";
    rule.description = "Exercise rewriter APIs under fuzzing.";
    rule.expand = expand_fuzz;
    rule.user_data = &state;
    FUZZ_CHECK(noc_register_rule(&context, rule));
    fuzz_token_apis(&context, data, size, state.selector);
    context.options.emit_line_directives = false;
    context.options.unknown_rule_is_error = false;
    context.options.skip_inactive_preprocessor_branches = (state.selector & 1u) != 0;
    (void)noc_transform_source(&context,
                               "fuzz.c",
                               (const char *)data,
                               size,
                               &result);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
    return 0;
}

#ifndef NOC_LIBFUZZER
static uint32_t fuzz_random(uint32_t *state)
{
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

int main(void)
{
    static const char *const corpus[] = {
        "",
        "int main(void) { return 0; }\n",
        "@fuzz(value)",
        "@fuzz va\\\nlue",
        "/* comment */ // line\\\ncontinued\n",
        "#if 0\n@missing\n#elif 1\n@fuzz(42)\n#endif\n",
        "#if FLAG\n@fuzz(\n#endif\n42\n#if FLAG\n)\n#endif\n",
        "([{}]) <::> %:%: /\\\n* block *\\\n/",
        "\"string\\n\\x41\" '\\123' 0x1p+2",
    };
    uint8_t bytes[512];
    uint32_t random_state = UINT32_C(0x4e4f4321);
    size_t i;
    for (i = 0; i < sizeof(corpus) / sizeof(corpus[0]); ++i) {
        FUZZ_CHECK(LLVMFuzzerTestOneInput((const uint8_t *)corpus[i],
                                         strlen(corpus[i])) == 0);
    }
    for (i = 0; i < 2000; ++i) {
        size_t count = fuzz_random(&random_state) % (sizeof(bytes) + 1);
        size_t j;
        for (j = 0; j < count; ++j) bytes[j] = (uint8_t)fuzz_random(&random_state);
        FUZZ_CHECK(LLVMFuzzerTestOneInput(bytes, count) == 0);
    }
    puts("noc fuzz smoke passed");
    return 0;
}
#endif
