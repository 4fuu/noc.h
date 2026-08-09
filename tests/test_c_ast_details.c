#include "test_support.h"

typedef struct {
    Noc_Workspace workspace;
    Noc_Document_Snapshot snapshot;
    Noc_C_Parse_Tree tree;
    Noc_C_Ast ast;
} Ast_Fixture;

static void fixture_build(Ast_Fixture *fixture,
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
    CHECK(noc_c_ast_is_syntax_complete(&fixture->ast));
}

static void fixture_free(Ast_Fixture *fixture)
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

static void check_operator(const Noc_C_Ast *ast,
                           Noc_C_Ast_Kind kind,
                           const char *source,
                           Noc_C_Ast_Operator expected)
{
    size_t index = find_source(ast, kind, source);
    if (index == NOC_C_AST_NODE_NONE) {
        fprintf(stderr,
                "missing %s node for operator source `%s`\n",
                noc_c_ast_kind_name(kind),
                source);
    } else if (noc_c_ast_node_operator(ast, index) != expected) {
        fprintf(stderr,
                "operator source `%s`: expected %s, got %s\n",
                source,
                noc_c_ast_operator_name(expected),
                noc_c_ast_operator_name(noc_c_ast_node_operator(ast, index)));
    }
    CHECK(index != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_operator(ast, index) == expected);
}

static void test_contextual_operator_classification(void)
{
    static const char source[] =
        "struct S { int member; };\n"
        "int operators(int a, int b, int *p, struct S s, struct S *sp) {\n"
        "    a = b; a += b; a -= b; a *= b; a /= b; a %= b;\n"
        "    a <<= b; a >>= b; a &= b; a ^= b; a |= b;\n"
        "    a + b; a - b; (a * b); a / b; a % b;\n"
        "    a & b; a | b; a ^ b; a && b; a || b;\n"
        "    a == b; a != b; a < b; a <= b; a > b; a >= b;\n"
        "    a << b; a >> b; !a; ~a; +a; -a; &a; *p;\n"
        "    s.member; sp->member; ++a; --a; a++; a--;\n"
        "    return a;\n"
        "}\n";
    static const struct {
        Noc_C_Ast_Kind kind;
        const char *source;
        Noc_C_Ast_Operator expected;
    } cases[] = {
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a = b", NOC_C_AST_OPERATOR_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a += b", NOC_C_AST_OPERATOR_ADD_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a -= b", NOC_C_AST_OPERATOR_SUBTRACT_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a *= b", NOC_C_AST_OPERATOR_MULTIPLY_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a /= b", NOC_C_AST_OPERATOR_DIVIDE_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a %= b", NOC_C_AST_OPERATOR_REMAINDER_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a <<= b", NOC_C_AST_OPERATOR_SHIFT_LEFT_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a >>= b", NOC_C_AST_OPERATOR_SHIFT_RIGHT_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a &= b", NOC_C_AST_OPERATOR_BIT_AND_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a ^= b", NOC_C_AST_OPERATOR_BIT_XOR_ASSIGN},
        {NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION, "a |= b", NOC_C_AST_OPERATOR_BIT_OR_ASSIGN},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a + b", NOC_C_AST_OPERATOR_ADD},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a - b", NOC_C_AST_OPERATOR_SUBTRACT},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a * b", NOC_C_AST_OPERATOR_MULTIPLY},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a / b", NOC_C_AST_OPERATOR_DIVIDE},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a % b", NOC_C_AST_OPERATOR_REMAINDER},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a & b", NOC_C_AST_OPERATOR_BIT_AND},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a | b", NOC_C_AST_OPERATOR_BIT_OR},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a ^ b", NOC_C_AST_OPERATOR_BIT_XOR},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a && b", NOC_C_AST_OPERATOR_LOGICAL_AND},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a || b", NOC_C_AST_OPERATOR_LOGICAL_OR},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a == b", NOC_C_AST_OPERATOR_EQUAL},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a != b", NOC_C_AST_OPERATOR_NOT_EQUAL},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a < b", NOC_C_AST_OPERATOR_LESS},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a <= b", NOC_C_AST_OPERATOR_LESS_EQUAL},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a > b", NOC_C_AST_OPERATOR_GREATER},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a >= b", NOC_C_AST_OPERATOR_GREATER_EQUAL},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a << b", NOC_C_AST_OPERATOR_SHIFT_LEFT},
        {NOC_C_AST_KIND_BINARY_EXPRESSION, "a >> b", NOC_C_AST_OPERATOR_SHIFT_RIGHT},
        {NOC_C_AST_KIND_UNARY_EXPRESSION, "!a", NOC_C_AST_OPERATOR_LOGICAL_NOT},
        {NOC_C_AST_KIND_UNARY_EXPRESSION, "~a", NOC_C_AST_OPERATOR_BIT_NOT},
        {NOC_C_AST_KIND_UNARY_EXPRESSION, "+a", NOC_C_AST_OPERATOR_POSITIVE},
        {NOC_C_AST_KIND_UNARY_EXPRESSION, "-a", NOC_C_AST_OPERATOR_NEGATIVE},
        {NOC_C_AST_KIND_POINTER_EXPRESSION, "&a", NOC_C_AST_OPERATOR_ADDRESS},
        {NOC_C_AST_KIND_POINTER_EXPRESSION, "*p", NOC_C_AST_OPERATOR_DEREFERENCE},
        {NOC_C_AST_KIND_FIELD_EXPRESSION, "s.member", NOC_C_AST_OPERATOR_MEMBER},
        {NOC_C_AST_KIND_FIELD_EXPRESSION, "sp->member", NOC_C_AST_OPERATOR_POINTER_MEMBER},
        {NOC_C_AST_KIND_UPDATE_EXPRESSION, "++a", NOC_C_AST_OPERATOR_PREFIX_INCREMENT},
        {NOC_C_AST_KIND_UPDATE_EXPRESSION, "--a", NOC_C_AST_OPERATOR_PREFIX_DECREMENT},
        {NOC_C_AST_KIND_UPDATE_EXPRESSION, "a++", NOC_C_AST_OPERATOR_POSTFIX_INCREMENT},
        {NOC_C_AST_KIND_UPDATE_EXPRESSION, "a--", NOC_C_AST_OPERATOR_POSTFIX_DECREMENT},
    };
    Ast_Fixture fixture;
    size_t index;

    fixture_build(&fixture,
                  "ast/operators.c",
                  source,
                  sizeof(source) - 1);
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        check_operator(&fixture.ast,
                       cases[index].kind,
                       cases[index].source,
                       cases[index].expected);
    }
    CHECK(noc_c_ast_node_operator(&fixture.ast, SIZE_MAX) ==
          NOC_C_AST_OPERATOR_NONE);
    fixture_free(&fixture);
}

