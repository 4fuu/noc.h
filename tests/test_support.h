#ifndef NOC_TEST_SUPPORT_H_INCLUDED
#define NOC_TEST_SUPPORT_H_INCLUDED

#include <noc/noc.h>

/* Implementation-only amalgamation helpers must not leak into a consumer TU. */
#if defined(NOC__MACRO_BUILTIN_BIT) || \
    defined(NOC__MACRO_BUILTIN_ALWAYS_MASK) || \
    defined(NOC__MACRO_BUILTIN_SUPPORTED_MASK)
#error "noc.h leaked an internal predefined-macro helper"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n",                      \
                    __FILE__, __LINE__, #condition);                            \
            failures += 1;                                                      \
        }                                                                       \
    } while (0)

typedef struct {
    size_t errors;
    size_t last_message_count;
    Noc_Location last_location;
    char last_path[64];
    char last_message[160];
} Diagnostic_State;

static inline void count_diagnostics(void *user_data,
                                     const Noc_Diagnostic *diagnostic)
{
    Diagnostic_State *state = (Diagnostic_State *)user_data;
    if (diagnostic->severity == NOC_DIAGNOSTIC_ERROR) {
        state->errors += 1;
        state->last_message_count = strlen(diagnostic->message);
        state->last_location = diagnostic->location;
        (void)snprintf(state->last_message, sizeof(state->last_message), "%s",
                       diagnostic->message);
        if (diagnostic->location.path) {
            (void)snprintf(state->last_path, sizeof(state->last_path), "%s",
                           diagnostic->location.path);
        } else {
            state->last_path[0] = '\0';
        }
    }
}

static inline bool slice_equals(Noc_Slice slice, const char *expected)
{
    size_t expected_count = strlen(expected);
    return slice.count == expected_count &&
           (expected_count == 0 || memcmp(slice.data, expected, expected_count) == 0);
}

/* Structured feature suites validate the actual lowering boundary by feeding
   generated text back through the bundled C parser, rather than relying only
   on substring expectations. */
static inline void check_complete_generated_c(const char *path,
                                              const char *source,
                                              size_t source_count)
{
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    noc_workspace_init(&workspace);
    CHECK(noc_workspace_open_document(&workspace,
                                      path,
                                      source,
                                      source_count,
                                      NOC_SOURCE_CLASS_GENERATED,
                                      &snapshot) == NOC_WORKSPACE_OK);
    CHECK(noc_c_parse_tree_build(&snapshot,
                                 noc_c_parse_default_options(),
                                 &tree) == NOC_C_PARSE_OK);
    CHECK(noc_c_parse_tree_is_valid(&tree));
    CHECK(!noc_c_parse_tree_has_error(&tree));
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
}

static int finish_suite(const char *name)
{
    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed in %s suite\n", failures, name);
        return 1;
    }
    printf("noc %s tests passed\n", name);
    return 0;
}

#endif
