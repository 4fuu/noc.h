#define NOB_IMPLEMENTATION
#include "nob.h"

#include <errno.h>

#if defined(_WIN32)
#include <direct.h>
#define NOC_EXE ".exe"
#else
#define NOC_EXE ""
#endif

#define NOC_FUZZ "build/noc-fuzz" NOC_EXE
#define NOC_C_PARSE_FUZZ "build/noc-c-parse-fuzz" NOC_EXE
#define NOC_EMBED_DIALECT "build/embed-dialect" NOC_EXE
#define NOC_EMBED_EXAMPLE "build/embed-example" NOC_EXE
#define NOC_RULES_DIALECT "build/rules-dialect" NOC_EXE
#define NOC_RULES_EXAMPLE "build/rules-example" NOC_EXE
#define NOC_IDE_EXAMPLE "build/ide-example" NOC_EXE
#define NOC_GENERATED_HEADER "build/generated/noc.h"
#define NOC_RELEASE_HEADER "release/noc.h"
#define NOC_AMALGAMATE "build/amalgamate" NOC_EXE
#define NOC_MODULES_TEST "build/noc-test-modules" NOC_EXE
#if defined(_WIN32)
#define NOC_IMPLEMENTATION_OBJECT "build/noc-implementation.obj"
#else
#define NOC_IMPLEMENTATION_OBJECT "build/noc-implementation.o"
#endif

static const char *amalgamation_sources[] = {
    "include/noc/noc.h",
    "src/internal.h",
    "third_party/tree-sitter/tree_sitter.c",
    "src/lexer.c",
    "src/source.c",
    "src/features.c",
    "src/macro_directives.c",
    "src/preprocessor.c",
    "src/macro_invocations.c",
    "src/macro_environment.c",
    "src/macro_expansion.c",
    "src/logical_source.c",
    "src/conditional.c",
    "src/conditional_groups.c",
    "src/include_control.c",
    "src/include_resolver.c",
    "src/include_expansion.c",
    "src/include_graph.c",
    "src/parser.c",
    "src/ast.c",
    "src/c_parse.c",
    "src/logical_c_parse.c",
    "src/logical_ast.c",
    "src/lower.c",
    "src/emit_c.c",
};

static const char *implementation_modules[] = {
    "third_party/tree-sitter/tree_sitter.c", "src/lexer.c", "src/source.c",
    "src/features.c", "src/macro_directives.c",
    "src/preprocessor.c", "src/macro_invocations.c", "src/macro_environment.c",
    "src/macro_expansion.c", "src/logical_source.c", "src/conditional.c",
    "src/conditional_groups.c",
    "src/include_control.c", "src/include_resolver.c", "src/include_expansion.c",
    "src/include_graph.c", "src/parser.c", "src/ast.c", "src/c_parse.c",
    "src/logical_c_parse.c", "src/logical_ast.c",
    "src/lower.c", "src/emit_c.c",
};

typedef struct {
    const char *key;
    const char *source;
    const char *output;
    bool compile_as_cpp;
} Test_Suite;