static void test_array_declarator_details(void)
{
    static const char source[] =
        "void arrays(int fixed[10], int minimum[static 10], "
        "int unspecified[], int variable[*]);\n";
    static const struct {
        const char *source;
        bool has_static_minimum;
        Noc_C_Ast_Array_Size size;
    } cases[] = {
        {"fixed[10]", false, NOC_C_AST_ARRAY_SIZE_EXPRESSION},
        {"minimum[static 10]", true, NOC_C_AST_ARRAY_SIZE_EXPRESSION},
        {"unspecified[]", false, NOC_C_AST_ARRAY_SIZE_NONE},
        {"variable[*]", false, NOC_C_AST_ARRAY_SIZE_STAR},
    };
    Ast_Fixture fixture;
    Noc_C_Ast_Array_Detail detail = {0};
    size_t case_index;

    fixture_build(&fixture,
                  "ast/arrays.c",
                  source,
                  sizeof(source) - 1);
    for (case_index = 0;
         case_index < sizeof(cases) / sizeof(cases[0]);
         ++case_index) {
        size_t node = find_source(&fixture.ast,
                                  NOC_C_AST_KIND_ARRAY_DECLARATOR,
                                  cases[case_index].source);
        CHECK(node != NOC_C_AST_NODE_NONE);
        CHECK(noc_c_ast_node_array_detail(&fixture.ast, node, &detail));
        CHECK(detail.has_static_minimum ==
              cases[case_index].has_static_minimum);
        CHECK(detail.size == cases[case_index].size);
    }
    CHECK(!noc_c_ast_node_array_detail(&fixture.ast, SIZE_MAX, &detail));
    CHECK(!noc_c_ast_node_array_detail(&fixture.ast, 0, NULL));
    fixture_free(&fixture);
}

static void check_specifier(const Noc_C_Ast *ast,
                            Noc_C_Ast_Kind kind,
                            const char *source,
                            Noc_C_Ast_Specifier expected,
                            Noc_C_Ast_Extension extension)
{
    size_t index = find_source(ast, kind, source);
    CHECK(index != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_specifier(ast, index) == expected);
    CHECK(noc_c_ast_node_extension(ast, index) == extension);
}

