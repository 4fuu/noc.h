#include "macro_expansion_test_support.h"

static const Noc_Logical_Source_File *file_for_site(
    const Noc_Logical_Source *source,
    Noc_Logical_Physical_Site site)
{
    return noc_logical_source_file_at(source, site.file_index);
}

static const Noc_Logical_Token *find_logical_token(
    const Noc_Logical_Source *source,
    const char *text,
    Noc_Macro_Expansion_Token_Origin origin,
    Noc_Logical_Token_Macro_Provenance *provenance)
{
    size_t index;
    for (index = 0; index < noc_logical_source_token_count(source); ++index) {
        Noc_Logical_Token_Macro_Provenance candidate;
        const Noc_Logical_Token *token =
            noc_logical_source_token_at(source, index);
        if (!token || !slice_equals(noc_logical_source_token_text(source, index),
                                    text) ||
            !noc_logical_source_token_macro_provenance(source,
                                                        index,
                                                        &candidate) ||
            candidate.macro_origin != origin) {
            continue;
        }
        if (provenance) *provenance = candidate;
        return token;
    }
    return NULL;
}

static void check_significant_relex_matches(const Noc_Logical_Source *source)
{
    Noc_Context context;
    Noc_Token_Stream reparsed = {0};
    Noc_Slice text = noc_logical_source_text(source);
    size_t logical_index = 0;
    size_t reparsed_index = 0;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context,
                       "logical-output.c",
                       text.data,
                       text.count,
                       &reparsed));
    while (logical_index < noc_logical_source_token_count(source) &&
           reparsed_index < reparsed.count) {
        const Noc_Logical_Token *logical =
            noc_logical_source_token_at(source, logical_index);
        Noc_Token physical = reparsed.items[reparsed_index];
        if (logical && (logical->flags &
                        NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) != 0) {
            logical_index += 1;
            continue;
        }
        if (logical && (logical->kind == NOC_TOKEN_EOF ||
                        logical->kind == NOC_TOKEN_WHITESPACE ||
                        logical->kind == NOC_TOKEN_NEWLINE ||
                        logical->kind == NOC_TOKEN_LINE_COMMENT ||
                        logical->kind == NOC_TOKEN_BLOCK_COMMENT)) {
            logical_index += 1;
            continue;
        }
        if (physical.kind == NOC_TOKEN_EOF || noc_token_is_trivia(physical)) {
            reparsed_index += 1;
            continue;
        }
        CHECK(logical != NULL);
        if (!logical) break;
        CHECK(logical->kind == physical.kind);
        CHECK(noc_slice_equal(noc_logical_source_token_text(source,
                                                            logical_index),
                              physical.text));
        logical_index += 1;
        reparsed_index += 1;
    }
    while (logical_index < noc_logical_source_token_count(source)) {
        const Noc_Logical_Token *logical =
            noc_logical_source_token_at(source, logical_index++);
        CHECK(logical &&
              ((logical->flags & NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) != 0 ||
               logical->kind == NOC_TOKEN_EOF ||
               logical->kind == NOC_TOKEN_WHITESPACE ||
               logical->kind == NOC_TOKEN_NEWLINE ||
               logical->kind == NOC_TOKEN_LINE_COMMENT ||
               logical->kind == NOC_TOKEN_BLOCK_COMMENT));
    }
    while (reparsed_index < reparsed.count) {
        Noc_Token token = reparsed.items[reparsed_index++];
        CHECK(token.kind == NOC_TOKEN_EOF || noc_token_is_trivia(token));
    }
    noc_token_stream_free(&reparsed);
    noc_context_deinit(&context);
}

static void test_canonical_boundaries_trivia_and_splices(void)
{
    static const char definitions[] =
        "#define OPS(a,b) a+b\n"
        "#define SLASH(x) /x\n"
        "#define DOT(x) .x\n";
    static const char input[] =
        "OPS(alpha,beta)\n"
        "SLASH(*)\n"
        "DOT(.)\n"
        "ab\\\ncd\n";
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    size_t separator_count = 0;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_is_valid(&source));
    CHECK(slice_equals(noc_logical_source_text(&source),
                       "alpha + beta\n/ *\n. .\nabcd\n"));
    for (index = 0; index < noc_logical_source_token_count(&source); ++index) {
        const Noc_Logical_Token *token =
            noc_logical_source_token_at(&source, index);
        Noc_Logical_Token_Macro_Provenance preserved;
        CHECK(token != NULL);
        if (!token ||
            (token->flags & NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) == 0) {
            continue;
        }
        separator_count += 1;
        memset(&preserved, 0xa5, sizeof(preserved));
        CHECK(!noc_logical_source_token_macro_provenance(&source,
                                                         index,
                                                         &preserved));
        CHECK(((const unsigned char *)&preserved)[0] == 0xa5);
        CHECK(slice_equals(noc_logical_source_token_text(&source, index), " "));
    }
    CHECK(separator_count == 4);
    check_significant_relex_matches(&source);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);

    macro_fixture_init(&fixture, "", "x/**/y\nx//z\nq");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(slice_equals(noc_logical_source_text(&source), "x/**/y\nx//z\nq"));
    for (index = 0; index < noc_logical_source_token_count(&source); ++index) {
        const Noc_Logical_Token *token =
            noc_logical_source_token_at(&source, index);
        CHECK(token != NULL);
        CHECK(token &&
              (token->flags & NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) == 0);
    }
    check_significant_relex_matches(&source);
    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
}

