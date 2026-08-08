#include "macro_expansion_test_support.h"

static Noc_Token_Range expression_body(const Macro_Expansion_Fixture *fixture,
                                       size_t directive_index)
{
    return noc_preprocessor_directive_body_tokens(&fixture->input,
                                                  directive_index);
}

static Noc_Preprocessor_Expression_Status evaluate_directive(
    Macro_Expansion_Fixture *fixture,
    size_t directive_index,
    size_t entry_limit,
    bool *value,
    size_t *problem_token_index)
{
    Noc_Token_Range range = expression_body(fixture, directive_index);
    CHECK(range.begin != NOC_TOKEN_INDEX_NONE);
    CHECK(noc_macro_expansion_build_condition(
              &fixture->environment,
              entry_limit,
              &fixture->input,
              range,
              noc_macro_expansion_default_limits(),
              &fixture->expansion) == NOC_MACRO_EXPANSION_OK);
    return noc_preprocessor_expression_evaluate(&fixture->expansion,
                                                value,
                                                problem_token_index);
}

static void test_status_names_and_directive_body_ranges(void)
{
    static const Noc_Preprocessor_Expression_Status statuses[] = {
        NOC_PREPROCESSOR_EXPRESSION_OK,
        NOC_PREPROCESSOR_EXPRESSION_INVALID_ARGUMENT,
        NOC_PREPROCESSOR_EXPRESSION_STALE,
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
        NOC_PREPROCESSOR_EXPRESSION_DIVISION_BY_ZERO,
        NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW,
        NOC_PREPROCESSOR_EXPRESSION_SHIFT_OUT_OF_RANGE,
        NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT,
        NOC_PREPROCESSOR_EXPRESSION_DEPTH_LIMIT,
        NOC_PREPROCESSOR_EXPRESSION_OUT_OF_MEMORY,
    };
    static const char *const names[] = {
        "ok",
        "invalid-argument",
        "stale",
        "malformed",
        "division-by-zero",
        "signed-overflow",
        "shift-out-of-range",
        "target-dependent",
        "depth-limit",
        "out-of-memory",
    };
    Macro_Expansion_Fixture fixture;
    Noc_Token_Range body;
    size_t index;

    for (index = 0; index < sizeof(statuses) / sizeof(statuses[0]); ++index) {
        CHECK(strcmp(noc_preprocessor_expression_status_name(statuses[index]),
                     names[index]) == 0);
    }
    CHECK(strcmp(noc_preprocessor_expression_status_name(
                     (Noc_Preprocessor_Expression_Status)99),
                 "unknown") == 0);

    macro_fixture_init(&fixture,
                       "",
                       "#if  1 /* internal */ + 2  /* trailing */\n"
                       "#else\n"
                       "#endif\n"
                       "#\n");
    CHECK(fixture.input.count == 4);
    body = expression_body(&fixture, 0);
    CHECK(body.begin < body.end);
    CHECK(fixture.input.preprocessing_tokens[body.begin].role ==
          NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY);
    CHECK(fixture.input.preprocessing_tokens[body.end - 1].role ==
          NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY);
    CHECK(noc_slice_equal_cstr(
        fixture.input.preprocessing_tokens[body.end - 1].token.text,
        "2"));
    CHECK(expression_body(&fixture, 1).begin == NOC_TOKEN_INDEX_NONE);
    CHECK(expression_body(&fixture, 2).begin == NOC_TOKEN_INDEX_NONE);
    CHECK(expression_body(&fixture, 3).begin == NOC_TOKEN_INDEX_NONE);
    CHECK(expression_body(&fixture, fixture.input.count).begin ==
          NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_directive_body_tokens(NULL, 0).begin ==
          NOC_TOKEN_INDEX_NONE);

    macro_fixture_deinit(&fixture);
}

