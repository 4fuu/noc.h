#include "macro_expansion_test_support.h"

static void test_stringification_spelling_rescan_and_provenance(void)
{
    static const char definitions[] =
        "#define ONE 1\n"
        "#define STR(x) #x\n"
        "#define DIGRAPH(x) %: x\n"
        "#define BOTH(x) #x x\n"
        "#define VSTR(...) #__VA_ARGS__\n"
        "#define WRAP(x) STR(x)\n"
        "#define PASTE a ## b\n"
        "#define OPEN STR(\n";
    static const char input[] =
        "STR(ONE)\n"
        "WRAP(ONE)\n"
        "STR( a   + /*comment*/ b )\n"
        "STR(\"a\\\\b\")\n"
        "STR()\n"
        "DIGRAPH(foo bar)\n"
        "BOTH(ONE)\n"
        "VSTR(a,b, c)\n"
        "VSTR()\n"
        "STR(ab\\\ncd)\n"
        "STR(PASTE)\n"
        "OPEN ONE)\n";
    static const char expected[] =
        "\"ONE\"\n"
        "\"1\"\n"
        "\"a + b\"\n"
        "\"\\\"a\\\\\\\\b\\\"\"\n"
        "\"\"\n"
        "\"foo bar\"\n"
        "\"ONE\" 1\n"
        "\"a,b, c\"\n"
        "\"\"\n"
        "\"abcd\"\n"
        "\"PASTE\"\n"
        "\"ONE\"\n";
    Macro_Expansion_Fixture fixture;
    size_t stringification_count = 0;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    for (index = 0; index < fixture.expansion.count; ++index) {
        const Noc_Macro_Expansion_Token *token =
            noc_macro_expansion_token_at(&fixture.expansion, index);
        CHECK(token != NULL);
        if (!token) continue;
        if (token->origin == NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION) {
            const Noc_Slice *spelling;
            Noc_Token operator_token;
            CHECK(token->unit == &fixture.definitions);
            CHECK(token->token.kind == NOC_TOKEN_STRING);
            CHECK(token->generated_spelling_index <
                  fixture.expansion.generated_spelling_count);
            spelling = &fixture.expansion.generated_spellings[
                token->generated_spelling_index];
            CHECK(token->token.text.data == spelling->data);
            CHECK(token->token.text.count == spelling->count);
            operator_token = token->unit->preprocessing_tokens[
                token->preprocessing_token_index].token;
            CHECK(noc_token_is_punct(operator_token, "#") ||
                  noc_token_is_punct(operator_token, "%:"));
            stringification_count += 1;
        } else {
            CHECK(token->generated_spelling_index == NOC_TOKEN_INDEX_NONE);
        }
    }
    CHECK(stringification_count == 12);
    CHECK(fixture.expansion.generated_spelling_count == stringification_count);
    CHECK(fixture.diagnostics.errors == 0);

    macro_fixture_deinit(&fixture);
}

static void test_stringification_survives_argument_forwarding(void)
{
    static const char definitions[] =
        "#define ID(x) x\n"
        "#define QUOTE(x) ID(#x)\n";
    Macro_Expansion_Fixture fixture;
    const Noc_Macro_Expansion_Token *token;
    const Noc_Macro_Expansion_Frame *frame;
    const Noc_Macro_Expansion_Frame *parent;

    macro_fixture_init(&fixture, definitions, "QUOTE(forwarded)");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\"forwarded\""));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(fixture.expansion.count == 1);
    token = noc_macro_expansion_token_at(&fixture.expansion, 0);
    CHECK(token != NULL);
    if (token) {
        CHECK(token->origin == NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION);
        CHECK(token->generated_spelling_index == 0);
        CHECK(noc_token_is_punct(
            token->unit->preprocessing_tokens[
                token->preprocessing_token_index].token,
            "#"));
        frame = noc_macro_expansion_frame_at(&fixture.expansion,
                                             token->frame_index);
        CHECK(frame != NULL);
        CHECK(frame && frame->environment_entry_index == 0);
        CHECK(frame && frame->parent_frame_index != NOC_TOKEN_INDEX_NONE);
        parent = frame
                     ? noc_macro_expansion_frame_at(
                           &fixture.expansion,
                           frame->parent_frame_index)
                     : NULL;
        CHECK(parent != NULL);
        CHECK(parent && parent->environment_entry_index == 1);
    }

    macro_fixture_deinit(&fixture);
}

static void test_stringification_phase_two_variadics_and_literals(void)
{
    static const char definitions[] =
        "#define STR(x) #x\n"
        "#define DIGRAPH(x) %\\\n: x\n"
        "#define VSTR(...) #__VA_ARGS__\n"
        "#define VTAIL(head,...) #__VA_ARGS__\n";
    static const char input[] =
        "STR(a\\\n+b)\n"
        "STR(a \\\n+b)\n"
        "DIGRAPH(token)\n"
        "VTAIL(x,)\n"
        "VTAIL(x,,)\n"
        "VSTR(,)\n"
        "STR('\\\\')\n"
        "STR('\\\"')\n";
    static const char expected[] =
        "\"a+b\"\n"
        "\"a +b\"\n"
        "\"token\"\n"
        "\"\"\n"
        "\",\"\n"
        "\",\"\n"
        "\"'\\\\\\\\'\"\n"
        "\"'\\\\\\\"'\"\n";
    Macro_Expansion_Fixture fixture;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(fixture.expansion.generated_spelling_count == 8);

    macro_fixture_deinit(&fixture);
}