static void test_specifiers_qualifiers_types_and_extensions(void)
{
    static const char source[] =
        "typedef int Alias;\n"
        "extern int external_value;\n"
        "static int static_value;\n"
        "inline int inline_fn(void) { return 0; }\n"
        "__inline int gnu_inline_fn(void) { return 0; }\n"
        "__inline__ int gnu_inline_alt_fn(void) { return 0; }\n"
        "__forceinline int force_inline_fn(void) { return 0; }\n"
        "thread_local int c23_tls;\n"
        "__thread int gnu_tls;\n"
        "const int const_value = 0;\n"
        "volatile int volatile_value;\n"
        "_Atomic int atomic_value;\n"
        "_Noreturn void c11_noreturn(void);\n"
        "noreturn void c23_noreturn(void);\n"
        "constexpr int c23_constant = 1;\n"
        "_Alignas(8) int c11_aligned;\n"
        "alignas(16) int c23_aligned;\n"
        "int * restrict restricted_pointer;\n"
        "int * __restrict__ gnu_restricted_pointer;\n"
        "int * _Nonnull clang_nonnull_pointer;\n"
        "unsigned long long int wide_value;\n"
        "signed short int short_value;\n"
        "bool c23_bool;\n"
        "__attribute__((unused)) int gnu_attribute_value;\n"
        "[[deprecated]] int c23_attribute_value;\n"
        "int gnu_alignment = __alignof__(int);\n"
        "int gnu_alignment_alt = __alignof(int);\n"
        "int c23_alignment = alignof(int);\n"
        "int c11_alignment = _Alignof(int);\n"
        "__declspec(dllexport) int ms_export;\n"
        "int __cdecl ms_call(void);\n"
        "void c23_literals(void) { true; false; nullptr; }\n"
        "void macro_like_names(void) { TRUE; FALSE; NULL; }\n"
        "void local_storage(void) { auto int automatic; register int reg; }\n";
    Ast_Fixture fixture;
    Noc_C_Ast_Type_Spelling type_spelling = {0};
    size_t node;

    fixture_build(&fixture,
                  "ast/details.c",
                  source,
                  sizeof(source) - 1);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_TYPE_DEFINITION,
                    "typedef int Alias;",
                    NOC_C_AST_SPECIFIER_TYPEDEF,
                    NOC_C_AST_EXTENSION_NONE);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "extern",
                    NOC_C_AST_SPECIFIER_EXTERN,
                    NOC_C_AST_EXTENSION_NONE);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "static",
                    NOC_C_AST_SPECIFIER_STATIC,
                    NOC_C_AST_EXTENSION_NONE);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "inline",
                    NOC_C_AST_SPECIFIER_INLINE,
                    NOC_C_AST_EXTENSION_NONE);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "__inline",
                    NOC_C_AST_SPECIFIER_GNU_INLINE,
                    NOC_C_AST_EXTENSION_GNU_INLINE);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "__inline__",
                    NOC_C_AST_SPECIFIER_GNU_INLINE_ALT,
                    NOC_C_AST_EXTENSION_GNU_INLINE_ALT);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "__forceinline",
                    NOC_C_AST_SPECIFIER_MS_FORCE_INLINE,
                    NOC_C_AST_EXTENSION_MS_FORCE_INLINE);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "thread_local",
                    NOC_C_AST_SPECIFIER_C23_THREAD_LOCAL,
                    NOC_C_AST_EXTENSION_C23_THREAD_LOCAL);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "__thread",
                    NOC_C_AST_SPECIFIER_GNU_THREAD_LOCAL,
                    NOC_C_AST_EXTENSION_GNU_THREAD_LOCAL);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "auto",
                    NOC_C_AST_SPECIFIER_AUTO,
                    NOC_C_AST_EXTENSION_NONE);
    check_specifier(&fixture.ast,
                    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
                    "register",
                    NOC_C_AST_SPECIFIER_REGISTER,
                    NOC_C_AST_EXTENSION_NONE);

