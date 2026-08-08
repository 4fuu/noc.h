#define NOC_IMPLEMENTATION
#include "noc.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_CHECK(condition) do { if (!(condition)) abort(); } while (0)

typedef struct {
    unsigned int selector;
} Fuzz_State;

static void ignore_diagnostic(void *user_data, const Noc_Diagnostic *diagnostic)
{
    (void)user_data;
    (void)diagnostic;
}

static bool expand_fuzz(Noc_Rewriter *rewriter,
                        const Noc_Rule *rule,
                        void *user_data)
{
    Fuzz_State *state = (Fuzz_State *)user_data;
    const Noc_Token_Stream *stream = noc_rw_token_stream(rewriter);
    Noc_Token_Range remaining = noc_rw_remaining_range(rewriter);
    const Noc_Token *next;
    (void)rule;
    FUZZ_CHECK(stream != NULL);
    FUZZ_CHECK(noc_token_range_is_valid(stream, remaining));
    (void)noc_rw_source_path(rewriter);
    (void)noc_rw_trigger_location(rewriter);
    (void)noc_rw_peek_raw(rewriter, state->selector % 4u);
    next = noc_rw_peek(rewriter, 0);

    switch (state->selector % 6u) {
    case 0: {
        Noc_Token token;
        (void)noc_rw_take_raw(rewriter, &token);
        break;
    }
    case 1:
        (void)noc_rw_match_punct(rewriter, "(");
        (void)noc_rw_match_identifier(rewriter, "value");
        break;
    case 2:
        if (next && noc_token_is_punct(*next, "(")) {
            Noc_Slice inside;
            if (!noc_rw_capture_balanced(rewriter, "(", ")", &inside)) return false;
            return noc_rw_emit_slice(rewriter, inside);
        }
        break;
    case 3: {
        const Noc_Syntax_Tree *tree = noc_rw_syntax_tree(rewriter);
        if (tree) {
            size_t cursor = remaining.begin;
            while (cursor < remaining.end &&
                   noc_token_is_trivia(stream->items[cursor])) {
                cursor += 1;
            }
            if (cursor < remaining.end) {
                size_t node = noc_syntax_node_at_token(tree, cursor);
                const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
                if (syntax && syntax->range.begin == cursor) {
                    size_t taken = NOC_SYNTAX_NONE;
                    FUZZ_CHECK(noc_rw_take_syntax(rewriter, syntax->kind, &taken));
                    FUZZ_CHECK(taken == node);
                }
            }
        }
        break;
    }
    case 4:
        FUZZ_CHECK(noc_rw_consume_range(
            rewriter,
            (Noc_Token_Range){remaining.begin, remaining.begin}));
        break;
    case 5:
        if (next && next->kind == NOC_TOKEN_IDENTIFIER) {
            Noc_Buffer logical = {0};
            Noc_Token token;
            FUZZ_CHECK(noc_token_logical_text(*next, &logical));
            FUZZ_CHECK(noc_rw_expect_identifier(rewriter, logical.items, &token));
            FUZZ_CHECK(noc_token_is_identifier(token, logical.items));
            noc_buffer_free(&logical);
        }
        break;
    }
    return noc_rw_emit_cstr(rewriter, "0");
}

static void fuzz_standalone_lexer(const uint8_t *data, size_t size)
{
    const char *source = (const char *)data;
    Noc_Lexer lexer;
    size_t offset = 0;
    size_t steps = 0;
    noc_lexer_init(&lexer, "fuzz.c", source, size);
    for (;;) {
        Noc_Token token = noc_lexer_next(&lexer);
        FUZZ_CHECK(token.location.offset == offset);
        FUZZ_CHECK(token.text.data == source + offset);
        FUZZ_CHECK(token.text.count <= size - offset);
        offset += token.text.count;
        steps += 1;
        FUZZ_CHECK(steps <= size + 3);
        if (token.kind == NOC_TOKEN_EOF) {
            FUZZ_CHECK(offset == size);
            break;
        }
    }
}

