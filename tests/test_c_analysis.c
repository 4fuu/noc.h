#include "test_support.h"

static bool external_name_equals(const Noc_C_Translation_Unit *unit,
                                 const Noc_C_External_Item *item,
                                 const char *expected)
{
    if (item->name_token == NOC_TOKEN_INDEX_NONE ||
        item->name_token >= unit->stream->count) {
        return false;
    }
    return noc_token_is_identifier(unit->stream->items[item->name_token], expected);
}

static void test_c_translation_unit_analysis(void)
{
    static const char source[] =
        "#define API extern\n"
        "typedef struct Pair { int x; int y; } Pair;\n"
        "struct Forward;\n"
        "enum Kind { KIND_A, KIND_B };\n"
        "static int global = make_value(1);\n"
        "int prototype(const char *name, int values[4]);\n"
        "int (*callback)(int);\n"
        "int decorated(void) __attribute__((unused));\n"
        "int log_message(const char *format, ...);\n"
        "int add(int left, int right) { return left + right; }\n"
        "int first, second;\n"
        "_Static_assert(1, \"ok\");\n"
        "unfinished";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_Syntax_Tree invalid_tree = {0};
    Noc_C_Translation_Unit unit = {0};
    Noc_C_Parameter_List parameters = {0};
    Noc_C_External_Item *preserved_items;
    Noc_C_Parameter *preserved_parameters;
    Diagnostic_State diagnostics = {0};
    const Noc_C_External_Item *item;

    noc_context_init(&context);
    noc_context_set_diagnostic(&context, count_diagnostics, &diagnostics);
    CHECK(noc_tokenize(&context, "analysis.c", source, sizeof(source) - 1, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(unit.count == 12);
    preserved_items = unit.items;

    item = noc_c_external_item(&unit, 0);
    CHECK(item != NULL);
    CHECK(item->kind == NOC_C_EXTERNAL_DECLARATION);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TYPEDEF);
    CHECK(external_name_equals(&unit, item, "Pair"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->range),
                       "typedef struct Pair { int x; int y; } Pair;"));

    item = noc_c_external_item(&unit, 1);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "Forward"));
    item = noc_c_external_item(&unit, 2);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "Kind"));

    item = noc_c_external_item(&unit, 3);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "global"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->signature),
                       "static int global = make_value(1)"));

    item = noc_c_external_item(&unit, 4);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "prototype"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->parameters),
                       "(const char *name, int values[4])"));
    CHECK(item->body.begin == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_c_parse_parameters(unit.stream, item->parameters, &parameters));
    CHECK(parameters.count == 2);
    CHECK(slice_equals(noc_token_range_source(unit.stream, parameters.items[0].range),
                       "const char *name"));
    CHECK(external_name_equals(&unit,
                               &(Noc_C_External_Item){.name_token =
                                                          parameters.items[0].name_token},
                               "name"));
    CHECK(external_name_equals(&unit,
                               &(Noc_C_External_Item){.name_token =
                                                          parameters.items[1].name_token},
                               "values"));
    CHECK(!parameters.items[0].is_variadic);
    preserved_parameters = parameters.items;
    CHECK(!noc_c_parse_parameters(unit.stream, item->body, &parameters));
    CHECK(parameters.items == preserved_parameters);

    item = noc_c_external_item(&unit, 5);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "callback"));

    item = noc_c_external_item(&unit, 6);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "decorated"));

    item = noc_c_external_item(&unit, 7);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "log_message"));
    CHECK(noc_c_parse_parameters(unit.stream, item->parameters, &parameters));
    CHECK(parameters.count == 2);
    CHECK(external_name_equals(&unit,
                               &(Noc_C_External_Item){.name_token =
                                                          parameters.items[0].name_token},
                               "format"));
    CHECK(parameters.items[1].is_variadic);
    CHECK(parameters.items[1].name_token == NOC_TOKEN_INDEX_NONE);

    item = noc_c_external_item(&unit, 8);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "add"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->signature),
                       "int add(int left, int right)"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->body),
                       "{ return left + right; }"));
    CHECK(noc_token_range_location(unit.stream, item->range).line == 10);
    CHECK(noc_c_compound_statement_is_valid(unit.stream, item->body));
    CHECK(slice_equals(noc_token_range_source(
                           unit.stream,
                           noc_c_compound_statement_inner(unit.stream, item->body)),
                       " return left + right; "));
    CHECK(!noc_c_compound_statement_is_valid(unit.stream, item->signature));
    CHECK(noc_c_compound_statement_inner(unit.stream, item->signature).begin ==
          NOC_TOKEN_INDEX_NONE);

    item = noc_c_external_item(&unit, 9);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_UNKNOWN);
    CHECK(item->name_token == NOC_TOKEN_INDEX_NONE);
    item = noc_c_external_item(&unit, 10);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_UNKNOWN);
    CHECK(item->name_token == NOC_TOKEN_INDEX_NONE);
    item = noc_c_external_item(&unit, 11);
    CHECK(item->kind == NOC_C_EXTERNAL_UNKNOWN);
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->range), "unfinished"));

    CHECK(strcmp(noc_c_external_kind_name(NOC_C_EXTERNAL_FUNCTION_DEFINITION),
                 "function definition") == 0);
    CHECK(strcmp(noc_c_external_kind_name((Noc_C_External_Kind)99),
                 "unknown external item") == 0);
    CHECK(strcmp(noc_c_declaration_kind_name(NOC_C_DECLARATION_TYPEDEF),
                 "typedef declaration") == 0);
    CHECK(strcmp(noc_c_declaration_kind_name((Noc_C_Declaration_Kind)99),
                 "unknown declaration") == 0);
    CHECK(noc_c_external_item(&unit, unit.count) == NULL);

    noc_syntax_tree_free(&tree);
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(!noc_c_translation_unit_build(&context, &invalid_tree, &unit));
    CHECK(unit.items == preserved_items);
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(diagnostics.errors == 1);

    CHECK(!noc_tokenize(&context, "bad-replacement.c", "/*", 2, &stream));
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(unit.items == preserved_items);
    CHECK(diagnostics.errors == 2);
    CHECK(noc_tokenize(&context, "replacement.c", "int replacement;", 16, &stream));
    CHECK(!noc_c_translation_unit_is_valid(&unit));
    CHECK(noc_c_external_item(&unit, 0) == NULL);
    noc_c_parameter_list_free(&parameters);
    CHECK(parameters.items == NULL && parameters.count == 0);
    noc_c_translation_unit_free(&unit);
    CHECK(!noc_c_translation_unit_is_valid(&unit));
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