static void test_owned_nested_and_generated_provenance(void)
{
    static const char definitions[] =
        "#define INNER(x) x+x\n"
        "#define OUTER(x) INNER(x)\n"
        "#define CAT(a,b) a##b\n"
        "#define STR(x) #x\n";
    static const char input[] =
        "OUTER(v) CAT(foo,bar) STR(a b) __LINE__";
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Token_Macro_Provenance provenance;
    const Noc_Logical_Macro_Frame *outer;
    const Noc_Logical_Macro_Frame *inner;
    const Noc_Logical_Source_File *file;
    size_t generation;
    size_t token_count;
    size_t frame_count;
    Noc_Slice text;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(noc_logical_source_is_valid(&source));
    CHECK(slice_equals(noc_logical_source_text(&source),
                       "v + v foobar \"a b\" 1"));
    CHECK(noc_logical_source_file_count(&source) == 2);
    CHECK(noc_logical_source_macro_frame_count(&source) >= 2);
    outer = noc_logical_source_macro_frame_at(&source, 0);
    inner = noc_logical_source_macro_frame_at(&source, 1);
    CHECK(outer != NULL && inner != NULL);
    CHECK(outer && outer->parent_macro_frame_index == NOC_TOKEN_INDEX_NONE);
    CHECK(inner && inner->parent_macro_frame_index == 0);
    file = outer ? file_for_site(&source, outer->definition) : NULL;
    CHECK(file != NULL && slice_equals(file->path, "macro-definitions.h"));
    file = outer ? file_for_site(&source, outer->invocation) : NULL;
    CHECK(file != NULL && slice_equals(file->path, "macro-input.c"));
    file = inner ? file_for_site(&source, inner->definition) : NULL;
    CHECK(file != NULL && slice_equals(file->path, "macro-definitions.h"));
    file = inner ? file_for_site(&source, inner->invocation) : NULL;
    CHECK(file != NULL && slice_equals(file->path, "macro-definitions.h"));
    CHECK(find_logical_token(&source,
                             "+",
                             NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT,
                             &provenance) != NULL);
    CHECK(provenance.macro_frame_index == 1);
    CHECK(find_logical_token(&source,
                             "v",
                             NOC_MACRO_EXPANSION_TOKEN_ARGUMENT,
                             &provenance) != NULL);
    CHECK(provenance.macro_frame_index == 1);
    CHECK(find_logical_token(&source,
                             "foobar",
                             NOC_MACRO_EXPANSION_TOKEN_PASTE,
                             &provenance) != NULL);
    file = file_for_site(&source, provenance.anchor);
    CHECK(file != NULL && slice_equals(file->path, "macro-definitions.h"));
    CHECK(provenance.anchor.bytes.end - provenance.anchor.bytes.begin == 2);
    CHECK(memcmp(definitions + provenance.anchor.bytes.begin, "##", 2) == 0);
    CHECK(find_logical_token(&source,
                             "\"a b\"",
                             NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION,
                             &provenance) != NULL);
    file = file_for_site(&source, provenance.anchor);
    CHECK(file != NULL && slice_equals(file->path, "macro-definitions.h"));
    CHECK(provenance.anchor.bytes.end - provenance.anchor.bytes.begin == 1);
    CHECK(definitions[provenance.anchor.bytes.begin] == '#');
    CHECK(find_logical_token(&source,
                             "1",
                             NOC_MACRO_EXPANSION_TOKEN_BUILTIN,
                             &provenance) != NULL);
    CHECK(provenance.builtin_kind == NOC_MACRO_BUILTIN_LINE);

    generation = noc_logical_source_generation(&source);
    token_count = noc_logical_source_token_count(&source);
    frame_count = noc_logical_source_macro_frame_count(&source);
    text = noc_logical_source_text(&source);
    macro_fixture_deinit(&fixture);

    CHECK(noc_logical_source_is_valid(&source));
    CHECK(noc_logical_source_generation(&source) == generation);
    CHECK(noc_logical_source_token_count(&source) == token_count);
    CHECK(noc_logical_source_macro_frame_count(&source) == frame_count);
    CHECK(noc_logical_source_text(&source).data == text.data);
    CHECK(slice_equals(noc_logical_source_text(&source),
                       "v + v foobar \"a b\" 1"));
    file = noc_logical_source_file_at(&source, 0);
    CHECK(file != NULL && file->path.data[file->path.count] == '\0');
    CHECK(find_logical_token(&source,
                             "+",
                             NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT,
                             &provenance) != NULL);
    CHECK(file_for_site(&source, provenance.anchor) != NULL);

    noc_logical_source_free(&source);
}

int main(void)
{
    test_canonical_boundaries_trivia_and_splices();
    test_owned_nested_and_generated_provenance();
    return finish_suite("logical-source");
}
