#include <noc/noc.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_CHECK(condition)                                                  \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr,                                                    \
                    "%s:%d: C parser fuzz invariant failed: %s\n",           \
                    __FILE__,                                                  \
                    __LINE__,                                                  \
                    #condition);                                               \
            abort();                                                           \
        }                                                                      \
    } while (0)

enum { C_PARSER_FUZZ_MAX_NODES = 8192 };

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t count)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t index;
    for (index = 0; index < count; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_size(uint64_t hash, size_t value)
{
    return hash_bytes(hash, &value, sizeof(value));
}

static uint64_t hash_slice(uint64_t hash, Noc_Slice value)
{
    hash = hash_size(hash, value.count);
    return value.count == 0 ? hash : hash_bytes(hash, value.data, value.count);
}

static void check_parse_topology(const Noc_C_Parse_Tree *tree,
                                 const uint8_t *data,
                                 size_t size)
{
    const Noc_Document_Snapshot *snapshot = noc_c_parse_tree_snapshot(tree);
    Noc_Slice retained;
    size_t count = noc_c_parse_tree_node_count(tree);
    bool visited[C_PARSER_FUZZ_MAX_NODES] = {false};
    size_t index;
    FUZZ_CHECK(noc_c_parse_tree_is_valid(tree));
    FUZZ_CHECK(snapshot != NULL);
    retained = noc_document_snapshot_source(snapshot);
    FUZZ_CHECK(retained.count == size);
    FUZZ_CHECK(size == 0 || memcmp(retained.data, data, size) == 0);
    FUZZ_CHECK(noc_c_parse_tree_root(tree) == 0);
    FUZZ_CHECK(count != 0);
    FUZZ_CHECK(count <= C_PARSER_FUZZ_MAX_NODES);
    FUZZ_CHECK(noc_c_parse_tree_node_at(tree, count) == NULL);
    FUZZ_CHECK(noc_c_parse_node_source(tree, 0).count == size);
    FUZZ_CHECK(size == 0 ||
               memcmp(noc_c_parse_node_source(tree, 0).data, data, size) == 0);
    visited[0] = true;

    for (index = 0; index < count; ++index) {
        const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, index);
        Noc_Slice spelling;
        size_t child;
        size_t children = 0;
        size_t last_child = NOC_C_PARSE_NODE_NONE;
        FUZZ_CHECK(node != NULL);
        FUZZ_CHECK(node->generation == noc_c_parse_tree_generation(tree));
        FUZZ_CHECK(node->bytes.begin <= node->bytes.end);
        FUZZ_CHECK(node->bytes.end <= size);
        FUZZ_CHECK(index == 0 ? node->parent == NOC_C_PARSE_NODE_NONE
                              : node->parent < index);
        spelling = noc_c_parse_node_source(tree, index);
        FUZZ_CHECK(spelling.count == node->bytes.end - node->bytes.begin);
        FUZZ_CHECK(spelling.data == retained.data + node->bytes.begin);
        child = node->first_child;
        while (child != NOC_C_PARSE_NODE_NONE) {
            const Noc_C_Parse_Node *child_node;
            FUZZ_CHECK(child < count);
            FUZZ_CHECK(!visited[child]);
            visited[child] = true;
            child_node = noc_c_parse_tree_node_at(tree, child);
            FUZZ_CHECK(child_node != NULL);
            FUZZ_CHECK(child_node->parent == index);
            FUZZ_CHECK(child_node->bytes.begin >= node->bytes.begin);
            FUZZ_CHECK(child_node->bytes.end <= node->bytes.end);
            children += 1;
            FUZZ_CHECK(children <= node->child_count);
            last_child = child;
            child = child_node->next_sibling;
        }
        FUZZ_CHECK(children == node->child_count);
        FUZZ_CHECK(node->child_count == 0
                       ? node->last_child == NOC_C_PARSE_NODE_NONE
                       : node->last_child != NOC_C_PARSE_NODE_NONE);
        FUZZ_CHECK(last_child == node->last_child);
    }
    for (index = 0; index < count; ++index) FUZZ_CHECK(visited[index]);
}

