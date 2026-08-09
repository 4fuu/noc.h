#ifndef NOC_TRANSLATION_TEST_SUPPORT_H_INCLUDED
#define NOC_TRANSLATION_TEST_SUPPORT_H_INCLUDED

#include "test_support.h"

enum {
    TRANSLATION_MAX_FILES = 16,
    TRANSLATION_MAX_BINDINGS = 32,
    TRANSLATION_MAX_RESOLVER_NAMES = 64
};

typedef struct {
    const char *name;
    size_t file;
} Translation_Binding;

typedef struct {
    Noc_Context context;
    Noc_Workspace workspace;
    Diagnostic_State diagnostics;
    Noc_Document_Snapshot files[TRANSLATION_MAX_FILES];
    size_t file_count;
    Translation_Binding bindings[TRANSLATION_MAX_BINDINGS];
    size_t binding_count;
    size_t resolver_calls;
    char resolver_names[TRANSLATION_MAX_RESOLVER_NAMES][64];
    size_t resolver_name_count;
} Translation_Fixture;

static inline size_t translation_add(Translation_Fixture *f, const char *path,
                                     const char *source,
                                     Noc_Source_Class source_class) {
    size_t index = f->file_count++;
    CHECK(index < TRANSLATION_MAX_FILES);
    CHECK(noc_workspace_open_document(&f->workspace, path, source,
                                      strlen(source), source_class,
                                      &f->files[index]) == NOC_WORKSPACE_OK);
    return index;
}

static inline void translation_fixture_init(Translation_Fixture *f,
                                            const char *root) {
    memset(f, 0, sizeof(*f));
    noc_context_init(&f->context);
    noc_context_set_diagnostic(&f->context, count_diagnostics, &f->diagnostics);
    noc_workspace_init(&f->workspace);
    (void)translation_add(f, "root.c", root, NOC_SOURCE_CLASS_PROJECT);
}

static inline void translation_bind(Translation_Fixture *f, const char *name,
                                    size_t file) {
    size_t index = f->binding_count++;
    CHECK(index < TRANSLATION_MAX_BINDINGS);
    f->bindings[index].name = name;
    f->bindings[index].file = file;
}

static inline Noc_Include_Resolve_Status
translation_resolve(void *user_data, const Noc_Include_Request *request,
                    Noc_Document_Snapshot *output) {
    Translation_Fixture *f = (Translation_Fixture *)user_data;
    size_t i;
    f->resolver_calls += 1;
    if (f->resolver_name_count < TRANSLATION_MAX_RESOLVER_NAMES) {
        size_t n = request->logical_name.count;
        if (n >= sizeof(f->resolver_names[0]))
            n = sizeof(f->resolver_names[0]) - 1;
        memcpy(f->resolver_names[f->resolver_name_count],
               request->logical_name.data, n);
        f->resolver_names[f->resolver_name_count][n] = '\0';
        f->resolver_name_count++;
    }
    CHECK(!noc_document_snapshot_is_valid(output));
    for (i = 0; i < f->binding_count; ++i) {
        if (slice_equals(request->logical_name, f->bindings[i].name)) {
            CHECK(noc_document_snapshot_clone(&f->files[f->bindings[i].file],
                                              output) == NOC_WORKSPACE_OK);
            return NOC_INCLUDE_RESOLVE_FOUND;
        }
    }
    return NOC_INCLUDE_RESOLVE_NOT_FOUND;
}

static inline Noc_Preprocessor_Translation_Result
translation_build(Translation_Fixture *f,
                  Noc_Preprocessor_Translation_Options options,
                  Noc_Preprocessor_Translation *output) {
    Noc_Include_Resolver resolver = {translation_resolve, f};
    return noc_preprocessor_translation_build(&f->context, &f->files[0], NULL,
                                              0, resolver, options, output);
}

static inline Noc_Preprocessor_Translation_Options
translation_full_options(void) {
    Noc_Preprocessor_Translation_Options options =
        noc_preprocessor_translation_default_options();
    options.macro_policy = NOC_MACROS_FULL;
    return options;
}

static inline bool translation_contains(const Noc_Preprocessor_Translation *t,
                                        const char *text) {
    const Noc_Logical_Source *source =
        noc_preprocessor_translation_logical_source(t);
    size_t i;
    size_t length = 0;
    char *joined;
    bool found;
    for (i = 0; i < noc_logical_source_token_count(source); ++i)
        length += noc_logical_source_token_text(source, i).count + 1;
    joined = (char *)malloc(length + 1);
    CHECK(joined != NULL);
    if (!joined)
        return false;
    length = 0;
    for (i = 0; i < noc_logical_source_token_count(source); ++i) {
        Noc_Slice token = noc_logical_source_token_text(source, i);
        memcpy(joined + length, token.data, token.count);
        length += token.count;
        joined[length++] = ' ';
    }
    joined[length] = '\0';
    found = strstr(joined, text) != NULL;
    free(joined);
    return found;
}

static inline size_t translation_token_occurrences(
    const Noc_Preprocessor_Translation *t, const char *text) {
    const Noc_Logical_Source *source =
        noc_preprocessor_translation_logical_source(t);
    size_t i, count = 0;
    for (i = 0; i < noc_logical_source_token_count(source); ++i)
        if (slice_equals(noc_logical_source_token_text(source, i), text))
            count++;
    return count;
}

static inline size_t translation_token_position(
    const Noc_Preprocessor_Translation *t, const char *text) {
    const Noc_Logical_Source *source =
        noc_preprocessor_translation_logical_source(t);
    size_t i;
    for (i = 0; i < noc_logical_source_token_count(source); ++i)
        if (slice_equals(noc_logical_source_token_text(source, i), text))
            return i;
    return NOC_TOKEN_INDEX_NONE;
}

static inline void translation_fixture_deinit(Translation_Fixture *f) {
    while (f->file_count)
        noc_document_snapshot_free(&f->files[--f->file_count]);
    noc_workspace_deinit(&f->workspace);
    noc_context_deinit(&f->context);
}

#endif
