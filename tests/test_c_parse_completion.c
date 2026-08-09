#include "test_support.h"

typedef struct {
    Noc_Workspace workspace;
    Noc_Document_Snapshot snapshot;
    Noc_C_Parse_Tree tree;
} Grammar_Fixture;

static bool grammar_fixture_build(Grammar_Fixture *fixture,
                                  const char *path,
                                  const char *source)
{
    noc_workspace_init(&fixture->workspace);
    if (noc_workspace_open_document(&fixture->workspace,
                                    path,
                                    source,
                                    strlen(source),
                                    NOC_SOURCE_CLASS_PROJECT,
                                    &fixture->snapshot) != NOC_WORKSPACE_OK) {
        return false;
    }
    return noc_c_parse_tree_build(&fixture->snapshot,
                                  noc_c_parse_default_options(),
                                  &fixture->tree) == NOC_C_PARSE_OK;
}

static void grammar_fixture_free(Grammar_Fixture *fixture)
{
    noc_c_parse_tree_free(&fixture->tree);
    noc_document_snapshot_free(&fixture->snapshot);
    noc_workspace_deinit(&fixture->workspace);
}

static const Noc_C_Grammar_Candidate *find_exact_candidate(
    const Noc_C_Grammar_Candidates *candidates,
    const char *spelling)
{
    size_t index;
    for (index = 0; index < candidates->count; ++index) {
        const Noc_C_Grammar_Candidate *candidate =
            noc_c_grammar_candidate_at(candidates, index);
        if (candidate && slice_equals(candidate->spelling, spelling)) {
            return candidate;
        }
    }
    return NULL;
}

static const Noc_C_Grammar_Candidate *find_category_candidate(
    const Noc_C_Grammar_Candidates *candidates,
    Noc_C_Ast_Expected_Kind kind)
{
    size_t index;
    for (index = 0; index < candidates->count; ++index) {
        const Noc_C_Grammar_Candidate *candidate =
            noc_c_grammar_candidate_at(candidates, index);
        if (candidate && candidate->kind == kind &&
            candidate->spelling.count == 0) {
            return candidate;
        }
    }
    return NULL;
}

static uint64_t grammar_candidates_hash(
    const Noc_C_Grammar_Candidates *candidates)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    hash ^= candidates->offset;
    hash *= UINT64_C(1099511628211);
    hash ^= candidates->replacement.begin;
    hash *= UINT64_C(1099511628211);
    hash ^= candidates->replacement.end;
    hash *= UINT64_C(1099511628211);
    hash ^= candidates->flags;
    hash *= UINT64_C(1099511628211);
    hash ^= candidates->count;
    hash *= UINT64_C(1099511628211);
    for (index = 0; index < candidates->count; ++index) {
        const Noc_C_Grammar_Candidate *candidate =
            noc_c_grammar_candidate_at(candidates, index);
        size_t byte;
        hash ^= candidate->kind;
        hash *= UINT64_C(1099511628211);
        hash ^= candidate->flags;
        hash *= UINT64_C(1099511628211);
        hash ^= candidate->spelling.count;
        hash *= UINT64_C(1099511628211);
        for (byte = 0; byte < candidate->spelling.count; ++byte) {
            hash ^= (unsigned char)candidate->spelling.data[byte];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static void check_malformed_result_is_rejected_transactionally(
    const Noc_C_Parse_Tree *tree,
    Noc_C_Grammar_Candidate_Options options,
    Noc_C_Grammar_Candidates *candidates)
{
    Noc_C_Grammar_Candidate *items = candidates->items;
    char *spellings = candidates->spelling_storage;
    size_t generation = candidates->generation;
    CHECK(!noc_c_grammar_candidates_is_valid(candidates));
    CHECK(noc_c_parse_grammar_candidates_build(tree,
                                               0,
                                               options,
                                               candidates) ==
          NOC_C_PARSE_INVALID_ARGUMENT);
    CHECK(candidates->items == items);
    CHECK(candidates->spelling_storage == spellings);
    CHECK(candidates->generation == generation);
}

static void test_empty_document_has_stable_initial_candidates(void)
{
    Grammar_Fixture fixture = {0};
    Noc_C_Grammar_Candidates candidates = {0};
    Noc_C_Grammar_Candidate_Options options =
        noc_c_grammar_candidate_default_options();
    uint64_t first_hash;

    CHECK(options.max_candidates != 0);
    CHECK(options.max_nodes_examined != 0);
    CHECK(options.max_symbols_examined != 0);
    options.max_candidates = 512;
    CHECK(grammar_fixture_build(&fixture, "completion/empty.c", ""));
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &candidates) == NOC_C_PARSE_OK);
    CHECK(noc_c_grammar_candidates_is_valid(&candidates));
    CHECK(candidates.offset == 0);
    CHECK(candidates.replacement.begin == 0);
    CHECK(candidates.replacement.end == 0);
    CHECK((candidates.flags &
           NOC_C_GRAMMAR_CANDIDATES_STATE_AVAILABLE) != 0);
    CHECK(candidates.count != 0);
    CHECK(find_category_candidate(&candidates,
                                  NOC_C_AST_EXPECTED_IDENTIFIER) != NULL);
    CHECK(noc_c_grammar_candidate_at(&candidates, candidates.count) == NULL);
    first_hash = grammar_candidates_hash(&candidates);
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &candidates) == NOC_C_PARSE_OK);
    CHECK(candidates.generation == 2);
    CHECK(grammar_candidates_hash(&candidates) == first_hash);
    noc_c_grammar_candidates_free(&candidates);
    CHECK(!noc_c_grammar_candidates_is_valid(&candidates));
    noc_c_grammar_candidates_free(NULL);
    grammar_fixture_free(&fixture);
}