static const Test_Suite test_suites[] = {
    {"header-c", "tests/test_header.c", "build/noc-test-header-c" NOC_EXE, false},
    {"header-cpp", "tests/test_header.c", "build/noc-test-header-cpp" NOC_EXE, true},
    {"public-header-c", "tests/test_public_header.c", "build/noc-test-public-header-c" NOC_EXE, false},
    {"public-header-cpp", "tests/test_public_header.c", "build/noc-test-public-header-cpp" NOC_EXE, true},
    {"workspace", "tests/test_workspace.c", "build/noc-test-workspace" NOC_EXE, false},
    {"preprocessing-tokens", "tests/test_preprocessing_tokens.c", "build/noc-test-preprocessing-tokens" NOC_EXE, false},
    {"macro-directives", "tests/test_macro_directives.c", "build/noc-test-macro-directives" NOC_EXE, false},
    {"macro-invocations", "tests/test_macro_invocations.c", "build/noc-test-macro-invocations" NOC_EXE, false},
    {"macro-environment", "tests/test_macro_environment.c", "build/noc-test-macro-environment" NOC_EXE, false},
    {"macro-expansion", "tests/test_macro_expansion.c", "build/noc-test-macro-expansion" NOC_EXE, false},
    {"logical-source", "tests/test_logical_source.c", "build/noc-test-logical-source" NOC_EXE, false},
    {"logical-source-lifecycle", "tests/test_logical_source_lifecycle.c", "build/noc-test-logical-source-lifecycle" NOC_EXE, false},
    {"logical-source-queries", "tests/test_logical_source_queries.c", "build/noc-test-logical-source-queries" NOC_EXE, false},
    {"function-macro-expansion", "tests/test_function_macro_expansion.c", "build/noc-test-function-macro-expansion" NOC_EXE, false},
    {"variadic-macro-expansion", "tests/test_variadic_macro_expansion.c", "build/noc-test-variadic-macro-expansion" NOC_EXE, false},
    {"macro-stringification", "tests/test_macro_stringification.c", "build/noc-test-macro-stringification" NOC_EXE, false},
    {"macro-token-paste", "tests/test_macro_token_paste.c", "build/noc-test-macro-token-paste" NOC_EXE, false},
    {"macro-builtins", "tests/test_macro_builtins.c", "build/noc-test-macro-builtins" NOC_EXE, false},
    {"configured-builtins", "tests/test_configured_builtins.c", "build/noc-test-configured-builtins" NOC_EXE, false},
    {"preprocessor-expressions", "tests/test_preprocessor_expressions.c", "build/noc-test-preprocessor-expressions" NOC_EXE, false},
    {"conditional-groups", "tests/test_conditional_groups.c", "build/noc-test-conditional-groups" NOC_EXE, false},
    {"pragma-once", "tests/test_pragma_once.c", "build/noc-test-pragma-once" NOC_EXE, false},
    {"include-guard", "tests/test_include_guard.c", "build/noc-test-include-guard" NOC_EXE, false},
    {"include-control-queries", "tests/test_include_control_queries.c", "build/noc-test-include-control-queries" NOC_EXE, false},
    {"include-operands", "tests/test_include_operands.c", "build/noc-test-include-operands" NOC_EXE, false},
    {"include-resolver", "tests/test_include_resolver.c", "build/noc-test-include-resolver" NOC_EXE, false},
    {"include-expansion", "tests/test_include_expansion.c", "build/noc-test-include-expansion" NOC_EXE, false},
    {"include-expansion-resolver", "tests/test_include_expansion_resolver.c", "build/noc-test-include-expansion-resolver" NOC_EXE, false},
    {"include-graph", "tests/test_include_graph.c", "build/noc-test-include-graph" NOC_EXE, false},
    {"include-graph-limits", "tests/test_include_graph_limits.c", "build/noc-test-include-graph-limits" NOC_EXE, false},
    {"include-graph-queries", "tests/test_include_graph_queries.c", "build/noc-test-include-graph-queries" NOC_EXE, false},
    {"release-header-runtime", "tests/test_release_header_runtime.c", "build/noc-test-release-header-runtime" NOC_EXE, false},
    {"preprocessor", "tests/test_preprocessor.c", "build/noc-test-preprocessor" NOC_EXE, false},
    {"lexing", "tests/test_lexing.c", "build/noc-test-lexing" NOC_EXE, false},
    {"syntax", "tests/test_syntax.c", "build/noc-test-syntax" NOC_EXE, false},
    {"c-analysis", "tests/test_c_analysis.c", "build/noc-test-c-analysis" NOC_EXE, false},
    {"c-parse-tree", "tests/test_c_parse_tree.c", "build/noc-test-c-parse-tree" NOC_EXE, false},
    {"c-parse-recovery", "tests/test_c_parse_recovery.c", "build/noc-test-c-parse-recovery" NOC_EXE, false},
    {"c-parse-lifecycle", "tests/test_c_parse_lifecycle.c", "build/noc-test-c-parse-lifecycle" NOC_EXE, false},
    {"c-parse-corpus", "tests/test_c_parse_corpus.c", "build/noc-test-c-parse-corpus" NOC_EXE, false},
    {"c-parse-malformed-corpus", "tests/test_c_parse_malformed_corpus.c", "build/noc-test-c-parse-malformed-corpus" NOC_EXE, false},
    {"c-parse-completion", "tests/test_c_parse_completion.c", "build/noc-test-c-parse-completion" NOC_EXE, false},
    {"c-parse-cpp-runtime", "tests/test_c_parse_cpp_runtime.c", "build/noc-test-c-parse-cpp-runtime" NOC_EXE, true},
    {"logical-c-parse", "tests/test_logical_c_parse.c", "build/noc-test-logical-c-parse" NOC_EXE, false},
    {"logical-c-parse-lifecycle", "tests/test_logical_c_parse_lifecycle.c", "build/noc-test-logical-c-parse-lifecycle" NOC_EXE, false},
    {"logical-c-ast", "tests/test_logical_c_ast.c", "build/noc-test-logical-c-ast" NOC_EXE, false},
    {"logical-c-ast-lifecycle", "tests/test_logical_c_ast_lifecycle.c", "build/noc-test-logical-c-ast-lifecycle" NOC_EXE, false},
    {"logical-c-ast-completion", "tests/test_logical_c_ast_completion.c", "build/noc-test-logical-c-ast-completion" NOC_EXE, false},
    {"c-ast", "tests/test_c_ast.c", "build/noc-test-c-ast" NOC_EXE, false},
    {"c-ast-details", "tests/test_c_ast_details.c", "build/noc-test-c-ast-details" NOC_EXE, false},
    {"c-ast-c11", "tests/test_c_ast_c11.c", "build/noc-test-c-ast-c11" NOC_EXE, false},
    {"c-ast-c11-constructs", "tests/test_c_ast_c11_constructs.c", "build/noc-test-c-ast-c11-constructs" NOC_EXE, false},
    {"c-ast-declarators", "tests/test_c_ast_declarators.c", "build/noc-test-c-ast-declarators" NOC_EXE, false},
    {"c-ast-completion", "tests/test_c_ast_completion.c", "build/noc-test-c-ast-completion" NOC_EXE, false},
    {"c-ast-queries", "tests/test_c_ast_queries.c", "build/noc-test-c-ast-queries" NOC_EXE, false},
    {"c-ast-recovery", "tests/test_c_ast_recovery.c", "build/noc-test-c-ast-recovery" NOC_EXE, false},
    {"tree-sitter-coexistence", "tests/test_tree_sitter_coexistence.c", "build/noc-test-tree-sitter-coexistence" NOC_EXE, false},
    {"rewriter", "tests/test_rewriter.c", "build/noc-test-rewriter" NOC_EXE, false},
    {"artifacts", "tests/test_artifacts.c", "build/noc-test-artifacts" NOC_EXE, false},
};

