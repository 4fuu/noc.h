#include "test_support.h"

typedef struct {
    Noc_Workspace workspace;
    Noc_Document_Snapshot snapshot;
    Noc_C_Parse_Tree tree;
    Noc_C_Ast ast;
} C11_Construct_Fixture;

static void fixture_build(C11_Construct_Fixture *fixture,
                          const char *path,
                          const char *source,
                          size_t source_count)
{
    memset(fixture, 0, sizeof(*fixture));
    noc_workspace_init(&fixture->workspace);
    CHECK(noc_workspace_open_document(&fixture->workspace,
                                      path,
                                      source,
                                      source_count,
                                      NOC_SOURCE_CLASS_PROJECT,
                                      &fixture->snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&fixture->snapshot,
                                 noc_c_parse_default_options(),
                                 &fixture->tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_ast_build(&fixture->tree,
                          noc_c_ast_default_options(),
                          &fixture->ast) == NOC_C_AST_OK);
}

static void fixture_free(C11_Construct_Fixture *fixture)
{
    noc_c_ast_free(&fixture->ast);
    noc_c_parse_tree_free(&fixture->tree);
    noc_document_snapshot_free(&fixture->snapshot);
    noc_workspace_deinit(&fixture->workspace);
}

static size_t find_source(const Noc_C_Ast *ast,
                          Noc_C_Ast_Kind kind,
                          const char *source)
{
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        if (node && node->kind == kind &&
            slice_equals(noc_c_ast_node_source(ast, index), source)) {
            return index;
        }
    }
    return NOC_C_AST_NODE_NONE;
}

static size_t count_kind(const Noc_C_Ast *ast, Noc_C_Ast_Kind kind)
{
    size_t count = 0;
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        if (node && node->kind == kind) count += 1;
    }
    return count;
}

static size_t count_kind_source(const Noc_C_Ast *ast,
                                Noc_C_Ast_Kind kind,
                                const char *source)
{
    size_t count = 0;
    size_t index;
    for (index = 0; index < noc_c_ast_node_count(ast); ++index) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, index);
        if (node && node->kind == kind &&
            slice_equals(noc_c_ast_node_source(ast, index), source)) {
            count += 1;
        }
    }
    return count;
}

static size_t find_child_field(const Noc_C_Ast *ast,
                               size_t parent,
                               Noc_C_Ast_Field field)
{
    const Noc_C_Ast_Node *parent_node = noc_c_ast_node_at(ast, parent);
    size_t child = parent_node ? parent_node->first_child : NOC_C_AST_NODE_NONE;
    while (child != NOC_C_AST_NODE_NONE) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, child);
        if (node && node->field == field) return child;
        child = node ? node->next_sibling : NOC_C_AST_NODE_NONE;
    }
    return NOC_C_AST_NODE_NONE;
}

static size_t find_child_kind(const Noc_C_Ast *ast,
                              size_t parent,
                              Noc_C_Ast_Kind kind)
{
    const Noc_C_Ast_Node *parent_node = noc_c_ast_node_at(ast, parent);
    size_t child = parent_node ? parent_node->first_child : NOC_C_AST_NODE_NONE;
    while (child != NOC_C_AST_NODE_NONE) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, child);
        if (node && node->kind == kind) return child;
        child = node ? node->next_sibling : NOC_C_AST_NODE_NONE;
    }
    return NOC_C_AST_NODE_NONE;
}

static void check_kind_source(const Noc_C_Ast *ast,
                              Noc_C_Ast_Kind kind,
                              const char *source)
{
    CHECK(find_source(ast, kind, source) != NOC_C_AST_NODE_NONE);
}