static void test_every_offset_has_deterministic_leaf_semantics(void)
{
    static const char source[] = "int value;\n";
    Grammar_Fixture fixture = {0};
    Noc_C_Grammar_Candidates candidates = {0};
    Noc_C_Grammar_Candidate_Options options =
        noc_c_grammar_candidate_default_options();
    size_t offset;

    CHECK(grammar_fixture_build(&fixture, "completion/offsets.c", source));
    for (offset = 0; offset <= sizeof(source) - 1; ++offset) {
        uint64_t first_hash;
        CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                                   offset,
                                                   options,
                                                   &candidates) ==
              NOC_C_PARSE_OK);
        CHECK(noc_c_grammar_candidates_is_valid(&candidates));
        CHECK(candidates.offset == offset);
        CHECK(candidates.file_id ==
              noc_document_snapshot_file_id(&fixture.snapshot));
        CHECK(candidates.document_generation ==
              noc_document_snapshot_generation(&fixture.snapshot));
        CHECK(candidates.parse_tree_generation ==
              noc_c_parse_tree_generation(&fixture.tree));
        if ((offset > 0 && offset < 3) ||
            (offset > 4 && offset < 9)) {
            CHECK((candidates.flags &
                   NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT) != 0);
            CHECK(candidates.replacement.begin < offset);
            CHECK(offset < candidates.replacement.end);
        } else {
            CHECK((candidates.flags &
                   NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT) == 0);
            CHECK(candidates.replacement.begin == offset);
            CHECK(candidates.replacement.end == offset);
        }
        first_hash = grammar_candidates_hash(&candidates);
        CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                                   offset,
                                                   options,
                                                   &candidates) ==
              NOC_C_PARSE_OK);
        CHECK(grammar_candidates_hash(&candidates) == first_hash);
    }
    noc_c_grammar_candidates_free(&candidates);
    grammar_fixture_free(&fixture);
}

