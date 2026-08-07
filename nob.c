#define NOB_IMPLEMENTATION
#include "nob.h"

#include <errno.h>

#if defined(_WIN32)
#include <direct.h>
#define NOC_EXE ".exe"
#else
#define NOC_EXE ""
#endif

#define NOC_TESTS "build/noc-tests" NOC_EXE
#define NOC_EMBED_DIALECT "build/embed-dialect" NOC_EXE
#define NOC_EMBED_EXAMPLE "build/embed-example" NOC_EXE
#define NOC_RULES_DIALECT "build/rules-dialect" NOC_EXE
#define NOC_RULES_EXAMPLE "build/rules-example" NOC_EXE

#if !defined(_MSC_VER) || defined(__clang__)
static const char *compiler(void)
{
    const char *cc = getenv("CC");
    return cc && cc[0] ? cc : "cc";
}
#endif

static bool ensure_build_directories(void)
{
    return nob_mkdir_if_not_exists("build") &&
           nob_mkdir_if_not_exists("build/generated");
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

static void append_compile_command(Nob_Cmd *command,
                                   const char *output,
                                   const char *source)
{
#if defined(_MSC_VER) && !defined(__clang__)
    nob_cmd_append(command,
                   "cl.exe",
                   "/std:c11",
                   "/W4",
                   "/WX",
                   "/nologo",
                   "/D_CRT_SECURE_NO_WARNINGS",
                   "/I.",
                   nob_temp_sprintf("/Fe:%s", output),
                   nob_temp_sprintf("/Fo:%s.obj", output),
                   source);
#else
    nob_cmd_append(command,
                   compiler(),
                   "-std=c11",
                   "-Wall",
                   "-Wextra",
                   "-Wpedantic",
                   "-Werror",
                   "-I.",
                   "-o",
                   output,
                   source);
#endif
}

static bool build_binary(const char *output, const char *source)
{
    const char *inputs[] = {source, "noc.h", "nob.c"};
    Nob_Cmd command = {0};
    int rebuild = output_rebuild_state(output, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild == 0) {
        return nob_file_exists(output) > 0;
    }
    append_compile_command(&command, output, source);
    return nob_cmd_run(&command);
}

static bool build_tests(void)
{
    return build_binary(NOC_TESTS, "tests/test_noc.c");
}

static bool build_embed_dialect(void)
{
    return build_binary(NOC_EMBED_DIALECT, "examples/embed/dialect.c");
}

static bool generate_embed_example(void)
{
    const char *inputs[] = {
        "examples/embed/app.c",
        "examples/embed/message.txt",
        NOC_EMBED_DIALECT,
        "nob.c",
    };
    Nob_Cmd command = {0};
    int rebuild = output_rebuild_state("build/generated/embed_app.c",
                                       inputs,
                                       NOB_ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (rebuild == 0) {
        return nob_file_exists("build/generated/embed_app.c") > 0;
    }
    nob_cmd_append(&command,
                   NOC_EMBED_DIALECT,
                   "examples/embed/app.c",
                   "-o",
                   "build/generated/embed_app.c");
    return nob_cmd_run(&command);
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
                           "build/generated/embed_app.c");
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
                           "build/generated/rules_app.c");
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
            "Usage: %s [test|example|describe|clean]\n",
            program);
}

int main(int argc, char **argv)
{
    const char *target = argc > 1 ? argv[1] : "test";
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (strcmp(target, "clean") == 0) return clean() ? 0 : 1;
    if (!ensure_build_directories()) return 1;

    if (strcmp(target, "test") == 0) {
        Nob_Cmd command = {0};
        if (!build_tests() || !build_embed_example() || !build_rules_example()) return 1;
        nob_cmd_append(&command, NOC_TESTS);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_EMBED_EXAMPLE);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_RULES_EXAMPLE);
        return nob_cmd_run(&command) ? 0 : 1;
    }
    if (strcmp(target, "example") == 0) {
        Nob_Cmd command = {0};
        if (!build_embed_example() || !build_rules_example()) return 1;
        nob_cmd_append(&command, NOC_EMBED_EXAMPLE);
        if (!nob_cmd_run(&command)) return 1;
        nob_cmd_append(&command, NOC_RULES_EXAMPLE);
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
