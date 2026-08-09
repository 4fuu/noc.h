#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_AST_IMPLEMENTATION_INCLUDED
#define NOC_AST_IMPLEMENTATION_INCLUDED

#define NOC__C_AST_ARRAY_COUNT(values) \
    (sizeof(values) / sizeof((values)[0]))

struct Noc_C_Ast_Impl {
    Noc_Document_Snapshot snapshot;
    Noc_C_Ast_Node *nodes;
    Noc__C_Ast_Detail *details;
    size_t count;
    size_t capacity;
    unsigned int issues;
};

static const char *const noc__c_ast_kind_names[] = {
    "unknown",
    "_abstract_declarator",
    "_declarator",
    "_field_declarator",
    "_type_declarator",
    "expression",
    "statement",
    "type_specifier",
    "abstract_array_declarator",
    "abstract_function_declarator",
    "abstract_parenthesized_declarator",
    "abstract_pointer_declarator",
    "alignas_qualifier",
    "alignof_expression",
    "argument_list",
    "array_declarator",
    "assignment_expression",
    "attribute",
    "attribute_declaration",
    "attribute_specifier",
    "attributed_declarator",
    "attributed_statement",
    "binary_expression",
    "bitfield_clause",
    "break_statement",
    "call_expression",
    "case_statement",
    "cast_expression",
    "char_literal",
    "comma_expression",
    "compound_literal_expression",
    "compound_statement",
    "concatenated_string",
    "conditional_expression",
    "continue_statement",
    "declaration",
    "declaration_list",
    "do_statement",
    "else_clause",
    "enum_specifier",
    "enumerator",
    "enumerator_list",
    "expression_statement",
    "extension_expression",
    "field_declaration",
    "field_declaration_list",
    "field_designator",
    "field_expression",
    "for_statement",
    "function_declarator",
    "function_definition",
    "generic_expression",
    "gnu_asm_clobber_list",
    "gnu_asm_expression",
    "gnu_asm_goto_list",
    "gnu_asm_input_operand",
    "gnu_asm_input_operand_list",
    "gnu_asm_output_operand",
    "gnu_asm_output_operand_list",
    "gnu_asm_qualifier",
    "goto_statement",
    "if_statement",
    "init_declarator",
    "initializer_list",
    "initializer_pair",
    "labeled_statement",
    "linkage_specification",
    "macro_type_specifier",
    "ms_based_modifier",
    "ms_call_modifier",
    "ms_declspec_modifier",
    "ms_pointer_modifier",
    "ms_unaligned_ptr_modifier",
    "null",
    "offsetof_expression",
    "parameter_declaration",
    "parameter_list",
    "parenthesized_declarator",
    "parenthesized_expression",
    "pointer_declarator",
    "pointer_expression",
    "preproc_call",
    "preproc_def",
    "preproc_defined",
    "preproc_elif",
    "preproc_elifdef",
    "preproc_else",
    "preproc_function_def",
    "preproc_if",
    "preproc_ifdef",
    "preproc_include",
    "preproc_params",
    "return_statement",
    "seh_except_clause",
    "seh_finally_clause",
    "seh_leave_statement",
    "seh_try_statement",
    "sized_type_specifier",
    "sizeof_expression",
    "storage_class_specifier",
    "string_literal",
    "struct_specifier",
    "subscript_designator",
    "subscript_expression",
    "subscript_range_designator",
    "switch_statement",
    "translation_unit",
    "type_definition",
    "type_descriptor",
    "type_qualifier",
    "unary_expression",
    "union_specifier",
    "update_expression",
    "variadic_parameter",
    "while_statement",
    "character",
    "comment",
    "escape_sequence",
    "false",
    "field_identifier",
    "identifier",
    "ms_restrict_modifier",
    "ms_signed_ptr_modifier",
    "ms_unsigned_ptr_modifier",
    "number_literal",
    "preproc_arg",
    "preproc_directive",
    "primitive_type",
    "statement_identifier",
    "string_content",
    "system_lib_string",
    "true",
    "type_identifier",
    "static_assert_declaration",
    "atomic_type_specifier",
    "error",
    "missing",
};

static const char *const noc__c_ast_field_names[] = {
    "none", "unknown", "alternative", "argument", "arguments",
    "assembly_code", "body", "clobbers", "condition", "consequence",
    "constraint", "declarator", "designator", "directive", "end", "field",
    "filter", "function", "goto_labels", "index", "initializer",
    "input_operands", "label", "left", "member", "name", "operand",
    "operator", "output_operands", "parameters", "path", "prefix",
    "register", "right", "size", "start", "symbol", "type",
    "underlying_type", "update", "value", "message",
};

typedef char Noc__C_Ast_Kind_Name_Count[
    NOC__C_AST_ARRAY_COUNT(noc__c_ast_kind_names) ==
            NOC_C_AST_KIND_MISSING + 1
        ? 1
        : -1];
typedef char Noc__C_Ast_Field_Name_Count[
    NOC__C_AST_ARRAY_COUNT(noc__c_ast_field_names) ==
            NOC_C_AST_FIELD_MESSAGE + 1
        ? 1
        : -1];

static bool noc__c_ast_slice_equal(Noc_Slice value, const char *text)
{
    size_t count = strlen(text);
    return value.count == count &&
           (count == 0 || memcmp(value.data, text, count) == 0);
}

static Noc_Slice noc__c_ast_trim(Noc_Slice value)
{
    while (value.count != 0 && isspace((unsigned char)value.data[0])) {
        value.data += 1;
        value.count -= 1;
    }
    while (value.count != 0 &&
           isspace((unsigned char)value.data[value.count - 1])) {
        value.count -= 1;
    }
    return value;
}

static Noc_C_Ast_Kind noc__c_ast_kind(Noc_Slice kind, unsigned int flags)
{
    size_t index;
    if ((flags & NOC_C_PARSE_NODE_MISSING) != 0) {
        return NOC_C_AST_KIND_MISSING;
    }
    if ((flags & NOC_C_PARSE_NODE_ERROR) != 0) {
        return NOC_C_AST_KIND_ERROR;
    }
    for (index = 1; index < (size_t)NOC_C_AST_KIND_ERROR; ++index) {
        if (noc__c_ast_slice_equal(kind, noc__c_ast_kind_names[index])) {
            return (Noc_C_Ast_Kind)index;
        }
    }
    return NOC_C_AST_KIND_UNKNOWN;
}

static Noc_C_Ast_Field noc__c_ast_field(Noc_Slice field)
{
    size_t index;
    if (field.count == 0) return NOC_C_AST_FIELD_NONE;
    for (index = (size_t)NOC_C_AST_FIELD_ALTERNATIVE;
         index <= (size_t)NOC_C_AST_FIELD_MESSAGE;
         ++index) {
        if (noc__c_ast_slice_equal(field, noc__c_ast_field_names[index])) {
            return (Noc_C_Ast_Field)index;
        }
    }
    return NOC_C_AST_FIELD_UNKNOWN;
}

static Noc_C_Ast_Operator noc__c_ast_operator(Noc_Slice spelling,
                                               Noc_C_Ast_Kind parent_kind,
                                               bool prefix)
{
    static const struct {
        const char *spelling;
        Noc_C_Ast_Operator operator_kind;
    } operators[] = {
        {"+", NOC_C_AST_OPERATOR_ADD},
        {"-", NOC_C_AST_OPERATOR_SUBTRACT},
        {"*", NOC_C_AST_OPERATOR_MULTIPLY},
        {"/", NOC_C_AST_OPERATOR_DIVIDE},
        {"%", NOC_C_AST_OPERATOR_REMAINDER},
        {"=", NOC_C_AST_OPERATOR_ASSIGN},
        {"+=", NOC_C_AST_OPERATOR_ADD_ASSIGN},
        {"-=", NOC_C_AST_OPERATOR_SUBTRACT_ASSIGN},
        {"*=", NOC_C_AST_OPERATOR_MULTIPLY_ASSIGN},
        {"/=", NOC_C_AST_OPERATOR_DIVIDE_ASSIGN},
        {"%=", NOC_C_AST_OPERATOR_REMAINDER_ASSIGN},
        {"<<=", NOC_C_AST_OPERATOR_SHIFT_LEFT_ASSIGN},
        {">>=", NOC_C_AST_OPERATOR_SHIFT_RIGHT_ASSIGN},
        {"&=", NOC_C_AST_OPERATOR_BIT_AND_ASSIGN},
        {"^=", NOC_C_AST_OPERATOR_BIT_XOR_ASSIGN},
        {"|=", NOC_C_AST_OPERATOR_BIT_OR_ASSIGN},
        {"&", NOC_C_AST_OPERATOR_BIT_AND},
        {"|", NOC_C_AST_OPERATOR_BIT_OR},
        {"^", NOC_C_AST_OPERATOR_BIT_XOR},
        {"&&", NOC_C_AST_OPERATOR_LOGICAL_AND},
        {"||", NOC_C_AST_OPERATOR_LOGICAL_OR},
        {"==", NOC_C_AST_OPERATOR_EQUAL},
        {"!=", NOC_C_AST_OPERATOR_NOT_EQUAL},
        {"<", NOC_C_AST_OPERATOR_LESS},
        {"<=", NOC_C_AST_OPERATOR_LESS_EQUAL},
        {">", NOC_C_AST_OPERATOR_GREATER},
        {">=", NOC_C_AST_OPERATOR_GREATER_EQUAL},
        {"<<", NOC_C_AST_OPERATOR_SHIFT_LEFT},
        {">>", NOC_C_AST_OPERATOR_SHIFT_RIGHT},
        {"!", NOC_C_AST_OPERATOR_LOGICAL_NOT},
        {"~", NOC_C_AST_OPERATOR_BIT_NOT},
        {".", NOC_C_AST_OPERATOR_MEMBER},
        {"->", NOC_C_AST_OPERATOR_POINTER_MEMBER},
    };
    size_t index;
    spelling = noc__c_ast_trim(spelling);
    if (parent_kind == NOC_C_AST_KIND_UPDATE_EXPRESSION) {
        if (noc__c_ast_slice_equal(spelling, "++")) {
            return prefix ? NOC_C_AST_OPERATOR_PREFIX_INCREMENT
                          : NOC_C_AST_OPERATOR_POSTFIX_INCREMENT;
        }
        if (noc__c_ast_slice_equal(spelling, "--")) {
            return prefix ? NOC_C_AST_OPERATOR_PREFIX_DECREMENT
                          : NOC_C_AST_OPERATOR_POSTFIX_DECREMENT;
        }
        return NOC_C_AST_OPERATOR_UNKNOWN;
    }
    if (parent_kind == NOC_C_AST_KIND_POINTER_EXPRESSION) {
        if (noc__c_ast_slice_equal(spelling, "&")) {
            return NOC_C_AST_OPERATOR_ADDRESS;
        }
        if (noc__c_ast_slice_equal(spelling, "*")) {
            return NOC_C_AST_OPERATOR_DEREFERENCE;
        }
        return NOC_C_AST_OPERATOR_UNKNOWN;
    }
    if (parent_kind == NOC_C_AST_KIND_UNARY_EXPRESSION) {
        if (noc__c_ast_slice_equal(spelling, "+")) {
            return NOC_C_AST_OPERATOR_POSITIVE;
        }
        if (noc__c_ast_slice_equal(spelling, "-")) {
            return NOC_C_AST_OPERATOR_NEGATIVE;
        }
    }
    for (index = 0; index < NOC__C_AST_ARRAY_COUNT(operators); ++index) {
        if (noc__c_ast_slice_equal(spelling, operators[index].spelling)) {
            return operators[index].operator_kind;
        }
    }
    return NOC_C_AST_OPERATOR_UNKNOWN;
}

static Noc_C_Ast_Specifier noc__c_ast_specifier(Noc_Slice spelling)
{
    static const struct {
        const char *spelling;
        Noc_C_Ast_Specifier specifier;
    } values[] = {
        {"extern", NOC_C_AST_SPECIFIER_EXTERN},
        {"static", NOC_C_AST_SPECIFIER_STATIC},
        {"auto", NOC_C_AST_SPECIFIER_AUTO},
        {"register", NOC_C_AST_SPECIFIER_REGISTER},
        {"typedef", NOC_C_AST_SPECIFIER_TYPEDEF},
        {"inline", NOC_C_AST_SPECIFIER_INLINE},
        {"__inline", NOC_C_AST_SPECIFIER_GNU_INLINE},
        {"__inline__", NOC_C_AST_SPECIFIER_GNU_INLINE_ALT},
        {"__forceinline", NOC_C_AST_SPECIFIER_MS_FORCE_INLINE},
        {"_Thread_local", NOC_C_AST_SPECIFIER_C11_THREAD_LOCAL},
        {"thread_local", NOC_C_AST_SPECIFIER_C23_THREAD_LOCAL},
        {"__thread", NOC_C_AST_SPECIFIER_GNU_THREAD_LOCAL},
    };
    size_t index;
    spelling = noc__c_ast_trim(spelling);
    for (index = 0; index < NOC__C_AST_ARRAY_COUNT(values); ++index) {
        if (noc__c_ast_slice_equal(spelling, values[index].spelling)) {
            return values[index].specifier;
        }
    }
    return NOC_C_AST_SPECIFIER_UNKNOWN;
}

static Noc_C_Ast_Qualifier noc__c_ast_qualifier(Noc_Slice spelling)
{
    static const struct {
        const char *spelling;
        Noc_C_Ast_Qualifier qualifier;
    } values[] = {
        {"const", NOC_C_AST_QUALIFIER_CONST},
        {"volatile", NOC_C_AST_QUALIFIER_VOLATILE},
        {"restrict", NOC_C_AST_QUALIFIER_RESTRICT},
        {"_Atomic", NOC_C_AST_QUALIFIER_ATOMIC},
        {"_Noreturn", NOC_C_AST_QUALIFIER_NORETURN},
        {"_Alignas", NOC_C_AST_QUALIFIER_C11_ALIGNAS},
        {"constexpr", NOC_C_AST_QUALIFIER_C23_CONSTEXPR},
        {"noreturn", NOC_C_AST_QUALIFIER_C23_NORETURN},
        {"alignas", NOC_C_AST_QUALIFIER_C23_ALIGNAS},
        {"__restrict__", NOC_C_AST_QUALIFIER_GNU_RESTRICT},
        {"__extension__", NOC_C_AST_QUALIFIER_GNU_EXTENSION},
        {"_Nonnull", NOC_C_AST_QUALIFIER_CLANG_NONNULL},
    };
    size_t index;
    spelling = noc__c_ast_trim(spelling);
    for (index = 0; index < NOC__C_AST_ARRAY_COUNT(values); ++index) {
        size_t count = strlen(values[index].spelling);
        if (spelling.count >= count &&
            memcmp(spelling.data, values[index].spelling, count) == 0 &&
            (spelling.count == count || spelling.data[count] == '(' ||
             isspace((unsigned char)spelling.data[count]))) {
            return values[index].qualifier;
        }
    }
    return NOC_C_AST_QUALIFIER_UNKNOWN;
}