#if !defined(_MSC_VER) || defined(__clang__)
static const char *compiler(void)
{
    const char *cc = getenv("CC");
    return cc && cc[0] ? cc : "cc";
}

static const char *cpp_compiler(void)
{
    const char *cxx = getenv("CXX");
    return cxx && cxx[0] ? cxx : "c++";
}
#endif

static bool ensure_build_directories(void)
{
    return nob_mkdir_if_not_exists("build") &&
           nob_mkdir_if_not_exists("build/generated") &&
           nob_mkdir_if_not_exists("build/generated/ide") &&
           nob_mkdir_if_not_exists("release");
}

static int output_rebuild_state(const char *output,
                                const char **inputs,
                                size_t inputs_count)
{
    int result = nob_needs_rebuild(output, inputs, inputs_count);
    if (result < 0) {
        nob_log(NOB_ERROR, "could not determine whether %s needs rebuilding", output);
    }
    return result;
}

static bool verify_generated_file(const char *actual_path, const char *expected_path)
{
    Nob_String_Builder actual = {0};
    Nob_String_Builder expected = {0};
    bool ok = false;
    if (!nob_read_entire_file(actual_path, &actual) ||
        !nob_read_entire_file(expected_path, &expected)) {
        goto done;
    }
    ok = actual.count == expected.count &&
         (actual.count == 0 || memcmp(actual.items, expected.items, actual.count) == 0);
    if (!ok) {
        nob_log(NOB_ERROR, "%s differs from %s", actual_path, expected_path);
    }

done:
    nob_sb_free(actual);
    nob_sb_free(expected);
    return ok;
}

