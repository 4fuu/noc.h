#include "translation_test_support.h"

static void test_duplicate_controls(void) {
    Translation_Fixture f;
    Noc_Preprocessor_Translation t = {0};
    size_t guard;
    size_t once;
    translation_fixture_init(&f, "#include \"guard.h\"\n#include \"guard.h\"\n"
                                 "#include \"once.h\"\n#include \"once.h\"\n");
    guard = translation_add(
        &f, "guard.h",
        "#ifndef GUARD_H\n#define GUARD_H\nint guarded;\n#endif\n",
        NOC_SOURCE_CLASS_PROJECT);
    once = translation_add(&f, "once.h", "#pragma once\nint once_body;\n",
                           NOC_SOURCE_CLASS_PROJECT);
    translation_bind(&f, "guard.h", guard);
    translation_bind(&f, "once.h", once);
    CHECK(translation_build(&f, translation_full_options(), &t).status ==
          NOC_PREPROCESSOR_TRANSLATION_OK);
    CHECK(f.resolver_calls == 4);
    CHECK(translation_token_occurrences(&t, "guarded") == 1);
    CHECK(translation_token_occurrences(&t, "once_body") == 1);
    noc_preprocessor_translation_free(&t);
    translation_fixture_deinit(&f);
}

static void test_inactive_cycle_and_transaction(void) {
    static const char failed_source[] =
        "#if 0\n#include \"inactive.h\"\n#endif\n#include \"cycle.h\"\n";
    Translation_Fixture f;
    Noc_Preprocessor_Translation t = {0};
    Noc_Preprocessor_Translation_Result result;
    size_t cycle;
    translation_fixture_init(&f, "int preserved;\n");
    CHECK(translation_build(&f, translation_full_options(), &t).status ==
          NOC_PREPROCESSOR_TRANSLATION_OK);
    CHECK(noc_workspace_update_document(
              &f.workspace, &f.files[0], failed_source,
              sizeof(failed_source) - 1, &f.files[0]) == NOC_WORKSPACE_OK);
    cycle = translation_add(&f, "cycle.h", "#include \"cycle.h\"\n",
                            NOC_SOURCE_CLASS_PROJECT);
    translation_bind(&f, "cycle.h", cycle);
    result = translation_build(&f, translation_full_options(), &t);
    CHECK(result.status == NOC_PREPROCESSOR_TRANSLATION_CYCLE);
    CHECK(f.resolver_calls == 2);
    CHECK(f.resolver_name_count == 2);
    CHECK(strcmp(f.resolver_names[0], "cycle.h") == 0);
    CHECK(strcmp(f.resolver_names[1], "cycle.h") == 0);
    CHECK(noc_preprocessor_translation_is_valid(&t));
    CHECK(translation_contains(&t, "preserved"));
    noc_preprocessor_translation_free(&t);
    CHECK(!noc_preprocessor_translation_is_valid(&t));
    translation_fixture_deinit(&f);
}

static void test_trusted_only_default(void) {
    Translation_Fixture f;
    Noc_Preprocessor_Translation t = {0};
    Noc_Preprocessor_Translation_Options options =
        noc_preprocessor_translation_default_options();
    translation_fixture_init(&f, "#define PROJECT_MACRO 1\nint value;\n");
    CHECK(options.macro_policy == NOC_MACROS_TRUSTED_ONLY);
    CHECK(translation_build(&f, options, &t).status !=
          NOC_PREPROCESSOR_TRANSLATION_OK);
    CHECK(!noc_preprocessor_translation_is_valid(&t));
    options = translation_full_options();
    options.macro_policy = (Noc_Macro_Policy)999;
    CHECK(translation_build(&f, options, &t).status ==
          NOC_PREPROCESSOR_TRANSLATION_INVALID_ARGUMENT);
    translation_fixture_deinit(&f);
}

int main(void) {
    test_duplicate_controls();
    test_inactive_cycle_and_transaction();
    test_trusted_only_default();
    return finish_suite("translation control");
}