static Noc_C_Ast_Primitive noc__c_ast_primitive(Noc_Slice spelling)
{
    static const char *const implementation_types[] = {
        "size_t", "ssize_t", "ptrdiff_t", "intptr_t", "uintptr_t",
        "charptr_t", "nullptr_t", "max_align_t", "int8_t", "int16_t",
        "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t",
        "uint64_t", "char8_t", "char16_t", "char32_t", "char64_t",
    };
    size_t index;
    spelling = noc__c_ast_trim(spelling);
    if (noc__c_ast_slice_equal(spelling, "void")) return NOC_C_AST_PRIMITIVE_VOID;
    if (noc__c_ast_slice_equal(spelling, "char")) return NOC_C_AST_PRIMITIVE_CHAR;
    if (noc__c_ast_slice_equal(spelling, "int")) return NOC_C_AST_PRIMITIVE_INT;
    if (noc__c_ast_slice_equal(spelling, "float")) return NOC_C_AST_PRIMITIVE_FLOAT;
    if (noc__c_ast_slice_equal(spelling, "double")) return NOC_C_AST_PRIMITIVE_DOUBLE;
    if (noc__c_ast_slice_equal(spelling, "_Bool")) return NOC_C_AST_PRIMITIVE_C11_BOOL;
    if (noc__c_ast_slice_equal(spelling, "bool")) return NOC_C_AST_PRIMITIVE_C23_BOOL;
    for (index = 0; index < NOC__C_AST_ARRAY_COUNT(implementation_types);
         ++index) {
        if (noc__c_ast_slice_equal(spelling, implementation_types[index])) {
            return NOC_C_AST_PRIMITIVE_IMPLEMENTATION_TYPE;
        }
    }
    return NOC_C_AST_PRIMITIVE_UNKNOWN;
}

static Noc_C_Ast_Extension noc__c_ast_extension(Noc_C_Ast_Kind kind,
                                                 Noc_Slice spelling)
{
    spelling = noc__c_ast_trim(spelling);
    switch (kind) {
    case NOC_C_AST_KIND_ATTRIBUTE_SPECIFIER:
        return NOC_C_AST_EXTENSION_GNU_ATTRIBUTE;
    case NOC_C_AST_KIND_ATTRIBUTE_DECLARATION:
        return NOC_C_AST_EXTENSION_C23_ATTRIBUTE;
    case NOC_C_AST_KIND_EXTENSION_EXPRESSION:
        return NOC_C_AST_EXTENSION_GNU_EXPRESSION;
    case NOC_C_AST_KIND_GNU_ASM_EXPRESSION:
        return NOC_C_AST_EXTENSION_GNU_ASM;
    case NOC_C_AST_KIND_ALIGNOF_EXPRESSION:
        if (spelling.count >= sizeof("__alignof__") - 1 &&
            memcmp(spelling.data, "__alignof__", sizeof("__alignof__") - 1) == 0) {
            return NOC_C_AST_EXTENSION_GNU_ALIGNOF;
        }
        if ((spelling.count >= sizeof("__alignof") - 1 &&
             memcmp(spelling.data, "__alignof", sizeof("__alignof") - 1) == 0) ||
            (spelling.count >= sizeof("_alignof") - 1 &&
             memcmp(spelling.data, "_alignof", sizeof("_alignof") - 1) == 0)) {
            return NOC_C_AST_EXTENSION_GNU_ALIGNOF_ALT;
        }
        if (spelling.count >= sizeof("alignof") - 1 &&
            memcmp(spelling.data, "alignof", sizeof("alignof") - 1) == 0) {
            return NOC_C_AST_EXTENSION_C23_ALIGNOF;
        }
        return NOC_C_AST_EXTENSION_NONE;
    case NOC_C_AST_KIND_GNU_ASM_QUALIFIER:
        if (noc__c_ast_slice_equal(spelling, "volatile")) {
            return NOC_C_AST_EXTENSION_GNU_ASM_VOLATILE;
        }
        if (noc__c_ast_slice_equal(spelling, "__volatile__")) {
            return NOC_C_AST_EXTENSION_GNU_ASM_VOLATILE_ALT;
        }
        if (noc__c_ast_slice_equal(spelling, "inline")) {
            return NOC_C_AST_EXTENSION_GNU_ASM_INLINE;
        }
        if (noc__c_ast_slice_equal(spelling, "goto")) {
            return NOC_C_AST_EXTENSION_GNU_ASM_GOTO;
        }
        return NOC_C_AST_EXTENSION_UNKNOWN;
    case NOC_C_AST_KIND_SUBSCRIPT_RANGE_DESIGNATOR:
        return NOC_C_AST_EXTENSION_GNU_SUBSCRIPT_RANGE;
    case NOC_C_AST_KIND_LINKAGE_SPECIFICATION:
        return NOC_C_AST_EXTENSION_CXX_LINKAGE;
    case NOC_C_AST_KIND_MACRO_TYPE_SPECIFIER:
        return NOC_C_AST_EXTENSION_MACRO_TYPE;
    case NOC_C_AST_KIND_TYPE_DEFINITION:
        if (spelling.count >= sizeof("__extension__") - 1 &&
            memcmp(spelling.data,
                   "__extension__",
                   sizeof("__extension__") - 1) == 0) {
            return NOC_C_AST_EXTENSION_GNU_EXTENSION_QUALIFIER;
        }
        return NOC_C_AST_EXTENSION_NONE;
    case NOC_C_AST_KIND_MS_DECLSPEC_MODIFIER:
        return NOC_C_AST_EXTENSION_MS_DECLSPEC;
    case NOC_C_AST_KIND_MS_BASED_MODIFIER:
        return NOC_C_AST_EXTENSION_MS_BASED;
    case NOC_C_AST_KIND_MS_CALL_MODIFIER:
        if (noc__c_ast_slice_equal(spelling, "__cdecl")) return NOC_C_AST_EXTENSION_MS_CDECL;
        if (noc__c_ast_slice_equal(spelling, "__clrcall")) return NOC_C_AST_EXTENSION_MS_CLRCALL;
        if (noc__c_ast_slice_equal(spelling, "__stdcall")) return NOC_C_AST_EXTENSION_MS_STDCALL;
        if (noc__c_ast_slice_equal(spelling, "__fastcall")) return NOC_C_AST_EXTENSION_MS_FASTCALL;
        if (noc__c_ast_slice_equal(spelling, "__thiscall")) return NOC_C_AST_EXTENSION_MS_THISCALL;
        if (noc__c_ast_slice_equal(spelling, "__vectorcall")) return NOC_C_AST_EXTENSION_MS_VECTORCALL;
        return NOC_C_AST_EXTENSION_UNKNOWN;
    case NOC_C_AST_KIND_MS_RESTRICT_MODIFIER:
        return NOC_C_AST_EXTENSION_MS_RESTRICT;
    case NOC_C_AST_KIND_MS_UNSIGNED_PTR_MODIFIER:
        return NOC_C_AST_EXTENSION_MS_UNSIGNED_POINTER;
    case NOC_C_AST_KIND_MS_SIGNED_PTR_MODIFIER:
        return NOC_C_AST_EXTENSION_MS_SIGNED_POINTER;
    case NOC_C_AST_KIND_MS_UNALIGNED_PTR_MODIFIER:
        return noc__c_ast_slice_equal(spelling, "_unaligned")
                   ? NOC_C_AST_EXTENSION_MS_UNALIGNED_POINTER
                   : NOC_C_AST_EXTENSION_MS_UNALIGNED_POINTER_ALT;
    case NOC_C_AST_KIND_SEH_TRY_STATEMENT:
        return NOC_C_AST_EXTENSION_MS_SEH_TRY;
    case NOC_C_AST_KIND_SEH_EXCEPT_CLAUSE:
        return NOC_C_AST_EXTENSION_MS_SEH_EXCEPT;
    case NOC_C_AST_KIND_SEH_FINALLY_CLAUSE:
        return NOC_C_AST_EXTENSION_MS_SEH_FINALLY;
    case NOC_C_AST_KIND_SEH_LEAVE_STATEMENT:
        return NOC_C_AST_EXTENSION_MS_SEH_LEAVE;
    case NOC_C_AST_KIND_TRUE:
        return NOC_C_AST_EXTENSION_C23_TRUE;
    case NOC_C_AST_KIND_FALSE:
        return NOC_C_AST_EXTENSION_C23_FALSE;
    case NOC_C_AST_KIND_NULL:
        return NOC_C_AST_EXTENSION_C23_NULL;
    case NOC_C_AST_KIND_STATIC_ASSERT_DECLARATION:
        return spelling.count >= sizeof("static_assert") - 1 &&
                       memcmp(spelling.data,
                              "static_assert",
                              sizeof("static_assert") - 1) == 0
                   ? NOC_C_AST_EXTENSION_C23_STATIC_ASSERT
                   : NOC_C_AST_EXTENSION_NONE;
    default:
        return NOC_C_AST_EXTENSION_NONE;
    }
}

static void noc__c_ast_apply_spelling_extensions(Noc__C_Ast_Detail *detail)
{
    switch (detail->specifier) {
    case NOC_C_AST_SPECIFIER_GNU_INLINE:
        detail->extension = NOC_C_AST_EXTENSION_GNU_INLINE;
        break;
    case NOC_C_AST_SPECIFIER_GNU_INLINE_ALT:
        detail->extension = NOC_C_AST_EXTENSION_GNU_INLINE_ALT;
        break;
    case NOC_C_AST_SPECIFIER_MS_FORCE_INLINE:
        detail->extension = NOC_C_AST_EXTENSION_MS_FORCE_INLINE;
        break;
    case NOC_C_AST_SPECIFIER_C23_THREAD_LOCAL:
        detail->extension = NOC_C_AST_EXTENSION_C23_THREAD_LOCAL;
        break;
    case NOC_C_AST_SPECIFIER_GNU_THREAD_LOCAL:
        detail->extension = NOC_C_AST_EXTENSION_GNU_THREAD_LOCAL;
        break;
    default:
        break;
    }
    switch (detail->qualifier) {
    case NOC_C_AST_QUALIFIER_C23_CONSTEXPR:
        detail->extension = NOC_C_AST_EXTENSION_C23_CONSTEXPR;
        break;
    case NOC_C_AST_QUALIFIER_C23_NORETURN:
        detail->extension = NOC_C_AST_EXTENSION_C23_NORETURN;
        break;
    case NOC_C_AST_QUALIFIER_C23_ALIGNAS:
        detail->extension = NOC_C_AST_EXTENSION_C23_ALIGNAS;
        break;
    case NOC_C_AST_QUALIFIER_GNU_RESTRICT:
        detail->extension = NOC_C_AST_EXTENSION_GNU_RESTRICT;
        break;
    case NOC_C_AST_QUALIFIER_GNU_EXTENSION:
        detail->extension = NOC_C_AST_EXTENSION_GNU_EXTENSION_QUALIFIER;
        break;
    case NOC_C_AST_QUALIFIER_CLANG_NONNULL:
        detail->extension = NOC_C_AST_EXTENSION_CLANG_NONNULL;
        break;
    default:
        break;
    }
    if (detail->type_spelling.primitive == NOC_C_AST_PRIMITIVE_C23_BOOL) {
        detail->extension = NOC_C_AST_EXTENSION_C23_BOOL;
    }
}

static bool noc__c_ast_is_array_kind(Noc_C_Ast_Kind kind)
{
    return kind == NOC_C_AST_KIND_ARRAY_DECLARATOR ||
           kind == NOC_C_AST_KIND_ABSTRACT_ARRAY_DECLARATOR;
}

static void noc__c_ast_impl_free(Noc_C_Ast_Impl *implementation)
{
    size_t index;
    if (!implementation) return;
    for (index = 0; index < implementation->count; ++index) {
        free(implementation->details[index].expected_spelling);
    }
    free(implementation->details);
    free(implementation->nodes);
    noc_document_snapshot_free(&implementation->snapshot);
    free(implementation);
}

static Noc_C_Ast_Status noc__c_ast_reserve(
    Noc__C_Ast_Normalized *implementation,
    size_t max_nodes)
{
    Noc__C_Ast_Normalized_Node *nodes;
    Noc__C_Ast_Detail *details;
    size_t old_capacity = implementation->capacity;
    size_t capacity;
    if (implementation->count < old_capacity) return NOC_C_AST_OK;
    if (implementation->count >= max_nodes) return NOC_C_AST_LIMIT_EXCEEDED;
    if (old_capacity == 0) {
        capacity = max_nodes < 128 ? max_nodes : 128;
    } else if (old_capacity > max_nodes / 2) {
        capacity = max_nodes;
    } else {
        capacity = old_capacity * 2;
    }
    if (capacity <= old_capacity ||
        capacity > SIZE_MAX / sizeof(*nodes) ||
        capacity > SIZE_MAX / sizeof(*details)) {
        return NOC_C_AST_LIMIT_EXCEEDED;
    }
    nodes = (Noc__C_Ast_Normalized_Node *)realloc(
        implementation->nodes,
        capacity * sizeof(*nodes));
    if (!nodes) return NOC_C_AST_OUT_OF_MEMORY;
    implementation->nodes = nodes;
    details = (Noc__C_Ast_Detail *)realloc(
        implementation->details,
        capacity * sizeof(*details));
    if (!details) return NOC_C_AST_OUT_OF_MEMORY;
    implementation->details = details;
    memset(implementation->nodes + old_capacity,
           0,
           (capacity - old_capacity) * sizeof(*nodes));
    memset(implementation->details + old_capacity,
           0,
           (capacity - old_capacity) * sizeof(*details));
    implementation->capacity = capacity;
    return NOC_C_AST_OK;
}

static void noc__c_ast_mark_unknown_detail(
    Noc__C_Ast_Normalized *implementation,
    size_t node_index)
{
    implementation->nodes[node_index].flags |= NOC_C_AST_NODE_UNKNOWN_DETAIL;
    implementation->issues |= NOC_C_AST_ISSUE_UNKNOWN_DETAIL;
}