#define CHECK_QUALIFIER(spelling, expected, expected_extension)                 \
    do {                                                                        \
        node = find_source(&fixture.ast, NOC_C_AST_KIND_TYPE_QUALIFIER,         \
                           (spelling));                                          \
        CHECK(node != NOC_C_AST_NODE_NONE);                                     \
        CHECK(noc_c_ast_node_qualifier(&fixture.ast, node) == (expected));       \
        CHECK(noc_c_ast_node_extension(&fixture.ast, node) ==                   \
              (expected_extension));                                            \
    } while (0)
    CHECK_QUALIFIER("const", NOC_C_AST_QUALIFIER_CONST,
                    NOC_C_AST_EXTENSION_NONE);
    CHECK_QUALIFIER("volatile", NOC_C_AST_QUALIFIER_VOLATILE,
                    NOC_C_AST_EXTENSION_NONE);
    CHECK_QUALIFIER("_Atomic", NOC_C_AST_QUALIFIER_ATOMIC,
                    NOC_C_AST_EXTENSION_NONE);
    CHECK_QUALIFIER("_Noreturn", NOC_C_AST_QUALIFIER_NORETURN,
                    NOC_C_AST_EXTENSION_NONE);
    CHECK_QUALIFIER("noreturn", NOC_C_AST_QUALIFIER_C23_NORETURN,
                    NOC_C_AST_EXTENSION_C23_NORETURN);
    CHECK_QUALIFIER("constexpr", NOC_C_AST_QUALIFIER_C23_CONSTEXPR,
                    NOC_C_AST_EXTENSION_C23_CONSTEXPR);
    CHECK_QUALIFIER("_Alignas(8)", NOC_C_AST_QUALIFIER_C11_ALIGNAS,
                    NOC_C_AST_EXTENSION_NONE);
    CHECK_QUALIFIER("alignas(16)", NOC_C_AST_QUALIFIER_C23_ALIGNAS,
                    NOC_C_AST_EXTENSION_C23_ALIGNAS);
    CHECK_QUALIFIER("restrict", NOC_C_AST_QUALIFIER_RESTRICT,
                    NOC_C_AST_EXTENSION_NONE);
    CHECK_QUALIFIER("__restrict__", NOC_C_AST_QUALIFIER_GNU_RESTRICT,
                    NOC_C_AST_EXTENSION_GNU_RESTRICT);
    CHECK_QUALIFIER("_Nonnull", NOC_C_AST_QUALIFIER_CLANG_NONNULL,
                    NOC_C_AST_EXTENSION_CLANG_NONNULL);
#undef CHECK_QUALIFIER

    node = find_source(&fixture.ast,
                       NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER,
                       "unsigned long long int");
    CHECK(node != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_type_spelling(&fixture.ast, node, &type_spelling));
    CHECK(type_spelling.primitive == NOC_C_AST_PRIMITIVE_INT);
    CHECK(type_spelling.flags == NOC_C_AST_TYPE_UNSIGNED);
    CHECK(type_spelling.long_count == 2);
    node = find_source(&fixture.ast,
                       NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER,
                       "signed short int");
    CHECK(node != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_type_spelling(&fixture.ast, node, &type_spelling));
    CHECK(type_spelling.primitive == NOC_C_AST_PRIMITIVE_INT);
    CHECK(type_spelling.flags ==
          (NOC_C_AST_TYPE_SIGNED | NOC_C_AST_TYPE_SHORT));
    CHECK(type_spelling.long_count == 0);
    node = find_source(&fixture.ast, NOC_C_AST_KIND_PRIMITIVE_TYPE, "bool");
    CHECK(node != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_type_spelling(&fixture.ast, node, &type_spelling));
    CHECK(type_spelling.primitive == NOC_C_AST_PRIMITIVE_C23_BOOL);
    CHECK(noc_c_ast_node_extension(&fixture.ast, node) ==
          NOC_C_AST_EXTENSION_C23_BOOL);
    CHECK(!noc_c_ast_node_type_spelling(&fixture.ast, SIZE_MAX, &type_spelling));
    CHECK(!noc_c_ast_node_type_spelling(&fixture.ast, node, NULL));

