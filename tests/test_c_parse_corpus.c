#include "test_support.h"

typedef struct {
    const char *name;
    const char *source;
} Corpus_Case;

typedef struct {
    Noc_Document_Snapshot snapshot;
    Noc_C_Parse_Tree tree;
    Noc_C_Ast ast;
} Corpus_Parse;

static bool slices_equal(Noc_Slice left, Noc_Slice right)
{
    return left.count == right.count &&
           (left.count == 0 ||
            (left.data && right.data &&
             memcmp(left.data, right.data, left.count) == 0));
}

static void corpus_parse(Noc_Workspace *workspace,
                         const char *path,
                         const char *source,
                         size_t source_count,
                         Corpus_Parse *output)
{
    memset(output, 0, sizeof(*output));
    CHECK(noc_workspace_open_document(workspace,
                                      path,
                                      source,
                                      source_count,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &output->snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&output->snapshot,
                                 noc_c_parse_default_options(),
                                 &output->tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&output->tree,
                          noc_c_ast_default_options(),
                          &output->ast) == NOC_C_AST_OK);
}

static void corpus_parse_free(Corpus_Parse *parsed)
{
    noc_c_ast_free(&parsed->ast);
    noc_c_parse_tree_free(&parsed->tree);
    noc_document_snapshot_free(&parsed->snapshot);
}

static void compare_parse_trees(const char *case_name,
                                const Noc_C_Parse_Tree *left,
                                const Noc_C_Parse_Tree *right)
{
    size_t count = noc_c_parse_tree_node_count(left);
    size_t index;
    if (noc_c_parse_tree_has_error(left) ||
        noc_c_parse_tree_has_error(right)) {
        fprintf(stderr, "corpus case `%s` contains parser recovery\n", case_name);
        for (index = 0; index < count; ++index) {
            const Noc_C_Parse_Node *node =
                noc_c_parse_tree_node_at(left, index);
            if (node &&
                (node->flags & (NOC_C_PARSE_NODE_ERROR |
                                NOC_C_PARSE_NODE_MISSING |
                                NOC_C_PARSE_NODE_SKIPPED_SOURCE)) != 0) {
                Noc_Slice spelling = noc_c_parse_node_source(left, index);
                fprintf(stderr,
                        "  %.*s [%zu,%zu): `%.*s`\n",
                        (int)node->kind.count,
                        node->kind.data,
                        node->bytes.begin,
                        node->bytes.end,
                        (int)spelling.count,
                        spelling.data ? spelling.data : "");
            }
        }
    }
    CHECK(noc_c_parse_tree_is_valid(left));
    CHECK(noc_c_parse_tree_is_valid(right));
    CHECK(!noc_c_parse_tree_has_error(left));
    CHECK(!noc_c_parse_tree_has_error(right));
    CHECK(count == noc_c_parse_tree_node_count(right));
    for (index = 0; index < count; ++index) {
        const Noc_C_Parse_Node *left_node =
            noc_c_parse_tree_node_at(left, index);
        const Noc_C_Parse_Node *right_node =
            noc_c_parse_tree_node_at(right, index);
        CHECK(left_node != NULL);
        CHECK(right_node != NULL);
        if (!left_node || !right_node) continue;
        CHECK(left_node->bytes.begin == right_node->bytes.begin);
        CHECK(left_node->bytes.end == right_node->bytes.end);
        CHECK(left_node->parent == right_node->parent);
        CHECK(left_node->first_child == right_node->first_child);
        CHECK(left_node->last_child == right_node->last_child);
        CHECK(left_node->next_sibling == right_node->next_sibling);
        CHECK(left_node->child_count == right_node->child_count);
        CHECK(left_node->flags == right_node->flags);
        CHECK(slices_equal(left_node->kind, right_node->kind));
        CHECK(slices_equal(left_node->field, right_node->field));
        CHECK(slices_equal(noc_c_parse_node_source(left, index),
                           noc_c_parse_node_source(right, index)));
    }
}