static bool noc__c_ast_requires_operator(Noc_C_Ast_Kind kind)
{
    return kind == NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION ||
           kind == NOC_C_AST_KIND_BINARY_EXPRESSION ||
           kind == NOC_C_AST_KIND_FIELD_EXPRESSION ||
           kind == NOC_C_AST_KIND_POINTER_EXPRESSION ||
           kind == NOC_C_AST_KIND_UNARY_EXPRESSION ||
           kind == NOC_C_AST_KIND_UPDATE_EXPRESSION;
}

static bool noc__c_ast_requires_extension(Noc_C_Ast_Kind kind)
{
    return kind == NOC_C_AST_KIND_ATTRIBUTE_DECLARATION ||
           kind == NOC_C_AST_KIND_ATTRIBUTE_SPECIFIER ||
           kind == NOC_C_AST_KIND_EXTENSION_EXPRESSION ||
           kind == NOC_C_AST_KIND_GNU_ASM_EXPRESSION ||
           kind == NOC_C_AST_KIND_GNU_ASM_QUALIFIER ||
           kind == NOC_C_AST_KIND_LINKAGE_SPECIFICATION ||
           kind == NOC_C_AST_KIND_MACRO_TYPE_SPECIFIER ||
           kind == NOC_C_AST_KIND_MS_BASED_MODIFIER ||
           kind == NOC_C_AST_KIND_MS_CALL_MODIFIER ||
           kind == NOC_C_AST_KIND_MS_DECLSPEC_MODIFIER ||
           kind == NOC_C_AST_KIND_MS_RESTRICT_MODIFIER ||
           kind == NOC_C_AST_KIND_MS_SIGNED_PTR_MODIFIER ||
           kind == NOC_C_AST_KIND_MS_UNSIGNED_PTR_MODIFIER ||
           kind == NOC_C_AST_KIND_MS_UNALIGNED_PTR_MODIFIER ||
           kind == NOC_C_AST_KIND_NULL ||
           kind == NOC_C_AST_KIND_SEH_EXCEPT_CLAUSE ||
           kind == NOC_C_AST_KIND_SEH_FINALLY_CLAUSE ||
           kind == NOC_C_AST_KIND_SEH_LEAVE_STATEMENT ||
           kind == NOC_C_AST_KIND_SEH_TRY_STATEMENT ||
           kind == NOC_C_AST_KIND_SUBSCRIPT_RANGE_DESIGNATOR ||
           kind == NOC_C_AST_KIND_TRUE ||
           kind == NOC_C_AST_KIND_FALSE;
}

static Noc_C_Ast_Status noc__c_ast_validate_details(
    Noc__C_Ast_Normalized *implementation,
    const Noc_C_Ast_Options *options)
{
    size_t index;
    for (index = 0; index < implementation->count; ++index) {
        Noc_C_Ast_Kind kind = implementation->nodes[index].kind;
        Noc__C_Ast_Detail *detail = &implementation->details[index];
        bool unknown = false;
        if ((index & 255u) == 0 && options->should_cancel &&
            options->should_cancel(options->cancel_user_data)) {
            return NOC_C_AST_CANCELLED;
        }
        if (noc__c_ast_requires_operator(kind) &&
            (detail->operator_kind == NOC_C_AST_OPERATOR_NONE ||
             detail->operator_kind == NOC_C_AST_OPERATOR_UNKNOWN)) {
            unknown = true;
        }
        if (kind == NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER &&
            (detail->specifier == NOC_C_AST_SPECIFIER_NONE ||
             detail->specifier == NOC_C_AST_SPECIFIER_UNKNOWN)) {
            unknown = true;
        }
        if ((kind == NOC_C_AST_KIND_TYPE_QUALIFIER ||
             kind == NOC_C_AST_KIND_ALIGNAS_QUALIFIER) &&
            (detail->qualifier == NOC_C_AST_QUALIFIER_NONE ||
             detail->qualifier == NOC_C_AST_QUALIFIER_UNKNOWN)) {
            unknown = true;
        }
        if (kind == NOC_C_AST_KIND_PRIMITIVE_TYPE &&
            (detail->type_spelling.primitive == NOC_C_AST_PRIMITIVE_NONE ||
             detail->type_spelling.primitive == NOC_C_AST_PRIMITIVE_UNKNOWN)) {
            unknown = true;
        }
        if (kind == NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER &&
            detail->type_spelling.primitive == NOC_C_AST_PRIMITIVE_NONE &&
            detail->type_spelling.flags == 0 &&
            detail->type_spelling.long_count == 0) {
            unknown = true;
        }
        if (noc__c_ast_requires_extension(kind) &&
            (detail->extension == NOC_C_AST_EXTENSION_NONE ||
             detail->extension == NOC_C_AST_EXTENSION_UNKNOWN)) {
            unknown = true;
        }
        if (unknown) noc__c_ast_mark_unknown_detail(implementation, index);
    }
    return NOC_C_AST_OK;
}

NOCDEF Noc_C_Ast_Options noc_c_ast_default_options(void)
{
    Noc_C_Ast_Options options;
    options.max_nodes = 1024 * 1024;
    options.should_cancel = NULL;
    options.cancel_user_data = NULL;
    return options;
}

NOCDEF const char *noc_c_ast_status_name(Noc_C_Ast_Status status)
{
    static const char *const names[] = {
        "ok",
        "invalid-argument",
        "cancelled",
        "limit-exceeded",
        "generation-exhausted",
        "out-of-memory",
    };
    return (unsigned int)status < NOC__C_AST_ARRAY_COUNT(names)
               ? names[(unsigned int)status]
               : "unknown";
}

NOC__PRIVATE void noc__c_ast_normalized_free(
    Noc__C_Ast_Normalized *normalized)
{
    size_t index;
    if (!normalized) return;
    if (normalized->details) {
        for (index = 0; index < normalized->count; ++index) {
            free(normalized->details[index].expected_spelling);
        }
    }
    free(normalized->details);
    free(normalized->nodes);
    memset(normalized, 0, sizeof(*normalized));
}

NOC__PRIVATE Noc_C_Ast_Status noc__c_ast_normalize(
    Noc__C_Ast_Input input,
    Noc_C_Ast_Options options,
    size_t generation,
    Noc__C_Ast_Normalized *output)
{
    Noc__C_Ast_Normalized normalized = {0};
    size_t *input_to_ast;
    size_t index;
    Noc_C_Ast_Status status = NOC_C_AST_OK;

    if (!output || !input.query || input.count == 0 ||
        input.count > SIZE_MAX / sizeof(*input_to_ast) ||
        options.max_nodes == 0 || generation == 0) {
        return NOC_C_AST_INVALID_ARGUMENT;
    }
    input_to_ast = (size_t *)malloc(input.count * sizeof(*input_to_ast));
    if (!input_to_ast) return NOC_C_AST_OUT_OF_MEMORY;

    for (index = 0; index < input.count; ++index) {
        Noc__C_Ast_Input_Node input_node;
        size_t parent = NOC_C_AST_NODE_NONE;
        bool select;
        if ((index & 255u) == 0 && options.should_cancel &&
            options.should_cancel(options.cancel_user_data)) {
            status = NOC_C_AST_CANCELLED;
            break;
        }
        memset(&input_node, 0, sizeof(input_node));
        if (!input.query(input.context, index, &input_node) ||
            input_node.bytes_begin > input_node.bytes_end ||
            (input_node.parent != NOC_C_PARSE_NODE_NONE &&
             input_node.parent >= index)) {
            status = NOC_C_AST_INVALID_ARGUMENT;
            break;
        }
        if (input_node.parent != NOC_C_PARSE_NODE_NONE) {
            parent = input_to_ast[input_node.parent];
        }
        input_to_ast[index] = parent;
        select = ((input_node.flags & NOC_C_PARSE_NODE_NAMED) != 0 &&
                  (input_node.flags & NOC_C_PARSE_NODE_EXTRA) == 0) ||
                 (input_node.flags &
                  (NOC_C_PARSE_NODE_ERROR | NOC_C_PARSE_NODE_MISSING)) != 0;

        if (!select) {
            if (parent != NOC_C_AST_NODE_NONE) {
                Noc__C_Ast_Normalized_Node *parent_node =
                    &normalized.nodes[parent];
                Noc__C_Ast_Detail *parent_detail =
                    &normalized.details[parent];
                Noc_Slice spelling = input_node.spelling;
                if (noc__c_ast_slice_equal(input_node.field, "operator")) {
                    parent_detail->operator_kind = noc__c_ast_operator(
                        spelling,
                        parent_node->kind,
                        input_node.bytes_begin == parent_node->bytes_begin);
                }
                if (parent_node->kind == NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER) {
                    spelling = noc__c_ast_trim(spelling);
                    if (noc__c_ast_slice_equal(spelling, "signed")) {
                        parent_detail->type_spelling.flags |=
                            NOC_C_AST_TYPE_SIGNED;
                    } else if (noc__c_ast_slice_equal(spelling, "unsigned")) {
                        parent_detail->type_spelling.flags |=
                            NOC_C_AST_TYPE_UNSIGNED;
                    } else if (noc__c_ast_slice_equal(spelling, "short")) {
                        parent_detail->type_spelling.flags |=
                            NOC_C_AST_TYPE_SHORT;
                    } else if (noc__c_ast_slice_equal(spelling, "long")) {
                        parent_detail->type_spelling.long_count += 1;
                    } else if (noc__c_ast_slice_equal(spelling, "_Complex")) {
                        parent_detail->type_spelling.flags |=
                            NOC_C_AST_TYPE_COMPLEX;
                    }
                }
                if (noc__c_ast_is_array_kind(parent_node->kind)) {
                    spelling = noc__c_ast_trim(spelling);
                    if (noc__c_ast_slice_equal(spelling, "static")) {
                        parent_detail->array_detail.has_static_minimum = true;
                    } else if (noc__c_ast_slice_equal(input_node.field, "size") &&
                               noc__c_ast_slice_equal(spelling, "*")) {
                        parent_detail->array_detail.size =
                            NOC_C_AST_ARRAY_SIZE_STAR;
                    }
                }
            }
            continue;
        }

        status = noc__c_ast_reserve(&normalized, options.max_nodes);
        if (status != NOC_C_AST_OK) break;
        {
            size_t node_index = normalized.count++;
            Noc__C_Ast_Normalized_Node *node =
                &normalized.nodes[node_index];
            Noc__C_Ast_Detail *detail = &normalized.details[node_index];
            Noc_Slice spelling = input_node.spelling;
            node->kind = noc__c_ast_kind(input_node.kind, input_node.flags);
            node->field = noc__c_ast_field(input_node.field);
            node->bytes_begin = input_node.bytes_begin;
            node->bytes_end = input_node.bytes_end;
            node->parent = parent;
            node->first_child = NOC_C_AST_NODE_NONE;
            node->last_child = NOC_C_AST_NODE_NONE;
            node->next_sibling = NOC_C_AST_NODE_NONE;
            node->generation = generation;
            if ((input_node.flags & NOC_C_PARSE_NODE_ERROR) != 0) {
                node->flags |= NOC_C_AST_NODE_ERROR;
            }
            if ((input_node.flags & NOC_C_PARSE_NODE_MISSING) != 0) {
                node->flags |= NOC_C_AST_NODE_MISSING;
            }
            if ((input_node.flags & NOC_C_PARSE_NODE_HAS_ERROR) != 0) {
                node->flags |= NOC_C_AST_NODE_HAS_ERROR;
            }
            if (node->kind == NOC_C_AST_KIND_UNKNOWN) {
                node->flags |= NOC_C_AST_NODE_UNKNOWN_KIND;
                normalized.issues |= NOC_C_AST_ISSUE_UNKNOWN_KIND;
            }
            if (node->field == NOC_C_AST_FIELD_UNKNOWN) {
                node->flags |= NOC_C_AST_NODE_UNKNOWN_FIELD;
                normalized.issues |= NOC_C_AST_ISSUE_UNKNOWN_FIELD;
            }
            if ((input_node.flags &
                 (NOC_C_PARSE_NODE_ERROR | NOC_C_PARSE_NODE_HAS_ERROR)) != 0) {
                normalized.issues |= NOC_C_AST_ISSUE_PARSE_ERROR;
            }
            if ((input_node.flags & NOC_C_PARSE_NODE_MISSING) != 0) {
                normalized.issues |= NOC_C_AST_ISSUE_MISSING;
            }
            if ((input_node.flags & NOC_C_PARSE_NODE_SKIPPED_SOURCE) != 0) {
                normalized.issues |= NOC_C_AST_ISSUE_SKIPPED_SOURCE;
            }

            if (node->kind == NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER) {
                detail->specifier = noc__c_ast_specifier(spelling);
            } else if (node->kind == NOC_C_AST_KIND_TYPE_DEFINITION) {
                detail->specifier = NOC_C_AST_SPECIFIER_TYPEDEF;
            }
            if (node->kind == NOC_C_AST_KIND_TYPE_QUALIFIER ||
                node->kind == NOC_C_AST_KIND_ALIGNAS_QUALIFIER) {
                detail->qualifier = noc__c_ast_qualifier(spelling);
            }
            if (node->kind == NOC_C_AST_KIND_PRIMITIVE_TYPE) {
                detail->type_spelling.primitive =
                    noc__c_ast_primitive(spelling);
            }
            detail->extension = noc__c_ast_extension(node->kind, spelling);
            noc__c_ast_apply_spelling_extensions(detail);

            if ((input_node.flags & NOC_C_PARSE_NODE_MISSING) != 0) {
                detail->expected_kind = noc__c_grammar_expected_kind(
                    input_node.kind,
                    (input_node.flags & NOC_C_PARSE_NODE_NAMED) != 0);
                if ((input_node.flags & NOC_C_PARSE_NODE_NAMED) == 0) {
                    detail->expected_spelling =
                        (char *)malloc(input_node.kind.count + 1);
                    if (!detail->expected_spelling) {
                        status = NOC_C_AST_OUT_OF_MEMORY;
                        break;
                    }
                    memcpy(detail->expected_spelling,
                           input_node.kind.data,
                           input_node.kind.count);
                    detail->expected_spelling[input_node.kind.count] = 0;
                }
                if (detail->expected_kind == NOC_C_AST_EXPECTED_UNKNOWN) {
                    noc__c_ast_mark_unknown_detail(&normalized, node_index);
                }
            }

            if (parent != NOC_C_AST_NODE_NONE) {
                Noc__C_Ast_Normalized_Node *parent_node =
                    &normalized.nodes[parent];
                Noc__C_Ast_Detail *parent_detail =
                    &normalized.details[parent];
                if (parent_node->last_child == NOC_C_AST_NODE_NONE) {
                    parent_node->first_child = node_index;
                } else {
                    normalized.nodes[parent_node->last_child].next_sibling =
                        node_index;
                }
                parent_node->last_child = node_index;
                parent_node->child_count += 1;
                if (parent_node->kind == NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER &&
                    node->kind == NOC_C_AST_KIND_PRIMITIVE_TYPE) {
                    parent_detail->type_spelling.primitive =
                        detail->type_spelling.primitive;
                }
                if (noc__c_ast_is_array_kind(parent_node->kind) &&
                    node->field == NOC_C_AST_FIELD_SIZE) {
                    parent_detail->array_detail.size =
                        NOC_C_AST_ARRAY_SIZE_EXPRESSION;
                }
                if (parent_node->kind == NOC_C_AST_KIND_TYPE_QUALIFIER &&
                    node->kind == NOC_C_AST_KIND_ALIGNAS_QUALIFIER) {
                    parent_detail->qualifier = detail->qualifier;
                    parent_detail->extension = detail->extension;
                }
                if (node->kind == NOC_C_AST_KIND_ATTRIBUTE &&
                    parent_detail->extension != NOC_C_AST_EXTENSION_NONE) {
                    detail->extension = parent_detail->extension;
                }
                if (parent_node->kind == NOC_C_AST_KIND_MS_POINTER_MODIFIER &&
                    detail->extension != NOC_C_AST_EXTENSION_NONE) {
                    parent_detail->extension = detail->extension;
                }
            }
            if (node->kind == NOC_C_AST_KIND_TRUE &&
                noc__c_ast_slice_equal(noc__c_ast_trim(spelling), "TRUE")) {
                node->kind = NOC_C_AST_KIND_IDENTIFIER;
                detail->extension = NOC_C_AST_EXTENSION_NONE;
            } else if (node->kind == NOC_C_AST_KIND_FALSE &&
                       noc__c_ast_slice_equal(noc__c_ast_trim(spelling),
                                              "FALSE")) {
                node->kind = NOC_C_AST_KIND_IDENTIFIER;
                detail->extension = NOC_C_AST_EXTENSION_NONE;
            } else if (node->kind == NOC_C_AST_KIND_NULL &&
                       noc__c_ast_slice_equal(noc__c_ast_trim(spelling),
                                              "NULL")) {
                node->kind = NOC_C_AST_KIND_IDENTIFIER;
                detail->extension = NOC_C_AST_EXTENSION_NONE;
            }
            input_to_ast[index] = node_index;
        }
    }

    free(input_to_ast);
    if (status == NOC_C_AST_OK) {
        status = noc__c_ast_validate_details(&normalized, &options);
        if (normalized.count == 0) status = NOC_C_AST_INVALID_ARGUMENT;
    }
    if (status != NOC_C_AST_OK) {
        noc__c_ast_normalized_free(&normalized);
        return status;
    }
    *output = normalized;
    return NOC_C_AST_OK;
}