static bool generate_header(const char *path)
{
    Nob_Cmd command = {0};
    size_t i;
    const char *tool_inputs[] = {"tools/amalgamate.c", "nob.c"};
    int rebuild = output_rebuild_state(NOC_AMALGAMATE,
                                       tool_inputs,
                                       NOB_ARRAY_LEN(tool_inputs));
    if (rebuild < 0) return false;
    if (rebuild != 0) {
#if defined(_MSC_VER) && !defined(__clang__)
        nob_cmd_append(&command, "cl.exe", "/std:c11", "/W4", "/WX", "/nologo",
                       "/D_CRT_SECURE_NO_WARNINGS",
                       nob_temp_sprintf("/Fe:%s", NOC_AMALGAMATE),
                       "tools/amalgamate.c");
#else
        nob_cmd_append(&command, compiler(), "-std=c11", "-Wall", "-Wextra",
                       "-Wpedantic", "-Werror", "-o", NOC_AMALGAMATE,
                       "tools/amalgamate.c");
#endif
        if (!nob_cmd_run(&command)) return false;
    }
    nob_cmd_append(&command, NOC_AMALGAMATE, path);
    for (i = 0; i < NOB_ARRAY_LEN(amalgamation_sources); ++i) {
        nob_cmd_append(&command, amalgamation_sources[i]);
    }
    return nob_cmd_run(&command);
}

static bool verify_release_header(void)
{
    const char *first = "build/generated/noc.verify-1.h";
    const char *second = "build/generated/noc.verify-2.h";
    bool ok = generate_header(first) && generate_header(second) &&
              verify_generated_file(first, second) &&
              verify_generated_file(first, NOC_RELEASE_HEADER);
    remove(first);
    remove(second);
    if (!ok) nob_log(NOB_ERROR, "%s is stale; run ./nob header", NOC_RELEASE_HEADER);
    return ok;
}

static bool prepare_generated_header(void)
{
    return generate_header(NOC_GENERATED_HEADER);
}

static void append_compile_command(Nob_Cmd *command,
                                   const char *output,
                                   const char *source,
                                   bool compile_as_cpp)
{
#if defined(_MSC_VER) && !defined(__clang__)
    nob_cmd_append(command,
                   "cl.exe",
                   compile_as_cpp ? "/std:c++14" : "/std:c11",
                   "/W4",
                   "/WX",
                   "/nologo",
                   "/D_CRT_SECURE_NO_WARNINGS",
                   "/Ibuild/generated",
                   "/Iinclude",
                   "/I.",
                   nob_temp_sprintf("/Fe:%s", output),
                   nob_temp_sprintf("/Fo:%s.obj", output),
                   nob_temp_sprintf(compile_as_cpp ? "/Tp%s" : "/Tc%s", source));
#else
    nob_cmd_append(command,
                   compile_as_cpp ? cpp_compiler() : compiler(),
                   compile_as_cpp ? "-std=c++11" : "-std=c11",
                   "-Wall",
                   "-Wextra",
                   "-Wpedantic",
                   "-Werror",
                   "-Ibuild/generated",
                   "-Iinclude",
                   "-I.");
    if (compile_as_cpp) {
        nob_cmd_append(command, "-x", "c++");
    }
    nob_cmd_append(command, "-o", output, source);
#endif
}