static void compare_asts(const char *case_name,
                         const Noc_C_Ast *left,
                         const Noc_C_Ast *right)
{
    size_t count = noc_c_ast_node_count(left);
    size_t index;
    if (!noc_c_ast_is_syntax_complete(left) ||
        !noc_c_ast_is_syntax_complete(right)) {
        fprintf(stderr, "corpus case `%s` has incomplete normalized AST\n",
                case_name);
    }
    CHECK(noc_c_ast_is_valid(left));
    CHECK(noc_c_ast_is_valid(right));
    CHECK(noc_c_ast_is_syntax_complete(left));
    CHECK(noc_c_ast_is_syntax_complete(right));
    CHECK(noc_c_ast_issues(left) == 0);
    CHECK(noc_c_ast_issues(right) == 0);
    CHECK(count == noc_c_ast_node_count(right));
    for (index = 0; index < count; ++index) {
        const Noc_C_Ast_Node *left_node = noc_c_ast_node_at(left, index);
        const Noc_C_Ast_Node *right_node = noc_c_ast_node_at(right, index);
        Noc_C_Ast_Type_Spelling left_type = {0};
        Noc_C_Ast_Type_Spelling right_type = {0};
        Noc_C_Ast_Array_Detail left_array = {0};
        Noc_C_Ast_Array_Detail right_array = {0};
        bool has_left_type;
        bool has_right_type;
        bool has_left_array;
        bool has_right_array;
        CHECK(left_node != NULL);
        CHECK(right_node != NULL);
        if (!left_node || !right_node) continue;
        CHECK(left_node->kind != NOC_C_AST_KIND_UNKNOWN);
        CHECK(left_node->field != NOC_C_AST_FIELD_UNKNOWN);
        CHECK(left_node->kind == right_node->kind);
        CHECK(left_node->field == right_node->field);
        CHECK(left_node->bytes.begin == right_node->bytes.begin);
        CHECK(left_node->bytes.end == right_node->bytes.end);
        CHECK(left_node->parent == right_node->parent);
        CHECK(left_node->first_child == right_node->first_child);
        CHECK(left_node->last_child == right_node->last_child);
        CHECK(left_node->next_sibling == right_node->next_sibling);
        CHECK(left_node->child_count == right_node->child_count);
        CHECK(left_node->flags == right_node->flags);
        CHECK(noc_c_ast_node_operator(left, index) ==
              noc_c_ast_node_operator(right, index));
        CHECK(noc_c_ast_node_specifier(left, index) ==
              noc_c_ast_node_specifier(right, index));
        CHECK(noc_c_ast_node_qualifier(left, index) ==
              noc_c_ast_node_qualifier(right, index));
        CHECK(noc_c_ast_node_extension(left, index) ==
              noc_c_ast_node_extension(right, index));
        CHECK(slices_equal(noc_c_ast_node_source(left, index),
                           noc_c_ast_node_source(right, index)));

        has_left_type = noc_c_ast_node_type_spelling(left,
                                                     index,
                                                     &left_type);
        has_right_type = noc_c_ast_node_type_spelling(right,
                                                      index,
                                                      &right_type);
        CHECK(has_left_type == has_right_type);
        if (has_left_type && has_right_type) {
            CHECK(left_type.primitive == right_type.primitive);
            CHECK(left_type.flags == right_type.flags);
            CHECK(left_type.long_count == right_type.long_count);
        }
        has_left_array = noc_c_ast_node_array_detail(left,
                                                     index,
                                                     &left_array);
        has_right_array = noc_c_ast_node_array_detail(right,
                                                      index,
                                                      &right_array);
        CHECK(has_left_array == has_right_array);
        if (has_left_array && has_right_array) {
            CHECK(left_array.has_static_minimum ==
                  right_array.has_static_minimum);
            CHECK(left_array.size == right_array.size);
        }
    }
}

static void test_iso_c11_corpus_round_trip(void)
{
    static const Corpus_Case cases[] = {
        {
            "declarations",
            "typedef unsigned long Size;\n"
            "extern const int external_value;\n"
            "static _Thread_local _Atomic(int) counter;\n"
            "int matrix[4][8];\n"
            "int (*signal_handler(int signal, int (*handler)(int)))(int);\n",
        },
        {
            "aggregates",
            "struct Packet { unsigned kind : 3; unsigned int : 0; char data[]; };\n"
            "union Scalar { long integer; double real; };\n"
            "enum State { STATE_IDLE, STATE_BUSY = 4, STATE_DONE };\n"
            "struct Packet packet = { .kind = 2 };\n"
            "char buffer[4] = { [0] = 'A' };\n",
        },
        {
            "expressions-and-statements",
            "int evaluate(int *values, int count) {\n"
            "    int result = 0;\n"
            "    for (int index = 0; index < count; ++index) {\n"
            "        if (values[index] < 0) continue;\n"
            "        result += values[index] ? values[index] : 1;\n"
            "    }\n"
            "    switch (result) { case 0: return 1; default: break; }\n"
            "    do { result--; } while (result > 10);\n"
            "    return result;\n"
            "}\n",
        },
        {
            "c11-constructs",
            "typedef int AtomicBase;\n"
            "_Alignas(_Atomic(AtomicBase)) _Atomic(AtomicBase) shared_value;\n"
            "_Static_assert(sizeof(_Atomic(int)) > 0, \"nonempty atomic\");\n"
            "int select_value(int value) {\n"
            "    return _Generic(value, int: value, default: 0);\n"
            "}\n",
        },
        {
            "preprocessing",
            "#define SCALE(value) ((value) * 2)\n"
            "#if defined(ENABLE_VALUES)\n"
            "static int configured = SCALE(3);\n"
            "#else\n"
            "static int configured = 0;\n"
            "#endif\n"
            "int read_configured(void) { return configured; }\n",
        },
        {
            "trivia-and-crlf",
            "/* Physical spelling must survive exactly. */\r\n"
            "int joined_name = 1; // trailing comment\r\n"
            "const char *message = \"line\\ntext\";\r\n",
        },
    };
    size_t case_index;

    for (case_index = 0;
         case_index < sizeof(cases) / sizeof(cases[0]);
         ++case_index) {
        Noc_Workspace workspace = {0};
        Corpus_Parse first;
        Corpus_Parse second;
        Noc_Slice physical;
        char first_path[128];
        char second_path[128];

        snprintf(first_path,
                 sizeof(first_path),
                 "corpus/%s.c",
                 cases[case_index].name);
        snprintf(second_path,
                 sizeof(second_path),
                 "corpus/%s-round-trip.c",
                 cases[case_index].name);
        noc_workspace_init(&workspace);
        corpus_parse(&workspace,
                     first_path,
                     cases[case_index].source,
                     strlen(cases[case_index].source),
                     &first);
        physical = noc_c_parse_node_source(&first.tree,
                                           noc_c_parse_tree_root(&first.tree));
        CHECK(slice_equals(physical, cases[case_index].source));
        corpus_parse(&workspace,
                     second_path,
                     physical.data,
                     physical.count,
                     &second);
        compare_parse_trees(cases[case_index].name,
                            &first.tree,
                            &second.tree);
        compare_asts(cases[case_index].name, &first.ast, &second.ast);
        corpus_parse_free(&first);
        corpus_parse_free(&second);
        noc_workspace_deinit(&workspace);
    }
}

int main(void)
{
    test_iso_c11_corpus_round_trip();
    return finish_suite("C11 parser corpus round-trip");
}