static bool noc__c_ast_physical_input_query(
    const void *context,
    size_t node_index,
    Noc__C_Ast_Input_Node *output)
{
    const Noc_C_Parse_Tree *tree = (const Noc_C_Parse_Tree *)context;
    const Noc_C_Parse_Node *node =
        noc_c_parse_tree_node_at(tree, node_index);
    if (!node || !output) return false;
    output->kind = node->kind;
    output->field = node->field;
    output->spelling = noc_c_parse_node_source(tree, node_index);
    output->bytes_begin = node->bytes.begin;
    output->bytes_end = node->bytes.end;
    output->parent = node->parent;
    output->flags = node->flags;
    return true;
}

NOCDEF Noc_C_Ast_Status noc_c_ast_build(const Noc_C_Parse_Tree *tree,
                                         Noc_C_Ast_Options options,
                                         Noc_C_Ast *output)
{
    Noc_C_Ast_Impl *parsed = NULL;
    Noc_C_Ast_Impl *previous;
    Noc__C_Ast_Normalized normalized = {0};
    Noc__C_Ast_Input input;
    size_t generation;
    size_t index;
    Noc_Workspace_Status snapshot_status;
    Noc_C_Ast_Status status;

    if (!noc_c_parse_tree_is_valid(tree) || !output || options.max_nodes == 0) {
        return NOC_C_AST_INVALID_ARGUMENT;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_C_AST_GENERATION_EXHAUSTED;
    }
    if ((output->impl || output->generation != 0) &&
        !noc_c_ast_is_valid(output)) {
        return NOC_C_AST_INVALID_ARGUMENT;
    }
    if (options.should_cancel &&
        options.should_cancel(options.cancel_user_data)) {
        return NOC_C_AST_CANCELLED;
    }
    parsed = (Noc_C_Ast_Impl *)calloc(1, sizeof(*parsed));
    if (!parsed) return NOC_C_AST_OUT_OF_MEMORY;
    snapshot_status = noc_document_snapshot_clone(
        noc_c_parse_tree_snapshot(tree),
        &parsed->snapshot);
    if (snapshot_status != NOC_WORKSPACE_OK) {
        noc__c_ast_impl_free(parsed);
        return snapshot_status == NOC_WORKSPACE_LIMIT_EXCEEDED
                   ? NOC_C_AST_LIMIT_EXCEEDED
               : snapshot_status == NOC_WORKSPACE_OUT_OF_MEMORY
                   ? NOC_C_AST_OUT_OF_MEMORY
                   : NOC_C_AST_INVALID_ARGUMENT;
    }
    generation = output->generation + 1;
    input.context = tree;
    input.count = noc_c_parse_tree_node_count(tree);
    input.query = noc__c_ast_physical_input_query;
    status = noc__c_ast_normalize(input, options, generation, &normalized);
    if (status != NOC_C_AST_OK) {
        noc__c_ast_impl_free(parsed);
        return status;
    }
    if (normalized.count > SIZE_MAX / sizeof(*parsed->nodes)) {
        noc__c_ast_normalized_free(&normalized);
        noc__c_ast_impl_free(parsed);
        return NOC_C_AST_LIMIT_EXCEEDED;
    }
    parsed->nodes = (Noc_C_Ast_Node *)malloc(
        normalized.count * sizeof(*parsed->nodes));
    if (!parsed->nodes) {
        noc__c_ast_normalized_free(&normalized);
        noc__c_ast_impl_free(parsed);
        return NOC_C_AST_OUT_OF_MEMORY;
    }
    for (index = 0; index < normalized.count; ++index) {
        const Noc__C_Ast_Normalized_Node *source = &normalized.nodes[index];
        Noc_C_Ast_Node *node = &parsed->nodes[index];
        if ((index & 255u) == 0 && options.should_cancel &&
            options.should_cancel(options.cancel_user_data)) {
            noc__c_ast_normalized_free(&normalized);
            noc__c_ast_impl_free(parsed);
            return NOC_C_AST_CANCELLED;
        }
        node->kind = source->kind;
        node->field = source->field;
        node->bytes.begin = source->bytes_begin;
        node->bytes.end = source->bytes_end;
        node->parent = source->parent;
        node->first_child = source->first_child;
        node->last_child = source->last_child;
        node->next_sibling = source->next_sibling;
        node->child_count = source->child_count;
        node->generation = source->generation;
        node->flags = source->flags;
    }
    if (options.should_cancel &&
        options.should_cancel(options.cancel_user_data)) {
        noc__c_ast_normalized_free(&normalized);
        noc__c_ast_impl_free(parsed);
        return NOC_C_AST_CANCELLED;
    }
    parsed->details = normalized.details;
    parsed->count = normalized.count;
    parsed->capacity = normalized.count;
    parsed->issues = normalized.issues;
    normalized.details = NULL;
    free(normalized.nodes);
    normalized.nodes = NULL;
    previous = output->impl;
    output->impl = parsed;
    output->generation = generation;
    noc__c_ast_impl_free(previous);
    return NOC_C_AST_OK;
}

NOCDEF void noc_c_ast_free(Noc_C_Ast *ast)
{
    Noc_C_Ast_Impl *implementation;
    if (!ast) return;
    implementation = ast->impl;
    memset(ast, 0, sizeof(*ast));
    noc__c_ast_impl_free(implementation);
}

NOCDEF bool noc_c_ast_is_valid(const Noc_C_Ast *ast)
{
    const Noc_C_Ast_Node *root;
    Noc_Slice source;
    if (!ast || !ast->impl || ast->generation == 0 ||
        !noc_document_snapshot_is_valid(&ast->impl->snapshot) ||
        !ast->impl->nodes || !ast->impl->details || ast->impl->count == 0 ||
        ast->impl->count > ast->impl->capacity) {
        return false;
    }
    root = &ast->impl->nodes[0];
    source = noc_document_snapshot_source(&ast->impl->snapshot);
    return root->parent == NOC_C_AST_NODE_NONE &&
           root->generation == ast->generation && root->bytes.begin == 0 &&
           root->bytes.end == source.count;
}

NOCDEF bool noc_c_ast_is_syntax_complete(const Noc_C_Ast *ast)
{
    return noc_c_ast_is_valid(ast) && ast->impl->issues == 0;
}

NOCDEF unsigned int noc_c_ast_issues(const Noc_C_Ast *ast)
{
    return noc_c_ast_is_valid(ast) ? ast->impl->issues : 0;
}

NOCDEF size_t noc_c_ast_generation(const Noc_C_Ast *ast)
{
    return noc_c_ast_is_valid(ast) ? ast->generation : 0;
}

NOCDEF size_t noc_c_ast_document_generation(const Noc_C_Ast *ast)
{
    return noc_c_ast_is_valid(ast)
               ? noc_document_snapshot_generation(&ast->impl->snapshot)
               : 0;
}

NOCDEF const Noc_Document_Snapshot *noc_c_ast_snapshot(const Noc_C_Ast *ast)
{
    return noc_c_ast_is_valid(ast) ? &ast->impl->snapshot : NULL;
}

NOCDEF size_t noc_c_ast_node_count(const Noc_C_Ast *ast)
{
    return noc_c_ast_is_valid(ast) ? ast->impl->count : 0;
}

NOCDEF size_t noc_c_ast_root(const Noc_C_Ast *ast)
{
    return noc_c_ast_is_valid(ast) ? 0 : NOC_C_AST_NODE_NONE;
}

NOCDEF const Noc_C_Ast_Node *noc_c_ast_node_at(const Noc_C_Ast *ast,
                                               size_t node_index)
{
    if (!noc_c_ast_is_valid(ast) || node_index >= ast->impl->count) return NULL;
    return &ast->impl->nodes[node_index];
}

NOCDEF Noc_Slice noc_c_ast_node_source(const Noc_C_Ast *ast,
                                       size_t node_index)
{
    Noc_Slice result = {0};
    const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, node_index);
    Noc_Slice source;
    if (!node) return result;
    source = noc_document_snapshot_source(&ast->impl->snapshot);
    if (node->bytes.begin > node->bytes.end || node->bytes.end > source.count) {
        return result;
    }
    result.data = source.data + node->bytes.begin;
    result.count = node->bytes.end - node->bytes.begin;
    return result;
}

NOCDEF Noc_Location noc_c_ast_node_location(const Noc_C_Ast *ast,
                                            size_t node_index)
{
    Noc_Location location = {0};
    const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, node_index);
    if (node) {
        (void)noc_document_snapshot_location(&ast->impl->snapshot,
                                             node->bytes.begin,
                                             &location);
    }
    return location;
}

NOCDEF size_t noc_c_ast_node_covering_range(const Noc_C_Ast *ast,
                                            Noc_Byte_Range range)
{
    Noc_Slice source;
    size_t node_index;
    if (!noc_c_ast_is_valid(ast) || range.begin >= range.end) {
        return NOC_C_AST_NODE_NONE;
    }
    source = noc_document_snapshot_source(&ast->impl->snapshot);
    if (range.end > source.count) return NOC_C_AST_NODE_NONE;
    node_index = noc_c_ast_root(ast);
    for (;;) {
        const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, node_index);
        size_t child;
        size_t covering = NOC_C_AST_NODE_NONE;
        if (!node || range.begin < node->bytes.begin ||
            range.end > node->bytes.end) {
            return NOC_C_AST_NODE_NONE;
        }
        child = node->first_child;
        while (child != NOC_C_AST_NODE_NONE) {
            const Noc_C_Ast_Node *candidate = noc_c_ast_node_at(ast, child);
            if (!candidate) return NOC_C_AST_NODE_NONE;
            if (candidate->bytes.begin <= range.begin &&
                range.end <= candidate->bytes.end) {
                covering = child;
                break;
            }
            child = candidate->next_sibling;
        }
        if (covering == NOC_C_AST_NODE_NONE) return node_index;
        node_index = covering;
    }
}

NOCDEF size_t noc_c_ast_node_at_offset(const Noc_C_Ast *ast, size_t offset)
{
    Noc_Byte_Range range;
    Noc_Slice source;
    if (!noc_c_ast_is_valid(ast)) return NOC_C_AST_NODE_NONE;
    source = noc_document_snapshot_source(&ast->impl->snapshot);
    if (offset >= source.count) return NOC_C_AST_NODE_NONE;
    range.begin = offset;
    range.end = offset + 1;
    return noc_c_ast_node_covering_range(ast, range);
}

NOCDEF size_t noc_c_ast_depth(const Noc_C_Ast *ast, size_t node_index)
{
    const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, node_index);
    size_t depth = 0;
    size_t count = noc_c_ast_node_count(ast);
    if (!node) return NOC_C_AST_NODE_NONE;
    while (node->parent != NOC_C_AST_NODE_NONE) {
        if (depth >= count) return NOC_C_AST_NODE_NONE;
        depth += 1;
        node = noc_c_ast_node_at(ast, node->parent);
        if (!node) return NOC_C_AST_NODE_NONE;
    }
    return depth;
}