static void test_missing_symbols_merge_with_lookahead(void)
{
    Grammar_Fixture semicolon_fixture = {0};
    Grammar_Fixture brace_fixture = {0};
    Noc_C_Grammar_Candidates candidates = {0};
    Noc_C_Grammar_Candidate_Options options =
        noc_c_grammar_candidate_default_options();
    const Noc_C_Grammar_Candidate *candidate;
    static const char missing_semicolon[] = "int value   ";
    static const char missing_brace[] = "int f(void) { int value;";

    CHECK(grammar_fixture_build(&semicolon_fixture,
                                "completion/missing-semicolon.c",
                                missing_semicolon));
    CHECK(noc_c_parse_grammar_candidates_build(
              &semicolon_fixture.tree,
              sizeof("int value") - 1,
              options,
              &candidates) == NOC_C_PARSE_OK);
    CHECK((candidates.flags &
           NOC_C_GRAMMAR_CANDIDATES_RECOVERY_HEURISTIC) != 0);
    candidate = find_exact_candidate(&candidates, ";");
    CHECK(candidate != NULL);
    if (candidate) {
        CHECK(candidate->kind == NOC_C_AST_EXPECTED_PUNCTUATOR);
        CHECK((candidate->flags & NOC_C_GRAMMAR_CANDIDATE_MISSING) != 0);
        CHECK((candidate->flags & NOC_C_GRAMMAR_CANDIDATE_LOOKAHEAD) != 0);
    }
    grammar_fixture_free(&semicolon_fixture);

    CHECK(grammar_fixture_build(&brace_fixture,
                                "completion/missing-brace.c",
                                missing_brace));
    CHECK(noc_c_parse_grammar_candidates_build(
              &brace_fixture.tree,
              sizeof(missing_brace) - 1,
              options,
              &candidates) == NOC_C_PARSE_OK);
    candidate = find_exact_candidate(&candidates, "}");
    CHECK(candidate != NULL);
    if (candidate) {
        CHECK((candidate->flags & NOC_C_GRAMMAR_CANDIDATE_MISSING) != 0);
    }
    noc_c_grammar_candidates_free(&candidates);
    grammar_fixture_free(&brace_fixture);
}

static void test_error_and_trivia_positions_are_explicit_heuristics(void)
{
    static const char error_source[] =
        "int f(void) { @@@ return 1; }\n";
    static const char trivia_source[] = "/* comment */  int value;\n";
    static const char collapsed_error_source[] = "int f(void) { ret";
    Grammar_Fixture error_fixture = {0};
    Grammar_Fixture trivia_fixture = {0};
    Grammar_Fixture collapsed_error_fixture = {0};
    Noc_C_Grammar_Candidates candidates = {0};
    Noc_C_Grammar_Candidate_Options options =
        noc_c_grammar_candidate_default_options();
    size_t error_offset = (size_t)(strstr(error_source, "@@@") - error_source) + 1;
    size_t comment_offset = 4;
    size_t index;

    CHECK(grammar_fixture_build(&error_fixture,
                                "completion/error.c",
                                error_source));
    CHECK(noc_c_parse_grammar_candidates_build(&error_fixture.tree,
                                               error_offset,
                                               options,
                                               &candidates) == NOC_C_PARSE_OK);
    CHECK((candidates.flags &
           NOC_C_GRAMMAR_CANDIDATES_RECOVERY_HEURISTIC) != 0);
    CHECK((candidates.flags &
           NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT) != 0);
    for (index = 0; index < candidates.count; ++index) {
        const Noc_C_Grammar_Candidate *candidate =
            noc_c_grammar_candidate_at(&candidates, index);
        CHECK(candidate->kind != NOC_C_AST_EXPECTED_NONE);
        CHECK(candidate->kind != NOC_C_AST_EXPECTED_UNKNOWN);
        CHECK(!slice_equals(candidate->spelling, "ERROR"));
        CHECK(!slice_equals(candidate->spelling, "end"));
        CHECK(candidate->spelling.count == 0 ||
              strstr(candidate->spelling.data, "repeat") == NULL);
    }
    grammar_fixture_free(&error_fixture);

    CHECK(grammar_fixture_build(&collapsed_error_fixture,
                                "completion/collapsed-error.c",
                                collapsed_error_source));
    CHECK(noc_c_parse_grammar_candidates_build(
              &collapsed_error_fixture.tree,
              (size_t)(strstr(collapsed_error_source, "ret") -
                       collapsed_error_source) + 1,
              options,
              &candidates) == NOC_C_PARSE_OK);
    CHECK((candidates.flags &
           NOC_C_GRAMMAR_CANDIDATES_RECOVERY_HEURISTIC) != 0);
    CHECK((candidates.flags &
           NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT) != 0);
    CHECK(candidates.replacement.begin ==
          (size_t)(strstr(collapsed_error_source, "ret") -
                   collapsed_error_source));
    CHECK(candidates.replacement.end == sizeof(collapsed_error_source) - 1);
    grammar_fixture_free(&collapsed_error_fixture);

    CHECK(grammar_fixture_build(&trivia_fixture,
                                "completion/trivia.c",
                                trivia_source));
    CHECK(noc_c_parse_grammar_candidates_build(&trivia_fixture.tree,
                                               comment_offset,
                                               options,
                                               &candidates) == NOC_C_PARSE_OK);
    CHECK((candidates.flags &
           NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT) == 0);
    CHECK(candidates.replacement.begin == comment_offset);
    CHECK(candidates.replacement.end == comment_offset);
    noc_c_grammar_candidates_free(&candidates);
    grammar_fixture_free(&trivia_fixture);
}