static void fuzz_workspace(const uint8_t *data, size_t size, unsigned int selector)
{
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot current = {0};
    Noc_Document_Snapshot original = {0};
    Noc_Slice source;
    size_t offsets[3];
    size_t index;
    noc_workspace_init(&workspace);
    FUZZ_CHECK(noc_workspace_open_document(
                   &workspace,
                   "memory://fuzz.c",
                   (const char *)data,
                   size,
                   (Noc_Source_Class)(selector % 4u),
                   &current) == NOC_WORKSPACE_OK);
    FUZZ_CHECK(noc_document_snapshot_clone(&current, &original) == NOC_WORKSPACE_OK);
    offsets[0] = 0;
    offsets[1] = size;
    offsets[2] = size == 0 ? 0 : selector % (size + 1);
    for (index = 0; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
        Noc_Location location;
        size_t round_trip = SIZE_MAX;
        FUZZ_CHECK(noc_document_snapshot_location(&current,
                                                  offsets[index],
                                                  &location) == NOC_WORKSPACE_OK);
        FUZZ_CHECK(noc_document_snapshot_offset(&current,
                                                location.line,
                                                location.column,
                                                &round_trip) == NOC_WORKSPACE_OK);
        FUZZ_CHECK(round_trip == offsets[index]);
    }
    source = noc_document_snapshot_source(&current);
    FUZZ_CHECK(noc_workspace_update_document(&workspace,
                                             &current,
                                             source.data + size / 2,
                                             size - size / 2,
                                             &current) == NOC_WORKSPACE_OK);
    FUZZ_CHECK(noc_document_snapshot_generation(&current) == 2);
    FUZZ_CHECK(noc_document_snapshot_source(&original).count == size);
    FUZZ_CHECK(noc_workspace_close_document(&workspace, &current) == NOC_WORKSPACE_OK);
    noc_workspace_deinit(&workspace);
    noc_document_snapshot_free(&original);
    noc_document_snapshot_free(&current);
}