NOCDEF size_t noc_c_ast_common_ancestor(const Noc_C_Ast *ast,
                                        size_t left,
                                        size_t right)
{
    size_t left_depth = noc_c_ast_depth(ast, left);
    size_t right_depth = noc_c_ast_depth(ast, right);
    if (left_depth == NOC_C_AST_NODE_NONE ||
        right_depth == NOC_C_AST_NODE_NONE) {
        return NOC_C_AST_NODE_NONE;
    }
    while (left_depth > right_depth) {
        left = ast->impl->nodes[left].parent;
        left_depth -= 1;
    }
    while (right_depth > left_depth) {
        right = ast->impl->nodes[right].parent;
        right_depth -= 1;
    }
    while (left != right) {
        if (left == NOC_C_AST_NODE_NONE || right == NOC_C_AST_NODE_NONE) {
            return NOC_C_AST_NODE_NONE;
        }
        left = ast->impl->nodes[left].parent;
        right = ast->impl->nodes[right].parent;
    }
    return left;
}

static bool noc__c_ast_is_expected_at_offset(const Noc_C_Ast_Node *node,
                                             size_t offset)
{
    return node && (node->flags & NOC_C_AST_NODE_MISSING) != 0 &&
           node->bytes.begin == offset && node->bytes.end == offset;
}

NOCDEF bool noc_c_ast_completion_context(
    const Noc_C_Ast *ast,
    size_t offset,
    Noc_C_Ast_Completion_Context *output)
{
    Noc_C_Ast_Completion_Context result;
    Noc_Slice source;
    size_t first_expected_parent = NOC_C_AST_NODE_NONE;
    size_t last_expected_parent = NOC_C_AST_NODE_NONE;
    size_t index;
    if (!noc_c_ast_is_valid(ast) || !output) return false;
    source = noc_document_snapshot_source(&ast->impl->snapshot);
    if (offset > source.count) return false;

    memset(&result, 0, sizeof(result));
    result.owner = ast;
    result.offset = offset;
    result.left_node = offset == 0
                           ? NOC_C_AST_NODE_NONE
                           : noc_c_ast_node_at_offset(ast, offset - 1);
    result.right_node = offset == source.count
                            ? NOC_C_AST_NODE_NONE
                            : noc_c_ast_node_at_offset(ast, offset);
    result.node = noc_c_ast_root(ast);
    result.file_id = noc_document_snapshot_file_id(&ast->impl->snapshot);
    result.generation = noc_c_ast_generation(ast);
    result.document_generation = noc_c_ast_document_generation(ast);
    if (result.left_node != NOC_C_AST_NODE_NONE &&
        result.right_node != NOC_C_AST_NODE_NONE) {
        result.node = noc_c_ast_common_ancestor(ast,
                                                result.left_node,
                                                result.right_node);
        if (result.node == NOC_C_AST_NODE_NONE) return false;
    }

    for (index = 0; index < ast->impl->count; ++index) {
        const Noc_C_Ast_Node *node = &ast->impl->nodes[index];
        size_t parent;
        if (!noc__c_ast_is_expected_at_offset(node, offset)) continue;
        result.expected_count += 1;
        parent = node->parent == NOC_C_AST_NODE_NONE
                     ? noc_c_ast_root(ast)
                     : node->parent;
        if (first_expected_parent == NOC_C_AST_NODE_NONE) {
            first_expected_parent = parent;
        }
        last_expected_parent = parent;
    }
    if (first_expected_parent != NOC_C_AST_NODE_NONE) {
        result.node = noc_c_ast_common_ancestor(ast,
                                                first_expected_parent,
                                                last_expected_parent);
        if (result.node == NOC_C_AST_NODE_NONE) return false;
    }
    *output = result;
    return true;
}

NOCDEF size_t noc_c_ast_completion_next_expected_node(
    const Noc_C_Ast *ast,
    const Noc_C_Ast_Completion_Context *context,
    size_t previous)
{
    size_t index;
    Noc_Slice source;
    if (!noc_c_ast_is_valid(ast) || !context || context->owner != ast ||
        context->file_id !=
            noc_document_snapshot_file_id(&ast->impl->snapshot) ||
        context->generation != noc_c_ast_generation(ast) ||
        context->document_generation != noc_c_ast_document_generation(ast)) {
        return NOC_C_AST_NODE_NONE;
    }
    source = noc_document_snapshot_source(&ast->impl->snapshot);
    if (context->offset > source.count) return NOC_C_AST_NODE_NONE;
    if (previous == NOC_C_AST_NODE_NONE) {
        index = 0;
    } else {
        const Noc_C_Ast_Node *previous_node = noc_c_ast_node_at(ast, previous);
        if (!noc__c_ast_is_expected_at_offset(previous_node, context->offset)) {
            return NOC_C_AST_NODE_NONE;
        }
        index = previous + 1;
    }
    for (; index < ast->impl->count; ++index) {
        const Noc_C_Ast_Node *node = &ast->impl->nodes[index];
        if (noc__c_ast_is_expected_at_offset(node, context->offset)) return index;
    }
    return NOC_C_AST_NODE_NONE;
}

NOCDEF Noc_C_Ast_Operator noc_c_ast_node_operator(const Noc_C_Ast *ast,
                                                  size_t node_index)
{
    return noc_c_ast_node_at(ast, node_index)
               ? ast->impl->details[node_index].operator_kind
               : NOC_C_AST_OPERATOR_NONE;
}

NOCDEF Noc_C_Ast_Specifier noc_c_ast_node_specifier(const Noc_C_Ast *ast,
                                                    size_t node_index)
{
    return noc_c_ast_node_at(ast, node_index)
               ? ast->impl->details[node_index].specifier
               : NOC_C_AST_SPECIFIER_NONE;
}

NOCDEF Noc_C_Ast_Qualifier noc_c_ast_node_qualifier(const Noc_C_Ast *ast,
                                                    size_t node_index)
{
    return noc_c_ast_node_at(ast, node_index)
               ? ast->impl->details[node_index].qualifier
               : NOC_C_AST_QUALIFIER_NONE;
}

NOCDEF bool noc_c_ast_node_type_spelling(const Noc_C_Ast *ast,
                                         size_t node_index,
                                         Noc_C_Ast_Type_Spelling *output)
{
    const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, node_index);
    if (!node || !output ||
        (node->kind != NOC_C_AST_KIND_PRIMITIVE_TYPE &&
         node->kind != NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER)) {
        return false;
    }
    *output = ast->impl->details[node_index].type_spelling;
    return true;
}

NOCDEF bool noc_c_ast_node_array_detail(const Noc_C_Ast *ast,
                                        size_t node_index,
                                        Noc_C_Ast_Array_Detail *output)
{
    const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, node_index);
    if (!node || !output || !noc__c_ast_is_array_kind(node->kind)) return false;
    *output = ast->impl->details[node_index].array_detail;
    return true;
}

NOCDEF Noc_C_Ast_Extension noc_c_ast_node_extension(const Noc_C_Ast *ast,
                                                    size_t node_index)
{
    return noc_c_ast_node_at(ast, node_index)
               ? ast->impl->details[node_index].extension
               : NOC_C_AST_EXTENSION_NONE;
}

NOCDEF Noc_C_Ast_Expected noc_c_ast_node_expected(const Noc_C_Ast *ast,
                                                  size_t node_index)
{
    Noc_C_Ast_Expected expected = {0};
    const Noc_C_Ast_Node *node = noc_c_ast_node_at(ast, node_index);
    if (node && (node->flags & NOC_C_AST_NODE_MISSING) != 0) {
        const Noc__C_Ast_Detail *detail = &ast->impl->details[node_index];
        expected.kind = detail->expected_kind;
        if (detail->expected_spelling) {
            expected.spelling.data = detail->expected_spelling;
            expected.spelling.count = strlen(detail->expected_spelling);
        }
    }
    return expected;
}

NOCDEF const char *noc_c_ast_kind_name(Noc_C_Ast_Kind kind)
{
    return (unsigned int)kind < NOC__C_AST_ARRAY_COUNT(noc__c_ast_kind_names)
               ? noc__c_ast_kind_names[(unsigned int)kind]
               : "unknown";
}

NOCDEF const char *noc_c_ast_field_name(Noc_C_Ast_Field field)
{
    return (unsigned int)field < NOC__C_AST_ARRAY_COUNT(noc__c_ast_field_names)
               ? noc__c_ast_field_names[(unsigned int)field]
               : "unknown";
}

NOCDEF const char *noc_c_ast_operator_name(Noc_C_Ast_Operator operator_kind)
{
    static const char *const names[] = {
        "none", "unknown", "add", "subtract", "multiply", "divide",
        "remainder", "assign", "add-assign", "subtract-assign",
        "multiply-assign", "divide-assign", "remainder-assign",
        "shift-left-assign", "shift-right-assign", "bit-and-assign",
        "bit-xor-assign", "bit-or-assign", "bit-and", "bit-or", "bit-xor",
        "logical-and", "logical-or", "equal", "not-equal", "less",
        "less-equal", "greater", "greater-equal", "shift-left",
        "shift-right", "logical-not", "bit-not", "positive", "negative",
        "address", "dereference", "member", "pointer-member",
        "prefix-increment", "prefix-decrement", "postfix-increment",
        "postfix-decrement",
    };
    return (unsigned int)operator_kind < NOC__C_AST_ARRAY_COUNT(names)
               ? names[(unsigned int)operator_kind]
               : "unknown";
}

NOCDEF const char *noc_c_ast_specifier_name(Noc_C_Ast_Specifier specifier)
{
    static const char *const names[] = {
        "none", "unknown", "extern", "static", "auto", "register",
        "typedef", "inline", "gnu-inline", "gnu-inline-alt",
        "ms-force-inline", "c11-thread-local", "c23-thread-local",
        "gnu-thread-local",
    };
    return (unsigned int)specifier < NOC__C_AST_ARRAY_COUNT(names)
               ? names[(unsigned int)specifier]
               : "unknown";
}

NOCDEF const char *noc_c_ast_qualifier_name(Noc_C_Ast_Qualifier qualifier)
{
    static const char *const names[] = {
        "none", "unknown", "const", "volatile", "restrict", "atomic",
        "noreturn", "c11-alignas", "c23-constexpr", "c23-noreturn",
        "c23-alignas", "gnu-restrict", "gnu-extension", "clang-nonnull",
    };
    return (unsigned int)qualifier < NOC__C_AST_ARRAY_COUNT(names)
               ? names[(unsigned int)qualifier]
               : "unknown";
}

NOCDEF const char *noc_c_ast_primitive_name(Noc_C_Ast_Primitive primitive)
{
    static const char *const names[] = {
        "none", "unknown", "void", "char", "int", "float", "double",
        "c11-bool", "c23-bool", "implementation-type",
    };
    return (unsigned int)primitive < NOC__C_AST_ARRAY_COUNT(names)
               ? names[(unsigned int)primitive]
               : "unknown";
}

NOCDEF const char *noc_c_ast_array_size_name(Noc_C_Ast_Array_Size size)
{
    static const char *const names[] = {
        "none", "unknown", "expression", "star",
    };
    return (unsigned int)size < NOC__C_AST_ARRAY_COUNT(names)
               ? names[(unsigned int)size]
               : "unknown";
}

NOCDEF const char *noc_c_ast_extension_name(Noc_C_Ast_Extension extension)
{
    static const char *const names[] = {
        "none", "unknown", "gnu-attribute", "c23-attribute",
        "gnu-expression", "gnu-asm", "gnu-asm-volatile",
        "gnu-asm-volatile-alt", "gnu-asm-inline", "gnu-asm-goto",
        "gnu-subscript-range", "gnu-alignof", "gnu-alignof-alt",
        "c23-alignof", "gnu-restrict", "gnu-extension-qualifier",
        "gnu-inline", "gnu-inline-alt", "gnu-thread-local", "ms-declspec",
        "ms-based", "ms-cdecl", "ms-clrcall", "ms-stdcall", "ms-fastcall",
        "ms-thiscall", "ms-vectorcall", "ms-force-inline", "ms-restrict",
        "ms-unsigned-pointer", "ms-signed-pointer", "ms-unaligned-pointer",
        "ms-unaligned-pointer-alt", "ms-seh-try", "ms-seh-except",
        "ms-seh-finally", "ms-seh-leave", "cxx-linkage",
        "c23-thread-local", "c23-constexpr", "c23-noreturn", "c23-alignas",
        "c23-bool", "c23-true", "c23-false", "c23-null", "clang-nonnull",
        "macro-type", "c23-static-assert",
    };
    return (unsigned int)extension < NOC__C_AST_ARRAY_COUNT(names)
               ? names[(unsigned int)extension]
               : "unknown";
}

NOCDEF const char *noc_c_ast_expected_kind_name(Noc_C_Ast_Expected_Kind kind)
{
    static const char *const names[] = {
        "none", "unknown", "punctuator", "keyword", "identifier", "type",
        "declaration", "statement", "expression",
    };
    return (unsigned int)kind < NOC__C_AST_ARRAY_COUNT(names)
               ? names[(unsigned int)kind]
               : "unknown";
}

typedef struct {
    size_t *items;
    size_t count;
    size_t capacity;
} Noc__Index_Stack;

static bool noc__index_stack_append(Noc__Index_Stack *stack, size_t item)
{
    size_t *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (stack->count > stack->capacity) return false;
    if (stack->count == stack->capacity) {
        if (stack->capacity >= maximum) return false;
        if (stack->capacity == 0) {
            capacity = maximum < 16 ? maximum : 16;
        } else if (stack->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = stack->capacity * 2;
        }
        if (capacity <= stack->capacity) return false;
        items = (size_t *)realloc(stack->items, capacity * sizeof(*items));
        if (!items) return false;
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count++] = item;
    return true;
}

static bool noc__syntax_append_node(Noc_Syntax_Tree *tree,
                                    Noc_Syntax_Node node,
                                    size_t *node_index)
{
    Noc_Syntax_Node *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (tree->count > tree->capacity) return false;
    if (tree->count == tree->capacity) {
        if (tree->capacity >= maximum) return false;
        if (tree->capacity == 0) {
            capacity = maximum < 256 ? maximum : 256;
        } else if (tree->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = tree->capacity * 2;
        }
        if (capacity <= tree->capacity) return false;
        items = (Noc_Syntax_Node *)realloc(tree->items, capacity * sizeof(*items));
        if (!items) return false;
        tree->items = items;
        tree->capacity = capacity;
    }
    if (node_index) *node_index = tree->count;
    tree->items[tree->count++] = node;
    return true;
}

