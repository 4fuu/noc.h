#include "macro_expansion_test_support.h"

static size_t find_token(const Noc_Logical_Source *source, const char *text)
{
    size_t index;
    for (index = 0; index < noc_logical_source_token_count(source); ++index) {
        if (slice_equals(noc_logical_source_token_text(source, index), text)) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static void test_logical_locations_and_owned_line_map(void)
{
    static const char input[] = "a\r\nb\\\r\nc\rd\n";
    static const char expected[] = "a\r\nbc\rd\n";
    static const size_t lines[] = {1, 1, 1, 2, 2, 2, 3, 3, 4};
    static const size_t columns[] = {1, 2, 3, 1, 2, 3, 1, 2, 1};
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Location location;
    Noc_Logical_Location preserved = {91, 92, 93};
    size_t preserved_offset = 123;
    size_t offset;

    macro_fixture_init(&fixture, "", input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(slice_equals(noc_logical_source_text(&source), expected));
    CHECK(noc_logical_source_line_count(&source) == 4);
    for (offset = 0; offset <= sizeof(expected) - 1; ++offset) {
        size_t round_trip = SIZE_MAX;
        CHECK(noc_logical_source_location(&source, offset, &location));
        CHECK(location.offset == offset);
        CHECK(location.line == lines[offset]);
        CHECK(location.byte_column == columns[offset]);
        CHECK(noc_logical_source_offset(&source,
                                        location.line,
                                        location.byte_column,
                                        &round_trip));
        CHECK(round_trip == offset);
    }

    location = preserved;
    CHECK(!noc_logical_source_location(&source,
                                       sizeof(expected),
                                       &location));
    CHECK(location.offset == preserved.offset &&
          location.line == preserved.line &&
          location.byte_column == preserved.byte_column);
    CHECK(!noc_logical_source_location(&source, 0, NULL));
    CHECK(!noc_logical_source_location(NULL, 0, &location));
    CHECK(!noc_logical_source_offset(&source, 0, 1, &preserved_offset));
    CHECK(preserved_offset == 123);
    CHECK(!noc_logical_source_offset(&source, 1, 0, &preserved_offset));
    CHECK(preserved_offset == 123);
    CHECK(!noc_logical_source_offset(&source, 5, 1, &preserved_offset));
    CHECK(preserved_offset == 123);
    CHECK(!noc_logical_source_offset(&source, 1, 4, &preserved_offset));
    CHECK(preserved_offset == 123);
    CHECK(!noc_logical_source_offset(&source, 1, 1, NULL));
    CHECK(!noc_logical_source_offset(NULL, 1, 1, &preserved_offset));

    macro_fixture_deinit(&fixture);
    CHECK(noc_logical_source_is_valid(&source));
    CHECK(noc_logical_source_line_count(&source) == 4);
    CHECK(noc_logical_source_location(&source, 6, &location));
    CHECK(location.line == 3 && location.byte_column == 1);

    noc_logical_source_free(&source);
    CHECK(noc_logical_source_line_count(&source) == 0);
    CHECK(!noc_logical_source_location(&source, 0, &location));
}

static void test_byte_to_token_ranges(void)
{
    Macro_Expansion_Fixture fixture;
    Noc_Logical_Source source = {0};
    Noc_Logical_Byte_Range bytes;
    Noc_Logical_Token_Range range;
    Noc_Logical_Token_Range preserved = {91, 92};
    const Noc_Logical_Token *token;
    size_t token_index;
    size_t token_count;

    macro_fixture_init(&fixture, "", "a\r\nb\\\r\nc\rd\n");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    token_count = noc_logical_source_token_count(&source);
    token_index = find_token(&source, "bc");
    CHECK(token_index != NOC_TOKEN_INDEX_NONE);
    token = noc_logical_source_token_at(&source, token_index);
    CHECK(token != NULL);
    if (token) {
        bytes = token->bytes;
        CHECK(noc_logical_source_token_range_for_bytes(&source,
                                                        bytes,
                                                        &range));
        CHECK(range.begin == token_index && range.end == token_index + 1);

        bytes.begin = token->bytes.begin + 1;
        bytes.end = bytes.begin;
        CHECK(noc_logical_source_token_range_for_bytes(&source,
                                                        bytes,
                                                        &range));
        CHECK(range.begin == token_index && range.end == token_index);

        bytes.begin = token->bytes.end;
        bytes.end = bytes.begin;
        CHECK(noc_logical_source_token_range_for_bytes(&source,
                                                        bytes,
                                                        &range));
        CHECK(range.begin == token_index + 1 && range.end == token_index + 1);
    }
    bytes.begin = 0;
    bytes.end = noc_logical_source_text(&source).count;
    CHECK(noc_logical_source_token_range_for_bytes(&source, bytes, &range));
    CHECK(range.begin == 0 && range.end == token_count);
    bytes.begin = bytes.end;
    CHECK(noc_logical_source_token_range_for_bytes(&source, bytes, &range));
    CHECK(range.begin == token_count && range.end == token_count);

    range = preserved;
    bytes.begin = 2;
    bytes.end = 1;
    CHECK(!noc_logical_source_token_range_for_bytes(&source, bytes, &range));
    CHECK(range.begin == preserved.begin && range.end == preserved.end);
    bytes.begin = 0;
    bytes.end = noc_logical_source_text(&source).count + 1;
    CHECK(!noc_logical_source_token_range_for_bytes(&source, bytes, &range));
    CHECK(range.begin == preserved.begin && range.end == preserved.end);
    CHECK(!noc_logical_source_token_range_for_bytes(&source, (Noc_Logical_Byte_Range){0, 0}, NULL));
    CHECK(!noc_logical_source_token_range_for_bytes(NULL, (Noc_Logical_Byte_Range){0, 0}, &range));

    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);

    macro_fixture_init(&fixture, "", ";\\\n+");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_logical_source_build_macro_expansion(
              &fixture.expansion,
              noc_logical_source_default_options(),
              &source) == NOC_LOGICAL_SOURCE_OK);
    CHECK(slice_equals(noc_logical_source_text(&source), "; +"));
    CHECK(noc_logical_source_token_count(&source) == 4);
    CHECK(noc_logical_source_token_at(&source, 1)->bytes.begin == 1);
    CHECK(noc_logical_source_token_at(&source, 1)->bytes.end == 1);
    CHECK((noc_logical_source_token_at(&source, 2)->flags &
           NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR) != 0);

    CHECK(noc_logical_source_token_range_for_bytes(
        &source, (Noc_Logical_Byte_Range){0, 1}, &range));
    CHECK(range.begin == 0 && range.end == 1);
    CHECK(noc_logical_source_token_range_for_bytes(
        &source, (Noc_Logical_Byte_Range){0, 2}, &range));
    CHECK(range.begin == 0 && range.end == 3);
    CHECK(noc_logical_source_token_range_for_bytes(
        &source, (Noc_Logical_Byte_Range){1, 1}, &range));
    CHECK(range.begin == 2 && range.end == 2);

    noc_logical_source_free(&source);
    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_logical_locations_and_owned_line_map();
    test_byte_to_token_ranges();
    return finish_suite("logical-source-queries");
}