static bool build_implementation_object(void)
{
    const char *inputs[] = {NOC_GENERATED_HEADER, "nob.c"};
    Nob_Cmd command = {0};
    int rebuild = output_rebuild_state(NOC_IMPLEMENTATION_OBJECT,
                                       inputs,
                                       NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild == 0) return nob_file_exists(NOC_IMPLEMENTATION_OBJECT) > 0;
#if defined(_MSC_VER) && !defined(__clang__)
    nob_cmd_append(&command,
                   "cl.exe",
                   "/std:c11",
                   "/W4",
                   "/WX",
                   "/nologo",
                   "/D_CRT_SECURE_NO_WARNINGS",
                   "/DNOC_IMPLEMENTATION",
                   "/c",
                   nob_temp_sprintf("/Tc%s", NOC_GENERATED_HEADER),
                   nob_temp_sprintf("/Fo:%s", NOC_IMPLEMENTATION_OBJECT));
#else
    nob_cmd_append(&command,
                   compiler(),
                   "-std=c11",
                   "-Wall",
                   "-Wextra",
                   "-Wpedantic",
                   "-Werror",
                   "-DNOC_IMPLEMENTATION",
                   "-x",
                   "c",
                   "-c",
                   NOC_GENERATED_HEADER,
                   "-o",
                   NOC_IMPLEMENTATION_OBJECT);
#endif
    return nob_cmd_run(&command);
}

static bool build_binary(const char *output, const char *source)
{
    const char *inputs[] = {source, NOC_IMPLEMENTATION_OBJECT, "nob.c"};
    Nob_Cmd command = {0};
    int rebuild;
    if (!build_implementation_object()) return false;
    rebuild = output_rebuild_state(output, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild == 0) {
        return nob_file_exists(output) > 0;
    }
    append_compile_command(&command, output, source, false);
    nob_cmd_append(&command, NOC_IMPLEMENTATION_OBJECT);
    return nob_cmd_run(&command);
}

static bool test_suite_links_implementation(const Test_Suite *suite)
{
    return strcmp(suite->key, "header-c") != 0 &&
           strcmp(suite->key, "header-cpp") != 0 &&
           strcmp(suite->key, "public-header-c") != 0 &&
           strcmp(suite->key, "public-header-cpp") != 0 &&
           strcmp(suite->key, "release-header-runtime") != 0;
}

static bool build_test_suite(const Test_Suite *suite)
{
    const char *inputs[10];
    size_t inputs_count = 0;
    bool link_implementation = test_suite_links_implementation(suite);
    const char *header_input =
        strcmp(suite->key, "release-header-runtime") == 0
            ? NOC_RELEASE_HEADER
            : NOC_GENERATED_HEADER;
    Nob_Cmd command = {0};
    int rebuild;
    if (link_implementation && !build_implementation_object()) return false;
    inputs[inputs_count++] = suite->source;
    inputs[inputs_count++] = "tests/test_support.h";
    inputs[inputs_count++] = "tests/macro_expansion_test_support.h";
    inputs[inputs_count++] = "tests/include_test_support.h";
    inputs[inputs_count++] = "tests/include_expansion_test_support.h";
    inputs[inputs_count++] = "tests/include_graph_test_support.h";
    inputs[inputs_count++] = "tests/include_control_test_support.h";
    inputs[inputs_count++] = link_implementation
                                 ? NOC_IMPLEMENTATION_OBJECT
                                 : header_input;
    inputs[inputs_count++] = "nob.c";
    rebuild = output_rebuild_state(suite->output, inputs, inputs_count);
    if (rebuild < 0) return false;
    if (rebuild == 0) return nob_file_exists(suite->output) > 0;
    append_compile_command(&command,
                           suite->output,
                           suite->source,
                           suite->compile_as_cpp);
    if (link_implementation) {
#if !defined(_MSC_VER) || defined(__clang__)
        /* -x applies to following inputs. Reset it only when the C++ source is
           followed by the separately compiled C implementation object. */
        if (suite->compile_as_cpp) nob_cmd_append(&command, "-x", "none");
#endif
        nob_cmd_append(&command, NOC_IMPLEMENTATION_OBJECT);
    }
    return nob_cmd_run(&command);
}