static bool noc__syntax_add_child(Noc_Syntax_Tree *tree,
                                  size_t parent,
                                  Noc_Syntax_Node node,
                                  size_t *node_index)
{
    size_t child;
    size_t previous;
    node.parent = parent;
    if (!noc__syntax_append_node(tree, node, &child)) return false;
    previous = tree->items[parent].last_child;
    if (previous == NOC_SYNTAX_NONE) {
        tree->items[parent].first_child = child;
    } else {
        tree->items[previous].next_sibling = child;
    }
    tree->items[parent].last_child = child;
    if (node_index) *node_index = child;
    return true;
}

static Noc_Syntax_Kind noc__syntax_open_kind(Noc_Token token, char *expected_close)
{
    if (noc_token_is_punct(token, "(")) {
        *expected_close = ')';
        return NOC_SYNTAX_PAREN_GROUP;
    }
    if (noc_token_is_punct(token, "[")) {
        *expected_close = ']';
        return NOC_SYNTAX_BRACKET_GROUP;
    }
    if (noc_token_is_punct(token, "{")) {
        *expected_close = '}';
        return NOC_SYNTAX_BRACE_GROUP;
    }
    *expected_close = 0;
    return NOC_SYNTAX_TOKEN;
}

static char noc__syntax_expected_close(Noc_Syntax_Kind kind)
{
    switch (kind) {
    case NOC_SYNTAX_PAREN_GROUP: return ')';
    case NOC_SYNTAX_BRACKET_GROUP: return ']';
    case NOC_SYNTAX_BRACE_GROUP: return '}';
    default: return 0;
    }
}

static bool noc__syntax_is_close(Noc_Token token)
{
    return noc_token_is_punct(token, ")") || noc_token_is_punct(token, "]") ||
           noc_token_is_punct(token, "}");
}

NOCDEF void noc_syntax_tree_free(Noc_Syntax_Tree *tree)
{
    free(tree->items);
    memset(tree, 0, sizeof(*tree));
}

NOCDEF bool noc_syntax_tree_is_valid(const Noc_Syntax_Tree *tree)
{
    const Noc_Syntax_Node *root;
    if (!tree || !noc_token_stream_is_valid(tree->stream) ||
        tree->stream_generation != tree->stream->generation || !tree->items ||
        tree->count == 0 || tree->count > tree->capacity) {
        return false;
    }
    root = &tree->items[0];
    return root->kind == NOC_SYNTAX_ROOT && root->parent == NOC_SYNTAX_NONE &&
           noc_token_range_is_valid(tree->stream, root->range);
}

NOCDEF bool noc_syntax_tree_build(Noc_Context *context,
                                  const Noc_Token_Stream *stream,
                                  Noc_Syntax_Tree *tree)
{
    Noc_Syntax_Tree parsed = {0};
    Noc__Index_Stack parents = {0};
    Noc_Location no_location = {0};
    Noc_Syntax_Node root;
    size_t eof_index;
    size_t i;
    bool ok = false;
    if (!noc_token_stream_is_valid(stream)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "cannot build a syntax tree from an invalid token stream");
        return false;
    }
    parsed.stream = stream;
    parsed.stream_generation = stream->generation;
    eof_index = stream->count - 1;
    memset(&root, 0, sizeof(root));
    root.kind = NOC_SYNTAX_ROOT;
    root.range.begin = 0;
    root.range.end = eof_index;
    root.parent = NOC_SYNTAX_NONE;
    root.first_child = NOC_SYNTAX_NONE;
    root.last_child = NOC_SYNTAX_NONE;
    root.next_sibling = NOC_SYNTAX_NONE;
    if (!noc__syntax_append_node(&parsed, root, NULL) ||
        !noc__index_stack_append(&parents, 0)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while starting syntax tree");
        goto done;
    }

    for (i = 0; i < eof_index; ++i) {
        Noc_Token token = stream->items[i];
        Noc_Syntax_Node node;
        Noc_Syntax_Kind open_kind;
        size_t parent = parents.items[parents.count - 1];
        size_t node_index;
        char expected_close;
        memset(&node, 0, sizeof(node));
        open_kind = noc__syntax_open_kind(token, &expected_close);
        if (expected_close != 0) {
            node.kind = open_kind;
            node.range.begin = i;
            node.range.end = NOC_TOKEN_INDEX_NONE;
            node.first_child = NOC_SYNTAX_NONE;
            node.last_child = NOC_SYNTAX_NONE;
            node.next_sibling = NOC_SYNTAX_NONE;
            if (!noc__syntax_add_child(&parsed, parent, node, &node_index) ||
                !noc__index_stack_append(&parents, node_index)) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "out of memory while building syntax tree");
                goto done;
            }
            continue;
        }
        if (noc__syntax_is_close(token)) {
            Noc_Syntax_Node *group;
            char expected;
            if (parents.count == 1) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "unexpected closing delimiter '%.*s'",
                            (int)token.text.count,
                            token.text.data);
                goto done;
            }
            group = &parsed.items[parents.items[parents.count - 1]];
            expected = noc__syntax_expected_close(group->kind);
            if (token.text.count != 1 || token.text.data[0] != expected) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "expected closing delimiter '%c', got '%.*s'",
                            expected,
                            (int)token.text.count,
                            token.text.data);
                goto done;
            }
            group->range.end = i + 1;
            parents.count -= 1;
            continue;
        }
        node.kind = NOC_SYNTAX_TOKEN;
        node.range.begin = i;
        node.range.end = i + 1;
        node.first_child = NOC_SYNTAX_NONE;
        node.last_child = NOC_SYNTAX_NONE;
        node.next_sibling = NOC_SYNTAX_NONE;
        if (!noc__syntax_add_child(&parsed, parent, node, NULL)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "out of memory while building syntax tree");
            goto done;
        }
    }
    if (parents.count != 1) {
        const Noc_Syntax_Node *group = &parsed.items[parents.items[parents.count - 1]];
        Noc_Location location = stream->items[group->range.begin].location;
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "unclosed '%.*s'; expected '%c'",
                    (int)stream->items[group->range.begin].text.count,
                    stream->items[group->range.begin].text.data,
                    noc__syntax_expected_close(group->kind));
        goto done;
    }
    noc_syntax_tree_free(tree);
    *tree = parsed;
    memset(&parsed, 0, sizeof(parsed));
    ok = true;

done:
    free(parents.items);
    noc_syntax_tree_free(&parsed);
    return ok;
}

NOCDEF const char *noc_syntax_kind_name(Noc_Syntax_Kind kind)
{
    switch (kind) {
    case NOC_SYNTAX_ROOT: return "root";
    case NOC_SYNTAX_TOKEN: return "token";
    case NOC_SYNTAX_PAREN_GROUP: return "parenthesis group";
    case NOC_SYNTAX_BRACKET_GROUP: return "bracket group";
    case NOC_SYNTAX_BRACE_GROUP: return "brace group";
    }
    return "unknown syntax";
}

NOCDEF size_t noc_syntax_root(const Noc_Syntax_Tree *tree)
{
    return noc_syntax_tree_is_valid(tree) ? 0 : NOC_SYNTAX_NONE;
}

NOCDEF const Noc_Syntax_Node *noc_syntax_node(const Noc_Syntax_Tree *tree,
                                              size_t node)
{
    if (!noc_syntax_tree_is_valid(tree) || node >= tree->count) return NULL;
    return &tree->items[node];
}

NOCDEF size_t noc_syntax_parent(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? syntax->parent : NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_first_child(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? syntax->first_child : NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_next_sibling(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? syntax->next_sibling : NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_child_count(const Noc_Syntax_Tree *tree, size_t node)
{
    size_t count = 0;
    size_t child = noc_syntax_first_child(tree, node);
    while (child != NOC_SYNTAX_NONE) {
        count += 1;
        child = noc_syntax_next_sibling(tree, child);
    }
    return count;
}

NOCDEF size_t noc_syntax_first_child_of_kind(const Noc_Syntax_Tree *tree,
                                             size_t node,
                                             Noc_Syntax_Kind kind)
{
    size_t child = noc_syntax_first_child(tree, node);
    while (child != NOC_SYNTAX_NONE) {
        const Noc_Syntax_Node *syntax = noc_syntax_node(tree, child);
        if (syntax && syntax->kind == kind) return child;
        child = noc_syntax_next_sibling(tree, child);
    }
    return NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_next_preorder(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax) return NOC_SYNTAX_NONE;
    if (syntax->first_child != NOC_SYNTAX_NONE) return syntax->first_child;
    while (node != NOC_SYNTAX_NONE) {
        syntax = noc_syntax_node(tree, node);
        if (!syntax) return NOC_SYNTAX_NONE;
        if (syntax->next_sibling != NOC_SYNTAX_NONE) return syntax->next_sibling;
        node = syntax->parent;
    }
    return NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_node_covering_range(const Noc_Syntax_Tree *tree,
                                             Noc_Token_Range range)
{
    size_t node;
    const Noc_Syntax_Node *syntax;
    if (!noc_syntax_tree_is_valid(tree) || range.begin >= range.end ||
        !noc_token_range_is_valid(tree->stream, range)) {
        return NOC_SYNTAX_NONE;
    }
    node = noc_syntax_root(tree);
    syntax = noc_syntax_node(tree, node);
    if (!syntax || range.begin < syntax->range.begin || range.end > syntax->range.end) {
        return NOC_SYNTAX_NONE;
    }
    for (;;) {
        size_t child = syntax->first_child;
        size_t covering = NOC_SYNTAX_NONE;
        while (child != NOC_SYNTAX_NONE) {
            const Noc_Syntax_Node *candidate = noc_syntax_node(tree, child);
            if (!candidate) return NOC_SYNTAX_NONE;
            if (candidate->range.begin <= range.begin &&
                range.end <= candidate->range.end) {
                covering = child;
                break;
            }
            if (candidate->range.begin > range.begin) break;
            child = candidate->next_sibling;
        }
        if (covering == NOC_SYNTAX_NONE) return node;
        node = covering;
        syntax = &tree->items[node];
    }
}

NOCDEF size_t noc_syntax_node_at_token(const Noc_Syntax_Tree *tree,
                                       size_t token_index)
{
    Noc_Token_Range range;
    if (!noc_syntax_tree_is_valid(tree) || token_index >= tree->stream->count - 1) {
        return NOC_SYNTAX_NONE;
    }
    range.begin = token_index;
    range.end = token_index + 1;
    return noc_syntax_node_covering_range(tree, range);
}

NOCDEF size_t noc_syntax_depth(const Noc_Syntax_Tree *tree, size_t node)
{
    size_t depth = 0;
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax) return NOC_SYNTAX_NONE;
    while (syntax->parent != NOC_SYNTAX_NONE) {
        if (depth >= tree->count) return NOC_SYNTAX_NONE;
        depth += 1;
        syntax = noc_syntax_node(tree, syntax->parent);
        if (!syntax) return NOC_SYNTAX_NONE;
    }
    return depth;
}

NOCDEF size_t noc_syntax_common_ancestor(const Noc_Syntax_Tree *tree,
                                         size_t left,
                                         size_t right)
{
    size_t left_depth = noc_syntax_depth(tree, left);
    size_t right_depth = noc_syntax_depth(tree, right);
    if (left_depth == NOC_SYNTAX_NONE || right_depth == NOC_SYNTAX_NONE) {
        return NOC_SYNTAX_NONE;
    }
    while (left_depth > right_depth) {
        left = tree->items[left].parent;
        left_depth -= 1;
    }
    while (right_depth > left_depth) {
        right = tree->items[right].parent;
        right_depth -= 1;
    }
    while (left != right) {
        if (left == NOC_SYNTAX_NONE || right == NOC_SYNTAX_NONE) {
            return NOC_SYNTAX_NONE;
        }
        left = tree->items[left].parent;
        right = tree->items[right].parent;
    }
    return left;
}

NOCDEF Noc_Token_Range noc_syntax_inner_range(const Noc_Syntax_Tree *tree,
                                              size_t node)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax || (syntax->kind != NOC_SYNTAX_PAREN_GROUP &&
                    syntax->kind != NOC_SYNTAX_BRACKET_GROUP &&
                    syntax->kind != NOC_SYNTAX_BRACE_GROUP) ||
        syntax->range.end < syntax->range.begin + 2) {
        return invalid;
    }
    invalid.begin = syntax->range.begin + 1;
    invalid.end = syntax->range.end - 1;
    return invalid;
}

NOCDEF Noc_Slice noc_syntax_source(const Noc_Syntax_Tree *tree, size_t node)
{
    Noc_Slice empty = {0};
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? noc_token_range_source(tree->stream, syntax->range) : empty;
}

NOCDEF Noc_Location noc_syntax_location(const Noc_Syntax_Tree *tree, size_t node)
{
    Noc_Location empty = {0};
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? noc_token_range_location(tree->stream, syntax->range) : empty;
}

NOCDEF const Noc_Token *noc_syntax_token(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax || syntax->kind != NOC_SYNTAX_TOKEN ||
        syntax->range.end != syntax->range.begin + 1) {
        return NULL;
    }
    return &tree->stream->items[syntax->range.begin];
}

typedef struct {
    bool has_typedef;
    bool has_equal;
    bool has_comma;
    bool has_tag_specifier;
    bool starts_with_static_assert;
    size_t tag_name_token;
    size_t name_token;
    bool has_declarator;
    bool declarator_is_function;
    Noc_Token_Range parameters;
} Noc__C_External_Analysis;

typedef enum {
    NOC__C_DECLARATOR_NONE = 0,
    NOC__C_DECLARATOR_POINTER,
    NOC__C_DECLARATOR_ARRAY,
    NOC__C_DECLARATOR_FUNCTION,
} Noc__C_Declarator_Operator;

typedef struct {
    size_t name_token;
    Noc__C_Declarator_Operator first_operator;
    Noc_Token_Range parameters;
} Noc__C_Declarator;

static bool noc__c_token_is_keyword(Noc_Token token)
{
    static const char *const keywords[] = {
        "_Alignas",       "_Alignof",      "_Atomic",       "_BitInt",
        "_Bool",          "_Complex",      "_Decimal128",   "_Decimal32",
        "_Decimal64",     "_Generic",      "_Imaginary",    "_Noreturn",
        "_Static_assert", "_Thread_local", "alignas",       "alignof",
        "auto",           "bool",          "break",         "case",
        "char",           "const",         "constexpr",     "continue",
        "default",        "do",            "double",        "else",
        "enum",           "extern",        "false",         "float",
        "for",            "goto",          "if",            "inline",
        "int",            "long",          "nullptr",       "register",
        "restrict",       "return",        "short",         "signed",
        "sizeof",         "static",        "static_assert", "struct",
        "switch",         "thread_local",  "true",          "typedef",
        "typeof",         "typeof_unqual", "union",         "unsigned",
        "void",           "volatile",      "while",
    };
    size_t i;
    if (token.kind != NOC_TOKEN_IDENTIFIER) return false;
    for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (noc_token_is_identifier(token, keywords[i])) return true;
    }
    return false;
}

