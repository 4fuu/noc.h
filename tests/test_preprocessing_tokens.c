#include "test_support.h"

static void test_preprocessing_token_view(void)
{
    static const char source[] =
        "int value;\n"
        "# /*lead*/ define VALUE(x) ((x) + @1) // tail\r\n"
        "#include <sys/types.h>\n"
        "#include \"local.h\"\n"
        "#include <>\n"
        "#include \"\"\n"
        "#include L\"wide.h\"\n"
        "#include u8\"utf.h\"\n"
        "%:pragma once\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    size_t source_offset = 0;
    size_t header_name_count = 0;
    size_t prefixed_literal_count = 0;
    size_t other_count = 0;
    size_t index;
    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "tokens.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(noc_preprocessor_unit_is_valid(&unit));
    CHECK(unit.count == 8);
    CHECK(unit.preprocessing_token_count > unit.stream.count);
    CHECK(strcmp(noc_preprocessing_token_role_name(
                     NOC_PREPROCESSING_TOKEN_SOURCE),
                 "source") == 0);
    CHECK(strcmp(noc_preprocessing_token_role_name(
                     NOC_PREPROCESSING_TOKEN_DIRECTIVE_MARKER),
                 "directive-marker") == 0);
    CHECK(strcmp(noc_preprocessing_token_role_name(
                     NOC_PREPROCESSING_TOKEN_DIRECTIVE_KEYWORD),
                 "directive-keyword") == 0);
    CHECK(strcmp(noc_preprocessing_token_role_name(
                     NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY),
                 "directive-body") == 0);
    CHECK(strcmp(noc_preprocessing_token_role_name(
                     NOC_PREPROCESSING_TOKEN_DIRECTIVE_TRIVIA),
                 "directive-trivia") == 0);
    CHECK(strcmp(noc_preprocessing_token_role_name(
                     (Noc_Preprocessing_Token_Role)99),
                 "unknown") == 0);
    CHECK(strcmp(noc_token_kind_name(NOC_TOKEN_HEADER_NAME), "header name") == 0);
    CHECK(strcmp(noc_token_kind_name(NOC_TOKEN_OTHER),
                 "non-whitespace character") == 0);

    for (index = 0; index < unit.preprocessing_token_count; ++index) {
        const Noc_Preprocessing_Token *preprocessing_token =
            noc_preprocessor_token_at(&unit, index);
        CHECK(preprocessing_token != NULL);
        if (!preprocessing_token) continue;
        CHECK(preprocessing_token->token.kind != NOC_TOKEN_PREPROCESSOR);
        CHECK(preprocessing_token->token.location.offset == source_offset);
        CHECK(preprocessing_token->token.text.data ==
              unit.stream.source + source_offset);
        CHECK(strcmp(preprocessing_token->token.location.path, "tokens.c") == 0);
        source_offset += preprocessing_token->token.text.count;
        if (preprocessing_token->token.kind == NOC_TOKEN_HEADER_NAME) {
            CHECK(preprocessing_token->role ==
                  NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY);
            CHECK(slice_equals(preprocessing_token->token.text, "<sys/types.h>") ||
                  slice_equals(preprocessing_token->token.text, "\"local.h\"") ||
                  slice_equals(preprocessing_token->token.text, "<>") ||
                  slice_equals(preprocessing_token->token.text, "\"\""));
            header_name_count += 1;
        }
        if (preprocessing_token->token.kind == NOC_TOKEN_STRING &&
            (slice_equals(preprocessing_token->token.text, "L\"wide.h\"") ||
             slice_equals(preprocessing_token->token.text, "u8\"utf.h\""))) {
            prefixed_literal_count += 1;
        }
        if (preprocessing_token->token.kind == NOC_TOKEN_OTHER) {
            CHECK(slice_equals(preprocessing_token->token.text, "@"));
            other_count += 1;
        }
        if (preprocessing_token->directive_index == NOC_TOKEN_INDEX_NONE) {
            CHECK(preprocessing_token->role == NOC_PREPROCESSING_TOKEN_SOURCE);
        } else {
            CHECK(preprocessing_token->directive_index < unit.count);
        }
    }
    CHECK(source_offset == sizeof(source) - 1);
    CHECK(header_name_count == 4);
    CHECK(prefixed_literal_count == 2);
    CHECK(other_count == 1);
    CHECK(unit.preprocessing_tokens[
              unit.preprocessing_token_count - 1].token.kind == NOC_TOKEN_EOF);
    CHECK(unit.preprocessing_tokens[
              unit.preprocessing_token_count - 1].directive_index ==
          NOC_TOKEN_INDEX_NONE);
    CHECK(noc_preprocessor_token_at(&unit,
                                    unit.preprocessing_token_count) == NULL);

    for (index = 0; index < unit.count; ++index) {
        const Noc_Preprocessor_Directive *directive =
            noc_preprocessor_directive_at(&unit, index);
        const Noc_Preprocessing_Token *first;
        const Noc_Preprocessing_Token *last;
        size_t token_index;
        CHECK(directive != NULL);
        if (!directive) continue;
        CHECK(directive->preprocessing_tokens.begin <
              directive->preprocessing_tokens.end);
        CHECK(directive->preprocessing_tokens.end <=
              unit.preprocessing_token_count);
        first = noc_preprocessor_token_at(&unit,
                                          directive->preprocessing_tokens.begin);
        last = noc_preprocessor_token_at(&unit,
                                         directive->preprocessing_tokens.end - 1);
        CHECK(first != NULL && last != NULL);
        if (first && last) {
            Noc_Slice spelling;
            spelling.data = first->token.text.data;
            spelling.count = (size_t)(last->token.text.data +
                                      last->token.text.count -
                                      spelling.data);
            CHECK(noc_slice_equal(spelling, directive->spelling));
            CHECK(first->role == NOC_PREPROCESSING_TOKEN_DIRECTIVE_MARKER);
        }
        for (token_index = directive->preprocessing_tokens.begin;
             token_index < directive->preprocessing_tokens.end;
             ++token_index) {
            CHECK(unit.preprocessing_tokens[token_index].directive_index == index);
        }
    }

    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_multiline_directive_tokens(void)
{
    static const char source[] =
        "#define VALUE /* lf\ncomment */ 1\n"
        "#include /* crlf\r\ncomment */ <x//y/*z*/.h>\r\n"
        "#include <split\\\r\n/name.h>\n"
        "#include \"quoted\\\r\n/name.h\"\n"
        "#define caf\\u00E9 1\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    size_t block_comment_count = 0;
    size_t header_name_count = 0;
    size_t ucn_identifier_count = 0;
    size_t index;
    noc_context_init(&context);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "multiline.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(unit.count == 5);
    for (index = 0; index < unit.preprocessing_token_count; ++index) {
        const Noc_Preprocessing_Token *token =
            noc_preprocessor_token_at(&unit, index);
        CHECK(token != NULL);
        if (!token) continue;
        CHECK(token->token.kind != NOC_TOKEN_INVALID);
        if (token->token.kind == NOC_TOKEN_BLOCK_COMMENT) {
            CHECK(token->role == NOC_PREPROCESSING_TOKEN_DIRECTIVE_TRIVIA);
            CHECK(token->directive_index < 2);
            block_comment_count += 1;
        } else if (token->token.kind == NOC_TOKEN_HEADER_NAME) {
            CHECK(token->role == NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY);
            CHECK(token->directive_index >= 1 && token->directive_index <= 3);
            header_name_count += 1;
        } else if (token->token.kind == NOC_TOKEN_IDENTIFIER &&
                   slice_equals(token->token.text, "caf\\u00E9")) {
            CHECK(token->directive_index == 4);
            ucn_identifier_count += 1;
        }
        if (token->token.kind == NOC_TOKEN_NUMBER &&
            slice_equals(token->token.text, "1") &&
            token->directive_index == 0) {
            CHECK(token->token.location.line == 2);
        }
    }
    CHECK(block_comment_count == 2);
    CHECK(header_name_count == 3);
    CHECK(ucn_identifier_count == 1);
    CHECK(slice_equals(unit.items[0].spelling,
                       "#define VALUE /* lf\ncomment */ 1\n"));
    CHECK(slice_equals(unit.items[1].spelling,
                       "#include /* crlf\r\ncomment */ <x//y/*z*/.h>\r\n"));

    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

static void test_incomplete_directive_view(void)
{
    static const char source[] = "#include \"unfinished\n";
    Noc_Context context;
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Diagnostic_State diagnostics = {0};
    size_t invalid_count = 0;
    size_t header_name_count = 0;
    size_t index;
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      "incomplete.c",
                                      source,
                                      sizeof(source) - 1,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_preprocessor_unit_build(&context,
                                      &snapshot,
                                      NOC_MACROS_FULL,
                                      &unit));
    CHECK(noc_preprocessor_unit_is_valid(&unit));
    CHECK(unit.count == 1);
    for (index = unit.items[0].preprocessing_tokens.begin;
         index < unit.items[0].preprocessing_tokens.end;
         ++index) {
        if (unit.preprocessing_tokens[index].token.kind == NOC_TOKEN_INVALID) {
            CHECK(unit.preprocessing_tokens[index].role ==
                  NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY);
            CHECK(slice_equals(unit.preprocessing_tokens[index].token.text,
                               "\"unfinished"));
            invalid_count += 1;
        } else if (unit.preprocessing_tokens[index].token.kind ==
                   NOC_TOKEN_HEADER_NAME) {
            header_name_count += 1;
        }
    }
    CHECK(invalid_count == 1);
    CHECK(header_name_count == 0);
    CHECK(context.error_count == 0);
    CHECK(diagnostics.errors == 0);

    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    noc_context_deinit(&context);
}

int main(void)
{
    test_preprocessing_token_view();
    test_multiline_directive_tokens();
    test_incomplete_directive_view();
    return finish_suite("preprocessing-tokens");
}
