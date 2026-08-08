#include "test_support.h"

static size_t identifier_on_line(const Noc_Preprocessor_Unit *unit,
                                 const char *name,
                                 size_t line)
{
    size_t index;
    for (index = 0; index < unit->preprocessing_token_count; ++index) {
        Noc_Token token = unit->preprocessing_tokens[index].token;
        if (token.location.line == line && noc_token_is_identifier(token, name)) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static size_t punctuation_after(const Noc_Preprocessor_Unit *unit,
                                size_t begin,
                                const char *punctuation)
{
    size_t index;
    for (index = begin; index < unit->preprocessing_token_count; ++index) {
        if (noc_token_is_punct(unit->preprocessing_tokens[index].token,
                               punctuation)) {
            return index;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static Noc_Slice range_source(const Noc_Preprocessor_Unit *unit,
                              Noc_Token_Range range)
{
    Noc_Slice result = {0};
    const Noc_Token *first;
    const Noc_Token *last;
    if (range.begin >= range.end ||
        range.end > unit->preprocessing_token_count) {
        return result;
    }
    first = &unit->preprocessing_tokens[range.begin].token;
    last = &unit->preprocessing_tokens[range.end - 1].token;
    result.data = first->text.data;
    result.count = (size_t)(last->text.data + last->text.count - result.data);
    return result;
}

static void check_names(void)
{
    CHECK(strcmp(noc_macro_invocation_status_name(
                     NOC_MACRO_INVOCATION_NOT_INVOKED),
                 "not-invoked") == 0);
    CHECK(strcmp(noc_macro_invocation_status_name(NOC_MACRO_INVOCATION_COMPLETE),
                 "complete") == 0);
    CHECK(strcmp(noc_macro_invocation_status_name(
                     NOC_MACRO_INVOCATION_INCOMPLETE),
                 "incomplete") == 0);
    CHECK(strcmp(noc_macro_invocation_status_name(
                     (Noc_Macro_Invocation_Status)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_macro_invocation_build_status_name(
                     NOC_MACRO_INVOCATION_BUILD_OK),
                 "ok") == 0);
    CHECK(strcmp(noc_macro_invocation_build_status_name(
                     NOC_MACRO_INVOCATION_BUILD_INVALID_ARGUMENT),
                 "invalid-argument") == 0);
    CHECK(strcmp(noc_macro_invocation_build_status_name(
                     NOC_MACRO_INVOCATION_BUILD_STALE),
                 "stale") == 0);
    CHECK(strcmp(noc_macro_invocation_build_status_name(
                     NOC_MACRO_INVOCATION_BUILD_GENERATION_EXHAUSTED),
                 "generation-exhausted") == 0);
    CHECK(strcmp(noc_macro_invocation_build_status_name(
                     NOC_MACRO_INVOCATION_BUILD_OUT_OF_MEMORY),
                 "out-of-memory") == 0);
    CHECK(strcmp(noc_macro_invocation_build_status_name(
                     (Noc_Macro_Invocation_Build_Status)99),
                 "unknown") == 0);
}

static void test_complete_and_incomplete_invocations(void)
{
    static const char source[] =
        "F (a, (b, c), , d) tail\n"
        "ZERO()\n"
        "COMMENT( /* only trivia */ )\n"
        "NEST(((x)))\n"
        "BARE value\n"
        "INCOMPLETE(a, (b, c)\n";
    static const char *const expected_arguments[] = {"a", " (b, c)", " ", " d"};
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Noc_Macro_Invocation invocation = {0};
    Diagnostic_State diagnostics = {0};
    size_t eof_index;
    size_t name_index;
    size_t index;

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "invocations.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    eof_index = unit.preprocessing_token_count - 1;

    name_index = identifier_on_line(&unit, "F", 1);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(noc_macro_invocation_is_valid(&invocation));
    CHECK(invocation.status == NOC_MACRO_INVOCATION_COMPLETE);
    CHECK(invocation.generation == 1);
    CHECK(invocation.tokens.begin == name_index);
    CHECK(slice_equals(range_source(&unit, invocation.tokens),
                       "F (a, (b, c), , d)"));
    CHECK(invocation.argument_count == 4);
    for (index = 0; index < invocation.argument_count; ++index) {
        const Noc_Macro_Argument *argument =
            noc_macro_invocation_argument_at(&invocation, index);
        CHECK(argument != NULL);
        if (argument) {
            CHECK(slice_equals(range_source(&unit, argument->tokens),
                               expected_arguments[index]));
        }
    }
    CHECK(noc_macro_invocation_argument_at(&invocation,
                                           invocation.argument_count) == NULL);

    name_index = identifier_on_line(&unit, "ZERO", 2);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_COMPLETE);
    CHECK(invocation.argument_count == 0);
    CHECK(invocation.generation == 2);

    name_index = identifier_on_line(&unit, "COMMENT", 3);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_COMPLETE);
    CHECK(invocation.argument_count == 0);

    name_index = identifier_on_line(&unit, "NEST", 4);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.argument_count == 1);
    CHECK(slice_equals(range_source(
                           &unit,
                           noc_macro_invocation_argument_at(&invocation, 0)->tokens),
                       "((x))"));

    name_index = identifier_on_line(&unit, "BARE", 5);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(noc_macro_invocation_is_valid(&invocation));
    CHECK(invocation.status == NOC_MACRO_INVOCATION_NOT_INVOKED);
    CHECK(invocation.open_token_index == NOC_TOKEN_INDEX_NONE);
    CHECK(invocation.tokens.end == name_index + 1);

    name_index = identifier_on_line(&unit, "INCOMPLETE", 6);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(noc_macro_invocation_is_valid(&invocation));
    CHECK(invocation.status == NOC_MACRO_INVOCATION_INCOMPLETE);
    CHECK(invocation.close_token_index == NOC_TOKEN_INDEX_NONE);
    CHECK(invocation.problem_token_index == eof_index);
    CHECK(invocation.tokens.end == eof_index);
    CHECK(invocation.argument_count == 2);
    CHECK(slice_equals(range_source(
                           &unit,
                           noc_macro_invocation_argument_at(&invocation, 0)->tokens),
                       "a"));
    CHECK(slice_equals(range_source(
                           &unit,
                           noc_macro_invocation_argument_at(&invocation, 1)->tokens),
                       " (b, c)\n"));
    CHECK(diagnostics.errors == 0);

    noc_macro_invocation_free(&invocation);
    CHECK(invocation.arguments == NULL && invocation.argument_count == 0);
    CHECK(invocation.generation == 6);
    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_bounds_transactionality_and_staleness(void)
{
    static const char source[] = "F(a, b)\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Noc_Preprocessor_Unit stale_unit;
    Noc_Macro_Invocation invocation = {0};
    Noc_Macro_Invocation exhausted = {0};
    Noc_Macro_Argument *preserved_arguments;
    size_t close_index;
    size_t comma_index;
    size_t name_index;
    size_t open_index;
    size_t preserved_count;
    size_t preserved_generation;

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "invocation-bounds.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    name_index = identifier_on_line(&unit, "F", 1);
    open_index = punctuation_after(&unit, name_index, "(");
    comma_index = punctuation_after(&unit, open_index + 1, ",");
    close_index = punctuation_after(&unit, comma_index + 1, ")");
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     open_index,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_NOT_INVOKED);

    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     open_index + 1,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_INCOMPLETE);
    CHECK(invocation.argument_count == 0);
    CHECK(invocation.problem_token_index == NOC_TOKEN_INDEX_NONE);

    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     comma_index,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_INCOMPLETE);
    CHECK(invocation.argument_count == 1);

    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     comma_index + 1,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_INCOMPLETE);
    CHECK(invocation.argument_count == 2);
    CHECK(noc_macro_invocation_argument_at(&invocation, 1)->tokens.begin ==
          noc_macro_invocation_argument_at(&invocation, 1)->tokens.end);

    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     close_index,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_INCOMPLETE);
    CHECK(invocation.argument_count == 2);

    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     close_index + 1,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_COMPLETE);
    CHECK(invocation.argument_count == 2);
    preserved_arguments = invocation.arguments;
    preserved_count = invocation.argument_count;
    preserved_generation = invocation.generation;

    CHECK(noc_macro_invocation_parse(&unit,
                                     open_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_INVALID_ARGUMENT);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count + 1,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_INVALID_ARGUMENT);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     name_index,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_INVALID_ARGUMENT);
    CHECK(invocation.arguments == preserved_arguments);
    CHECK(invocation.argument_count == preserved_count);
    CHECK(invocation.generation == preserved_generation);
    CHECK(noc_macro_invocation_is_valid(&invocation));

    invocation.arguments[0].tokens.end = invocation.arguments[1].tokens.end;
    CHECK(!noc_macro_invocation_is_valid(&invocation));
    invocation.arguments[0].tokens.end = comma_index;
    CHECK(noc_macro_invocation_is_valid(&invocation));

    stale_unit = unit;
    stale_unit.token_stream_generation -= 1;
    CHECK(noc_macro_invocation_parse(&stale_unit,
                                     name_index,
                                     stale_unit.preprocessing_token_count + 1,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_STALE);
    CHECK(invocation.arguments == preserved_arguments);
    CHECK(invocation.argument_count == preserved_count);
    CHECK(invocation.generation == preserved_generation);

    exhausted.generation = SIZE_MAX;
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &exhausted) ==
          NOC_MACRO_INVOCATION_BUILD_GENERATION_EXHAUSTED);
    CHECK(exhausted.arguments == NULL && exhausted.argument_count == 0);

    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(!noc_macro_invocation_is_valid(&invocation));
    CHECK(noc_macro_invocation_argument_at(&invocation, 0) == NULL);

    noc_macro_invocation_free(&exhausted);
    noc_macro_invocation_free(&invocation);
    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_empty_slots_nesting_and_phase2(void)
{
    static const char source[] =
        "EMPTY()\n"
        "TRIVIA(/* only trivia */)\n"
        "LEADING(,a)\n"
        "TRAILING(a,)\n"
        "DOUBLE(a,,b)\n"
        "BRACES([a,b], {c,d})\n"
        "LITERALS(\"a,b\", ',', /* , */ x)\n"
        "SPLICE\\\n(a)\n"
        "#define BAD F(\"unfinished\n";
    static const char *const brace_arguments[] = {"[a", "b]", " {c", "d}"};
    static const char *const literal_arguments[] = {
        "\"a,b\"", " ','", " /* , */ x",
    };
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Noc_Macro_Invocation invocation = {0};
    size_t name_index;
    size_t index;

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "invocation-slots.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));

    name_index = identifier_on_line(&unit, "EMPTY", 1);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_COMPLETE);
    CHECK(invocation.argument_count == 0);

    name_index = identifier_on_line(&unit, "TRIVIA", 2);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.argument_count == 0);

    name_index = identifier_on_line(&unit, "LEADING", 3);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.argument_count == 2);
    CHECK(invocation.arguments[0].tokens.begin ==
          invocation.arguments[0].tokens.end);
    CHECK(slice_equals(range_source(&unit, invocation.arguments[1].tokens), "a"));

    name_index = identifier_on_line(&unit, "TRAILING", 4);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.argument_count == 2);
    CHECK(invocation.arguments[1].tokens.begin ==
          invocation.arguments[1].tokens.end);

    name_index = identifier_on_line(&unit, "DOUBLE", 5);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.argument_count == 3);
    CHECK(invocation.arguments[1].tokens.begin ==
          invocation.arguments[1].tokens.end);

    name_index = identifier_on_line(&unit, "BRACES", 6);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.argument_count == 4);
    for (index = 0; index < invocation.argument_count; ++index) {
        CHECK(slice_equals(range_source(&unit, invocation.arguments[index].tokens),
                           brace_arguments[index]));
    }

    name_index = identifier_on_line(&unit, "LITERALS", 7);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.argument_count == 3);
    for (index = 0; index < invocation.argument_count; ++index) {
        CHECK(slice_equals(range_source(&unit, invocation.arguments[index].tokens),
                           literal_arguments[index]));
    }

    name_index = identifier_on_line(&unit, "SPLICE", 8);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.preprocessing_token_count,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_COMPLETE);
    CHECK(invocation.argument_count == 1);
    CHECK(slice_equals(range_source(&unit, invocation.arguments[0].tokens), "a"));

    CHECK(unit.count == 1);
    name_index = identifier_on_line(&unit, "F", 10);
    CHECK(noc_macro_invocation_parse(&unit,
                                     name_index,
                                     unit.items[0].preprocessing_tokens.end,
                                     &invocation) ==
          NOC_MACRO_INVOCATION_BUILD_OK);
    CHECK(invocation.status == NOC_MACRO_INVOCATION_INCOMPLETE);
    CHECK(invocation.problem_token_index != NOC_TOKEN_INDEX_NONE);
    CHECK(unit.preprocessing_tokens[
              invocation.problem_token_index].token.kind == NOC_TOKEN_INVALID);
    CHECK(noc_macro_invocation_is_valid(&invocation));

    noc_macro_invocation_free(&invocation);
    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

int main(void)
{
    check_names();
    test_complete_and_incomplete_invocations();
    test_bounds_transactionality_and_staleness();
    test_empty_slots_nesting_and_phase2();
    return finish_suite("macro-invocations");
}