static uint64_t parse_tree_hash(const Noc_C_Parse_Tree *tree)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t count = noc_c_parse_tree_node_count(tree);
    size_t index;
    hash = hash_size(hash, count);
    for (index = 0; index < count; ++index) {
        const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, index);
        hash = hash_size(hash, node->bytes.begin);
        hash = hash_size(hash, node->bytes.end);
        hash = hash_size(hash, node->parent);
        hash = hash_size(hash, node->first_child);
        hash = hash_size(hash, node->last_child);
        hash = hash_size(hash, node->next_sibling);
        hash = hash_size(hash, node->child_count);
        hash = hash_size(hash, node->flags);
        hash = hash_slice(hash, node->kind);
        hash = hash_slice(hash, node->field);
    }
    return hash;
}

static int grammar_candidate_compare(const Noc_C_Grammar_Candidate *left,
                                     const Noc_C_Grammar_Candidate *right)
{
    size_t common;
    int compared;
    if (left->kind != right->kind) return left->kind < right->kind ? -1 : 1;
    common = left->spelling.count < right->spelling.count
                 ? left->spelling.count
                 : right->spelling.count;
    compared = common == 0
                   ? 0
                   : memcmp(left->spelling.data,
                            right->spelling.data,
                            common);
    if (compared != 0) return compared;
    if (left->spelling.count == right->spelling.count) return 0;
    return left->spelling.count < right->spelling.count ? -1 : 1;
}

static uint64_t grammar_candidates_hash(const Noc_C_Parse_Tree *tree,
                                        size_t source_size)
{
    const Noc_Document_Snapshot *snapshot = noc_c_parse_tree_snapshot(tree);
    Noc_C_Grammar_Candidate_Options options =
        noc_c_grammar_candidate_default_options();
    size_t offsets[] = {0, source_size / 2, source_size};
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t offset_index;
    options.max_candidates = 64;
    options.max_nodes_examined = C_PARSER_FUZZ_MAX_NODES * 2 + 1;
    for (offset_index = 0;
         offset_index < sizeof(offsets) / sizeof(offsets[0]);
         ++offset_index) {
        Noc_C_Grammar_Candidates candidates = {0};
        size_t candidate_index;
        bool saw_missing = false;
        FUZZ_CHECK(noc_c_parse_grammar_candidates_build(
                       tree,
                       offsets[offset_index],
                       options,
                       &candidates) == NOC_C_PARSE_OK);
        FUZZ_CHECK(noc_c_grammar_candidates_is_valid(&candidates));
        FUZZ_CHECK(candidates.count <= options.max_candidates);
        FUZZ_CHECK(candidates.offset == offsets[offset_index]);
        FUZZ_CHECK(candidates.file_id ==
                   noc_document_snapshot_file_id(snapshot));
        FUZZ_CHECK(candidates.document_generation ==
                   noc_document_snapshot_generation(snapshot));
        FUZZ_CHECK(candidates.parse_tree_generation ==
                   noc_c_parse_tree_generation(tree));
        FUZZ_CHECK(candidates.replacement.begin <= candidates.replacement.end);
        FUZZ_CHECK(candidates.replacement.end <= source_size);
        if ((candidates.flags &
             NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT) != 0) {
            FUZZ_CHECK(candidates.replacement.begin <
                       offsets[offset_index]);
            FUZZ_CHECK(offsets[offset_index] < candidates.replacement.end);
        } else {
            FUZZ_CHECK(candidates.replacement.begin == offsets[offset_index]);
            FUZZ_CHECK(candidates.replacement.end == offsets[offset_index]);
        }
        hash = hash_size(hash, candidates.offset);
        hash = hash_size(hash, candidates.replacement.begin);
        hash = hash_size(hash, candidates.replacement.end);
        hash = hash_size(hash, candidates.flags);
        hash = hash_size(hash, candidates.count);
        for (candidate_index = 0;
             candidate_index < candidates.count;
             ++candidate_index) {
            const Noc_C_Grammar_Candidate *candidate =
                noc_c_grammar_candidate_at(&candidates, candidate_index);
            FUZZ_CHECK(candidate != NULL);
            FUZZ_CHECK(candidate->kind > NOC_C_AST_EXPECTED_UNKNOWN);
            FUZZ_CHECK(candidate->kind <= NOC_C_AST_EXPECTED_EXPRESSION);
            FUZZ_CHECK((candidate->flags &
                        ~(NOC_C_GRAMMAR_CANDIDATE_LOOKAHEAD |
                          NOC_C_GRAMMAR_CANDIDATE_MISSING |
                          NOC_C_GRAMMAR_CANDIDATE_NON_C11)) == 0);
            FUZZ_CHECK((candidate->flags &
                        (NOC_C_GRAMMAR_CANDIDATE_LOOKAHEAD |
                         NOC_C_GRAMMAR_CANDIDATE_MISSING)) != 0);
            FUZZ_CHECK(candidate->spelling.count == 0 ||
                       candidate->spelling.data != NULL);
            if (candidate_index != 0) {
                FUZZ_CHECK(grammar_candidate_compare(
                               &candidates.items[candidate_index - 1],
                               candidate) < 0);
            }
            if ((candidate->flags & NOC_C_GRAMMAR_CANDIDATE_MISSING) != 0) {
                saw_missing = true;
            }
            hash = hash_size(hash, candidate->kind);
            hash = hash_size(hash, candidate->flags);
            hash = hash_slice(hash, candidate->spelling);
        }
        if (saw_missing) {
            bool found = false;
            size_t node_index;
            for (node_index = 0;
                 node_index < noc_c_parse_tree_node_count(tree);
                 ++node_index) {
                const Noc_C_Parse_Node *node =
                    noc_c_parse_tree_node_at(tree, node_index);
                if ((node->flags & NOC_C_PARSE_NODE_MISSING) != 0 &&
                    node->bytes.begin == offsets[offset_index] &&
                    node->bytes.end == offsets[offset_index]) {
                    found = true;
                    break;
                }
            }
            FUZZ_CHECK(found);
        }
        FUZZ_CHECK(noc_c_grammar_candidate_at(&candidates,
                                              candidates.count) == NULL);
        noc_c_grammar_candidates_free(&candidates);
    }
    return hash;
}