static bool run_test_suite(const Test_Suite *suite)
{
    Nob_Cmd command = {0};
    if (!build_test_suite(suite)) return false;
    nob_cmd_append(&command, suite->output);
    return nob_cmd_run(&command);
}

static bool run_modules_test(void)
{
    Nob_Cmd command = {0};
    size_t i;
#if defined(_MSC_VER) && !defined(__clang__)
    nob_cmd_append(&command,
                   "cl.exe",
                   "/std:c11",
                   "/W4",
                   "/WX",
                   "/nologo",
                   "/D_CRT_SECURE_NO_WARNINGS",
                   "/Iinclude",
                   "/I.",
                   nob_temp_sprintf("/Fe:%s", NOC_MODULES_TEST),
                   "/Fo:build\\",
                   "tests/test_modules.c");
#else
    append_compile_command(&command,
                           NOC_MODULES_TEST,
                           "tests/test_modules.c",
                           false);
#endif
    for (i = 0; i < NOB_ARRAY_LEN(implementation_modules); ++i) {
        nob_cmd_append(&command, implementation_modules[i]);
    }
    if (!nob_cmd_run(&command)) return false;
    nob_cmd_append(&command, NOC_MODULES_TEST);
    return nob_cmd_run(&command);
}

static const Test_Suite *find_test_suite(const char *key)
{
    size_t i;
    for (i = 0; i < NOB_ARRAY_LEN(test_suites); ++i) {
        if (strcmp(test_suites[i].key, key) == 0) return &test_suites[i];
    }
    return NULL;
}

static void print_test_suites(void)
{
    size_t i;
    fprintf(stderr, "Valid test suites:");
    for (i = 0; i < NOB_ARRAY_LEN(test_suites); ++i) {
        fprintf(stderr, " %s", test_suites[i].key);
    }
    fprintf(stderr, " modules");
    fputc('\n', stderr);
}

static bool build_fuzz(void)
{
    return build_binary(NOC_FUZZ, "tests/fuzz_noc.c");
}

static bool build_c_parse_fuzz(void)
{
    return build_binary(NOC_C_PARSE_FUZZ, "tests/fuzz_c_parser.c");
}

static bool build_embed_dialect(void)
{
    return build_binary(NOC_EMBED_DIALECT, "examples/embed/dialect.c");
}

static bool generate_embed_example(void)
{
    const char *output = "build/generated/embed_app.c";
    const char *depfile = "build/generated/embed_app.d";
    const char *inputs[] = {
        "examples/embed/app.c",
        "examples/embed/message.txt",
        NOC_EMBED_DIALECT,
        "nob.c",
    };
    Nob_Cmd command = {0};
    int rebuild = output_rebuild_state(output, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild != 0 || nob_file_exists(depfile) <= 0) {
        nob_cmd_append(&command,
                       NOC_EMBED_DIALECT,
                       "examples/embed/app.c",
                       "-o",
                       output,
                       "--depfile",
                       depfile);
        if (!nob_cmd_run(&command)) return false;
    }
    return nob_file_exists(output) > 0 &&
           verify_generated_file(depfile, "tests/golden/embed_app.d");
}

static bool build_embed_example(void)
{
    const char *inputs[] = {"build/generated/embed_app.c", "nob.c"};
    Nob_Cmd command = {0};
    int rebuild;
    if (!build_embed_dialect() || !generate_embed_example()) return false;
    rebuild = output_rebuild_state(NOC_EMBED_EXAMPLE, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild == 0) {
        return nob_file_exists(NOC_EMBED_EXAMPLE) > 0;
    }
    append_compile_command(&command,
                           NOC_EMBED_EXAMPLE,
                           "build/generated/embed_app.c",
                           false);
    return nob_cmd_run(&command);
}

static bool build_rules_dialect(void)
{
    return build_binary(NOC_RULES_DIALECT, "examples/rules/dialect.c");
}

