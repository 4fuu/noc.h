#include "macro_expansion_test_support.h"

static void test_paste_categories_rescan_and_provenance(void)
{
    static const char definitions[] =
        "#define AB 7\n"
        "#define A q\n"
        "#define OBJ foo##bar\n"
        "#define DIG left%:%:right\n"
        "#define CAT(a,b) a##b\n"
        "#define BOTH(x) x+x##z\n"
        "#define PUNCT(a,b) a##b\n"
        "#define PREFIX(a,b) a##b\n"
        "#define HASH # ## #\n"
        "#define CHAIN(a,b,c) a##b##c\n"
        "#define KEEP(a,b) before a /*left*/ ## /*right*/ b after\n"
        "#define PHASE_HASH(a,b) a#\\\n#b\n"
        "#define PHASE_DIGRAPH(a,b) a%:\\\n%:b\n"
        "#define ID(x) x\n"
        "#define FORWARD(a,b) ID(a##b)\n";
    static const char input[] =
        "OBJ\n"
        "DIG\n"
        "CAT(A,B)\n"
        "BOTH(A)\n"
        "PUNCT(<,<)\n"
        "PREFIX(L,\"x\")\n"
        "HASH\n"
        "CHAIN(1,e,+)\n"
        "KEEP(foo,bar)\n"
        "PHASE_HASH(x,y)\n"
        "PHASE_DIGRAPH(m,n)\n"
        "FORWARD(f,oo)\n";
    static const char expected[] =
        "foobar\n"
        "leftright\n"
        "7\n"
        "q+Az\n"
        "<<\n"
        "L\"x\"\n"
        "##\n"
        "1e+\n"
        "before foobar after\n"
        "xy\n"
        "mn\n"
        "foo\n";
    Macro_Expansion_Fixture fixture;
    size_t paste_count = 0;
    size_t index;

    CHECK(strcmp(noc_macro_expansion_status_name(
                     NOC_MACRO_EXPANSION_INVALID_PASTE),
                 "invalid-paste") == 0);
    CHECK(strcmp(noc_macro_expansion_token_origin_name(
                     NOC_MACRO_EXPANSION_TOKEN_PASTE),
                 "paste") == 0);
    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(fixture.expansion.generated_spelling_count == 13);
    for (index = 0; index < fixture.expansion.count; ++index) {
        const Noc_Macro_Expansion_Token *token =
            noc_macro_expansion_token_at(&fixture.expansion, index);
        CHECK(token != NULL);
        if (!token || token->origin != NOC_MACRO_EXPANSION_TOKEN_PASTE) continue;
        CHECK(token->generated_spelling_index <
              fixture.expansion.generated_spelling_count);
        CHECK(token->token.text.data ==
              fixture.expansion.generated_spellings[
                  token->generated_spelling_index].data);
        CHECK(token->token.text.count ==
              fixture.expansion.generated_spellings[
                  token->generated_spelling_index].count);
        CHECK(token->unit == &fixture.definitions);
        CHECK(noc_token_is_punct(
                  token->unit->preprocessing_tokens[
                      token->preprocessing_token_index].token,
                  "##") ||
              noc_token_is_punct(
                  token->unit->preprocessing_tokens[
                      token->preprocessing_token_index].token,
                  "%:%:"));
        paste_count += 1;
    }
    CHECK(paste_count == 11);
    CHECK(fixture.diagnostics.errors == 0);

    macro_fixture_deinit(&fixture);
}

static void test_pasted_only_argument_is_not_prescanned(void)
{
    static const char definitions[] =
        "#define A q\n"
        "#define CAT(a,b) a##b\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Limits limits = noc_macro_expansion_default_limits();

    macro_fixture_init(&fixture, definitions, "CAT(A,z)");
    limits.max_expansions = 1;
    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    fixture.environment.count,
                                    &fixture.input,
                                    macro_fixture_full_input(&fixture),
                                    limits,
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "Az"));
    CHECK(fixture.expansion.frame_count == 1);

    macro_fixture_deinit(&fixture);
}

static void test_paste_placemarkers_raw_arguments_and_hide_sets(void)
{
    static const char definitions[] =
        "#define AB 7\n"
        "#define EMPTY\n"
        "#define CAT(a,b) a##b\n"
        "#define WRAP(a,b) [a##b]\n"
        "#define CHAIN(a,b,c) a##b##c\n"
        "#define F(a,b) a##b\n";
    static const char input[] =
        "WRAP(,x)\n"
        "WRAP(x,)\n"
        "WRAP(,)\n"
        "WRAP(/*empty*/,x)\n"
        "CAT(EMPTY,x)\n"
        "CHAIN(a,,b)\n"
        "CAT(a b,c d)\n"
        "CAT(A,B)\n"
        "CAT(CA,T)(A,B)\n"
        "F(F,)(F,)\n";
    static const char expected[] =
        "[x]\n"
        "[x]\n"
        "[]\n"
        "[x]\n"
        "EMPTYx\n"
        "ab\n"
        "a bc d\n"
        "7\n"
        "CAT(A,B)\n"
        "F(F,)\n";
    Macro_Expansion_Fixture fixture;
    size_t identity_argument_count = 0;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    for (index = 0; index < fixture.expansion.count; ++index) {
        const Noc_Macro_Expansion_Token *token = &fixture.expansion.items[index];
        if (token->origin == NOC_MACRO_EXPANSION_TOKEN_ARGUMENT &&
            token->token.kind == NOC_TOKEN_IDENTIFIER &&
            noc_token_is_identifier(token->token, "x")) {
            CHECK(token->generated_spelling_index == NOC_TOKEN_INDEX_NONE);
            identity_argument_count += 1;
        }
    }
    CHECK(identity_argument_count == 3);

    macro_fixture_deinit(&fixture);
}