static bool noc__c_token_is_attribute_name(Noc_Token token)
{
    return noc_token_is_identifier(token, "__attribute__") ||
           noc_token_is_identifier(token, "__declspec") ||
           noc_token_is_identifier(token, "_Alignas") ||
           noc_token_is_identifier(token, "alignas");
}

static bool noc__c_node_is_ignored(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Token *token = noc_syntax_token(tree, node);
    return token && (noc_token_is_trivia(*token) ||
                     token->kind == NOC_TOKEN_PREPROCESSOR);
}

static size_t noc__c_next_significant_node(const Noc_Syntax_Tree *tree,
                                           size_t node,
                                           size_t end)
{
    while (node != end && node != NOC_SYNTAX_NONE &&
           noc__c_node_is_ignored(tree, node)) {
        node = tree->items[node].next_sibling;
    }
    return node;
}

static bool noc__c_group_is_attribute(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax || syntax->kind != NOC_SYNTAX_BRACKET_GROUP ||
        syntax->range.end < syntax->range.begin + 4) {
        return false;
    }
    return noc_token_is_punct(tree->stream->items[syntax->range.begin + 1], "[") &&
           noc_token_is_punct(tree->stream->items[syntax->range.end - 2], "]");
}

static bool noc__c_parse_declarator(const Noc_Syntax_Tree *tree,
                                    size_t node,
                                    size_t end,
                                    size_t depth,
                                    Noc__C_Declarator *declarator,
                                    size_t *after)
{
    Noc__C_Declarator parsed;
    bool has_pointer = false;
    const Noc_Syntax_Node *syntax;
    const Noc_Token *token;
    if (depth > tree->count) return false;
    parsed.name_token = NOC_TOKEN_INDEX_NONE;
    parsed.first_operator = NOC__C_DECLARATOR_NONE;
    parsed.parameters.begin = NOC_TOKEN_INDEX_NONE;
    parsed.parameters.end = NOC_TOKEN_INDEX_NONE;
    node = noc__c_next_significant_node(tree, node, end);

    while (node != end && node != NOC_SYNTAX_NONE) {
        size_t next;
        syntax = &tree->items[node];
        token = noc_syntax_token(tree, node);
        if (token && noc_token_is_punct(*token, "*")) {
            has_pointer = true;
            node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            continue;
        }
        if (has_pointer && token &&
            (noc_token_is_identifier(*token, "const") ||
             noc_token_is_identifier(*token, "restrict") ||
             noc_token_is_identifier(*token, "volatile") ||
             noc_token_is_identifier(*token, "_Atomic"))) {
            node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            continue;
        }
        if (has_pointer && token && noc__c_token_is_attribute_name(*token)) {
            next = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            if (next != end && next != NOC_SYNTAX_NONE &&
                tree->items[next].kind == NOC_SYNTAX_PAREN_GROUP) {
                node = noc__c_next_significant_node(tree,
                                                    tree->items[next].next_sibling,
                                                    end);
                continue;
            }
        }
        break;
    }

    if (node == end || node == NOC_SYNTAX_NONE) return false;
    syntax = &tree->items[node];
    token = noc_syntax_token(tree, node);
    if (token && token->kind == NOC_TOKEN_IDENTIFIER &&
        !noc__c_token_is_keyword(*token) && !noc__c_token_is_attribute_name(*token)) {
        parsed.name_token = syntax->range.begin;
    } else if (syntax->kind == NOC_SYNTAX_PAREN_GROUP) {
        size_t inner_after;
        if (!noc__c_parse_declarator(tree,
                                     syntax->first_child,
                                     NOC_SYNTAX_NONE,
                                     depth + 1,
                                     &parsed,
                                     &inner_after) ||
            noc__c_next_significant_node(tree,
                                         inner_after,
                                         NOC_SYNTAX_NONE) != NOC_SYNTAX_NONE) {
            return false;
        }
    } else {
        return false;
    }
    node = noc__c_next_significant_node(tree, syntax->next_sibling, end);

    while (node != end && node != NOC_SYNTAX_NONE) {
        size_t next;
        syntax = &tree->items[node];
        token = noc_syntax_token(tree, node);
        if (syntax->kind == NOC_SYNTAX_PAREN_GROUP) {
            if (parsed.first_operator == NOC__C_DECLARATOR_NONE) {
                parsed.first_operator = NOC__C_DECLARATOR_FUNCTION;
                parsed.parameters = syntax->range;
            }
            node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            continue;
        }
        if (syntax->kind == NOC_SYNTAX_BRACKET_GROUP) {
            if (noc__c_group_is_attribute(tree, node)) {
                node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
                continue;
            }
            if (parsed.first_operator == NOC__C_DECLARATOR_NONE) {
                parsed.first_operator = NOC__C_DECLARATOR_ARRAY;
            }
            node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            continue;
        }
        if (token && noc__c_token_is_attribute_name(*token)) {
            next = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            if (next != end && next != NOC_SYNTAX_NONE &&
                tree->items[next].kind == NOC_SYNTAX_PAREN_GROUP) {
                node = noc__c_next_significant_node(tree,
                                                    tree->items[next].next_sibling,
                                                    end);
                continue;
            }
        }
        break;
    }
    if (has_pointer && parsed.first_operator == NOC__C_DECLARATOR_NONE) {
        parsed.first_operator = NOC__C_DECLARATOR_POINTER;
    }
    *declarator = parsed;
    *after = node;
    return true;
}

static bool noc__c_find_declarator(const Noc_Syntax_Tree *tree,
                                   size_t first,
                                   size_t end,
                                   Noc__C_Declarator *declarator)
{
    size_t node = noc__c_next_significant_node(tree, first, end);
    while (node != end && node != NOC_SYNTAX_NONE) {
        Noc__C_Declarator parsed;
        size_t after;
        if (noc__c_parse_declarator(tree, node, end, 0, &parsed, &after) &&
            noc__c_next_significant_node(tree, after, end) == end) {
            *declarator = parsed;
            return true;
        }
        node = noc__c_next_significant_node(tree,
                                            tree->items[node].next_sibling,
                                            end);
    }
    return false;
}

static Noc__C_External_Analysis noc__c_analyze_external(
    const Noc_Syntax_Tree *tree,
    size_t first,
    size_t end)
{
    Noc__C_External_Analysis analysis;
    size_t node = first;
    size_t previous = NOC_SYNTAX_NONE;
    size_t declarator_end = end;
    size_t significant_count = 0;
    bool expect_tag_name = false;
    memset(&analysis, 0, sizeof(analysis));
    analysis.tag_name_token = NOC_TOKEN_INDEX_NONE;
    analysis.name_token = NOC_TOKEN_INDEX_NONE;
    analysis.parameters.begin = NOC_TOKEN_INDEX_NONE;
    analysis.parameters.end = NOC_TOKEN_INDEX_NONE;

    while (node != end && node != NOC_SYNTAX_NONE) {
        const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
        const Noc_Token *token;
        if (!syntax) break;
        if (noc__c_node_is_ignored(tree, node)) {
            node = syntax->next_sibling;
            continue;
        }
        token = noc_syntax_token(tree, node);
        if (token) {
            if (significant_count == 0) {
                analysis.starts_with_static_assert =
                    noc_token_is_identifier(*token, "_Static_assert") ||
                    noc_token_is_identifier(*token, "static_assert");
            }
            if (!analysis.has_equal &&
                (noc_token_is_identifier(*token, "struct") ||
                 noc_token_is_identifier(*token, "union") ||
                 noc_token_is_identifier(*token, "enum"))) {
                analysis.has_tag_specifier = true;
                expect_tag_name = true;
            } else if (expect_tag_name && noc__c_token_is_attribute_name(*token)) {
                /* The following group belongs to the attribute, not the tag. */
            } else if (expect_tag_name && token->kind == NOC_TOKEN_IDENTIFIER) {
                analysis.tag_name_token = syntax->range.begin;
                expect_tag_name = false;
            } else if (expect_tag_name && token->kind != NOC_TOKEN_IDENTIFIER) {
                expect_tag_name = false;
            }
            if (noc_token_is_identifier(*token, "typedef")) analysis.has_typedef = true;
            if (noc_token_is_punct(*token, "=")) analysis.has_equal = true;
            if (noc_token_is_punct(*token, ",")) analysis.has_comma = true;
            if ((noc_token_is_punct(*token, "=") || noc_token_is_punct(*token, ",")) &&
                declarator_end == end) {
                declarator_end = node;
            }
        } else if (expect_tag_name) {
            const Noc_Token *previous_token = noc_syntax_token(tree, previous);
            if (!noc__c_group_is_attribute(tree, node) &&
                !(syntax->kind == NOC_SYNTAX_PAREN_GROUP && previous_token &&
                  noc__c_token_is_attribute_name(*previous_token))) {
                expect_tag_name = false;
            }
        }
        previous = node;
        significant_count += 1;
        node = syntax->next_sibling;
    }
    {
        Noc__C_Declarator declarator;
        if (noc__c_find_declarator(tree, first, declarator_end, &declarator)) {
            analysis.has_declarator = true;
            analysis.name_token = declarator.name_token;
            analysis.declarator_is_function =
                declarator.first_operator == NOC__C_DECLARATOR_FUNCTION;
            analysis.parameters = declarator.parameters;
        }
    }
    return analysis;
}

static bool noc__c_external_append(Noc_C_Translation_Unit *unit,
                                   Noc_C_External_Item item)
{
    Noc_C_External_Item *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (unit->count > unit->capacity) return false;
    if (unit->count == unit->capacity) {
        if (unit->capacity >= maximum) return false;
        if (unit->capacity == 0) {
            capacity = maximum < 16 ? maximum : 16;
        } else if (unit->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = unit->capacity * 2;
        }
        if (capacity <= unit->capacity) return false;
        items = (Noc_C_External_Item *)realloc(unit->items,
                                               capacity * sizeof(*items));
        if (!items) return false;
        unit->items = items;
        unit->capacity = capacity;
    }
    unit->items[unit->count++] = item;
    return true;
}

static Noc_C_External_Item noc__c_make_external(const Noc_Syntax_Tree *tree,
                                                size_t first,
                                                size_t end,
                                                Noc_Token_Range range,
                                                bool function_definition,
                                                Noc_Token_Range body)
{
    Noc_C_External_Item item;
    Noc__C_External_Analysis analysis = noc__c_analyze_external(tree, first, end);
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    memset(&item, 0, sizeof(item));
    item.kind = function_definition ? NOC_C_EXTERNAL_FUNCTION_DEFINITION
                                    : NOC_C_EXTERNAL_DECLARATION;
    item.range = range;
    item.signature = range;
    item.name_token = NOC_TOKEN_INDEX_NONE;
    item.parameters = invalid;
    item.body = invalid;
    if (function_definition) {
        item.signature.end = body.begin;
        item.signature = noc_token_range_trim_trivia(tree->stream, item.signature);
        item.body = body;
        item.declaration_kind = NOC_C_DECLARATION_FUNCTION;
        item.name_token = analysis.name_token;
        item.parameters = analysis.parameters;
        return item;
    }
    if (item.signature.end > item.signature.begin &&
        noc_token_is_punct(tree->stream->items[item.signature.end - 1], ";")) {
        item.signature.end -= 1;
        item.signature = noc_token_range_trim_trivia(tree->stream, item.signature);
    }
    if (analysis.has_typedef) {
        item.declaration_kind = NOC_C_DECLARATION_TYPEDEF;
    } else if (analysis.starts_with_static_assert) {
        item.declaration_kind = NOC_C_DECLARATION_UNKNOWN;
    } else if (analysis.has_comma) {
        item.declaration_kind = NOC_C_DECLARATION_UNKNOWN;
    } else if (analysis.has_declarator && analysis.declarator_is_function &&
               !analysis.has_equal) {
        item.declaration_kind = NOC_C_DECLARATION_FUNCTION;
    } else if (analysis.has_tag_specifier && !analysis.has_equal &&
               (!analysis.has_declarator ||
                analysis.name_token == analysis.tag_name_token)) {
        item.declaration_kind = NOC_C_DECLARATION_TAG;
    } else {
        item.declaration_kind = NOC_C_DECLARATION_OBJECT;
    }
    if (analysis.declarator_is_function) item.parameters = analysis.parameters;
    if (!analysis.has_comma) {
        item.name_token = analysis.name_token;
        if (item.declaration_kind == NOC_C_DECLARATION_TAG &&
            item.name_token == NOC_TOKEN_INDEX_NONE) {
            item.name_token = analysis.tag_name_token;
        }
    }
    return item;
}

NOCDEF void noc_c_translation_unit_free(Noc_C_Translation_Unit *unit)
{
    free(unit->items);
    memset(unit, 0, sizeof(*unit));
}

NOCDEF bool noc_c_translation_unit_is_valid(const Noc_C_Translation_Unit *unit)
{
    return unit && noc_token_stream_is_valid(unit->stream) &&
           unit->stream_generation == unit->stream->generation &&
           unit->count <= unit->capacity && (unit->count == 0 || unit->items);
}