static bool generate_rules_example(void)
{
    const char *inputs[] = {
        "examples/rules/app.c",
        NOC_RULES_DIALECT,
        "nob.c",
    };
    Nob_Cmd command = {0};
    int rebuild = output_rebuild_state("build/generated/rules_app.c",
                                       inputs,
                                       NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild == 0) {
        return nob_file_exists("build/generated/rules_app.c") > 0;
    }
    nob_cmd_append(&command,
                   NOC_RULES_DIALECT,
                   "examples/rules/app.c",
                   "-o",
                   "build/generated/rules_app.c");
    return nob_cmd_run(&command);
}

static bool build_rules_example(void)
{
    const char *inputs[] = {"build/generated/rules_app.c", "nob.c"};
    Nob_Cmd command = {0};
    int rebuild;
    if (!build_rules_dialect() || !generate_rules_example()) return false;
    rebuild = output_rebuild_state(NOC_RULES_EXAMPLE, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild == 0) {
        return nob_file_exists(NOC_RULES_EXAMPLE) > 0;
    }
    append_compile_command(&command,
                           NOC_RULES_EXAMPLE,
                           "build/generated/rules_app.c",
                           false);
    return nob_cmd_run(&command);
}

static bool generate_ide_overlays(void)
{
    const char *app_output = "build/generated/ide/app.c";
    const char *header_output = "build/generated/ide/math.h";
    const char *inputs[] = {
        "examples/ide/app.c",
        "examples/ide/math.h",
        NOC_RULES_DIALECT,
        "nob.c",
    };
    Nob_Cmd command = {0};
    int app_rebuild = output_rebuild_state(app_output, inputs, NOB_ARRAY_LEN(inputs));
    int header_rebuild = output_rebuild_state(header_output,
                                              inputs,
                                              NOB_ARRAY_LEN(inputs));
    if (app_rebuild < 0 || header_rebuild < 0) return false;
    if (app_rebuild != 0 || header_rebuild != 0 ||
        nob_file_exists("build/generated/ide/app.c.d") <= 0 ||
        nob_file_exists("build/generated/ide/math.h.d") <= 0) {
        nob_cmd_append(&command,
                       NOC_RULES_DIALECT,
                       "--batch-depfiles",
                       "examples/ide",
                       "build/generated/ide",
                       "examples/ide/app.c",
                       "examples/ide/math.h");
        if (!nob_cmd_run(&command)) return false;
    }
    return nob_file_exists(app_output) > 0 && nob_file_exists(header_output) > 0;
}

static bool generate_ide_metadata(void)
{
    const char *output = "build/generated/ide/rules_metadata.h";
    const char *inputs[] = {NOC_RULES_DIALECT, "nob.c"};
    Nob_Cmd command = {0};
    int rebuild = output_rebuild_state(output, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild == 0) return nob_file_exists(output) > 0;
    nob_cmd_append(&command, NOC_RULES_DIALECT, "--ide-metadata", output);
    return nob_cmd_run(&command);
}

static bool generate_ide_command_signature(void)
{
    const char *output = "build/generated/ide/compile.sig";
    const char *inputs[] = {NOC_RULES_DIALECT, "nob.c"};
    Nob_Cmd command = {0};
    int rebuild = output_rebuild_state(output, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild != 0) {
        nob_cmd_append(&command,
                       NOC_RULES_DIALECT,
                       "--command-signature",
                       output,
                       "--",
                       "cc",
                       "-std=c11",
                       "-Ibuild/generated/ide",
                       "-c",
                       "build/generated/ide/app.c");
        if (!nob_cmd_run(&command)) return false;
    }
    return verify_generated_file(output, "tests/golden/ide_command.sig");
}