static void test_condition_expansion_defined_and_macro_provenance(void)
{
    static const char definitions[] =
        "#define A 3\n"
        "#define ADD(x,y) ((x)+(y))\n"
        "#define defined 99\n"
        "#define FLAG 1\n"
        "#undef FLAG\n"
        "#define ID(x) x\n"
        "#define WRAP(x) ID(x)\n"
        "#define D defined\n"
        "#define CHECK(x) defined(x)\n"
        "#define m_A 1\n"
        "#define CHECK_PASTE(x) defined(m_##x)\n";
    static const char input[] =
        "#if defined(A) && !defined MISSING && ADD(A, 2) == 5 && UNKNOWN == 0\n"
        "#if defined FLAG\n"
        "#if defined(__STDC__) && __STDC_VERSION__ >= 201112L\n"
        "#if de\\\nfined(A)\n"
        "#if ID(defined(A))\n"
        "#if WRAP(defined(A))\n"
        "#if D(A)\n"
        "#if CHECK(A)\n"
        "#if CHECK_PASTE(A)\n";
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion ordinary = {0};
    Noc_Token_Range body;
    bool value = false;
    size_t problem = 123;
    size_t defined_count = 0;
    size_t a_count = 0;
    size_t add_count = 0;
    size_t index;

    macro_fixture_init(&fixture, definitions, input);
    CHECK(fixture.environment.count == 11);
    CHECK(evaluate_directive(&fixture,
                             0,
                             fixture.environment.count,
                             &value,
                             &problem) == NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);
    CHECK(problem == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_macro_expansion_is_valid(&fixture.expansion));
    for (index = 0; index < fixture.expansion.count; ++index) {
        Noc_Token token = fixture.expansion.items[index].token;
        if (noc_token_is_identifier(token, "defined")) defined_count += 1;
        if (noc_token_is_identifier(token, "A")) a_count += 1;
        if (noc_token_is_identifier(token, "ADD")) add_count += 1;
    }
    CHECK(defined_count == 2);
    CHECK(a_count == 1);
    CHECK(add_count == 0);

    CHECK(evaluate_directive(&fixture, 1, 4, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);
    CHECK(evaluate_directive(&fixture, 1, 5, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(!value);
    CHECK(evaluate_directive(&fixture, 2, 5, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);
    CHECK(evaluate_directive(&fixture, 3, 5, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);
    CHECK(evaluate_directive(&fixture, 4, 11, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);
    CHECK(evaluate_directive(&fixture, 5, 11, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);
    CHECK(evaluate_directive(&fixture, 6, 11, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);
    value = true;
    CHECK(evaluate_directive(&fixture, 7, 11, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_MALFORMED);
    CHECK(value);
    CHECK(evaluate_directive(&fixture, 8, 11, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);

    body = expression_body(&fixture, 0);
    CHECK(noc_macro_expansion_build(&fixture.environment,
                                    fixture.environment.count,
                                    &fixture.input,
                                    body,
                                    noc_macro_expansion_default_limits(),
                                    &ordinary) == NOC_MACRO_EXPANSION_OK);
    a_count = 0;
    for (index = 0; index < ordinary.count; ++index) {
        if (noc_token_is_identifier(ordinary.items[index].token, "A")) {
            a_count += 1;
        }
    }
    CHECK(a_count == 0);

    noc_macro_expansion_free(&ordinary);
    macro_fixture_deinit(&fixture);
}

static void test_operator_precedence_literals_and_short_circuit(void)
{
    static const char input[] =
        "#if 2 + 3 * 4 == 14 && (1 << 4) == 16 && 16 >> 2 == 4\n"
        "#if (7 & 3) == 3 && (4 ^ 1) == 5 && (4 | 1) == 5\n"
        "#if 0xffU == 0377UL && ~0U > 0 && (-1 < 1U) == 0\n"
        "#if +3 == 3 && -3 + 4 == 1 && !0 && ~~3 == 3\n"
        "#if '\\0' == 0 && '\\101' == 65 && '\\x41' == 65\n"
        "#if 0 && (1 / 0)\n"
        "#if 1 || (1 / 0)\n"
        "#if 1 ? ('\\101' == 65) : (1 / 0)\n"
        "#if 0 ? L'x' : 7\n"
        "#if UNKNOWN_IDENTIFIER\n"
        "#if 1 ? 0 : 1\n"
        "#if ((1 ? -1 : 1U) < 0) == 0\n"
        "#if (0 ? 1U : -1) > 0\n"
        "#if (1 ? -1 : (1U + 0)) > 0\n"
        "#if (0 ? (+1U) : -1) > 0\n"
        "#if 0 && (1, 2)\n"
        "#if 1 || (1, 2)\n"
        "#if 0 ? (1, 2) : 3\n";
    static const bool expected[] = {
        true, true, true, true, true, false, true, true, true, false, false,
        true, true, true, true, false, true, true,
    };
    Macro_Expansion_Fixture fixture;
    size_t index;

    macro_fixture_init(&fixture, "", input);
    CHECK(fixture.input.count == sizeof(expected) / sizeof(expected[0]));
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        bool value = !expected[index];
        size_t problem = 99;
        CHECK(evaluate_directive(&fixture,
                                 index,
                                 0,
                                 &value,
                                 &problem) ==
              NOC_PREPROCESSOR_EXPRESSION_OK);
        CHECK(value == expected[index]);
        CHECK(problem == NOC_TOKEN_INDEX_NONE);
    }

    macro_fixture_deinit(&fixture);
}

static void test_errors_problem_tokens_and_transactionality(void)
{
    static const char input[] =
        "#if 1\n"
        "#if 1 +\n"
        "#if 1 / 0\n"
        "#if 1 << 999\n"
        "#if -1 >> 1\n"
        "#if 'A'\n"
        "#if L'x'\n"
        "#if 1.0\n"
        "#if defined(1)\n"
        "#if 1 2\n"
        "#if 1 ? (1, 2) : 3\n"
        "#if (1\n"
        "#if defined(A\n"
        "#if 1 ? 2\n";
    static const Noc_Preprocessor_Expression_Status expected[] = {
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
        NOC_PREPROCESSOR_EXPRESSION_DIVISION_BY_ZERO,
        NOC_PREPROCESSOR_EXPRESSION_SHIFT_OUT_OF_RANGE,
        NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT,
        NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT,
        NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT,
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
    };
    Macro_Expansion_Fixture fixture;
    Noc_Macro_Expansion_Token *preserved_items;
    Noc_Macro_Expansion_Frame *preserved_frames;
    Noc_Slice *preserved_spellings;
    size_t preserved_generation;
    size_t index;

    macro_fixture_init(&fixture, "", input);
    CHECK(evaluate_directive(&fixture, 0, 0, &(bool){false}, &(size_t){0}) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        bool value = true;
        size_t problem = NOC_TOKEN_INDEX_NONE;
        CHECK(evaluate_directive(&fixture,
                                 index + 1,
                                 0,
                                 &value,
                                 &problem) == expected[index]);
        CHECK(value);
        CHECK(problem == NOC_TOKEN_INDEX_NONE ||
              problem < fixture.expansion.count);
        if (index >= 10) CHECK(problem == NOC_TOKEN_INDEX_NONE);
    }

    preserved_items = fixture.expansion.items;
    preserved_frames = fixture.expansion.frames;
    preserved_spellings = fixture.expansion.generated_spellings;
    preserved_generation = fixture.expansion.generation;
    {
        Noc_Macro_Expansion_Limits limits = noc_macro_expansion_default_limits();
        Noc_Token_Range body = expression_body(&fixture, 8);
        limits.max_output_tokens = 1;
        CHECK(noc_macro_expansion_build_condition(&fixture.environment,
                                                  0,
                                                  &fixture.input,
                                                  body,
                                                  limits,
                                                  &fixture.expansion) ==
              NOC_MACRO_EXPANSION_OUTPUT_LIMIT);
    }
    CHECK(fixture.expansion.items == preserved_items);
    CHECK(fixture.expansion.frames == preserved_frames);
    CHECK(fixture.expansion.generated_spellings == preserved_spellings);
    CHECK(fixture.expansion.generation == preserved_generation);

    macro_fixture_deinit(&fixture);
}

static void test_overflow_depth_and_staleness(void)
{
    Noc_Buffer input = {0};
    Macro_Expansion_Fixture fixture;
    bool value = true;
    size_t problem = NOC_TOKEN_INDEX_NONE;
    size_t index;

    CHECK(noc_buffer_appendf(&input, "#if %jd + 1\n#if ", INTMAX_MAX));
    for (index = 0; index < 256; ++index) {
        CHECK(noc_buffer_append_cstr(&input, "("));
    }
    CHECK(noc_buffer_append_cstr(&input, "1"));
    for (index = 0; index < 256; ++index) {
        CHECK(noc_buffer_append_cstr(&input, ")"));
    }
    CHECK(noc_buffer_append_cstr(&input, "\n#if "));
    for (index = 0; index < 257; ++index) {
        CHECK(noc_buffer_append_cstr(&input, "("));
    }
    CHECK(noc_buffer_append_cstr(&input, "1"));
    for (index = 0; index < 257; ++index) {
        CHECK(noc_buffer_append_cstr(&input, ")"));
    }
    CHECK(noc_buffer_append_cstr(&input, "\n"));
    CHECK(noc_buffer_terminate(&input));

    macro_fixture_init(&fixture, "", input.items);
    CHECK(evaluate_directive(&fixture, 0, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW);
    CHECK(value);
    CHECK(problem < fixture.expansion.count);
    CHECK(evaluate_directive(&fixture, 1, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK);
    CHECK(value);
    CHECK(problem == NOC_TOKEN_INDEX_NONE);
    CHECK(evaluate_directive(&fixture, 2, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_DEPTH_LIMIT);
    CHECK(problem < fixture.expansion.count);

    CHECK(noc_preprocessor_expression_evaluate(NULL, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_INVALID_ARGUMENT);
    CHECK(noc_preprocessor_expression_evaluate(&fixture.expansion,
                                               NULL,
                                               &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_INVALID_ARGUMENT);
    CHECK(noc_preprocessor_unit_build(&fixture.context,
                                      &fixture.input_snapshot,
                                      NOC_MACROS_FULL,
                                      &fixture.input));
    problem = 17;
    CHECK(noc_preprocessor_expression_evaluate(&fixture.expansion,
                                               &value,
                                               &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_STALE);
    CHECK(problem == NOC_TOKEN_INDEX_NONE);
    CHECK(value);

    macro_fixture_deinit(&fixture);
    noc_buffer_free(&input);
}

static void test_literal_and_arithmetic_boundaries(void)
{
    Noc_Buffer input = {0};
    Macro_Expansion_Fixture fixture;
    uintmax_t above_signed = (uintmax_t)INTMAX_MAX + 1u;
    bool value = false;
    size_t problem = NOC_TOKEN_INDEX_NONE;

    CHECK(noc_buffer_appendf(
        &input,
        "#if %jd\n"
        "#if %ju\n"
        "#if %juU > 0\n"
        "#if 0x%jx > 0\n"
        "#if %juU > 0\n"
        "#if 1uL && 1Lu && 1ULL && 1llU\n"
        "#if 08\n"
        "#if 1UU\n"
        "#if %jd + 1\n"
        "#if (-%jd - 1) / -1\n"
        "#if (-%jd - 1) %% -1\n"
        "#if %jd * 2\n"
        "#if (-%jd - 1) - 1\n"
        "#if (0U - 1U) > 0\n"
        "#if 0 && ((-%jd - 1) / -1)\n"
        "#if '\\177' == 127\n"
        "#if '\\200'\n",
        INTMAX_MAX,
        above_signed,
        above_signed,
        above_signed,
        UINTMAX_MAX,
        INTMAX_MAX,
        INTMAX_MAX,
        INTMAX_MAX,
        INTMAX_MAX,
        INTMAX_MAX,
        INTMAX_MAX));
    CHECK(noc_buffer_terminate(&input));
    macro_fixture_init(&fixture, "", input.items);
    CHECK(fixture.input.count == 17);

    CHECK(evaluate_directive(&fixture, 0, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK && value);
    value = true;
    CHECK(evaluate_directive(&fixture, 1, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_MALFORMED && value);
    CHECK(evaluate_directive(&fixture, 2, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK && value);
    CHECK(evaluate_directive(&fixture, 3, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK && value);
    CHECK(evaluate_directive(&fixture, 4, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK && value);
    CHECK(evaluate_directive(&fixture, 5, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK && value);
    value = true;
    CHECK(evaluate_directive(&fixture, 6, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_MALFORMED && value);
    CHECK(evaluate_directive(&fixture, 7, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_MALFORMED && value);
    CHECK(evaluate_directive(&fixture, 8, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW && value);
    CHECK(evaluate_directive(&fixture, 9, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW && value);
    CHECK(evaluate_directive(&fixture, 10, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW && value);
    CHECK(evaluate_directive(&fixture, 11, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW && value);
    CHECK(evaluate_directive(&fixture, 12, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW && value);
    CHECK(evaluate_directive(&fixture, 13, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK && value);
    CHECK(evaluate_directive(&fixture, 14, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK && !value);
    CHECK(evaluate_directive(&fixture, 15, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_OK && value);
    CHECK(evaluate_directive(&fixture, 16, 0, &value, &problem) ==
          NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT);

    macro_fixture_deinit(&fixture);
    noc_buffer_free(&input);
}

int main(void)
{
    test_status_names_and_directive_body_ranges();
    test_condition_expansion_defined_and_macro_provenance();
    test_operator_precedence_literals_and_short_circuit();
    test_errors_problem_tokens_and_transactionality();
    test_overflow_depth_and_staleness();
    test_literal_and_arithmetic_boundaries();
    return finish_suite("preprocessor-expressions");
}