#define CHECK_EXTENSION(kind, spelling, expected)                              \
    do {                                                                        \
        node = find_source(&fixture.ast, (kind), (spelling));                   \
        CHECK(node != NOC_C_AST_NODE_NONE);                                     \
        CHECK(noc_c_ast_node_extension(&fixture.ast, node) == (expected));      \
    } while (0)
    CHECK_EXTENSION(NOC_C_AST_KIND_ATTRIBUTE_SPECIFIER,
                    "__attribute__((unused))",
                    NOC_C_AST_EXTENSION_GNU_ATTRIBUTE);
    CHECK_EXTENSION(NOC_C_AST_KIND_ATTRIBUTE_DECLARATION,
                    "[[deprecated]]",
                    NOC_C_AST_EXTENSION_C23_ATTRIBUTE);
    CHECK_EXTENSION(NOC_C_AST_KIND_ALIGNOF_EXPRESSION,
                    "__alignof__(int)",
                    NOC_C_AST_EXTENSION_GNU_ALIGNOF);
    CHECK_EXTENSION(NOC_C_AST_KIND_ALIGNOF_EXPRESSION,
                    "__alignof(int)",
                    NOC_C_AST_EXTENSION_GNU_ALIGNOF_ALT);
    CHECK_EXTENSION(NOC_C_AST_KIND_ALIGNOF_EXPRESSION,
                    "alignof(int)",
                    NOC_C_AST_EXTENSION_C23_ALIGNOF);
    CHECK_EXTENSION(NOC_C_AST_KIND_ALIGNOF_EXPRESSION,
                    "_Alignof(int)",
                    NOC_C_AST_EXTENSION_NONE);
    CHECK_EXTENSION(NOC_C_AST_KIND_MS_DECLSPEC_MODIFIER,
                    "__declspec(dllexport)",
                    NOC_C_AST_EXTENSION_MS_DECLSPEC);
    CHECK_EXTENSION(NOC_C_AST_KIND_MS_CALL_MODIFIER,
                    "__cdecl",
                    NOC_C_AST_EXTENSION_MS_CDECL);
    CHECK_EXTENSION(NOC_C_AST_KIND_TRUE,
                    "true",
                    NOC_C_AST_EXTENSION_C23_TRUE);
    CHECK_EXTENSION(NOC_C_AST_KIND_FALSE,
                    "false",
                    NOC_C_AST_EXTENSION_C23_FALSE);
    CHECK_EXTENSION(NOC_C_AST_KIND_NULL,
                    "nullptr",
                    NOC_C_AST_EXTENSION_C23_NULL);
#undef CHECK_EXTENSION

    node = find_source(&fixture.ast, NOC_C_AST_KIND_IDENTIFIER, "TRUE");
    CHECK(node != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_extension(&fixture.ast, node) ==
          NOC_C_AST_EXTENSION_NONE);
    node = find_source(&fixture.ast, NOC_C_AST_KIND_IDENTIFIER, "FALSE");
    CHECK(node != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_extension(&fixture.ast, node) ==
          NOC_C_AST_EXTENSION_NONE);
    node = find_source(&fixture.ast, NOC_C_AST_KIND_IDENTIFIER, "NULL");
    CHECK(node != NOC_C_AST_NODE_NONE);
    CHECK(noc_c_ast_node_extension(&fixture.ast, node) ==
          NOC_C_AST_EXTENSION_NONE);

    CHECK(strcmp(noc_c_ast_kind_name(NOC_C_AST_KIND_FUNCTION_DEFINITION),
                 "function_definition") == 0);
    CHECK(strcmp(noc_c_ast_field_name(NOC_C_AST_FIELD_ASSEMBLY_CODE),
                 "assembly_code") == 0);
    CHECK(strcmp(noc_c_ast_operator_name(NOC_C_AST_OPERATOR_DEREFERENCE),
                 "dereference") == 0);
    CHECK(strcmp(noc_c_ast_specifier_name(NOC_C_AST_SPECIFIER_TYPEDEF),
                 "typedef") == 0);
    CHECK(strcmp(noc_c_ast_qualifier_name(NOC_C_AST_QUALIFIER_C23_ALIGNAS),
                 "c23-alignas") == 0);
    CHECK(strcmp(noc_c_ast_primitive_name(NOC_C_AST_PRIMITIVE_C23_BOOL),
                 "c23-bool") == 0);
    CHECK(strcmp(noc_c_ast_array_size_name(NOC_C_AST_ARRAY_SIZE_STAR),
                 "star") == 0);
    CHECK(strcmp(noc_c_ast_extension_name(NOC_C_AST_EXTENSION_MS_DECLSPEC),
                 "ms-declspec") == 0);
    CHECK(strcmp(noc_c_ast_expected_kind_name(NOC_C_AST_EXPECTED_EXPRESSION),
                 "expression") == 0);
    CHECK(strcmp(noc_c_ast_kind_name((Noc_C_Ast_Kind)999), "unknown") == 0);
    CHECK(strcmp(noc_c_ast_extension_name((Noc_C_Ast_Extension)999),
                 "unknown") == 0);
    fixture_free(&fixture);
}

int main(void)
{
    test_contextual_operator_classification();
    test_array_declarator_details();
    test_specifiers_qualifiers_types_and_extensions();
    return finish_suite("C AST typed details");
}