static void test_boundaries_use_right_leaf_and_eof_states(void)
{
    static const char statement_source[] =
        "int f(void) { return value; }\n";
    static const char eof_source[] = "int value;";
    Grammar_Fixture statement_fixture = {0};
    Grammar_Fixture eof_fixture = {0};
    Noc_C_Grammar_Candidates candidates = {0};
    Noc_C_Grammar_Candidate_Options options =
        noc_c_grammar_candidate_default_options();
    size_t return_boundary =
        (size_t)(strstr(statement_source, "return") - statement_source) +
        sizeof("return") - 1;

    CHECK(grammar_fixture_build(&statement_fixture,
                                "completion/right-leaf.c",
                                statement_source));
    CHECK(noc_c_parse_grammar_candidates_build(&statement_fixture.tree,
                                               return_boundary,
                                               options,
                                               &candidates) == NOC_C_PARSE_OK);
    CHECK(find_exact_candidate(&candidates, "sizeof") != NULL);
    CHECK(find_category_candidate(&candidates,
                                  NOC_C_AST_EXPECTED_EXPRESSION) != NULL);
    grammar_fixture_free(&statement_fixture);

    CHECK(grammar_fixture_build(&eof_fixture,
                                "completion/eof-state.c",
                                eof_source));
    CHECK(noc_c_parse_grammar_candidates_build(&eof_fixture.tree,
                                               sizeof(eof_source) - 1,
                                               options,
                                               &candidates) == NOC_C_PARSE_OK);
    CHECK(find_exact_candidate(&candidates, "asm") != NULL);
    CHECK(candidates.replacement.begin == sizeof(eof_source) - 1);
    CHECK(candidates.replacement.end == sizeof(eof_source) - 1);
    noc_c_grammar_candidates_free(&candidates);
    grammar_fixture_free(&eof_fixture);
}