static void check_ast_topology(const Noc_C_Ast *ast,
                               const uint8_t *data,
                               size_t size)
{
    const Noc_Document_Snapshot *snapshot = noc_c_ast_snapshot(ast);
    Noc_Slice retained;
    size_t count = noc_c_ast_node_count(ast);
    bool visited[C_PARSER_FUZZ_MAX_NODES] = {false};
    size_t index;
    FUZZ_CHECK(noc_c_ast_is_valid(ast));
    FUZZ_CHECK(snapshot != NULL);
    retained = noc_document_snapshot_source(snapshot);
    FUZZ_CHECK(retained.count == size);
    FUZZ_CHECK(size == 0 || memcmp(retained.data, data, size) == 0);
    FUZZ_CHECK(noc_c_ast_root(ast) == 0);
    FUZZ_CHECK(count != 0);
    FUZZ_CHECK(count <= C_PARSER_FUZZ_MAX_NODES);
    FUZZ_CHECK(noc_c_ast_node_at(ast, count) == NULL);
    FUZZ_CHECK(noc_c_ast_node_source(ast, 0).count == size);
    FUZZ_CHECK(noc_c_ast_is_syntax_complete(ast) ==
               (noc_c_ast_issues(ast) == 0));
    visited[0] = true;

    for (index = 0; index < count; ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        Noc_C_Ast_Expected expected;
        Noc_Slice spelling;
        size_t child;
        size_t children = 0;
        size_t last_child = NOC_C_AST_NODE_NONE;
        FUZZ_CHECK(node != NULL);
        FUZZ_CHECK(node->generation == noc_c_ast_generation(ast));
        FUZZ_CHECK(node->bytes.begin <= node->bytes.end);
        FUZZ_CHECK(node->bytes.end <= size);
        FUZZ_CHECK(index == 0 ? node->parent == NOC_C_AST_NODE_NONE
                              : node->parent < index);
        spelling = noc_c_ast_node_source(ast, index);
        FUZZ_CHECK(spelling.count == node->bytes.end - node->bytes.begin);
        FUZZ_CHECK(spelling.data == retained.data + node->bytes.begin);
        FUZZ_CHECK(noc_c_ast_kind_name(node->kind) != NULL);
        FUZZ_CHECK(noc_c_ast_field_name(node->field) != NULL);
        expected = noc_c_ast_node_expected(ast, index);
        FUZZ_CHECK(expected.kind == NOC_C_AST_EXPECTED_NONE ||
                   (node->flags & NOC_C_AST_NODE_MISSING) != 0);
        child = node->first_child;
        while (child != NOC_C_AST_NODE_NONE) {
            const Noc_C_Ast_Node *child_node;
            FUZZ_CHECK(child < count);
            FUZZ_CHECK(!visited[child]);
            visited[child] = true;
            child_node = noc_c_ast_node_at(ast, child);
            FUZZ_CHECK(child_node != NULL);
            FUZZ_CHECK(child_node->parent == index);
            FUZZ_CHECK(child_node->bytes.begin >= node->bytes.begin);
            FUZZ_CHECK(child_node->bytes.end <= node->bytes.end);
            children += 1;
            FUZZ_CHECK(children <= node->child_count);
            last_child = child;
            child = child_node->next_sibling;
        }
        FUZZ_CHECK(children == node->child_count);
        FUZZ_CHECK(node->child_count == 0
                       ? node->last_child == NOC_C_AST_NODE_NONE
                       : node->last_child != NOC_C_AST_NODE_NONE);
        FUZZ_CHECK(last_child == node->last_child);
    }
    for (index = 0; index < count; ++index) FUZZ_CHECK(visited[index]);

    {
        size_t offsets[] = {0, size / 2, size};
        size_t offset_index;
        for (offset_index = 0;
             offset_index < sizeof(offsets) / sizeof(offsets[0]);
             ++offset_index) {
            Noc_C_Ast_Completion_Context context = {0};
            size_t expected_count = 0;
            size_t expected_index;
            FUZZ_CHECK(noc_c_ast_completion_context(ast,
                                                    offsets[offset_index],
                                                    &context));
            FUZZ_CHECK(context.offset == offsets[offset_index]);
            FUZZ_CHECK(context.node < count);
            FUZZ_CHECK(context.owner == ast);
            FUZZ_CHECK(context.left_node == NOC_C_AST_NODE_NONE ||
                       context.left_node < count);
            FUZZ_CHECK(context.right_node == NOC_C_AST_NODE_NONE ||
                       context.right_node < count);
            FUZZ_CHECK(context.left_node ==
                       (offsets[offset_index] == 0
                            ? NOC_C_AST_NODE_NONE
                            : noc_c_ast_node_at_offset(
                                  ast,
                                  offsets[offset_index] - 1)));
            FUZZ_CHECK(context.right_node ==
                       (offsets[offset_index] == size
                            ? NOC_C_AST_NODE_NONE
                            : noc_c_ast_node_at_offset(
                                  ast,
                                  offsets[offset_index])));
            FUZZ_CHECK(context.file_id ==
                       noc_document_snapshot_file_id(snapshot));
            FUZZ_CHECK(context.generation == noc_c_ast_generation(ast));
            FUZZ_CHECK(context.document_generation ==
                       noc_c_ast_document_generation(ast));
            for (expected_index = 0; expected_index < count; ++expected_index) {
                const Noc_C_Ast_Node *expected_node =
                    noc_c_ast_node_at(ast, expected_index);
                if ((expected_node->flags & NOC_C_AST_NODE_MISSING) != 0 &&
                    expected_node->bytes.begin == offsets[offset_index] &&
                    expected_node->bytes.end == offsets[offset_index]) {
                    expected_count += 1;
                }
            }
            FUZZ_CHECK(context.expected_count == expected_count);
            {
                size_t previous = NOC_C_AST_NODE_NONE;
                size_t enumerated = 0;
                for (;;) {
                    size_t expected =
                        noc_c_ast_completion_next_expected_node(ast,
                                                                &context,
                                                                previous);
                    if (expected == NOC_C_AST_NODE_NONE) break;
                    FUZZ_CHECK(expected < count);
                    FUZZ_CHECK(previous == NOC_C_AST_NODE_NONE ||
                               expected > previous);
                    FUZZ_CHECK((noc_c_ast_node_at(ast, expected)->flags &
                                NOC_C_AST_NODE_MISSING) != 0);
                    FUZZ_CHECK(noc_c_ast_node_at(ast, expected)->bytes.begin ==
                               offsets[offset_index]);
                    previous = expected;
                    enumerated += 1;
                    FUZZ_CHECK(enumerated <= expected_count);
                }
                FUZZ_CHECK(enumerated == expected_count);
            }
        }
    }
}

