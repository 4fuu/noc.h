#include "translation_test_support.h"

static void test_ordered_macro_effects_and_queries(void) {
    Translation_Fixture f;
    Noc_Preprocessor_Translation t = {0};
    Noc_Preprocessor_Translation_Result result;
    size_t child;
    size_t nested;

    translation_fixture_init(&f,
                             "#define HEADER \"child.h\"\n"
                             "#define PARENT 1\n"
                             "#define GONE 1\n"
                             "#include HEADER\n"
                             "#if CHILD\nint after_define;\n#endif\n"
                             "#if GONE\nint after_undef;\n#endif\nint last;\n");
    child =
        translation_add(&f, "child.h",
                        "#if PARENT\nint child_first;\n#endif\n"
                        "#include \"nested.h\"\n#define CHILD 1\n#undef GONE\n",
                        NOC_SOURCE_CLASS_PROJECT);
    nested = translation_add(&f, "nested.h", "int nested_second;\n",
                             NOC_SOURCE_CLASS_PROJECT);
    translation_bind(&f, "child.h", child);
    translation_bind(&f, "nested.h", nested);
    result = translation_build(&f, translation_full_options(), &t);
    CHECK(result.status == NOC_PREPROCESSOR_TRANSLATION_OK);
    CHECK(noc_preprocessor_translation_is_valid(&t));
    CHECK(noc_preprocessor_translation_file_count(&t) == 3);
    CHECK(noc_preprocessor_translation_snapshot_at(&t, 1) != NULL);
    CHECK(noc_preprocessor_translation_unit_at(&t, 2) != NULL);
    CHECK(noc_preprocessor_translation_environment(&t) != NULL);
    CHECK(noc_preprocessor_translation_logical_source(&t) != NULL);
    CHECK(translation_contains(&t, "child_first"));
    CHECK(translation_contains(&t, "nested_second"));
    CHECK(translation_contains(&t, "after_define"));
    CHECK(!translation_contains(&t, "after_undef"));
    {
        size_t child_first = translation_token_position(&t, "child_first");
        size_t nested_second = translation_token_position(&t, "nested_second");
        size_t after_define = translation_token_position(&t, "after_define");
        size_t last = translation_token_position(&t, "last");
        CHECK(child_first < nested_second);
        CHECK(nested_second < after_define);
        CHECK(after_define < last);
    }
    CHECK(noc_macro_environment_lookup(
              noc_preprocessor_translation_environment(&t),
              (Noc_Slice){"CHILD", 5}) != NULL);
    CHECK(noc_macro_environment_lookup(
              noc_preprocessor_translation_environment(&t),
              (Noc_Slice){"GONE", 4}) == NULL);
    noc_preprocessor_translation_free(&t);
    translation_fixture_deinit(&f);
}

static void test_logical_source_builds_complete_ast(void) {
    Translation_Fixture f;
    Noc_Preprocessor_Translation t = {0};
    Noc_Logical_C_Parse_Tree tree = {0};
    Noc_Logical_C_Ast ast = {0};
    Noc_Preprocessor_Translation_Result result;
    size_t header;

    translation_fixture_init(
        &f, "#include \"decl.h\"\nint main(void) { return value; }\n");
    header = translation_add(&f, "decl.h", "int value = 3;\n",
                             NOC_SOURCE_CLASS_PROJECT);
    translation_bind(&f, "decl.h", header);
    result = translation_build(&f, translation_full_options(), &t);
    CHECK(result.status == NOC_PREPROCESSOR_TRANSLATION_OK);
    CHECK(noc_logical_c_parse_tree_build(
              noc_preprocessor_translation_logical_source(&t),
              noc_c_parse_default_options(), &tree) == NOC_C_PARSE_OK);
    CHECK(!noc_logical_c_parse_tree_has_error(&tree));
    CHECK(noc_logical_c_ast_build(&tree, noc_c_ast_default_options(), &ast) ==
          NOC_C_AST_OK);
    CHECK(noc_logical_c_ast_is_syntax_complete(&ast));
    noc_logical_c_ast_free(&ast);
    noc_logical_c_parse_tree_free(&tree);
    noc_preprocessor_translation_free(&t);
    translation_fixture_deinit(&f);
}

int main(void) {
    CHECK(strcmp(noc_preprocessor_translation_status_name(
                     NOC_PREPROCESSOR_TRANSLATION_OK),
                 "ok") == 0);
    CHECK(strcmp(noc_preprocessor_translation_status_name(
                     (Noc_Preprocessor_Translation_Status)999),
                 "unknown") == 0);
    test_ordered_macro_effects_and_queries();
    test_logical_source_builds_complete_ast();
    return finish_suite("translation execution");
}
