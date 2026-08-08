#include "test_support.h"

static Noc_Slice preprocessing_range_source(const Noc_Preprocessor_Unit *unit,
                                            Noc_Token_Range range)
{
    Noc_Slice result = {0};
    const Noc_Token *first;
    const Noc_Token *last;
    if (range.begin == NOC_TOKEN_INDEX_NONE || range.end == NOC_TOKEN_INDEX_NONE ||
        range.begin > range.end || range.end > unit->preprocessing_token_count ||
        range.begin == range.end) {
        return result;
    }
    first = &unit->preprocessing_tokens[range.begin].token;
    last = &unit->preprocessing_tokens[range.end - 1].token;
    result.data = first->text.data;
    result.count = (size_t)(last->text.data + last->text.count - result.data);
    return result;
}

static Noc_Slice macro_name(const Noc_Preprocessor_Unit *unit,
                            const Noc_Macro_Directive *macro)
{
    Noc_Slice empty = {0};
    const Noc_Preprocessing_Token *token =
        noc_preprocessor_token_at(unit, macro->name_token_index);
    return token ? token->token.text : empty;
}

static void test_valid_macro_directives(void)
{
    static const char source[] =
        "#define EMPTY\n"
        "#define OBJECT (x)\n"
        "#define FUNCTION(x, y) ((x) + (y))\n"
        "#define SPLICE\\\n(a, ...) a\n"
        "#define COMMENT/**/(x) x\n"
        "#define ZERO() 0\n"
        "#undef OBJECT\n";
    static const Noc_Macro_Directive_Kind kinds[] = {
        NOC_MACRO_DIRECTIVE_DEFINE_OBJECT,
        NOC_MACRO_DIRECTIVE_DEFINE_OBJECT,
        NOC_MACRO_DIRECTIVE_DEFINE_FUNCTION,
        NOC_MACRO_DIRECTIVE_DEFINE_FUNCTION,
        NOC_MACRO_DIRECTIVE_DEFINE_OBJECT,
        NOC_MACRO_DIRECTIVE_DEFINE_FUNCTION,
        NOC_MACRO_DIRECTIVE_UNDEF,
    };
    static const char *const names[] = {
        "EMPTY", "OBJECT", "FUNCTION", "SPLICE", "COMMENT", "ZERO", "OBJECT",
    };
    static const size_t parameter_counts[] = {0, 0, 2, 2, 0, 0, 0};
    static const char *const replacements[] = {
        "", "(x)", "((x) + (y))", "a", "(x) x", "0", NULL,
    };
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Diagnostic_State diagnostics = {0};
    size_t index;
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "valid-macros.h",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_TRUSTED_ONLY,
                                      &unit));
    CHECK(noc_preprocessor_unit_is_valid(&unit));
    CHECK(unit.count == 7);
    CHECK(unit.macro_directive_count == 7);
    CHECK(unit.invalid_macro_directive_count == 0);
    CHECK(strcmp(noc_macro_directive_kind_name(
                     NOC_MACRO_DIRECTIVE_DEFINE_OBJECT),
                 "object-define") == 0);
    CHECK(strcmp(noc_macro_directive_kind_name(
                     NOC_MACRO_DIRECTIVE_DEFINE_FUNCTION),
                 "function-define") == 0);
    CHECK(strcmp(noc_macro_directive_kind_name(NOC_MACRO_DIRECTIVE_UNDEF),
                 "undef") == 0);
    CHECK(strcmp(noc_macro_directive_kind_name((Noc_Macro_Directive_Kind)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_macro_directive_status_name(
                     NOC_MACRO_DIRECTIVE_STATUS_VALID),
                 "valid") == 0);
    CHECK(strcmp(noc_macro_directive_status_name(
                     NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE),
                 "incomplete") == 0);
    CHECK(strcmp(noc_macro_directive_status_name(
                     NOC_MACRO_DIRECTIVE_STATUS_MALFORMED),
                 "malformed") == 0);
    CHECK(strcmp(noc_macro_directive_status_name(
                     (Noc_Macro_Directive_Status)99),
                 "unknown") == 0);

    for (index = 0; index < unit.macro_directive_count; ++index) {
        const Noc_Macro_Directive *macro = noc_macro_directive_at(&unit, index);
        CHECK(macro != NULL);
        if (!macro) continue;
        CHECK(macro->kind == kinds[index]);
        CHECK(macro->status == NOC_MACRO_DIRECTIVE_STATUS_VALID);
        CHECK(macro->directive_index == index);
        CHECK(slice_equals(macro_name(&unit, macro), names[index]));
        CHECK(macro->parameter_count == parameter_counts[index]);
        CHECK(unit.items[index].macro_directive_index == index);
        if (replacements[index]) {
            CHECK(slice_equals(preprocessing_range_source(
                                   &unit,
                                   macro->replacement_tokens),
                               replacements[index]));
        } else {
            CHECK(macro->replacement_tokens.begin == NOC_TOKEN_INDEX_NONE);
            CHECK(macro->replacement_tokens.end == NOC_TOKEN_INDEX_NONE);
        }
    }
    CHECK(noc_macro_directive_at(&unit, unit.macro_directive_count) == NULL);
    CHECK(unit.macro_directives[0].replacement_tokens.begin ==
          unit.macro_directives[0].replacement_tokens.end);
    CHECK(slice_equals(preprocessing_range_source(
                           &unit,
                           unit.macro_directives[2].parameter_tokens),
                       "x, y"));
    CHECK(slice_equals(preprocessing_range_source(
                           &unit,
                           unit.macro_directives[3].parameter_tokens),
                       "a, ..."));
    CHECK(unit.macro_directives[3].variadic);
    CHECK(noc_macro_parameter_at(&unit, &unit.macro_directives[2], 0) != NULL);
    CHECK(slice_equals(unit.preprocessing_tokens[
                           noc_macro_parameter_at(
                               &unit,
                               &unit.macro_directives[2],
                               0)->token_index].token.text,
                       "x"));
    CHECK(slice_equals(unit.preprocessing_tokens[
                           noc_macro_parameter_at(
                               &unit,
                               &unit.macro_directives[2],
                               1)->token_index].token.text,
                       "y"));
    CHECK(!noc_macro_parameter_at(&unit,
                                  &unit.macro_directives[3],
                                  0)->variadic);
    CHECK(noc_macro_parameter_at(&unit,
                                 &unit.macro_directives[3],
                                 1)->variadic);
    CHECK(slice_equals(unit.preprocessing_tokens[
                           noc_macro_parameter_at(
                               &unit,
                               &unit.macro_directives[3],
                               1)->token_index].token.text,
                       "..."));
    CHECK(noc_macro_parameter_at(&unit, &unit.macro_directives[3], 2) == NULL);
    CHECK(noc_macro_parameter_at(&unit, NULL, 0) == NULL);
    CHECK(noc_preprocessor_unit_validate_macro_directives(&context, &unit));
    CHECK(diagnostics.errors == 0);

    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_recoverable_invalid_macro_directives(void)
{
    static const char source[] =
        "#define\n"
        "#define 123 value\n"
        "#define F(\n"
        "#define G(x,\n"
        "#define H(x y) x\n"
        "#define I(x,,y) x\n"
        "#define J(x,...) x\n"
        "#define K(args...) args\n"
        "#define L \"unfinished\n"
        "#undef\n"
        "#undef X extra\n";
    static const Noc_Macro_Directive_Status statuses[] = {
        NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
        NOC_MACRO_DIRECTIVE_STATUS_MALFORMED,
        NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
        NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
        NOC_MACRO_DIRECTIVE_STATUS_MALFORMED,
        NOC_MACRO_DIRECTIVE_STATUS_MALFORMED,
        NOC_MACRO_DIRECTIVE_STATUS_VALID,
        NOC_MACRO_DIRECTIVE_STATUS_MALFORMED,
        NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
        NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
        NOC_MACRO_DIRECTIVE_STATUS_MALFORMED,
    };
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Diagnostic_State diagnostics = {0};
    size_t index;
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "invalid-macros.h",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(noc_preprocessor_unit_is_valid(&unit));
    CHECK(context.error_count == 0);
    CHECK(diagnostics.errors == 0);
    CHECK(unit.macro_directive_count ==
          sizeof(statuses) / sizeof(statuses[0]));
    CHECK(unit.invalid_macro_directive_count == 10);
    for (index = 0; index < unit.macro_directive_count; ++index) {
        const Noc_Macro_Directive *macro = noc_macro_directive_at(&unit, index);
        CHECK(macro != NULL);
        if (macro) CHECK(macro->status == statuses[index]);
    }
    CHECK(unit.macro_directives[2].kind ==
          NOC_MACRO_DIRECTIVE_DEFINE_FUNCTION);
    CHECK(unit.macro_directives[3].parameter_count == 1);
    CHECK(slice_equals(preprocessing_range_source(
                           &unit,
                           unit.macro_directives[4].replacement_tokens),
                       "x"));
    CHECK(slice_equals(preprocessing_range_source(
                           &unit,
                           unit.macro_directives[5].replacement_tokens),
                       "x"));
    CHECK(unit.macro_directives[6].variadic);
    CHECK(unit.macro_directives[6].parameter_count == 2);
    CHECK(!unit.macro_directives[7].variadic);
    CHECK(unit.macro_directives[9].kind == NOC_MACRO_DIRECTIVE_UNDEF);
    CHECK(!noc_preprocessor_unit_validate_macro_directives(&context, &unit));
    CHECK(context.error_count == 10);
    CHECK(diagnostics.errors == 10);
    CHECK(strcmp(diagnostics.last_path, "invalid-macros.h") == 0);
    CHECK(strstr(diagnostics.last_message, "malformed #undef") != NULL);

    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_incomplete_tokens_and_synchronized_recovery(void)
{
    static const char source[] =
        "#define RECOVER(x y) replacement\n"
        "#define STRING \"unfinished\n"
        "#define CHARACTER 'unfinished\n"
        "#define COMMENT /* unfinished";
    static const Noc_Macro_Directive_Status statuses[] = {
        NOC_MACRO_DIRECTIVE_STATUS_MALFORMED,
        NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
        NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
        NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
    };
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Diagnostic_State diagnostics = {0};
    size_t index;

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "incomplete-macros.h",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(noc_preprocessor_unit_is_valid(&unit));
    CHECK(unit.count == 4);
    CHECK(unit.macro_directive_count == 4);
    CHECK(unit.invalid_macro_directive_count == 4);
    CHECK(context.error_count == 0);
    CHECK(diagnostics.errors == 0);
    for (index = 0; index < unit.macro_directive_count; ++index) {
        const Noc_Macro_Directive *macro = noc_macro_directive_at(&unit, index);
        CHECK(macro != NULL);
        if (macro) CHECK(macro->status == statuses[index]);
    }
    CHECK(slice_equals(preprocessing_range_source(
                           &unit,
                           unit.macro_directives[0].replacement_tokens),
                       "replacement"));
    CHECK(unit.macro_directives[1].problem_token_index != NOC_TOKEN_INDEX_NONE);
    CHECK(unit.macro_directives[2].problem_token_index != NOC_TOKEN_INDEX_NONE);
    CHECK(unit.macro_directives[3].problem_token_index != NOC_TOKEN_INDEX_NONE);
    CHECK(!noc_preprocessor_unit_validate_macro_directives(&context, &unit));
    CHECK(context.error_count == 4);
    CHECK(diagnostics.errors == 4);
    CHECK(strcmp(diagnostics.last_path, "incomplete-macros.h") == 0);
    CHECK(strstr(diagnostics.last_message, "incomplete #define") != NULL);

    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_macro_accessor_provenance(void)
{
    static const char left_source[] = "#define LEFT(x) x\n";
    static const char right_source[] = "#define RIGHT(y) y\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot left_snapshot = {0};
    Noc_Document_Snapshot right_snapshot = {0};
    Noc_Preprocessor_Unit left = {0};
    Noc_Preprocessor_Unit right = {0};
    const Noc_Macro_Directive *left_macro;
    const Noc_Macro_Directive *right_macro;

    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "left.h",
                                      left_source,
                                      sizeof(left_source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &left_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_workspace_open_document(&workspace,
                                      "right.h",
                                      right_source,
                                      sizeof(right_source) - 1,
                                      NOC_SOURCE_CLASS_TRUSTED,
                                      &right_snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &left_snapshot,
                                      NOC_MACROS_FULL,
                                      &left));
    CHECK(noc_preprocessor_unit_build(&context,
                                      &right_snapshot,
                                      NOC_MACROS_FULL,
                                      &right));
    left_macro = noc_macro_directive_at(&left, 0);
    right_macro = noc_macro_directive_at(&right, 0);
    CHECK(left_macro != NULL && right_macro != NULL);
    CHECK(noc_macro_parameter_at(&left, left_macro, 0) != NULL);
    CHECK(noc_macro_parameter_at(&right, right_macro, 0) != NULL);
    CHECK(noc_macro_parameter_at(&right, left_macro, 0) == NULL);
    CHECK(noc_macro_parameter_at(&left, right_macro, 0) == NULL);

    noc_preprocessor_unit_free(&right);
    noc_preprocessor_unit_free(&left);
    noc_document_snapshot_free(&right_snapshot);
    noc_document_snapshot_free(&left_snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

int main(void)
{
    test_valid_macro_directives();
    test_recoverable_invalid_macro_directives();
    test_incomplete_tokens_and_synchronized_recovery();
    test_macro_accessor_provenance();
    return finish_suite("macro-directives");
}