static uint64_t ast_hash(const Noc_C_Ast *ast)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t count = noc_c_ast_node_count(ast);
    size_t index;
    hash = hash_size(hash, count);
    hash = hash_size(hash, noc_c_ast_issues(ast));
    for (index = 0; index < count; ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        Noc_C_Ast_Expected expected = noc_c_ast_node_expected(ast, index);
        Noc_C_Ast_Type_Spelling type_spelling = {0};
        Noc_C_Ast_Array_Detail array_detail = {0};
        bool has_type = noc_c_ast_node_type_spelling(ast,
                                                     index,
                                                     &type_spelling);
        bool has_array = noc_c_ast_node_array_detail(ast,
                                                     index,
                                                     &array_detail);
        hash = hash_size(hash, node->kind);
        hash = hash_size(hash, node->field);
        hash = hash_size(hash, node->bytes.begin);
        hash = hash_size(hash, node->bytes.end);
        hash = hash_size(hash, node->parent);
        hash = hash_size(hash, node->first_child);
        hash = hash_size(hash, node->last_child);
        hash = hash_size(hash, node->next_sibling);
        hash = hash_size(hash, node->child_count);
        hash = hash_size(hash, node->flags);
        hash = hash_size(hash, noc_c_ast_node_operator(ast, index));
        hash = hash_size(hash, noc_c_ast_node_specifier(ast, index));
        hash = hash_size(hash, noc_c_ast_node_qualifier(ast, index));
        hash = hash_size(hash, noc_c_ast_node_extension(ast, index));
        hash = hash_size(hash, expected.kind);
        hash = hash_slice(hash, expected.spelling);
        hash = hash_size(hash, has_type);
        if (has_type) {
            hash = hash_size(hash, type_spelling.primitive);
            hash = hash_size(hash, type_spelling.flags);
            hash = hash_size(hash, type_spelling.long_count);
        }
        hash = hash_size(hash, has_array);
        if (has_array) {
            hash = hash_size(hash, array_detail.has_static_minimum);
            hash = hash_size(hash, array_detail.size);
        }
    }
    return hash;
}