static void test_generated_spelling_growth_preserves_token_text(void)
{
    static const char input[] =
        "STR(a)\nSTR(b)\nSTR(c)\nSTR(d)\nSTR(e)\n"
        "STR(f)\nSTR(g)\nSTR(h)\nSTR(i)\nSTR(j)\n"
        "STR(k)\nSTR(l)\nSTR(m)\nSTR(n)\nSTR(o)\n"
        "STR(p)\nSTR(q)\nSTR(r)\nSTR(s)\nSTR(t)\n";
    Macro_Expansion_Fixture fixture;
    size_t index;
    size_t generated_count = 0;

    macro_fixture_init(&fixture, "#define STR(x) #x\n", input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(fixture.expansion.generated_spelling_count == 20);
    CHECK(fixture.expansion.generated_spelling_capacity >= 20);
    for (index = 0; index < fixture.expansion.count; ++index) {
        const Noc_Macro_Expansion_Token *token = &fixture.expansion.items[index];
        if (token->origin == NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION) {
            const Noc_Slice *spelling = &fixture.expansion.generated_spellings[
                token->generated_spelling_index];
            CHECK(token->token.text.data == spelling->data);
            CHECK(token->token.text.count == spelling->count);
            CHECK(token->token.text.count == 3);
            CHECK(token->token.text.data[0] == '"');
            CHECK(token->token.text.data[2] == '"');
            generated_count += 1;
        }
    }
    CHECK(generated_count == 20);

    macro_fixture_deinit(&fixture);
}

static void test_object_hash_is_not_stringification(void)
{
    Macro_Expansion_Fixture fixture;

    macro_fixture_init(&fixture, "#define HASH #\n", "HASH");
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "#"));
    CHECK(fixture.expansion.generated_spelling_count == 0);

    macro_fixture_deinit(&fixture);
}

static void test_stringification_errors_are_transactional(void)
{
    static const char definitions[] =
        "#define GOOD(x) #x\n"
        "#define BAD_END(x) #\n"
        "#define BAD_NAME(x) # missing\n"
        "#define DOUBLE(x) # # x\n";
    static const char input[] =
        "GOOD(ok)\n"
        "BAD_END(value)\n"
        "BAD_NAME(value)\n"
        "DOUBLE(value)\n"
        "GOOD(unterminated\n";
    static const Noc_Macro_Expansion_Status statuses[] = {
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INCOMPLETE_INVOCATION,
    };
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Macro_Expansion_Frame *preserved_frames;
    Noc_Slice *preserved_spellings;
    size_t preserved_count;
    size_t preserved_spelling_count;
    size_t preserved_generation;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_input_line(&fixture, 1)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\"ok\"\n"));
    preserved_items = fixture.expansion.items;
    preserved_frames = fixture.expansion.frames;
    preserved_spellings = fixture.expansion.generated_spellings;
    preserved_count = fixture.expansion.count;
    preserved_spelling_count = fixture.expansion.generated_spelling_count;
    preserved_generation = fixture.expansion.generation;
    for (index = 0; index < sizeof(statuses) / sizeof(statuses[0]); ++index) {
        Noc_Token_Range range = macro_fixture_input_line(&fixture, index + 2);
        CHECK(range.begin != NOC_TOKEN_INDEX_NONE);
        CHECK(macro_fixture_expand(&fixture, range) == statuses[index]);
        CHECK(fixture.expansion.items == preserved_items);
        CHECK(fixture.expansion.frames == preserved_frames);
        CHECK(fixture.expansion.generated_spellings == preserved_spellings);
        CHECK(fixture.expansion.count == preserved_count);
        CHECK(fixture.expansion.generated_spelling_count ==
              preserved_spelling_count);
        CHECK(fixture.expansion.generation == preserved_generation);
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    }

    macro_fixture_deinit(&fixture);
}

static void test_generated_spelling_failures_are_transactional(void)
{
    static const char definitions[] =
        "#define GOOD(x) #x\n"
        "#define EXACT(x) #x#x#x#x#x#x\n"
        "#define OVER(x) #x#x#x#x#x#x#x\n";
    static const char input[] =
        "GOOD(ok)\n"
        "EXACT(a)z\n"
        "OVER(a)z\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Limits limits;
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Macro_Expansion_Frame *preserved_frames;
    Noc_Slice *preserved_spellings;
    size_t preserved_generation;
    size_t line;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_input_line(&fixture, 1)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "\"ok\"\n"));
    preserved_items = fixture.expansion.items;
    preserved_frames = fixture.expansion.frames;
    preserved_spellings = fixture.expansion.generated_spellings;
    preserved_generation = fixture.expansion.generation;
    for (line = 2; line <= 3; ++line) {
        Noc_Token_Range range = macro_fixture_input_line(&fixture, line);
        limits = noc_macro_expansion_default_limits();
        limits.max_output_tokens = range.end - range.begin;
        CHECK(noc_macro_expansion_build(&fixture.environment,
                                        fixture.environment.count,
                                        &fixture.input,
                                        range,
                                        limits,
                                        &fixture.expansion) ==
              NOC_MACRO_EXPANSION_OUTPUT_LIMIT);
        CHECK(fixture.expansion.items == preserved_items);
        CHECK(fixture.expansion.frames == preserved_frames);
        CHECK(fixture.expansion.generated_spellings == preserved_spellings);
        CHECK(fixture.expansion.generated_spelling_count == 1);
        CHECK(fixture.expansion.generation == preserved_generation);
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
        CHECK(macro_fixture_render_equals(&fixture, "\"ok\"\n"));
    }

    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_stringification_spelling_rescan_and_provenance();
    test_stringification_survives_argument_forwarding();
    test_stringification_phase_two_variadics_and_literals();
    test_generated_spelling_growth_preserves_token_text();
    test_object_hash_is_not_stringification();
    test_stringification_errors_are_transactional();
    test_generated_spelling_failures_are_transactional();
    return finish_suite("macro-stringification");
}