static void fuzz_preprocessor(Noc_Context *context,
                              const uint8_t *data,
                              size_t size,
                              unsigned int selector)
{
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_Preprocessor_Unit unit = {0};
    Noc_Macro_Environment environment = {0};
    Noc_Macro_Environment conditional_initial = {0};
    Noc_Macro_Expansion expansion = {0};
    Noc_Macro_Invocation invocation = {0};
    Noc_Preprocessor_Conditional_Groups groups = {0};
    size_t index;
    noc_workspace_init(&workspace);
    FUZZ_CHECK(noc_workspace_open_document(
                   &workspace,
                   "memory://preprocessor-fuzz.c",
                   (const char *)data,
                   size,
                   (Noc_Source_Class)(selector % 4u),
                   &snapshot) == NOC_WORKSPACE_OK);
    if (noc_preprocessor_unit_build(context,
                                    &snapshot,
                                    (Noc_Macro_Policy)(selector % 4u),
                                    &unit)) {
        size_t source_offset = 0;
        FUZZ_CHECK(noc_preprocessor_unit_is_valid(&unit));
        for (index = 0; index < unit.preprocessing_token_count; ++index) {
            const Noc_Preprocessing_Token *token =
                noc_preprocessor_token_at(&unit, index);
            Noc_Location expected_location;
            FUZZ_CHECK(token != NULL);
            FUZZ_CHECK(token->token.kind != NOC_TOKEN_PREPROCESSOR);
            FUZZ_CHECK(token->token.location.offset == source_offset);
            FUZZ_CHECK(token->token.text.data == unit.stream.source + source_offset);
            FUZZ_CHECK(token->token.text.count <= size - source_offset);
            FUZZ_CHECK(token->role >= NOC_PREPROCESSING_TOKEN_SOURCE &&
                       token->role <= NOC_PREPROCESSING_TOKEN_DIRECTIVE_TRIVIA);
            FUZZ_CHECK(noc_document_snapshot_location(&snapshot,
                                                      source_offset,
                                                      &expected_location) ==
                       NOC_WORKSPACE_OK);
            FUZZ_CHECK(token->token.location.line == expected_location.line);
            FUZZ_CHECK(token->token.location.column == expected_location.column);
            source_offset += token->token.text.count;
            if (token->directive_index == NOC_TOKEN_INDEX_NONE) {
                FUZZ_CHECK(token->role == NOC_PREPROCESSING_TOKEN_SOURCE);
            } else {
                FUZZ_CHECK(token->directive_index < unit.count);
            }
        }
        FUZZ_CHECK(source_offset == size);
        FUZZ_CHECK(unit.preprocessing_tokens[
                       unit.preprocessing_token_count - 1].token.kind ==
                   NOC_TOKEN_EOF);
        FUZZ_CHECK(unit.preprocessing_tokens[
                       unit.preprocessing_token_count - 1].token.text.count == 0);
        FUZZ_CHECK(unit.preprocessing_tokens[
                       unit.preprocessing_token_count - 1].role ==
                   NOC_PREPROCESSING_TOKEN_SOURCE);
        FUZZ_CHECK(unit.preprocessing_tokens[
                       unit.preprocessing_token_count - 1].directive_index ==
                   NOC_TOKEN_INDEX_NONE);
        if (unit.preprocessing_token_count > 1) {
            size_t candidate = selector % (unit.preprocessing_token_count - 1);
            size_t visited;
            for (visited = 0;
                 visited < unit.preprocessing_token_count - 1;
                 ++visited) {
                Noc_Macro_Invocation_Build_Status status;
                size_t limit;
                index = (candidate + visited) %
                        (unit.preprocessing_token_count - 1);
                if (unit.preprocessing_tokens[index].token.kind !=
                    NOC_TOKEN_IDENTIFIER) {
                    continue;
                }
                limit = index + 1 +
                        selector % (unit.preprocessing_token_count - index);
                status = noc_macro_invocation_parse(&unit,
                                                    index,
                                                    limit,
                                                    &invocation);
                FUZZ_CHECK(status == NOC_MACRO_INVOCATION_BUILD_OK ||
                           status == NOC_MACRO_INVOCATION_BUILD_OUT_OF_MEMORY);
                if (status == NOC_MACRO_INVOCATION_BUILD_OK) {
                    size_t argument_index;
                    FUZZ_CHECK(noc_macro_invocation_is_valid(&invocation));
                    for (argument_index = 0;
                         argument_index < invocation.argument_count;
                         ++argument_index) {
                        FUZZ_CHECK(noc_macro_invocation_argument_at(
                                       &invocation,
                                       argument_index) != NULL);
                    }
                }
                break;
            }
        }
        for (index = 0; index < unit.count; ++index) {
            const Noc_Preprocessor_Directive *directive =
                noc_preprocessor_directive_at(&unit, index);
            const Noc_Preprocessing_Token *first;
            const Noc_Preprocessing_Token *last;
            size_t directive_token_index;
            Noc_Slice spelling;
            FUZZ_CHECK(directive != NULL);
            FUZZ_CHECK(directive->token_index < unit.stream.count);
            FUZZ_CHECK(unit.stream.items[directive->token_index].kind ==
                       NOC_TOKEN_PREPROCESSOR);
            FUZZ_CHECK(directive->preprocessing_tokens.begin <
                       directive->preprocessing_tokens.end);
            FUZZ_CHECK(directive->preprocessing_tokens.end <=
                       unit.preprocessing_token_count);
            first = noc_preprocessor_token_at(
                &unit,
                directive->preprocessing_tokens.begin);
            last = noc_preprocessor_token_at(
                &unit,
                directive->preprocessing_tokens.end - 1);
            FUZZ_CHECK(first != NULL && last != NULL);
            spelling.data = first->token.text.data;
            spelling.count = (size_t)(last->token.text.data +
                                      last->token.text.count -
                                      spelling.data);
            FUZZ_CHECK(noc_slice_equal(spelling, directive->spelling));
            for (directive_token_index = directive->preprocessing_tokens.begin;
                 directive_token_index < directive->preprocessing_tokens.end;
                 ++directive_token_index) {
                FUZZ_CHECK(unit.preprocessing_tokens[
                               directive_token_index].directive_index == index);
            }
        }
        {
            size_t invalid_macro_count = 0;
            size_t effective_macro_count = 0;
            for (index = 0; index < unit.macro_directive_count; ++index) {
                const Noc_Macro_Directive *macro =
                    noc_macro_directive_at(&unit, index);
                const Noc_Preprocessor_Directive *directive;
                size_t parameter_index;
                bool found_variadic = false;
                FUZZ_CHECK(macro != NULL);
                FUZZ_CHECK(macro->kind >= NOC_MACRO_DIRECTIVE_DEFINE_OBJECT &&
                           macro->kind <= NOC_MACRO_DIRECTIVE_UNDEF);
                FUZZ_CHECK(macro->status >= NOC_MACRO_DIRECTIVE_STATUS_VALID &&
                           macro->status <= NOC_MACRO_DIRECTIVE_STATUS_MALFORMED);
                FUZZ_CHECK(macro->directive_index < unit.count);
                directive = &unit.items[macro->directive_index];
                FUZZ_CHECK(directive->kind == NOC_PREPROCESSOR_DIRECTIVE_DEFINE ||
                           directive->kind == NOC_PREPROCESSOR_DIRECTIVE_UNDEF);
                FUZZ_CHECK(directive->macro_directive_index == index);
                FUZZ_CHECK(macro->parameter_begin <= unit.macro_parameter_count);
                FUZZ_CHECK(macro->parameter_count <=
                           unit.macro_parameter_count - macro->parameter_begin);
                if (macro->name_token_index != NOC_TOKEN_INDEX_NONE) {
                    FUZZ_CHECK(macro->name_token_index <
                               unit.preprocessing_token_count);
                }
                if (macro->parameter_tokens.begin != NOC_TOKEN_INDEX_NONE) {
                    FUZZ_CHECK(macro->parameter_tokens.begin <=
                               macro->parameter_tokens.end);
                    FUZZ_CHECK(macro->parameter_tokens.end <=
                               unit.preprocessing_token_count);
                } else {
                    FUZZ_CHECK(macro->parameter_tokens.end == NOC_TOKEN_INDEX_NONE);
                }
                if (macro->replacement_tokens.begin != NOC_TOKEN_INDEX_NONE) {
                    FUZZ_CHECK(macro->replacement_tokens.begin <=
                               macro->replacement_tokens.end);
                    FUZZ_CHECK(macro->replacement_tokens.end <=
                               unit.preprocessing_token_count);
                } else {
                    FUZZ_CHECK(macro->replacement_tokens.end ==
                               NOC_TOKEN_INDEX_NONE);
                }
                for (parameter_index = 0;
                     parameter_index < macro->parameter_count;
                     ++parameter_index) {
                    const Noc_Macro_Parameter *parameter =
                        noc_macro_parameter_at(&unit, macro, parameter_index);
                    FUZZ_CHECK(parameter != NULL);
                    FUZZ_CHECK(parameter->token_index <
                               unit.preprocessing_token_count);
                    FUZZ_CHECK(unit.preprocessing_tokens[
                                   parameter->token_index].token.kind ==
                                   NOC_TOKEN_IDENTIFIER ||
                               noc_token_is_punct(unit.preprocessing_tokens[
                                                      parameter->token_index].token,
                                                  "..."));
                    found_variadic = found_variadic || parameter->variadic;
                }
                FUZZ_CHECK(found_variadic == macro->variadic);
                if (macro->status != NOC_MACRO_DIRECTIVE_STATUS_VALID) {
                    invalid_macro_count += 1;
                } else if (directive->macro_definition_allowed) {
                    FUZZ_CHECK(noc_macro_environment_apply(&environment,
                                                           &unit,
                                                           index) ==
                               NOC_MACRO_ENVIRONMENT_OK);
                    effective_macro_count += 1;
                }
            }
            FUZZ_CHECK(invalid_macro_count == unit.invalid_macro_directive_count);
            FUZZ_CHECK(noc_macro_environment_is_valid(&environment));
            FUZZ_CHECK(environment.count == effective_macro_count);
            for (index = 0; index < environment.count; ++index) {
                const Noc_Macro_Environment_Entry *entry =
                    noc_macro_environment_entry_at(&environment, index);
                const Noc_Macro_Directive *macro =
                    noc_macro_environment_entry_directive(&environment, index);
                FUZZ_CHECK(entry != NULL && macro != NULL);
                FUZZ_CHECK(entry->previous_entry_index == NOC_TOKEN_INDEX_NONE ||
                           entry->previous_entry_index < index);
                FUZZ_CHECK(macro->status == NOC_MACRO_DIRECTIVE_STATUS_VALID);
            }
            {
                Noc_Macro_Expansion_Limits limits =
                    noc_macro_expansion_default_limits();
                Noc_Macro_Expansion_Status status;
                limits.max_depth = 16;
                limits.max_output_tokens = 4096;
                limits.max_expansions = 1024;
                status = noc_macro_expansion_build(
                    &environment,
                    environment.count,
                    &unit,
                    (Noc_Token_Range){0, unit.preprocessing_token_count},
                    limits,
                    &expansion);
                FUZZ_CHECK(status == NOC_MACRO_EXPANSION_OK ||
                           status == NOC_MACRO_EXPANSION_INCOMPLETE_INVOCATION ||
                           status == NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH ||
                           status == NOC_MACRO_EXPANSION_INVALID_DEFINITION ||
                           status == NOC_MACRO_EXPANSION_DEPTH_LIMIT ||
                           status == NOC_MACRO_EXPANSION_OUTPUT_LIMIT ||
                           status == NOC_MACRO_EXPANSION_COUNT_LIMIT ||
                           status == NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR ||
                           status == NOC_MACRO_EXPANSION_UNSUPPORTED_VARIADIC ||
                           status == NOC_MACRO_EXPANSION_INVALID_PASTE ||
                           status == NOC_MACRO_EXPANSION_OUT_OF_MEMORY);
                if (status == NOC_MACRO_EXPANSION_OK) {
                    FUZZ_CHECK(noc_macro_expansion_is_valid(&expansion));
                    for (index = 0; index < expansion.count; ++index) {
                        const Noc_Macro_Expansion_Token *token =
                            noc_macro_expansion_token_at(&expansion, index);
                        FUZZ_CHECK(token != NULL);
                        FUZZ_CHECK(token->preprocessing_token_index <
                                   token->unit->preprocessing_token_count);
                    }
                }
                for (index = 0; index < unit.count; ++index) {
                    const Noc_Preprocessor_Directive *directive =
                        noc_preprocessor_directive_at(&unit, index);
                    Noc_Token_Range body =
                        noc_preprocessor_directive_body_tokens(&unit, index);
                    FUZZ_CHECK(directive != NULL);
                    if (body.begin == NOC_TOKEN_INDEX_NONE) {
                        FUZZ_CHECK(body.end == NOC_TOKEN_INDEX_NONE);
                        continue;
                    }
                    FUZZ_CHECK(body.begin < body.end);
                    FUZZ_CHECK(body.end <= unit.preprocessing_token_count);
                    if (directive->kind == NOC_PREPROCESSOR_DIRECTIVE_IF ||
                        directive->kind == NOC_PREPROCESSOR_DIRECTIVE_ELIF) {
                        bool value = false;
                        size_t problem = NOC_TOKEN_INDEX_NONE;
                        status = noc_macro_expansion_build_condition(
                            &environment,
                            environment.count,
                            &unit,
                            body,
                            limits,
                            &expansion);
                        if (status == NOC_MACRO_EXPANSION_OK) {
                            Noc_Preprocessor_Expression_Status expression_status =
                                noc_preprocessor_expression_evaluate(&expansion,
                                                                     &value,
                                                                     &problem);
                            FUZZ_CHECK(expression_status >=
                                           NOC_PREPROCESSOR_EXPRESSION_OK &&
                                       expression_status <=
                                           NOC_PREPROCESSOR_EXPRESSION_OUT_OF_MEMORY);
                            FUZZ_CHECK(problem == NOC_TOKEN_INDEX_NONE ||
                                       problem < expansion.count);
                        }
                    }
                }
            }
        }
        (void)noc_preprocessor_unit_validate_macro_policy(context, &unit);
        (void)noc_preprocessor_unit_validate_macro_directives(context, &unit);
        {
            Noc_Conditional_Groups_Build_Status status =
                noc_preprocessor_conditional_groups_build(
                    &conditional_initial, 0, &unit,
                    noc_macro_expansion_default_limits(), &groups);
            FUZZ_CHECK(status >= NOC_CONDITIONAL_GROUPS_OK &&
                       status <= NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY);
            if (status == NOC_CONDITIONAL_GROUPS_OK) {
                FUZZ_CHECK(noc_preprocessor_conditional_groups_is_valid(&groups));
                FUZZ_CHECK(noc_preprocessor_conditional_environment(&groups) ==
                           &groups.environment);
                for (index = 0; index < groups.group_count; ++index) {
                    const Noc_Preprocessor_Conditional_Group *group =
                        noc_preprocessor_conditional_group_at(&groups, index);
                    FUZZ_CHECK(group && group->preprocessing_tokens.begin <=
                                           group->preprocessing_tokens.end);
                    FUZZ_CHECK(group->preprocessing_tokens.end <=
                               unit.preprocessing_token_count);
                    FUZZ_CHECK(group->opener_directive_index < unit.count);
                    FUZZ_CHECK(group->first_branch_index < groups.branch_count);
                    FUZZ_CHECK(group->last_branch_index < groups.branch_count);
                }
                for (index = 0; index < groups.branch_count; ++index) {
                    const Noc_Preprocessor_Conditional_Branch *branch =
                        noc_preprocessor_conditional_branch_at(&groups, index);
                    FUZZ_CHECK(branch != NULL);
                    FUZZ_CHECK(branch->group_index < groups.group_count);
                    FUZZ_CHECK(branch->directive_index < unit.count);
                    FUZZ_CHECK(branch->content_tokens.begin <=
                               branch->content_tokens.end);
                    FUZZ_CHECK(branch->content_tokens.end <=
                               unit.preprocessing_token_count);
                    FUZZ_CHECK(branch->condition_environment_entry_limit ==
                                   NOC_TOKEN_INDEX_NONE ||
                               branch->condition_environment_entry_limit <=
                                   groups.environment.count);
                    if (branch->problem_unit) {
                        FUZZ_CHECK(branch->problem_token_index <
                                   branch->problem_unit->
                                       preprocessing_token_count);
                    }
                }
                for (index = 0;
                     index < groups.preprocessing_token_count;
                     ++index) {
                    Noc_Preprocessor_Activity activity =
                        noc_preprocessor_conditional_token_activity(&groups,
                                                                    index);
                    size_t entry_limit =
                        noc_preprocessor_conditional_token_macro_entry_limit(
                            &groups,
                            index);
                    FUZZ_CHECK(activity >= NOC_PREPROCESSOR_ACTIVITY_UNKNOWN &&
                               activity <= NOC_PREPROCESSOR_ACTIVITY_INACTIVE);
                    FUZZ_CHECK(entry_limit == NOC_TOKEN_INDEX_NONE ||
                               entry_limit <= groups.environment.count);
                }
            }
        }
    }
    noc_preprocessor_conditional_groups_free(&groups);
    noc_macro_environment_free(&conditional_initial);
    noc_macro_invocation_free(&invocation);
    noc_macro_expansion_free(&expansion);
    noc_macro_environment_free(&environment);
    noc_preprocessor_unit_free(&unit);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static void fuzz_token_apis(Noc_Context *context,
                            const uint8_t *data,
                            size_t size,
                            unsigned int selector)
{
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_C_Translation_Unit unit = {0};
    Noc_Preprocessor_Map preprocessor = {0};
    Noc_Argument_List arguments = {0};
    Noc_Token_Cursor cursor;
    size_t i;
    size_t offset = 0;
    if (!noc_tokenize(context, "fuzz.c", (const char *)data, size, &stream)) return;
    FUZZ_CHECK(noc_token_stream_is_valid(&stream));
    FUZZ_CHECK(stream.source_count == size);
    FUZZ_CHECK(size == 0 || memcmp(stream.source, data, size) == 0);
    for (i = 0; i < stream.count; ++i) {
        Noc_Token token = stream.items[i];
        FUZZ_CHECK(token.location.offset == offset);
        FUZZ_CHECK(token.text.data == stream.source + offset);
        FUZZ_CHECK(token.text.count <= size - offset);
        offset += token.text.count;
    }
    FUZZ_CHECK(offset == size);

    noc_token_cursor_init(&cursor, &stream);
    FUZZ_CHECK(noc_token_cursor_rewind(&cursor, selector % stream.count));
    (void)noc_token_cursor_peek_raw(&cursor, selector % 8u);
    (void)noc_token_cursor_peek(&cursor, selector % 8u);
    (void)noc_token_cursor_match_identifier(&cursor, "value", NULL);
    (void)noc_token_cursor_match_punct(&cursor, "(", NULL);
    (void)noc_token_cursor_take_balanced(&cursor, "(", ")", NULL, NULL);
    (void)noc_parse_arguments(&stream,
                              (Noc_Token_Range){0, stream.count - 1},
                              &arguments);

    if (noc_syntax_tree_build(context, &stream, &tree)) {
        Noc_Slice root_source;
        size_t node = noc_syntax_root(&tree);
        size_t visited = 0;
        FUZZ_CHECK(noc_syntax_tree_is_valid(&tree));
        root_source = noc_syntax_source(&tree, node);
        FUZZ_CHECK(root_source.count == size);
        FUZZ_CHECK(size == 0 || memcmp(root_source.data, data, size) == 0);
        while (node != NOC_SYNTAX_NONE) {
            FUZZ_CHECK(noc_syntax_node(&tree, node) != NULL);
            FUZZ_CHECK(noc_syntax_depth(&tree, node) != NOC_SYNTAX_NONE);
            visited += 1;
            FUZZ_CHECK(visited <= tree.count);
            node = noc_syntax_next_preorder(&tree, node);
        }
        FUZZ_CHECK(visited == tree.count);
        for (i = 0; i + 1 < stream.count; ++i) {
            size_t owner = noc_syntax_node_at_token(&tree, i);
            const Noc_Syntax_Node *syntax = noc_syntax_node(&tree, owner);
            FUZZ_CHECK(syntax != NULL);
            FUZZ_CHECK(syntax->range.begin <= i && i < syntax->range.end);
            FUZZ_CHECK(noc_syntax_node_covering_range(
                           &tree,
                           (Noc_Token_Range){i, i + 1}) == owner);
            FUZZ_CHECK(noc_syntax_common_ancestor(&tree, owner, owner) == owner);
        }
        (void)noc_c_translation_unit_build(context, &tree, &unit);
    }
    (void)noc_preprocessor_map_build(context, &stream, &preprocessor);

    noc_preprocessor_map_free(&preprocessor);
    noc_c_translation_unit_free(&unit);
    noc_syntax_tree_free(&tree);
    noc_argument_list_free(&arguments);
    noc_token_stream_free(&stream);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Noc_Context context;
    Noc_Transform_Result result = {0};
    Fuzz_State state;
    Noc_Rule rule;
    fuzz_standalone_lexer(data, size);
    fuzz_workspace(data, size, size > 0 ? data[0] : 0);
    noc_context_init(&context);
    noc_context_set_diagnostic(&context, ignore_diagnostic, NULL);
    state.selector = size > 0 ? data[0] : 0;
    fuzz_preprocessor(&context, data, size, state.selector);
    memset(&rule, 0, sizeof(rule));
    rule.name = "fuzz";
    rule.scope = NOC_RULE_TOKEN;
    rule.syntax = "@fuzz ...";
    rule.description = "Exercise rewriter APIs under fuzzing.";
    rule.expand = expand_fuzz;
    rule.user_data = &state;
    FUZZ_CHECK(noc_register_rule(&context, rule));
    fuzz_token_apis(&context, data, size, state.selector);
    context.options.emit_line_directives = false;
    context.options.unknown_rule_is_error = false;
    context.options.skip_inactive_preprocessor_branches = (state.selector & 1u) != 0;
    (void)noc_transform_source(&context,
                               "fuzz.c",
                               (const char *)data,
                               size,
                               &result);
    noc_transform_result_free(&result);
    noc_context_deinit(&context);
    return 0;
}

#ifndef NOC_LIBFUZZER
static uint32_t fuzz_random(uint32_t *state)
{
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

int main(void)
{
    static const char *const corpus[] = {
        "",
        "int main(void) { return 0; }\n",
        "@fuzz(value)",
        "@fuzz va\\\nlue",
        "/* comment */ // line\\\ncontinued\n",
        "#if 0\n@missing\n#elif 1\n@fuzz(42)\n#endif\n",
        "#if FLAG\n@fuzz(\n#endif\n42\n#if FLAG\n)\n#endif\n",
        ("#define FLAG 1\n#if 'A'\n#undef FLAG\n#else\n"
         "#define OTHER 2\n#endif\n#if FLAG\nvalue\n#endif\n"),
        ("#else\n#if 1\n#else junk\n#if 0\n#endif\n"
         "#elifdef FEATURE\n#endif trailing\n#if 1\n"),
        "F(a, (b, c),) G(unterminated\n",
        ("#define ID(x) x\n#define CALL(f,x) f(x)\n"
         "#define OPEN ID(\n#define V(...) __VA_ARGS__\n"
         "CALL(ID, ID(1)) OPEN 2) V(ID(3),4)\n"),
        "([{}]) <::> %:%: /\\\n* block *\\\n/",
        "\"string\\n\\x41\" '\\123' 0x1p+2",
    };
    uint8_t bytes[512];
    uint32_t random_state = UINT32_C(0x4e4f4321);
    size_t i;
    for (i = 0; i < sizeof(corpus) / sizeof(corpus[0]); ++i) {
        FUZZ_CHECK(LLVMFuzzerTestOneInput((const uint8_t *)corpus[i],
                                         strlen(corpus[i])) == 0);
    }
    for (i = 0; i < 2000; ++i) {
        size_t count = fuzz_random(&random_state) % (sizeof(bytes) + 1);
        size_t j;
        for (j = 0; j < count; ++j) bytes[j] = (uint8_t)fuzz_random(&random_state);
        FUZZ_CHECK(LLVMFuzzerTestOneInput(bytes, count) == 0);
    }
    puts("noc fuzz smoke passed");
    return 0;
}
#endif