NOCDEF bool noc_c_translation_unit_build(Noc_Context *context,
                                         const Noc_Syntax_Tree *tree,
                                         Noc_C_Translation_Unit *unit)
{
    Noc_C_Translation_Unit parsed = {0};
    Noc_Location no_location = {0};
    const Noc_Syntax_Node *root;
    size_t first = NOC_SYNTAX_NONE;
    size_t node;
    bool ok = false;
    if (!noc_syntax_tree_is_valid(tree)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "cannot analyze an invalid syntax tree");
        return false;
    }
    parsed.stream = tree->stream;
    parsed.stream_generation = tree->stream_generation;
    root = &tree->items[noc_syntax_root(tree)];
    node = root->first_child;
    while (node != NOC_SYNTAX_NONE) {
        const Noc_Syntax_Node *syntax = &tree->items[node];
        const Noc_Token *token = noc_syntax_token(tree, node);
        size_t next = syntax->next_sibling;
        if (first == NOC_SYNTAX_NONE) {
            if (noc__c_node_is_ignored(tree, node)) {
                node = next;
                continue;
            }
            first = node;
        }
        if (token && noc_token_is_punct(*token, ";")) {
            Noc_Token_Range range = {tree->items[first].range.begin, syntax->range.end};
            Noc_C_External_Item item = noc__c_make_external(tree,
                                                            first,
                                                            node,
                                                            range,
                                                            false,
                                                            (Noc_Token_Range){0, 0});
            if (!noc__c_external_append(&parsed, item)) goto out_of_memory;
            first = NOC_SYNTAX_NONE;
        } else if (syntax->kind == NOC_SYNTAX_BRACE_GROUP) {
            Noc__C_External_Analysis analysis = noc__c_analyze_external(tree,
                                                                        first,
                                                                        node);
            if (analysis.has_declarator && analysis.declarator_is_function &&
                !analysis.has_equal && !analysis.has_typedef) {
                Noc_Token_Range range = {tree->items[first].range.begin,
                                         syntax->range.end};
                Noc_C_External_Item item = noc__c_make_external(tree,
                                                                first,
                                                                node,
                                                                range,
                                                                true,
                                                                syntax->range);
                if (!noc__c_external_append(&parsed, item)) goto out_of_memory;
                first = NOC_SYNTAX_NONE;
            } else if (first == node ||
                       (!analysis.has_tag_specifier && !analysis.has_equal &&
                        !analysis.has_typedef)) {
                Noc_Token_Range range = {tree->items[first].range.begin,
                                         syntax->range.end};
                Noc_C_External_Item item = noc__c_make_external(tree,
                                                                first,
                                                                node,
                                                                range,
                                                                false,
                                                                (Noc_Token_Range){0, 0});
                item.kind = NOC_C_EXTERNAL_UNKNOWN;
                item.declaration_kind = NOC_C_DECLARATION_UNKNOWN;
                item.name_token = NOC_TOKEN_INDEX_NONE;
                item.parameters.begin = NOC_TOKEN_INDEX_NONE;
                item.parameters.end = NOC_TOKEN_INDEX_NONE;
                if (!noc__c_external_append(&parsed, item)) goto out_of_memory;
                first = NOC_SYNTAX_NONE;
            }
        }
        node = next;
    }
    if (first != NOC_SYNTAX_NONE) {
        Noc_Token_Range range = {tree->items[first].range.begin, root->range.end};
        range = noc_token_range_trim_trivia(tree->stream, range);
        if (range.begin != range.end) {
            Noc_C_External_Item item = noc__c_make_external(tree,
                                                            first,
                                                            NOC_SYNTAX_NONE,
                                                            range,
                                                            false,
                                                            (Noc_Token_Range){0, 0});
            item.kind = NOC_C_EXTERNAL_UNKNOWN;
            item.declaration_kind = NOC_C_DECLARATION_UNKNOWN;
            if (!noc__c_external_append(&parsed, item)) goto out_of_memory;
        }
    }
    noc_c_translation_unit_free(unit);
    *unit = parsed;
    memset(&parsed, 0, sizeof(parsed));
    ok = true;
    goto done;

out_of_memory:
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "out of memory while analyzing C translation unit");
done:
    noc_c_translation_unit_free(&parsed);
    return ok;
}

NOCDEF const Noc_C_External_Item *noc_c_external_item(const Noc_C_Translation_Unit *unit,
                                                      size_t item)
{
    if (!noc_c_translation_unit_is_valid(unit) || item >= unit->count) return NULL;
    return &unit->items[item];
}

NOCDEF const char *noc_c_external_kind_name(Noc_C_External_Kind kind)
{
    switch (kind) {
    case NOC_C_EXTERNAL_UNKNOWN: return "unknown external item";
    case NOC_C_EXTERNAL_DECLARATION: return "declaration";
    case NOC_C_EXTERNAL_FUNCTION_DEFINITION: return "function definition";
    }
    return "unknown external item";
}

NOCDEF const char *noc_c_declaration_kind_name(Noc_C_Declaration_Kind kind)
{
    switch (kind) {
    case NOC_C_DECLARATION_UNKNOWN: return "unknown declaration";
    case NOC_C_DECLARATION_OBJECT: return "object declaration";
    case NOC_C_DECLARATION_FUNCTION: return "function declaration";
    case NOC_C_DECLARATION_TYPEDEF: return "typedef declaration";
    case NOC_C_DECLARATION_TAG: return "tag declaration";
    }
    return "unknown declaration";
}

static bool noc__c_delimited_range_is_valid(const Noc_Token_Stream *stream,
                                            Noc_Token_Range range,
                                            const char *open,
                                            const char *close)
{
    Noc_Token_Cursor cursor;
    Noc_Token_Range whole;
    if (!noc_token_cursor_init_range(&cursor, stream, range) ||
        !noc_token_cursor_take_balanced(&cursor, open, close, &whole, NULL)) {
        return false;
    }
    return whole.begin == range.begin && whole.end == range.end &&
           noc_token_cursor_at_end(&cursor);
}

static bool noc__c_token_is_builtin_type(Noc_Token token)
{
    return noc_token_is_identifier(token, "_Atomic") ||
           noc_token_is_identifier(token, "_BitInt") ||
           noc_token_is_identifier(token, "_Bool") ||
           noc_token_is_identifier(token, "_Complex") ||
           noc_token_is_identifier(token, "_Decimal128") ||
           noc_token_is_identifier(token, "_Decimal32") ||
           noc_token_is_identifier(token, "_Decimal64") ||
           noc_token_is_identifier(token, "_Imaginary") ||
           noc_token_is_identifier(token, "bool") ||
           noc_token_is_identifier(token, "char") ||
           noc_token_is_identifier(token, "double") ||
           noc_token_is_identifier(token, "enum") ||
           noc_token_is_identifier(token, "float") ||
           noc_token_is_identifier(token, "int") ||
           noc_token_is_identifier(token, "long") ||
           noc_token_is_identifier(token, "short") ||
           noc_token_is_identifier(token, "signed") ||
           noc_token_is_identifier(token, "struct") ||
           noc_token_is_identifier(token, "typeof") ||
           noc_token_is_identifier(token, "typeof_unqual") ||
           noc_token_is_identifier(token, "union") ||
           noc_token_is_identifier(token, "unsigned") ||
           noc_token_is_identifier(token, "void");
}

static size_t noc__c_next_significant_token(const Noc_Token_Stream *stream,
                                            size_t token,
                                            size_t end)
{
    while (token < end && noc_token_is_trivia(stream->items[token])) token += 1;
    return token;
}

static size_t noc__c_parenthesized_pointer_name(const Noc_Token_Stream *stream,
                                                Noc_Token_Range range)
{
    size_t i;
    size_t parentheses = 0;
    size_t brackets = 0;
    size_t braces = 0;
    for (i = range.begin; i < range.end; ++i) {
        size_t token;
        Noc_Token current = stream->items[i];
        if (noc_token_is_punct(current, "(")) {
            if (parentheses == 0 && brackets == 0 && braces == 0) {
                token = noc__c_next_significant_token(stream, i + 1, range.end);
                if (token < range.end && noc_token_is_punct(stream->items[token], "*")) {
                    do {
                        token = noc__c_next_significant_token(stream,
                                                              token + 1,
                                                              range.end);
                    } while (token < range.end &&
                             noc_token_is_punct(stream->items[token], "*"));
                    while (token < range.end &&
                           (noc_token_is_identifier(stream->items[token], "const") ||
                            noc_token_is_identifier(stream->items[token], "restrict") ||
                            noc_token_is_identifier(stream->items[token], "volatile") ||
                            noc_token_is_identifier(stream->items[token], "_Atomic"))) {
                        token = noc__c_next_significant_token(stream,
                                                              token + 1,
                                                              range.end);
                    }
                    if (token < range.end &&
                        stream->items[token].kind == NOC_TOKEN_IDENTIFIER &&
                        !noc__c_token_is_keyword(stream->items[token]) &&
                        !noc__c_token_is_attribute_name(stream->items[token])) {
                        size_t close = noc__c_next_significant_token(stream,
                                                                    token + 1,
                                                                    range.end);
                        if (close < range.end &&
                            noc_token_is_punct(stream->items[close], ")")) {
                            return token;
                        }
                    }
                }
            }
            parentheses += 1;
        } else if (noc_token_is_punct(current, ")")) {
            if (parentheses > 0) parentheses -= 1;
        } else if (noc_token_is_punct(current, "[")) {
            brackets += 1;
        } else if (noc_token_is_punct(current, "]")) {
            if (brackets > 0) brackets -= 1;
        } else if (noc_token_is_punct(current, "{")) {
            braces += 1;
        } else if (noc_token_is_punct(current, "}")) {
            if (braces > 0) braces -= 1;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static size_t noc__c_parameter_name(const Noc_Token_Stream *stream,
                                    Noc_Token_Range range)
{
    size_t pointer_name = noc__c_parenthesized_pointer_name(stream, range);
    size_t candidate = NOC_TOKEN_INDEX_NONE;
    size_t candidate_depth = SIZE_MAX;
    size_t candidate_count = 0;
    size_t depth = 0;
    size_t i;
    bool has_builtin_type = false;
    bool expect_tag_name = false;
    if (pointer_name != NOC_TOKEN_INDEX_NONE) return pointer_name;
    for (i = range.begin; i < range.end; ++i) {
        Noc_Token token = stream->items[i];
        if (noc_token_is_punct(token, "[")) {
            size_t nested = noc__c_next_significant_token(stream, i + 1, range.end);
            if (nested < range.end && noc_token_is_punct(stream->items[nested], "[")) {
                Noc_Token_Cursor cursor;
                Noc_Token_Range whole;
                if (noc_token_cursor_init_range(&cursor,
                                                stream,
                                                (Noc_Token_Range){i, range.end}) &&
                    noc_token_cursor_take_balanced(&cursor, "[", "]", &whole, NULL) &&
                    whole.begin == i) {
                    i = whole.end - 1;
                    continue;
                }
            }
        }
        if (noc_token_is_punct(token, "(") || noc_token_is_punct(token, "[") ||
            noc_token_is_punct(token, "{")) {
            depth += 1;
            continue;
        }
        if (noc_token_is_punct(token, ")") || noc_token_is_punct(token, "]") ||
            noc_token_is_punct(token, "}")) {
            if (depth > 0) depth -= 1;
            continue;
        }
        if (token.kind != NOC_TOKEN_IDENTIFIER) continue;
        if (noc__c_token_is_attribute_name(token)) {
            size_t group = noc__c_next_significant_token(stream, i + 1, range.end);
            Noc_Token_Cursor cursor;
            Noc_Token_Range whole;
            if (group < range.end &&
                noc_token_cursor_init_range(&cursor,
                                            stream,
                                            (Noc_Token_Range){group, range.end}) &&
                noc_token_cursor_take_balanced(&cursor, "(", ")", &whole, NULL)) {
                i = whole.end - 1;
            }
            continue;
        }
        if (expect_tag_name) {
            expect_tag_name = false;
            continue;
        }
        if (noc_token_is_identifier(token, "struct") ||
            noc_token_is_identifier(token, "union") ||
            noc_token_is_identifier(token, "enum")) {
            has_builtin_type = true;
            expect_tag_name = true;
            continue;
        }
        if (noc__c_token_is_builtin_type(token)) has_builtin_type = true;
        if (noc__c_token_is_keyword(token)) continue;
        if (depth < candidate_depth) {
            candidate = i;
            candidate_depth = depth;
            candidate_count = 1;
        } else if (depth == candidate_depth) {
            candidate = i;
            candidate_count += 1;
        }
    }
    if (candidate_depth != 0 || (candidate_count == 1 && !has_builtin_type)) {
        return NOC_TOKEN_INDEX_NONE;
    }
    return candidate;
}

static bool noc__c_parameter_append(Noc_C_Parameter_List *list,
                                    Noc_C_Parameter parameter)
{
    Noc_C_Parameter *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (list->count > list->capacity) return false;
    if (list->count == list->capacity) {
        if (list->capacity >= maximum) return false;
        if (list->capacity == 0) {
            capacity = maximum < 8 ? maximum : 8;
        } else if (list->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = list->capacity * 2;
        }
        if (capacity <= list->capacity) return false;
        items = (Noc_C_Parameter *)realloc(list->items, capacity * sizeof(*items));
        if (!items) return false;
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = parameter;
    return true;
}

NOCDEF void noc_c_parameter_list_free(Noc_C_Parameter_List *list)
{
    free(list->items);
    memset(list, 0, sizeof(*list));
}

NOCDEF bool noc_c_parse_parameters(const Noc_Token_Stream *stream,
                                   Noc_Token_Range parameters,
                                   Noc_C_Parameter_List *list)
{
    Noc_C_Parameter_List parsed = {0};
    Noc_Argument_List arguments = {0};
    Noc_Token_Range inner;
    size_t i;
    bool ok = false;
    if (!noc__c_delimited_range_is_valid(stream, parameters, "(", ")")) return false;
    inner.begin = parameters.begin + 1;
    inner.end = parameters.end - 1;
    if (!noc_parse_arguments(stream, inner, &arguments)) return false;
    for (i = 0; i < arguments.count; ++i) {
        Noc_C_Parameter parameter;
        Noc_Token_Range trimmed = noc_token_range_trim_trivia(stream,
                                                               arguments.items[i]);
        parameter.range = trimmed;
        parameter.name_token = noc__c_parameter_name(stream, trimmed);
        parameter.is_variadic = trimmed.end == trimmed.begin + 1 &&
                                noc_token_is_punct(stream->items[trimmed.begin], "...");
        if (parameter.is_variadic) parameter.name_token = NOC_TOKEN_INDEX_NONE;
        if (!noc__c_parameter_append(&parsed, parameter)) goto done;
    }
    noc_c_parameter_list_free(list);
    *list = parsed;
    memset(&parsed, 0, sizeof(parsed));
    ok = true;

done:
    noc_argument_list_free(&arguments);
    noc_c_parameter_list_free(&parsed);
    return ok;
}

NOCDEF bool noc_c_compound_statement_is_valid(const Noc_Token_Stream *stream,
                                              Noc_Token_Range compound)
{
    return noc__c_delimited_range_is_valid(stream, compound, "{", "}");
}

NOCDEF Noc_Token_Range noc_c_compound_statement_inner(const Noc_Token_Stream *stream,
                                                      Noc_Token_Range compound)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    if (!noc_c_compound_statement_is_valid(stream, compound)) return invalid;
    compound.begin += 1;
    compound.end -= 1;
    return compound;
}

#undef NOC__C_AST_ARRAY_COUNT

#endif /* NOC_AST_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