static void test_c_analysis_rebuild_and_preprocessor(void)
{
    static const char conditional[] =
        "int\n"
        "#if FLAG\n"
        "const\n"
        "#endif\n"
        "conditional;";
    static const char directives[] = "#define M(x) { x; }\n\n";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_C_Translation_Unit unit = {0};
    const Noc_C_External_Item *item;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, "old.c", "int old;", 8, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(unit.count == 1);

    CHECK(noc_tokenize(&context,
                       "conditional.c",
                       conditional,
                       sizeof(conditional) - 1,
                       &stream));
    CHECK(!noc_c_translation_unit_is_valid(&unit));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(unit.count == 1);
    item = noc_c_external_item(&unit, 0);
    CHECK(external_name_equals(&unit, item, "conditional"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->range), conditional));

    CHECK(noc_tokenize(&context,
                       "directives.c",
                       directives,
                       sizeof(directives) - 1,
                       &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(noc_c_translation_unit_is_valid(&unit));
    CHECK(unit.count == 0);
    CHECK(unit.items == NULL);

    noc_c_translation_unit_free(&unit);
    noc_syntax_tree_free(&tree);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

static void test_c_declarator_boundaries(void)
{
    static const char source[] =
        "int __attribute__((noinline)) prefixed(void) { return 0; }\n"
        "int after_prefixed;\n"
        "struct Result { int x; } make_result(void) { return (struct Result){0}; }\n"
        "int after_result;\n"
        "int (*factory(void))(int);\n"
        "int (*callback)(int);\n"
        "int (*factory_definition(void))(int) { return callback; }\n"
        "int after_factory;\n"
        "struct { int x; } point;\n"
        "enum { OFF, ON } state;\n"
        "struct __attribute__((packed)) Packed { int x; };\n"
        "int f(void), g(void);\n"
        "int values[] = {1, 2};\n"
        "struct Result value = { .x = 1 };\n"
        "int trailing(void) __attribute__((noinline)) { return 0; }\n"
        "int (parenthesized);\n"
        "int (* const qualified_callback)(int);\n"
        "int old_style(a)\n"
        "int a;\n"
        "{ return a; }\n"
        "int after_old_style;\n"
        "int value_before, callback_after(void);\n"
        "static struct { int x; } static_point;\n"
        "static struct StaticResult { int x; } static_make(void) "
        "{ return (struct StaticResult){0}; }\n"
        "int after_static;\n"
        "[[deprecated]] struct Tagged { int x; };\n"
        "union { int integer; } union_value;\n"
        "static enum State { STATE_OFF, STATE_ON } static_state;\n"
        "struct [[deprecated]] TagPositioned;\n";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_C_Translation_Unit unit = {0};
    const Noc_C_External_Item *item;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, "declarators.c", source, sizeof(source) - 1, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(unit.count == 28);

    item = noc_c_external_item(&unit, 0);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "prefixed"));
    item = noc_c_external_item(&unit, 1);
    CHECK(external_name_equals(&unit, item, "after_prefixed"));

    item = noc_c_external_item(&unit, 2);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "make_result"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->body),
                       "{ return (struct Result){0}; }"));
    item = noc_c_external_item(&unit, 3);
    CHECK(external_name_equals(&unit, item, "after_result"));

    item = noc_c_external_item(&unit, 4);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(external_name_equals(&unit, item, "factory"));
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->parameters), "(void)"));
    item = noc_c_external_item(&unit, 5);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "callback"));
    item = noc_c_external_item(&unit, 6);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "factory_definition"));
    item = noc_c_external_item(&unit, 7);
    CHECK(external_name_equals(&unit, item, "after_factory"));

    item = noc_c_external_item(&unit, 8);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "point"));
    item = noc_c_external_item(&unit, 9);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "state"));
    item = noc_c_external_item(&unit, 10);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "Packed"));
    item = noc_c_external_item(&unit, 11);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_UNKNOWN);
    CHECK(item->name_token == NOC_TOKEN_INDEX_NONE);
    item = noc_c_external_item(&unit, 12);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "values"));
    item = noc_c_external_item(&unit, 13);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "value"));
    item = noc_c_external_item(&unit, 14);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "trailing"));
    item = noc_c_external_item(&unit, 15);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "parenthesized"));
    item = noc_c_external_item(&unit, 16);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "qualified_callback"));
    item = noc_c_external_item(&unit, 18);
    CHECK(item->kind == NOC_C_EXTERNAL_UNKNOWN);
    CHECK(slice_equals(noc_token_range_source(unit.stream, item->range), "{ return a; }"));
    item = noc_c_external_item(&unit, 19);
    CHECK(item->kind == NOC_C_EXTERNAL_DECLARATION);
    CHECK(external_name_equals(&unit, item, "after_old_style"));
    item = noc_c_external_item(&unit, 20);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_UNKNOWN);
    CHECK(item->name_token == NOC_TOKEN_INDEX_NONE);
    item = noc_c_external_item(&unit, 21);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "static_point"));
    item = noc_c_external_item(&unit, 22);
    CHECK(item->kind == NOC_C_EXTERNAL_FUNCTION_DEFINITION);
    CHECK(external_name_equals(&unit, item, "static_make"));
    item = noc_c_external_item(&unit, 23);
    CHECK(external_name_equals(&unit, item, "after_static"));
    item = noc_c_external_item(&unit, 24);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "Tagged"));
    item = noc_c_external_item(&unit, 25);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "union_value"));
    item = noc_c_external_item(&unit, 26);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_OBJECT);
    CHECK(external_name_equals(&unit, item, "static_state"));
    item = noc_c_external_item(&unit, 27);
    CHECK(item->declaration_kind == NOC_C_DECLARATION_TAG);
    CHECK(external_name_equals(&unit, item, "TagPositioned"));

    noc_c_translation_unit_free(&unit);
    noc_syntax_tree_free(&tree);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