static bool build_ide_example(void)
{
    const char *output_source = "build/generated/ide/app.c";
    const char *output_header = "build/generated/ide/math.h";
    const char *metadata = "build/generated/ide/rules_metadata.h";
    const char *signature = "build/generated/ide/compile.sig";
    const char *inputs[] = {
        output_source,
        output_header,
        metadata,
        signature,
        "nob.c",
    };
    Nob_Cmd command = {0};
    int rebuild;
    if (!build_rules_dialect() ||
        !generate_ide_overlays() ||
        !generate_ide_metadata() ||
        !verify_generated_file(metadata, "tests/golden/rules_metadata.h") ||
        !generate_ide_command_signature()) {
        return false;
    }
    rebuild = output_rebuild_state(NOC_IDE_EXAMPLE, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild == 0) return nob_file_exists(NOC_IDE_EXAMPLE) > 0;
    append_compile_command(&command, NOC_IDE_EXAMPLE, output_source, false);
    return nob_cmd_run(&command);
}

static bool remove_build_entry(Nob_Walk_Entry entry)
{
    int result;
#ifdef _WIN32
    result = entry.type == NOB_FILE_DIRECTORY ? _rmdir(entry.path) : remove(entry.path);
#else
    result = remove(entry.path);
#endif
    if (result != 0) {
        nob_log(NOB_ERROR, "could not remove %s: %s", entry.path, strerror(errno));
        return false;
    }
    return true;
}

static bool clean(void)
{
    if (nob_file_exists("build") == 0) return true;
    return nob_walk_dir("build", remove_build_entry, .post_order = true);
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [test [suite]|fuzz|example|describe|header|verify-header|clean]\n",
            program);
}

int main(int argc, char **argv)
{
    const char *target = argc > 1 ? argv[1] : "test";
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (strcmp(target, "clean") == 0) return clean() ? 0 : 1;
    if (!ensure_build_directories()) return 1;
    if (strcmp(target, "header") == 0) {
        return generate_header(NOC_RELEASE_HEADER) && prepare_generated_header() ? 0 : 1;
    }
    if (strcmp(target, "verify-header") == 0) {
        return verify_release_header() ? 0 : 1;
    }
    if (!prepare_generated_header()) return 1;

    if (strcmp(target, "test") == 0) {
        Nob_Cmd command = {0};
        size_t i;
        if (argc > 2) {
            if (strcmp(argv[2], "modules") == 0) {
                return run_modules_test() ? 0 : 1;
            }
            const Test_Suite *suite = find_test_suite(argv[2]);
            if (!suite) {
                fprintf(stderr, "Unknown test suite: %s\n", argv[2]);
                print_test_suites();
                return 2;
            }
            return run_test_suite(suite) ? 0 : 1;
        }
        for (i = 0; i < NOB_ARRAY_LEN(test_suites); ++i) {
            if (!run_test_suite(&test_suites[i])) return 1;
        }
        if (!run_modules_test()) return 1;
        if (!build_fuzz() || !build_c_parse_fuzz() ||
            !build_embed_example() || !build_rules_example() ||
            !build_ide_example()) return 1;
        nob_cmd_append(&command, NOC_FUZZ);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_C_PARSE_FUZZ);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_EMBED_EXAMPLE);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_RULES_EXAMPLE);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_IDE_EXAMPLE);
        return nob_cmd_run(&command) ? 0 : 1;
    }
    if (strcmp(target, "fuzz") == 0) {
        Nob_Cmd command = {0};
        if (!build_fuzz() || !build_c_parse_fuzz()) return 1;
        nob_cmd_append(&command, NOC_FUZZ);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_C_PARSE_FUZZ);
        return nob_cmd_run(&command) ? 0 : 1;
    }
    if (strcmp(target, "example") == 0) {
        Nob_Cmd command = {0};
        if (!build_embed_example() || !build_rules_example() || !build_ide_example()) return 1;
        nob_cmd_append(&command, NOC_EMBED_EXAMPLE);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_RULES_EXAMPLE);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_IDE_EXAMPLE);
        return nob_cmd_run(&command) ? 0 : 1;
    }
    if (strcmp(target, "describe") == 0) {
        Nob_Cmd command = {0};
        if (!build_embed_dialect() || !build_rules_dialect()) return 1;
        nob_cmd_append(&command, NOC_EMBED_DIALECT, "--describe");
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_RULES_DIALECT, "--describe");
        return nob_cmd_run(&command) ? 0 : 1;
    }

    usage(argv[0]);
    return 2;
}
