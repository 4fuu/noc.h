#include "test_support.h"

typedef struct {
    Noc_Context context;
    Noc_Workspace workspace;
    Noc_Document_Snapshot definitions_snapshot;
    Noc_Document_Snapshot input_snapshot;
    Noc_Preprocessor_Unit definitions;
    Noc_Preprocessor_Unit input;
    Noc_Macro_Environment environment;
    Noc_Macro_Expansion expansion;
    Noc_Buffer rendered;
    Diagnostic_State diagnostics;
} Function_Macro_Fixture;

static void fixture_init(Function_Macro_Fixture *fixture,
                         const char *definitions,
                         const char *input)
{
    size_t index;
    memset(fixture, 0, sizeof(*fixture));
    noc_context_init(&fixture->context);
    noc_context_set_diagnostic(&fixture->context,
                               count_diagnostics,
                               &fixture->diagnostics);
    noc_workspace_init(&fixture->workspace);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      "function-definitions.h",
                                      definitions,
                                      strlen(definitions),
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &fixture->definitions_snapshot) ==
          NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      "function-input.c",
                                      input,
                                      strlen(input),
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &fixture->input_snapshot) ==
          NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&fixture->context,
                                      &fixture->definitions_snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture->definitions));
    CHECK(noc_preprocessor_unit_build(&fixture->context,
                                      &fixture->input_snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture->input));
    for (index = 0;
         index < fixture->definitions.macro_directive_count;
         ++index) {
        CHECK(noc_macro_environment_apply(&fixture->environment,
                                          &fixture->definitions,
                                          index) == NOC_MACRO_ENVIRONMENT_OK);
    }
}

static void fixture_deinit(Function_Macro_Fixture *fixture)
{
    noc_buffer_free(&fixture->rendered);
    noc_macro_expansion_free(&fixture->expansion);
    noc_macro_environment_free(&fixture->environment);
    noc_preprocessor_unit_free(&fixture->input);
    noc_preprocessor_unit_free(&fixture->definitions);
    noc_document_snapshot_free(&fixture->input_snapshot);
    noc_document_snapshot_free(&fixture->definitions_snapshot);
    noc_workspace_deinit(&fixture->workspace);
    noc_context_deinit(&fixture->context);
}

static Noc_Token_Range fixture_full_input(Function_Macro_Fixture *fixture)
{
    Noc_Token_Range range;
    range.begin = 0;
    range.end = fixture->input.preprocessing_token_count - 1;
    return range;
}

static Noc_Token_Range fixture_input_line(Function_Macro_Fixture *fixture,
                                          size_t line)
{
    Noc_Token_Range range = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    size_t index;
    for (index = 0; index < fixture->input.preprocessing_token_count; ++index) {
        Noc_Token token = fixture->input.preprocessing_tokens[index].token;
        if (token.kind == NOC_TOKEN_EOF || token.location.line > line) break;
        if (token.location.line != line) continue;
        if (range.begin == NOC_TOKEN_INDEX_NONE) range.begin = index;
        range.end = index + 1;
    }
    return range;
}

static Noc_Macro_Expansion_Status fixture_expand(
    Function_Macro_Fixture *fixture,
    Noc_Token_Range range)
{
    return noc_macro_expansion_build(&fixture->environment,
                                     fixture->environment.count,
                                     &fixture->input,
                                     range,
                                     noc_macro_expansion_default_limits(),
                                     &fixture->expansion);
}

static bool fixture_render_equals(Function_Macro_Fixture *fixture,
                                  const char *expected)
{
    if (!noc_macro_expansion_render(&fixture->expansion, &fixture->rendered)) {
        return false;
    }
    return slice_equals(
        (Noc_Slice){fixture->rendered.items, fixture->rendered.count},
        expected);
}

static bool fixture_frame_is_macro(const Function_Macro_Fixture *fixture,
                                   size_t frame_index,
                                   const char *name)
{
    const Noc_Macro_Expansion_Frame *frame =
        noc_macro_expansion_frame_at(&fixture->expansion, frame_index);
    const Noc_Macro_Directive *directive;
    const Noc_Preprocessing_Token *name_token;
    if (!frame) return false;
    directive = noc_macro_environment_entry_directive(
        &fixture->environment,
        frame->environment_entry_index);
    if (!directive) return false;
    name_token = noc_preprocessor_token_at(
        fixture->environment.items[
            frame->environment_entry_index].unit,
        directive->name_token_index);
    return name_token && noc_token_is_identifier(name_token->token, name);
}

