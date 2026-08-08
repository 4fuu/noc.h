#ifndef NOC_TEST_SUPPORT_H_INCLUDED
#define NOC_TEST_SUPPORT_H_INCLUDED

#define NOC_IMPLEMENTATION
#include "noc.h"

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

static void count_diagnostics(void *user_data, const Noc_Diagnostic *diagnostic)
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

static bool slice_equals(Noc_Slice slice, const char *expected)
{
    size_t expected_count = strlen(expected);
    return slice.count == expected_count &&
           (expected_count == 0 || memcmp(slice.data, expected, expected_count) == 0);
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