static void test_variadic_paste_is_strict_c11(void)
{
    static const char definitions[] =
        "#define V1(head,...) head##__VA_ARGS__\n"
        "#define V2(...) __VA_ARGS__##tail\n"
        "#define G(...) x,##__VA_ARGS__\n";
    static const char input[] =
        "V1(pre,fix)\n"
        "V1(pre,)\n"
        "V1(pre,a,b)\n"
        "V2(a,b)\n"
        "V2()\n"
        "G()\n"
        "G(a)\n";
    static const char expected[] =
        "prefix\n"
        "pre\n"
        "prea,b\n"
        "a,btail\n"
        "tail\n"
        "x,\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Slice *preserved_spellings;
    size_t preserved_generation;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    fixture.environment.count,
                                    &fixture.input,
                                    (Noc_Token_Range){
                                        macro_fixture_input_line(&fixture, 1).begin,
                                        macro_fixture_input_line(&fixture, 6).end},
                                    noc_macro_expansion_default_limits(),
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    preserved_items = fixture.expansion.items;
    preserved_spellings = fixture.expansion.generated_spellings;
    preserved_generation = fixture.expansion.generation;
    CHECK(macro_fixture_expand(&fixture, macro_fixture_input_line(&fixture, 7)) ==
          NOC_MACRO_EXPANSION_INVALID_PASTE);
    CHECK(fixture.expansion.items == preserved_items);
    CHECK(fixture.expansion.generated_spellings == preserved_spellings);
    CHECK(fixture.expansion.generation == preserved_generation);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));

    macro_fixture_deinit(&fixture);
}

static void test_invalid_pastes_are_transactional(void)
{
    static const char definitions[] =
        "#define GOOD(a,b) a##b\n"
        "#define LEAD ##x\n"
        "#define TRAIL x##\n"
        "#define DOUBLE x####y\n"
        "#define SLASH /##/\n"
        "#define BLOCK /##*\n"
        "#define DOT .##.\n"
        "#define PLUS x##+\n";
    static const char input[] =
        "GOOD(o,k)\n"
        "LEAD\n"
        "TRAIL\n"
        "DOUBLE\n"
        "SLASH\n"
        "BLOCK\n"
        "DOT\n"
        "PLUS\n"
        "GOOD(x,+)\n";
    static const Noc_Macro_Expansion_Status statuses[] = {
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INVALID_PASTE,
        NOC_MACRO_EXPANSION_INVALID_PASTE,
        NOC_MACRO_EXPANSION_INVALID_PASTE,
        NOC_MACRO_EXPANSION_INVALID_PASTE,
        NOC_MACRO_EXPANSION_INVALID_PASTE,
    };
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Macro_Expansion_Frame *preserved_frames;
    Noc_Slice *preserved_spellings;
    size_t preserved_spelling_count;
    size_t preserved_generation;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_input_line(&fixture, 1)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "ok\n"));
    preserved_items = fixture.expansion.items;
    preserved_frames = fixture.expansion.frames;
    preserved_spellings = fixture.expansion.generated_spellings;
    preserved_spelling_count = fixture.expansion.generated_spelling_count;
    preserved_generation = fixture.expansion.generation;
    for (index = 0; index < sizeof(statuses) / sizeof(statuses[0]); ++index) {
        CHECK(macro_fixture_expand(
                  &fixture,
                  macro_fixture_input_line(&fixture, index + 2)) ==
              statuses[index]);
        CHECK(fixture.expansion.items == preserved_items);
        CHECK(fixture.expansion.frames == preserved_frames);
        CHECK(fixture.expansion.generated_spellings == preserved_spellings);
        CHECK(fixture.expansion.generated_spelling_count ==
              preserved_spelling_count);
        CHECK(fixture.expansion.generation == preserved_generation);
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    }

    macro_fixture_deinit(&fixture);
}

static void test_paste_spelling_arena_growth(void)
{
    static const char input[] =
        "CAT(a,0)\nCAT(a,1)\nCAT(a,2)\nCAT(a,3)\nCAT(a,4)\n"
        "CAT(a,5)\nCAT(a,6)\nCAT(a,7)\nCAT(a,8)\nCAT(a,9)\n"
        "CAT(b,0)\nCAT(b,1)\nCAT(b,2)\nCAT(b,3)\nCAT(b,4)\n"
        "CAT(b,5)\nCAT(b,6)\nCAT(b,7)\nCAT(b,8)\nCAT(b,9)\n";
    Macro_Expansion_Fixture fixture;
    size_t index;
    size_t paste_count = 0;

    macro_fixture_init(&fixture, "#define CAT(a,b) a##b\n", input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(fixture.expansion.generated_spelling_count == 20);
    CHECK(fixture.expansion.generated_spelling_capacity >= 20);
    for (index = 0; index < fixture.expansion.count; ++index) {
        const Noc_Macro_Expansion_Token *token = &fixture.expansion.items[index];
        if (token->origin != NOC_MACRO_EXPANSION_TOKEN_PASTE) continue;
        CHECK(token->token.text.data ==
              fixture.expansion.generated_spellings[
                  token->generated_spelling_index].data);
        CHECK(token->token.text.count == 2);
        paste_count += 1;
    }
    CHECK(paste_count == 20);

    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_paste_categories_rescan_and_provenance();
    test_pasted_only_argument_is_not_prescanned();
    test_paste_placemarkers_raw_arguments_and_hide_sets();
    test_variadic_paste_is_strict_c11();
    test_invalid_pastes_are_transactional();
    test_paste_spelling_arena_growth();
    return finish_suite("macro-token-paste");
}
