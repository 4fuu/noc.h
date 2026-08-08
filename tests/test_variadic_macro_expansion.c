#include "macro_expansion_test_support.h"

static void test_variadic_prescan_substitution_and_provenance(void)
{
    static const char definitions[] =
        "#define ONE 1\n"
        "#define ID(x) x\n"
        "#define PAIR(x,y) x:y\n"
        "#define V(...) __VA_ARGS__\n"
        "#define PREFIX(head,...) head + __VA_ARGS__\n"
        "#define APPLY(callable,...) callable(__VA_ARGS__)\n"
        "#define DUP(...) __VA_ARGS__|__VA_ARGS__\n"
        "#define DISCARD(...) 7\n"
        "#define NEST(...) V(__VA_ARGS__)\n"
        "#define SELF(...) SELF(__VA_ARGS__)\n"
        "#define STR(x) #x\n"
        "#define ALIAS V\n"
        "#define OPEN V(\n";
    static const char input[] =
        "V(ONE,ID(2));\n"
        "PREFIX(ONE,ID(2),3);\n"
        "APPLY(PAIR,4,ID(5));\n"
        "DUP(ID(6),7);\n"
        "DISCARD(STR(unsupported));\n"
        "NEST(ID(8),9);\n"
        "SELF(10,11);\n"
        "V();\n"
        "V(,);\n"
        "PREFIX(1,);\n"
        "V;\n"
        "ALIAS(12,13);\n"
        "OPEN 14,15);\n";
    static const char expected[] =
        "1,2;\n"
        "1 + 2,3;\n"
        "4:5;\n"
        "6,7|6,7;\n"
        "7;\n"
        "8,9;\n"
        "SELF(10,11);\n"
        ";\n"
        ",;\n"
        "1 + ;\n"
        "V;\n"
        "12,13;\n"
        " 14,15;\n";
    Macro_Expansion_Fixture fixture;
    size_t argument_comma_count = 0;
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
        if (token && token->origin == NOC_MACRO_EXPANSION_TOKEN_ARGUMENT &&
            noc_token_is_punct(token->token, ",")) {
            CHECK(token->unit == &fixture.input);
            argument_comma_count += 1;
        }
    }
    CHECK(argument_comma_count == 9);
    CHECK(fixture.diagnostics.errors == 0);

    macro_fixture_deinit(&fixture);
}

static void test_reserved_va_args_macro_names_are_transactional(void)
{
    static const char definitions[] =
        "#define GOOD 1\n"
        "#define __VA_ARGS__ 2\n"
        "#undef __VA_ARGS__\n"
        "#define __VA_\\\nARGS__(...) 3\n";
    static const char input[] =
        "GOOD\n"
        "__VA_ARGS__\n"
        "__VA_ARGS__()\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Macro_Expansion_Frame *preserved_frames;
    size_t preserved_count;
    size_t preserved_generation;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_input_line(&fixture, 1)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "1\n"));
    preserved_items = fixture.expansion.items;
    preserved_frames = fixture.expansion.frames;
    preserved_count = fixture.expansion.count;
    preserved_generation = fixture.expansion.generation;

    CHECK(noc_macro_expansion_build(
              &fixture.environment,
              2,
              &fixture.input,
              macro_fixture_input_line(&fixture, 2),
              noc_macro_expansion_default_limits(),
              &fixture.expansion) == NOC_MACRO_EXPANSION_INVALID_DEFINITION);
    CHECK(fixture.expansion.items == preserved_items);
    CHECK(fixture.expansion.frames == preserved_frames);
    CHECK(fixture.expansion.count == preserved_count);
    CHECK(fixture.expansion.generation == preserved_generation);

    CHECK(noc_macro_expansion_build(
              &fixture.environment,
              fixture.environment.count,
              &fixture.input,
              macro_fixture_input_line(&fixture, 3),
              noc_macro_expansion_default_limits(),
              &fixture.expansion) == NOC_MACRO_EXPANSION_INVALID_DEFINITION);
    CHECK(fixture.expansion.items == preserved_items);
    CHECK(fixture.expansion.frames == preserved_frames);
    CHECK(fixture.expansion.count == preserved_count);
    CHECK(fixture.expansion.generation == preserved_generation);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));

    macro_fixture_deinit(&fixture);
}

static void test_variadic_recursive_hide_sets(void)
{
    static const char definitions[] =
        "#define V(...) __VA_ARGS__\n"
        "#define ID(x) x\n"
        "#define F(...) F(__VA_ARGS__)\n"
        "#define LEFT(...) RIGHT(__VA_ARGS__)\n"
        "#define RIGHT(...) LEFT(__VA_ARGS__)\n";
    static const char input[] =
        "V(V(1,2),ID(3))\n"
        "F(4)\n"
        "LEFT(5)\n";
    static const char expected[] =
        "1,2,3\n"
        "F(4)\n"
        "LEFT(5)\n";
    Macro_Expansion_Fixture fixture;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));

    macro_fixture_deinit(&fixture);
}

static void test_variadic_errors_are_transactional(void)
{
    static const char definitions[] =
        "#define GOOD(...) __VA_ARGS__\n"
        "#define NEED(head,...) head\n"
        "#define TWO(a,b,...) a+b\n"
        "#define BAD_FIXED(__VA_ARGS__) __VA_ARGS__\n"
        "#define BAD_USE(x) __VA_ARGS__\n"
        "#define BAD_OBJECT __VA_ARGS__\n"
        "#define DUPLICATE(x,x,...) x\n"
        "#define PASTE(...) __VA_ARGS__ ## suffix\n";
    static const char input[] =
        "GOOD(1)\n"
        "NEED(1)\n"
        "NEED()\n"
        "TWO(1,2)\n"
        "BAD_FIXED(1)\n"
        "BAD_USE(1)\n"
        "BAD_OBJECT\n"
        "DUPLICATE(1,2,3)\n"
        "PASTE(1)\n"
        "GOOD(unterminated\n";
    static const Noc_Macro_Expansion_Status statuses[] = {
        NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH,
        NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH,
        NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH,
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR,
        NOC_MACRO_EXPANSION_INCOMPLETE_INVOCATION,
    };
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Macro_Expansion_Frame *preserved_frames;
    size_t preserved_count;
    size_t preserved_generation;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(macro_fixture_expand(&fixture, macro_fixture_input_line(&fixture, 1)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(macro_fixture_render_equals(&fixture, "1\n"));
    preserved_items = fixture.expansion.items;
    preserved_frames = fixture.expansion.frames;
    preserved_count = fixture.expansion.count;
    preserved_generation = fixture.expansion.generation;
    for (index = 0; index < sizeof(statuses) / sizeof(statuses[0]); ++index) {
        Noc_Token_Range range = macro_fixture_input_line(&fixture, index + 2);
        CHECK(range.begin != NOC_TOKEN_INDEX_NONE);
        CHECK(macro_fixture_expand(&fixture, range) == statuses[index]);
        CHECK(fixture.expansion.items == preserved_items);
        CHECK(fixture.expansion.frames == preserved_frames);
        CHECK(fixture.expansion.count == preserved_count);
        CHECK(fixture.expansion.generation == preserved_generation);
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    }

    macro_fixture_deinit(&fixture);
}

int main(void)
{
    test_variadic_prescan_substitution_and_provenance();
    test_variadic_recursive_hide_sets();
    test_variadic_errors_are_transactional();
    test_reserved_va_args_macro_names_are_transactional();
    return finish_suite("variadic-macro-expansion");
}