static void fuzz_c_parser(const uint8_t *data, size_t size)
{
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    Noc_C_Ast ast = {0};
    Noc_C_Parse_Options parse_options = noc_c_parse_default_options();
    Noc_C_Parse_Status parse_status;
    uint64_t first_tree_hash;
    uint64_t first_candidates_hash;
    size_t first_tree_generation;

    noc_workspace_init(&workspace);
    FUZZ_CHECK(noc_workspace_open_document(&workspace,
                                           "memory://c-parser-fuzz.c",
                                           (const char *)data,
                                           size,
                                           NOC_SOURCE_CLASS_PROJECT,
                                           &snapshot) == NOC_WORKSPACE_OK);
    parse_options.max_source_bytes = size == 0 ? 1 : size;
    parse_options.max_nodes = C_PARSER_FUZZ_MAX_NODES;
    parse_status = noc_c_parse_tree_build(&snapshot, parse_options, &tree);
    FUZZ_CHECK(parse_status == NOC_C_PARSE_OK ||
               parse_status == NOC_C_PARSE_LIMIT_EXCEEDED ||
               parse_status == NOC_C_PARSE_OUT_OF_MEMORY);
    if (parse_status == NOC_C_PARSE_OK) {
        Noc_C_Ast_Options ast_options = noc_c_ast_default_options();
        Noc_C_Ast_Status ast_status;
        check_parse_topology(&tree, data, size);
        first_tree_hash = parse_tree_hash(&tree);
        first_candidates_hash = grammar_candidates_hash(&tree, size);
        first_tree_generation = noc_c_parse_tree_generation(&tree);

        ast_options.max_nodes = C_PARSER_FUZZ_MAX_NODES;
        ast_status = noc_c_ast_build(&tree, ast_options, &ast);
        FUZZ_CHECK(ast_status == NOC_C_AST_OK ||
                   ast_status == NOC_C_AST_OUT_OF_MEMORY);
        if (ast_status == NOC_C_AST_OK) {
            uint64_t first_ast_hash;
            size_t first_ast_generation;
            check_ast_topology(&ast, data, size);
            first_ast_hash = ast_hash(&ast);
            first_ast_generation = noc_c_ast_generation(&ast);

            FUZZ_CHECK(noc_c_parse_tree_build(&snapshot,
                                              parse_options,
                                              &tree) == NOC_C_PARSE_OK);
            FUZZ_CHECK(noc_c_parse_tree_generation(&tree) ==
                       first_tree_generation + 1);
            check_parse_topology(&tree, data, size);
            FUZZ_CHECK(parse_tree_hash(&tree) == first_tree_hash);
            FUZZ_CHECK(grammar_candidates_hash(&tree, size) ==
                       first_candidates_hash);
            FUZZ_CHECK(noc_c_ast_build(&tree, ast_options, &ast) ==
                       NOC_C_AST_OK);
            FUZZ_CHECK(noc_c_ast_generation(&ast) == first_ast_generation + 1);
            check_ast_topology(&ast, data, size);
            FUZZ_CHECK(ast_hash(&ast) == first_ast_hash);
        }
    }
    noc_c_ast_free(&ast);
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    fuzz_c_parser(data, size);
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
        "struct Bits { unsigned value : 3, : 0, rest : 2; };\n",
        "_Atomic(int) value; _Static_assert(sizeof(value) > 0, \"value\");\n",
        "int (*handler(int signal))(int);\n",
        "int function(int value) { return _Generic(value, int: value); }\n",
        "int function(void) { return 1 }\n",
        "int function(int value) { return value + ; }\n",
        "enum State { STATE_A, STATE_B = 2;\n",
        "#if FLAG\nint enabled;\n#else\nint disabled;\n#endif\n",
        "\vint skipped;\n",
    };
    static const uint8_t binary_seed[] = {0, 1, 2, 255};
    uint8_t bytes[512];
    uint32_t random_state = UINT32_C(0x43504152);
    size_t index;
    for (index = 0; index < sizeof(corpus) / sizeof(corpus[0]); ++index) {
        FUZZ_CHECK(LLVMFuzzerTestOneInput((const uint8_t *)corpus[index],
                                         strlen(corpus[index])) == 0);
    }
    FUZZ_CHECK(LLVMFuzzerTestOneInput(binary_seed, sizeof(binary_seed)) == 0);
    for (index = 0; index < 512; ++index) {
        size_t count = fuzz_random(&random_state) % (sizeof(bytes) + 1);
        size_t byte_index;
        for (byte_index = 0; byte_index < count; ++byte_index) {
            bytes[byte_index] = (uint8_t)fuzz_random(&random_state);
        }
        FUZZ_CHECK(LLVMFuzzerTestOneInput(bytes, count) == 0);
    }
    puts("noc C parser fuzz smoke passed");
    return 0;
}
#endif