static void test_prescan_substitution_and_logical_rescan(void)
{
    static const char definitions[] =
        "#define ONE 1\n"
        "#define ID(x) x\n"
        "#define ADD(x,y) ((x)+(y))\n"
        "#define CALL(f,x) f(x)\n"
        "#define ALIAS ID\n"
        "#define OPEN ID(\n"
        "#define FORWARD() ID\n"
        "#define TWICE(x) x+x\n"
        "#define ZERO() 0\n"
        "#define PAIR(x,y) x:y\n"
        "#define DISCARD(x) 7\n";
    static const char input[] =
        "ADD(ONE,ID(2));\n"
        "ALIAS(3);\n"
        "OPEN 4);\n"
        "FORWARD()(5);\n"
        "CALL(ID,6);\n"
        "TWICE(ONE);\n"
        "ZERO();\n"
        "PAIR(,8);\n"
        "ID();\n"
        "DISCARD(ONE);\n";
    static const char expected[] =
        "((1)+(2));\n"
        "3;\n"
        " 4;\n"
        "5;\n"
        "6;\n"
        "1+1;\n"
        "0;\n"
        ":8;\n"
        ";\n"
        "7;\n";
    Function_Macro_Fixture fixture;
    size_t argument_count = 0;
    size_t one_frame_count = 0;
    size_t index;

    fixture_init(&fixture, definitions, input);
    CHECK(fixture_expand(&fixture, fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(fixture_render_equals(&fixture, expected));
    CHECK(fixture.expansion.frame_count == 17);
    for (index = 0; index < fixture.expansion.frame_count; ++index) {
        const Noc_Macro_Expansion_Frame *frame =
            noc_macro_expansion_frame_at(&fixture.expansion, index);
        CHECK(frame != NULL);
        if (frame && frame->parent_frame_index != NOC_TOKEN_INDEX_NONE) {
            CHECK(frame->parent_frame_index < index);
        }
        if (fixture_frame_is_macro(&fixture, index, "ONE")) {
            one_frame_count += 1;
        }
    }
    CHECK(one_frame_count == 2);
    for (index = 0; index < fixture.expansion.count; ++index) {
        const Noc_Macro_Expansion_Token *token =
            noc_macro_expansion_token_at(&fixture.expansion, index);
        CHECK(token != NULL);
        if (token && token->origin == NOC_MACRO_EXPANSION_TOKEN_ARGUMENT) {
            argument_count += 1;
            CHECK(token->frame_index < fixture.expansion.frame_count);
        }
    }
    CHECK(argument_count > 0);
    CHECK(fixture.diagnostics.errors == 0);

    fixture_deinit(&fixture);
}

static void test_function_hide_sets_and_nested_prescan(void)
{
    static const char definitions[] =
        "#define ONE 1\n"
        "#define ID(x) x\n"
        "#define WRAP(x) ID(x)\n"
        "#define SELF(x) SELF(x)\n"
        "#define LEFT(x) RIGHT(x)\n"
        "#define RIGHT(x) LEFT(x)\n";
    static const char input[] =
        "SELF(1)\n"
        "LEFT(2)\n"
        "ID(ID(3))\n"
        "WRAP(ID(ONE))\n";
    static const char expected[] =
        "SELF(1)\n"
        "LEFT(2)\n"
        "3\n"
        "1\n";
    Function_Macro_Fixture fixture;

    fixture_init(&fixture, definitions, input);
    CHECK(fixture_expand(&fixture, fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    CHECK(fixture.diagnostics.errors == 0);

    fixture_deinit(&fixture);
}

static void test_c11_recursive_rescan_example(void)
{
    static const char definitions[] =
        "#define x 3\n"
        "#define f(a) f(x * (a))\n"
        "#undef x\n"
        "#define x 2\n"
        "#define g f\n"
        "#define z z[0]\n";
    static const char input[] =
        "f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);\n";
    static const char expected[] =
        "f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % t(t(f)(0) + t)(1);\n";
    Function_Macro_Fixture fixture;

    fixture_init(&fixture, definitions, input);
    CHECK(fixture_expand(&fixture, fixture_full_input(&fixture)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(fixture_render_equals(&fixture, expected));
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));

    fixture_deinit(&fixture);
}

static void test_function_hide_sets_do_not_consume_output_limit(void)
{
    static const char definitions[] =
        "#define A() B()\n"
        "#define B() C()\n"
        "#define C() D()\n"
        "#define D() E()\n"
        "#define E() 1\n";
    Function_Macro_Fixture fixture;
    Noc_Macro_Expansion_Limits limits = noc_macro_expansion_default_limits();

    fixture_init(&fixture, definitions, "A()");
    limits.max_depth = 8;
    limits.max_output_tokens = 3;
    limits.max_expansions = 8;
    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    fixture.environment.count,
                                    &fixture.input,
                                    fixture_full_input(&fixture),
                                    limits,
                                    &fixture.expansion) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(fixture.expansion.count == 1);
    CHECK(fixture.expansion.frame_count == 5);
    CHECK(fixture_render_equals(&fixture, "1"));

    fixture_deinit(&fixture);
}

static void test_history_prefix_switches_macro_kind(void)
{
    static const char definitions[] =
        "#define M 1\n"
        "#undef M\n"
        "#define M(x) x\n"
        "#undef M\n"
        "#define M 2\n";
    static const char *const expected[] = {
        "1(3)",
        "M(3)",
        "3",
        "M(3)",
        "2(3)",
    };
    Function_Macro_Fixture fixture;
    size_t entry_limit;

    fixture_init(&fixture, definitions, "M(3)");
    CHECK(fixture.environment.count ==
          sizeof(expected) / sizeof(expected[0]));
    for (entry_limit = 1;
         entry_limit <= sizeof(expected) / sizeof(expected[0]);
         ++entry_limit) {
        CHECK(noc_macro_expansion_build(
                  &fixture.environment,
                  entry_limit,
                  &fixture.input,
                  fixture_full_input(&fixture),
                  noc_macro_expansion_default_limits(),
                  &fixture.expansion) == NOC_MACRO_EXPANSION_OK);
        CHECK(fixture_render_equals(&fixture, expected[entry_limit - 1]));
        CHECK(fixture.expansion.environment_entry_limit == entry_limit);
    }

    fixture_deinit(&fixture);
}

static void test_function_errors_are_transactional(void)
{
    static const char definitions[] =
        "#define ID(x) x\n"
        "#define TWO(x,y) x+y\n"
        "#define VAR(x,...) x\n"
        "#define STR(x) #x\n"
        "#define DIGRAPH_STR(x) %: x\n"
        "#define PASTE(x,y) x ## y\n"
        "#define DIGRAPH_PASTE(x,y) x %:%: y\n"
        "#define DUP(x,x) x\n";
    static const char input[] =
        "ID(1)\n"
        "TWO(1)\n"
        "TWO(1,2,3)\n"
        "VAR(1,2)\n"
        "STR(x)\n"
        "DIGRAPH_STR(x)\n"
        "PASTE(a,b)\n"
        "DIGRAPH_PASTE(a,b)\n"
        "DUP(1,2)\n"
        "ID(unterminated\n";
    static const Noc_Macro_Expansion_Status statuses[] = {
        NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH,
        NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH,
        NOC_MACRO_EXPANSION_UNSUPPORTED_VARIADIC,
        NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR,
        NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR,
        NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR,
        NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR,
        NOC_MACRO_EXPANSION_INVALID_DEFINITION,
        NOC_MACRO_EXPANSION_INCOMPLETE_INVOCATION,
    };
    Function_Macro_Fixture fixture;
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Macro_Expansion_Frame *preserved_frames;
    size_t preserved_count;
    size_t preserved_generation;
    size_t index;

    fixture_init(&fixture, definitions, input);
    CHECK(fixture_expand(&fixture, fixture_input_line(&fixture, 1)) ==
          NOC_MACRO_EXPANSION_OK);
    CHECK(fixture_render_equals(&fixture, "1\n"));
    preserved_items = fixture.expansion.items;
    preserved_frames = fixture.expansion.frames;
    preserved_count = fixture.expansion.count;
    preserved_generation = fixture.expansion.generation;
    for (index = 0; index < sizeof(statuses) / sizeof(statuses[0]); ++index) {
        Noc_Token_Range range = fixture_input_line(&fixture, index + 2);
        CHECK(range.begin != NOC_TOKEN_INDEX_NONE);
        CHECK(fixture_expand(&fixture, range) == statuses[index]);
        CHECK(fixture.expansion.items == preserved_items);
        CHECK(fixture.expansion.frames == preserved_frames);
        CHECK(fixture.expansion.count == preserved_count);
        CHECK(fixture.expansion.generation == preserved_generation);
        CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    }
    CHECK(fixture.diagnostics.errors == 0);

    fixture_deinit(&fixture);
}

int main(void)
{
    test_prescan_substitution_and_logical_rescan();
    test_function_hide_sets_and_nested_prescan();
    test_c11_recursive_rescan_example();
    test_function_hide_sets_do_not_consume_output_limit();
    test_history_prefix_switches_macro_kind();
    test_function_errors_are_transactional();
    return finish_suite("function-macro-expansion");
}