static void test_non_c11_spelling_and_stable_truncation(void)
{
    static const char source[] = "int value;\n";
    Grammar_Fixture fixture = {0};
    Noc_C_Grammar_Candidates complete = {0};
    Noc_C_Grammar_Candidates limited = {0};
    Noc_C_Grammar_Candidate_Options options =
        noc_c_grammar_candidate_default_options();
    const Noc_C_Grammar_Candidate *extension;
    const Noc_C_Grammar_Candidate *standard;
    const Noc_C_Grammar_Candidate *category;

    options.max_candidates = 512;
    CHECK(grammar_fixture_build(&fixture, "completion/extensions.c", source));
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &complete) == NOC_C_PARSE_OK);
    extension = find_exact_candidate(&complete, "__attribute__");
    standard = find_exact_candidate(&complete, "typedef");
    category = find_category_candidate(&complete,
                                       NOC_C_AST_EXPECTED_IDENTIFIER);
    CHECK(extension != NULL);
    CHECK(standard != NULL);
    CHECK(category != NULL);
    if (extension) {
        CHECK((extension->flags & NOC_C_GRAMMAR_CANDIDATE_NON_C11) != 0);
    }
    if (standard) {
        CHECK((standard->flags & NOC_C_GRAMMAR_CANDIDATE_NON_C11) == 0);
    }
    if (extension && standard && category) {
        Noc_C_Grammar_Candidate *mutable_extension =
            (Noc_C_Grammar_Candidate *)extension;
        Noc_C_Grammar_Candidate *mutable_standard =
            (Noc_C_Grammar_Candidate *)standard;
        Noc_C_Grammar_Candidate *mutable_category =
            (Noc_C_Grammar_Candidate *)category;
        Noc_C_Grammar_Candidate saved_extension = *extension;
        Noc_C_Grammar_Candidate saved_standard = *standard;
        Noc_C_Grammar_Candidate saved_category = *category;

        mutable_standard->spelling.data = NULL;
        mutable_standard->spelling.count = 0;
        check_malformed_result_is_rejected_transactionally(&fixture.tree,
                                                            options,
                                                            &complete);
        *mutable_standard = saved_standard;

        mutable_standard->kind = NOC_C_AST_EXPECTED_IDENTIFIER;
        check_malformed_result_is_rejected_transactionally(&fixture.tree,
                                                            options,
                                                            &complete);
        *mutable_standard = saved_standard;

        mutable_category->flags |= NOC_C_GRAMMAR_CANDIDATE_NON_C11;
        check_malformed_result_is_rejected_transactionally(&fixture.tree,
                                                            options,
                                                            &complete);
        *mutable_category = saved_category;

        mutable_standard->flags |= NOC_C_GRAMMAR_CANDIDATE_NON_C11;
        check_malformed_result_is_rejected_transactionally(&fixture.tree,
                                                            options,
                                                            &complete);
        *mutable_standard = saved_standard;

        mutable_extension->flags &= ~NOC_C_GRAMMAR_CANDIDATE_NON_C11;
        check_malformed_result_is_rejected_transactionally(&fixture.tree,
                                                            options,
                                                            &complete);
        *mutable_extension = saved_extension;
        CHECK(noc_c_grammar_candidates_is_valid(&complete));
    }

    options.max_candidates = 1;
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &limited) == NOC_C_PARSE_OK);
    CHECK(limited.count == 1);
    CHECK((limited.flags & NOC_C_GRAMMAR_CANDIDATES_TRUNCATED) != 0);
    CHECK(limited.items[0].kind == complete.items[0].kind);
    CHECK(noc_slice_equal(limited.items[0].spelling,
                          complete.items[0].spelling));
    noc_c_grammar_candidates_free(&limited);
    noc_c_grammar_candidates_free(&complete);
    grammar_fixture_free(&fixture);
}

static bool cancel_immediately(void *user_data)
{
    size_t *calls = (size_t *)user_data;
    *calls += 1;
    return true;
}

typedef struct {
    size_t calls;
    size_t cancel_on_call;
} Delayed_Cancel;

static bool cancel_after_poll(void *user_data)
{
    Delayed_Cancel *cancel = (Delayed_Cancel *)user_data;
    cancel->calls += 1;
    return cancel->calls >= cancel->cancel_on_call;
}

static void test_cancellation_during_node_scan(void)
{
    char source[4096];
    size_t count = 0;
    size_t declaration;
    Grammar_Fixture fixture = {0};
    Noc_C_Grammar_Candidates candidates = {0};
    Noc_C_Grammar_Candidate_Options options =
        noc_c_grammar_candidate_default_options();
    Delayed_Cancel cancel = {0, 2};

    for (declaration = 0; declaration < 300; ++declaration) {
        static const char text[] = "int x;\n";
        memcpy(source + count, text, sizeof(text) - 1);
        count += sizeof(text) - 1;
    }
    source[count] = '\0';
    CHECK(grammar_fixture_build(&fixture,
                                "completion/cancel-during-scan.c",
                                source));
    CHECK(noc_c_parse_tree_node_count(&fixture.tree) > 256);
    options.should_cancel = cancel_after_poll;
    options.cancel_user_data = &cancel;
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &candidates) ==
          NOC_C_PARSE_CANCELLED);
    CHECK(cancel.calls == 2);
    CHECK(!noc_c_grammar_candidates_is_valid(&candidates));
    grammar_fixture_free(&fixture);
}