static void test_c_parameter_name_boundaries(void)
{
    static const char source[] =
        "void parameter_cases(int [N], int (*)(Event *), typeof(expression), "
        "struct Local { int *member; } value, int values[N], "
        "int (*callback)(Event *), int (* const qualified)(void), size_t count, "
        "struct __attribute__((packed)) Packed, "
        "struct __attribute__((packed)) Packed packed_value, "
        "int array_values[sizeof(*p)], int [sizeof(*p)], "
        "typeof((*expression)) typed_value, "
        "int (* __attribute__((unused)) attributed_callback)(void), "
        "struct [[deprecated]] Tagged);";
    Noc_Context context;
    Noc_Token_Stream stream = {0};
    Noc_Syntax_Tree tree = {0};
    Noc_C_Translation_Unit unit = {0};
    Noc_C_Parameter_List parameters = {0};
    const Noc_C_External_Item *function;

    noc_context_init(&context);
    CHECK(noc_tokenize(&context, "parameters.c", source, sizeof(source) - 1, &stream));
    CHECK(noc_syntax_tree_build(&context, &stream, &tree));
    CHECK(noc_c_translation_unit_build(&context, &tree, &unit));
    CHECK(unit.count == 1);
    function = noc_c_external_item(&unit, 0);
    CHECK(function->declaration_kind == NOC_C_DECLARATION_FUNCTION);
    CHECK(noc_c_parse_parameters(unit.stream, function->parameters, &parameters));
    CHECK(parameters.count == 15);
    CHECK(parameters.items[0].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(parameters.items[1].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(parameters.items[2].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[3].name_token],
                                  "value"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[4].name_token],
                                  "values"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[5].name_token],
                                  "callback"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[6].name_token],
                                  "qualified"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[7].name_token],
                                  "count"));
    CHECK(parameters.items[8].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[9].name_token],
                                  "packed_value"));
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[10].name_token],
                                  "array_values"));
    CHECK(parameters.items[11].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(noc_token_is_identifier(unit.stream->items[parameters.items[12].name_token],
                                  "typed_value"));
    CHECK(parameters.items[13].name_token == NOC_TOKEN_INDEX_NONE);
    CHECK(parameters.items[14].name_token == NOC_TOKEN_INDEX_NONE);

    noc_c_parameter_list_free(&parameters);
    noc_c_translation_unit_free(&unit);
    noc_syntax_tree_free(&tree);
    noc_token_stream_free(&stream);
    noc_context_deinit(&context);
}

int main(void)
{
    test_c_translation_unit_analysis();
    test_c_analysis_rebuild_and_preprocessor();
    test_c_declarator_boundaries();
    test_c_parameter_name_boundaries();
    return finish_suite("c-analysis");
}