static void test_c11_aggregate_initializer_and_expression_constructs(void)
{
    static const char source[] =
        "struct Pair {\n"
        "    _Atomic(int) first;\n"
        "    unsigned int bits : 3, : 0, more_bits : 2;\n"
        "};\n"
        "union Number { int integer; double real; };\n"
        "enum Color { COLOR_RED, COLOR_GREEN = 3 };\n"
        "_Alignas(_Atomic(int)) _Atomic(int) aligned_atomic;\n"
        "_Atomic int qualified_atomic;\n"
        "struct Pair pair = { .first = 1, .bits = 2 };\n"
        "int values[4] = { [1] = 7, [3] = 9 };\n"
        "int helper(int value) { return value; }\n"
        "int constructs(int input) {\n"
        "    _Atomic(int) local = 0;\n"
        "    struct Pair temporary = (struct Pair){ .first = input, .bits = 1 };\n"
        "    int selected = _Generic(input, int: input, default: 0);\n"
        "    _Alignas(16) char storage[16];\n"
        "    int result = input ? helper(sizeof temporary) : _Alignof(struct Pair);\n"
        "    result = (result, selected);\n"
        "start:\n"
        "    if (result < 0) { result = -result; } else { result += 1; }\n"
        "    while (local < 1) { local++; }\n"
        "    do { local--; } while (local > 0);\n"
        "    for (int index = 0; index < 2; ++index) {\n"
        "        if (index == input) continue;\n"
        "        result += values[index];\n"
        "    }\n"
        "    switch (input) {\n"
        "    case 0: result += 2; break;\n"
        "    default: goto done;\n"
        "    }\n"
        "    if (result == 42) goto start;\n"
        "done:\n"
        "    return result + temporary.first + storage[0];\n"
        "}\n";
    C11_Construct_Fixture fixture;
    size_t atomic;
    size_t atomic_type;
    size_t qualifier;

    fixture_build(&fixture,
                  "ast/c11-constructs.c",
                  source,
                  sizeof(source) - 1);
    CHECK(noc_c_ast_is_syntax_complete(&fixture.ast));
    CHECK(noc_c_ast_issues(&fixture.ast) == 0);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER) == 4);
    atomic = find_source(&fixture.ast,
                         NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER,
                         "_Atomic(int)");
    CHECK(atomic != NOC_C_AST_NODE_NONE);
    atomic_type = find_child_field(&fixture.ast, atomic, NOC_C_AST_FIELD_TYPE);
    CHECK(atomic_type != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_at(&fixture.ast, atomic_type)->kind ==
          NOC_C_AST_KIND_TYPE_DESCRIPTOR);
    CHECK(slice_equals(noc_c_ast_node_source(&fixture.ast, atomic_type), "int"));
    CHECK(noc_c_ast_node_extension(&fixture.ast, atomic) ==
          NOC_C_AST_EXTENSION_NONE);
    qualifier = find_source(&fixture.ast,
                            NOC_C_AST_KIND_TYPE_QUALIFIER,
                            "_Atomic");
    CHECK(qualifier != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_qualifier(&fixture.ast, qualifier) ==
          NOC_C_AST_QUALIFIER_ATOMIC);
    CHECK(noc_c_ast_node_extension(&fixture.ast, qualifier) ==
          NOC_C_AST_EXTENSION_NONE);

    check_kind_source(&fixture.ast,
                      NOC_C_AST_KIND_ALIGNAS_QUALIFIER,
                      "_Alignas(_Atomic(int))");
    check_kind_source(&fixture.ast,
                      NOC_C_AST_KIND_STRUCT_SPECIFIER,
                      "struct Pair");
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_UNION_SPECIFIER) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_ENUM_SPECIFIER) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_BITFIELD_CLAUSE) == 3);
    check_kind_source(&fixture.ast,
                      NOC_C_AST_KIND_BITFIELD_CLAUSE,
                      ": 0");
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_FIELD_DESIGNATOR) >= 4);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_SUBSCRIPT_DESIGNATOR) == 2);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_INITIALIZER_PAIR) >= 6);
    check_kind_source(&fixture.ast,
                      NOC_C_AST_KIND_COMPOUND_LITERAL_EXPRESSION,
                      "(struct Pair){ .first = input, .bits = 1 }");
    check_kind_source(&fixture.ast,
                      NOC_C_AST_KIND_GENERIC_EXPRESSION,
                      "_Generic(input, int: input, default: 0)");
    check_kind_source(&fixture.ast,
                      NOC_C_AST_KIND_CONDITIONAL_EXPRESSION,
                      "input ? helper(sizeof temporary) : _Alignof(struct Pair)");
    check_kind_source(&fixture.ast,
                      NOC_C_AST_KIND_COMMA_EXPRESSION,
                      "result, selected");
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_CALL_EXPRESSION) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_SIZEOF_EXPRESSION) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_ALIGNOF_EXPRESSION) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_IF_STATEMENT) >= 3);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_WHILE_STATEMENT) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_DO_STATEMENT) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_FOR_STATEMENT) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_SWITCH_STATEMENT) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_CASE_STATEMENT) == 2);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_BREAK_STATEMENT) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_CONTINUE_STATEMENT) == 1);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_GOTO_STATEMENT) == 2);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_LABELED_STATEMENT) == 2);
    CHECK(strcmp(noc_c_ast_kind_name(NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER),
                 "atomic_type_specifier") == 0);
    fixture_free(&fixture);
}