static void test_failures_preserve_output_and_result_owns_spellings(void)
{
    static const char source[] = "int value";
    Grammar_Fixture fixture = {0};
    Noc_C_Grammar_Candidates candidates = {0};
    Noc_C_Grammar_Candidate_Options options =
        noc_c_grammar_candidate_default_options();
    Noc_C_Grammar_Candidate *items;
    char *spellings;
    size_t generation;
    uint64_t hash;
    size_t cancel_calls = 0;

    CHECK(grammar_fixture_build(&fixture, "completion/failures.c", source));
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               sizeof(source) - 1,
                                               options,
                                               &candidates) == NOC_C_PARSE_OK);
    items = candidates.items;
    spellings = candidates.spelling_storage;
    generation = candidates.generation;
    hash = grammar_candidates_hash(&candidates);

    options.max_nodes_examined = 1;
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &candidates) ==
          NOC_C_PARSE_LIMIT_EXCEEDED);
    CHECK(candidates.items == items);
    CHECK(candidates.spelling_storage == spellings);
    CHECK(candidates.generation == generation);
    CHECK(grammar_candidates_hash(&candidates) == hash);

    options = noc_c_grammar_candidate_default_options();
    options.max_symbols_examined = 1;
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &candidates) ==
          NOC_C_PARSE_LIMIT_EXCEEDED);
    CHECK(candidates.items == items);
    CHECK(grammar_candidates_hash(&candidates) == hash);

    options = noc_c_grammar_candidate_default_options();
    options.should_cancel = cancel_immediately;
    options.cancel_user_data = &cancel_calls;
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &candidates) ==
          NOC_C_PARSE_CANCELLED);
    CHECK(cancel_calls == 1);
    CHECK(candidates.items == items);
    CHECK(grammar_candidates_hash(&candidates) == hash);

    options = noc_c_grammar_candidate_default_options();
    options.max_candidates = 0;
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &candidates) ==
          NOC_C_PARSE_INVALID_ARGUMENT);
    CHECK(candidates.items == items);

    options = noc_c_grammar_candidate_default_options();
    candidates.generation = SIZE_MAX;
    CHECK(noc_c_parse_grammar_candidates_build(&fixture.tree,
                                               0,
                                               options,
                                               &candidates) ==
          NOC_C_PARSE_GENERATION_EXHAUSTED);
    candidates.generation = generation;
    {
        size_t previous_tree_generation =
            noc_c_parse_tree_generation(&fixture.tree);
        CHECK(noc_c_parse_tree_build(&fixture.snapshot,
                                     noc_c_parse_default_options(),
                                     &fixture.tree) == NOC_C_PARSE_OK);
        CHECK(noc_c_parse_tree_generation(&fixture.tree) ==
              previous_tree_generation + 1);
        CHECK(candidates.parse_tree_generation == previous_tree_generation);
        CHECK(noc_c_grammar_candidates_is_valid(&candidates));
        CHECK(grammar_candidates_hash(&candidates) == hash);
    }
    grammar_fixture_free(&fixture);
    CHECK(noc_c_grammar_candidates_is_valid(&candidates));
    CHECK(find_exact_candidate(&candidates, ";") != NULL);
    CHECK(grammar_candidates_hash(&candidates) == hash);
    noc_c_grammar_candidates_free(&candidates);
}

int main(void)
{
    test_empty_document_has_stable_initial_candidates();
    test_every_offset_has_deterministic_leaf_semantics();
    test_missing_symbols_merge_with_lookahead();
    test_error_and_trivia_positions_are_explicit_heuristics();
    test_boundaries_use_right_leaf_and_eof_states();
    test_non_c11_spelling_and_stable_truncation();
    test_cancellation_during_node_scan();
    test_failures_preserve_output_and_result_owns_spellings();
    return finish_suite("C parser grammar completion");
}