static void test_atomic_type_names_and_nested_syntax(void)
{
    static const char source[] =
        "typedef int AtomicBase;\n"
        "_Atomic(AtomicBase) alias_atomic;\n"
        "_Atomic(int *) pointer_atomic;\n"
        "int atomic_size(void) { return sizeof(_Atomic(AtomicBase)); }\n"
        "_Atomic(_Atomic(int)) nested_atomic;\n";
    C11_Construct_Fixture fixture;
    size_t alias;
    size_t alias_type;
    size_t pointer;
    size_t pointer_type;
    size_t outer;
    size_t outer_type;

    fixture_build(&fixture,
                  "ast/c11-atomic-type-names.c",
                  source,
                  sizeof(source) - 1);
    CHECK(noc_c_ast_is_syntax_complete(&fixture.ast));
    CHECK(noc_c_ast_issues(&fixture.ast) == 0);

    /* type_descriptor, rather than a bare type specifier, is required here:
       typedef names and abstract declarators are valid type-name syntax. */
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER) == 5);
    CHECK(count_kind(&fixture.ast, NOC_C_AST_KIND_MACRO_TYPE_SPECIFIER) == 0);
    CHECK(count_kind_source(&fixture.ast,
                            NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER,
                            "_Atomic(AtomicBase)") == 2);
    alias = find_source(&fixture.ast,
                        NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER,
                        "_Atomic(AtomicBase)");
    alias_type = find_child_field(&fixture.ast, alias, NOC_C_AST_FIELD_TYPE);
    CHECK(alias_type != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_at(&fixture.ast, alias_type)->kind ==
          NOC_C_AST_KIND_TYPE_DESCRIPTOR);
    CHECK(slice_equals(noc_c_ast_node_source(&fixture.ast, alias_type),
                       "AtomicBase"));

    pointer = find_source(&fixture.ast,
                          NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER,
                          "_Atomic(int *)");
    pointer_type = find_child_field(&fixture.ast, pointer, NOC_C_AST_FIELD_TYPE);
    CHECK(pointer_type != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_at(&fixture.ast, pointer_type)->kind ==
          NOC_C_AST_KIND_TYPE_DESCRIPTOR);
    CHECK(slice_equals(noc_c_ast_node_source(&fixture.ast, pointer_type),
                       "int *"));
    CHECK(find_source(&fixture.ast,
                      NOC_C_AST_KIND_ABSTRACT_POINTER_DECLARATOR,
                      "*") != NOC_C_AST_NODE_NONE);

    /* Nested atomic syntax is representable by the grammar. ISO C11 forbids
       an atomic type as the operand; semantic analysis must diagnose it. */
    outer = find_source(&fixture.ast,
                        NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER,
                        "_Atomic(_Atomic(int))");
    outer_type = find_child_field(&fixture.ast, outer, NOC_C_AST_FIELD_TYPE);
    CHECK(outer_type != NOC_C_AST_NODE_NONE);
    CHECK(find_child_kind(&fixture.ast,
                          outer_type,
                          NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER) !=
          NOC_C_AST_NODE_NONE);
    fixture_free(&fixture);
}

static void test_atomic_type_recovery_is_not_silently_complete(void)
{
    static const char *const malformed[] = {
        "_Atomic() empty_atomic;\n",
        "_Atomic(int broken_atomic;\n",
        "_Atomic(int) missing_semicolon\n",
    };
    size_t case_index;
    for (case_index = 0;
         case_index < sizeof(malformed) / sizeof(malformed[0]);
         ++case_index) {
        C11_Construct_Fixture fixture;
        fixture_build(&fixture,
                      "ast/c11-atomic-recovery.c",
                      malformed[case_index],
                      strlen(malformed[case_index]));
        CHECK(!noc_c_ast_is_syntax_complete(&fixture.ast));
        CHECK((noc_c_ast_issues(&fixture.ast) &
               (NOC_C_AST_ISSUE_PARSE_ERROR | NOC_C_AST_ISSUE_MISSING)) != 0);
        fixture_free(&fixture);
    }
}

int main(void)
{
    test_c11_aggregate_initializer_and_expression_constructs();
    test_atomic_type_names_and_nested_syntax();
    test_atomic_type_recovery_is_not_silently_complete();
    return finish_suite("C AST C11 constructs");
}
