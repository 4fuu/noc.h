#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_EMIT_C_IMPLEMENTATION_INCLUDED
#define NOC_EMIT_C_IMPLEMENTATION_INCLUDED

static size_t noc__pattern_match(const char *pattern,
                                 const Noc_Token *tokens,
                                 size_t count,
                                 size_t start,
                                 size_t *significant_count)
{
    Noc_Lexer lexer;
    Noc_Token expected;
    size_t cursor = start;
    size_t matched = 0;
    noc_lexer_init(&lexer, "<rule-pattern>", pattern, strlen(pattern));
    for (;;) {
        do {
            expected = noc_lexer_next(&lexer);
        } while (noc_token_is_trivia(expected));
        if (expected.kind == NOC_TOKEN_EOF) break;
        if (matched) {
            while (cursor < count && noc_token_is_trivia(tokens[cursor])) cursor += 1;
        }
        if (cursor >= count || tokens[cursor].kind != expected.kind ||
            !noc__slices_logically_equal(expected.text, tokens[cursor].text)) {
            return NOC_TOKEN_INDEX_NONE;
        }
        ++matched;
        ++cursor;
    }
    *significant_count = matched;
    return cursor;
}

NOC__PRIVATE bool noc__transform_source(Noc_Context *context,
                                  const char *path,
                                  const char *source,
                                  size_t source_count,
                                  Noc_Transform_Result *result,
                                  size_t expansion_depth,
                                  bool emit_line_directives,
                                  bool analyze_preprocessor_activity)
{
    Noc_Lexer lexer;
    Noc__Tokens tokens = {0};
    Noc_Preprocessor_Map preprocessor = {0};
    Noc_Buffer output = {0};
    Noc__String_List dependencies = {0};
    Noc_Token token;
    size_t index = 0;
    size_t errors_before = context->error_count;
    bool ok = true;
    Noc_Location no_location = {0};

    memset(result, 0, sizeof(*result));
    context->active_transforms += 1;
    if (!noc__reject_trigraphs(context, path, source, source_count)) {
        ok = false;
        goto done;
    }
    noc_lexer_init(&lexer, path, source, source_count);
    do {
        token = noc_lexer_next(&lexer);
        if (!noc__tokens_append(&tokens, token)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while tokenizing '%s'",
                        path);
            ok = false;
            goto done;
        }
        if (token.kind == NOC_TOKEN_INVALID) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "unterminated or invalid token '%.*s%s'",
                        (int)(token.text.count < 80 ? token.text.count : 80),
                        token.text.data,
                        token.text.count > 80 ? "..." : "");
            ok = false;
        }
    } while (token.kind != NOC_TOKEN_EOF);
    if (!ok) goto done;
    tokens.source = (char *)source;
    tokens.source_count = source_count;
    tokens.path = (char *)(path ? path : "<memory>");
    tokens.generation = 1;

    if (analyze_preprocessor_activity &&
        !noc_preprocessor_map_build(context, &tokens, &preprocessor)) {
        ok = false;
        goto done;
    }

    if (emit_line_directives && !noc__emit_line_directive_at(&output, path, 1)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while starting output for '%s'",
                    path);
        ok = false;
        goto done;
    }

    while (index < tokens.count && tokens.items[index].kind != NOC_TOKEN_EOF) {
        token = tokens.items[index];
        if (!noc_token_is_trivia(token) && token.kind != NOC_TOKEN_PREPROCESSOR &&
            (!analyze_preprocessor_activity ||
             noc_preprocessor_activity_at(&preprocessor, index) !=
                 NOC_PREPROCESSOR_ACTIVITY_INACTIVE)) {
            size_t selected = NOC_TOKEN_INDEX_NONE;
            size_t trigger_end = 0;
            size_t best_count = 0;
            size_t r;
            size_t legacy_name = index + 1;
            if (noc_token_is_punct(token, "@")) {
                while (legacy_name < tokens.count &&
                       noc_token_is_trivia(tokens.items[legacy_name])) {
                    legacy_name += 1;
                }
                if (legacy_name < tokens.count &&
                    tokens.items[legacy_name].kind == NOC_TOKEN_IDENTIFIER) {
                    r = noc__find_rule_token(context, tokens.items[legacy_name]);
                    if (r != NOC_TOKEN_INDEX_NONE) {
                        selected = r;
                        trigger_end = legacy_name + 1;
                        best_count = 2;
                    }
                }
            }
            for (r = 0; r < context->rules_count; ++r) {
                if (context->rule_patterns[r]) {
                    size_t significant_count = 0;
                    size_t end = noc__pattern_match(context->rule_patterns[r],
                                                    tokens.items,
                                                    tokens.count,
                                                    index,
                                                    &significant_count);
                    if (end != NOC_TOKEN_INDEX_NONE &&
                        significant_count > best_count) {
                        selected = r;
                        trigger_end = end;
                        best_count = significant_count;
                    }
                }
            }
            if (selected != NOC_TOKEN_INDEX_NONE) {
                const Noc_Rule *rule = &context->rules[selected];
                if (!context->rule_enabled[selected]) {
                    if (context->options.disabled_rule_is_error) {
                        noc__report(context,
                                    NOC_DIAGNOSTIC_ERROR,
                                    token.location,
                                    "rule '%s' is disabled",
                                    rule->name);
                        ok = false;
                        goto done;
                    }
                    while (index < trigger_end) {
                        if (!noc_buffer_append_slice(&output, tokens.items[index++].text)) {
                            noc__report(context,
                                        NOC_DIAGNOSTIC_ERROR,
                                        token.location,
                                        "out of memory while preserving disabled rule '%s'",
                                        rule->name);
                            ok = false;
                            goto done;
                        }
                    }
                    continue;
                } else {
                    Noc_Rewriter rewriter;
                    size_t expansion_errors = context->error_count;
                    bool expanded;
                    memset(&rewriter, 0, sizeof(rewriter));
                    rewriter.context = context;
                    rewriter.rule = rule;
                    rewriter.path = path;
                    rewriter.source = source;
                    rewriter.source_count = source_count;
                    rewriter.tokens = tokens.items;
                    rewriter.tokens_count = tokens.count;
                    rewriter.cursor = trigger_end;
                    rewriter.stream = &tokens;
                    rewriter.trigger_location = token.location;
                    rewriter.trigger_range.begin = index;
                    rewriter.trigger_range.end = trigger_end;
                    rewriter.output = &output;
                    rewriter.dependencies = &dependencies;
                    rewriter.expansion_depth = expansion_depth;
                    expanded = rule->expand(&rewriter, rule, rule->user_data);
                    noc_syntax_tree_free(&rewriter.syntax_tree);
                    if (!expanded && context->error_count == expansion_errors) {
                        noc_rw_error(&rewriter,
                                     "expansion callback for rule '%s' failed without reporting an error",
                                     rule->name);
                    }
                    if (!expanded || context->error_count != expansion_errors) {
                        rewriter.failed = true;
                    }
                    if (rewriter.failed) {
                        ok = false;
                        goto done;
                    }
                    index = rewriter.cursor;
                    continue;
                }
            } else if (noc_token_is_punct(token, "@") && legacy_name < tokens.count &&
                       tokens.items[legacy_name].kind == NOC_TOKEN_IDENTIFIER) {
                if (context->options.unknown_rule_is_error) {
                    noc__report(context,
                                NOC_DIAGNOSTIC_ERROR,
                                token.location,
                                "unknown dialect rule '@%.*s%s'",
                                (int)(tokens.items[legacy_name].text.count < 80
                                          ? tokens.items[legacy_name].text.count
                                          : 80),
                                tokens.items[legacy_name].text.data,
                                tokens.items[legacy_name].text.count > 80 ? "..." : "");
                    ok = false;
                    goto done;
                }
                while (index <= legacy_name) {
                    if (!noc_buffer_append_slice(&output, tokens.items[index++].text)) {
                        noc__report(context,
                                    NOC_DIAGNOSTIC_ERROR,
                                    token.location,
                                    "out of memory while preserving an unknown dialect rule");
                        ok = false;
                        goto done;
                    }
                }
                continue;
            }
        }
        if (!noc_buffer_append_slice(&output, token.text)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "out of memory while transforming '%s'",
                        path);
            ok = false;
            goto done;
        }
        index += 1;
    }

    if (!noc_buffer_terminate(&output)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while finishing output for '%s'",
                    path);
        ok = false;
        goto done;
    }
    if (context->error_count != errors_before) {
        ok = false;
        goto done;
    }
    result->output = output.items;
    result->output_count = output.count;
    result->dependencies = dependencies.items;
    result->dependency_count = dependencies.count;
    output.items = NULL;
    output.count = 0;
    output.capacity = 0;
    dependencies.items = NULL;
    dependencies.count = 0;
    dependencies.capacity = 0;

done:
    result->error_count = context->error_count - errors_before;
    noc_preprocessor_map_free(&preprocessor);
    free(tokens.items);
    noc_buffer_free(&output);
    noc__string_list_free(&dependencies);
    context->active_transforms -= 1;
    return ok && result->error_count == 0;
}

NOCDEF bool noc_transform_source(Noc_Context *context,
                                 const char *path,
                                 const char *source,
                                 size_t source_count,
                                 Noc_Transform_Result *result)
{
    return noc__transform_source(context,
                                 path,
                                 source,
                                 source_count,
                                 result,
                                 0,
                                 context->options.emit_line_directives,
                                 context->options.skip_inactive_preprocessor_branches);
}

NOCDEF void noc_transform_result_free(Noc_Transform_Result *result)
{
    size_t i;
    free(result->output);
    for (i = 0; i < result->dependency_count; ++i) free(result->dependencies[i]);
    free(result->dependencies);
    memset(result, 0, sizeof(*result));
}

static bool noc__read_file(Noc_Context *context,
                           const char *path,
                           Noc_Buffer *contents,
                           Noc_Location location)
{
    FILE *file = fopen(path, "rb");
    char chunk[8192];
    size_t count;
    if (!file) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not open '%s': %s",
                    path,
                    strerror(errno));
        return false;
    }
    while ((count = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (!noc_buffer_append(contents, chunk, count)) {
            fclose(file);
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "out of memory while reading '%s'",
                        path);
            return false;
        }
    }
    if (ferror(file)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not read '%s': %s",
                    path,
                    strerror(errno));
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool noc__paths_refer_to_same_file(const char *left, const char *right)
{
    if (strcmp(left, right) == 0) return true;
#ifdef _WIN32
    {
        HANDLE left_handle = CreateFileA(left,
                                         0,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                         NULL,
                                         OPEN_EXISTING,
                                         FILE_FLAG_BACKUP_SEMANTICS,
                                         NULL);
        HANDLE right_handle = CreateFileA(right,
                                          0,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                          NULL,
                                          OPEN_EXISTING,
                                          FILE_FLAG_BACKUP_SEMANTICS,
                                          NULL);
        BY_HANDLE_FILE_INFORMATION left_info;
        BY_HANDLE_FILE_INFORMATION right_info;
        bool same = false;
        if (left_handle != INVALID_HANDLE_VALUE && right_handle != INVALID_HANDLE_VALUE &&
            GetFileInformationByHandle(left_handle, &left_info) &&
            GetFileInformationByHandle(right_handle, &right_info)) {
            same = left_info.dwVolumeSerialNumber == right_info.dwVolumeSerialNumber &&
                   left_info.nFileIndexHigh == right_info.nFileIndexHigh &&
                   left_info.nFileIndexLow == right_info.nFileIndexLow;
        }
        if (left_handle != INVALID_HANDLE_VALUE) CloseHandle(left_handle);
        if (right_handle != INVALID_HANDLE_VALUE) CloseHandle(right_handle);
        return same;
    }
#else
    {
        struct stat left_stat;
        struct stat right_stat;
        return stat(left, &left_stat) == 0 && stat(right, &right_stat) == 0 &&
               left_stat.st_dev == right_stat.st_dev &&
               left_stat.st_ino == right_stat.st_ino;
    }
#endif
}

static FILE *noc__open_unique_output(Noc_Context *context,
                                     const char *output_path,
                                     Noc_Buffer *temporary_path,
                                     Noc_Location location)
{
    unsigned long long salt = (unsigned long long)time(NULL) ^
                              (unsigned long long)clock() ^
                              (unsigned long long)(uintptr_t)temporary_path;
    size_t attempt;
    for (attempt = 0; attempt < 128; ++attempt) {
        FILE *file;
        temporary_path->count = 0;
        if (!noc_buffer_appendf(temporary_path,
                                "%s.noc-tmp-%llx-%zu",
                                output_path,
                                salt,
                                attempt) ||
            !noc_buffer_terminate(temporary_path)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "out of memory while creating temporary output path");
            return NULL;
        }
        errno = 0;
        file = fopen(temporary_path->items, "wbx");
        if (file) return file;
        if (errno != EEXIST) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "could not create '%s': %s",
                        temporary_path->items,
                        strerror(errno));
            return NULL;
        }
    }
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                location,
                "could not create a unique temporary file for '%s'",
                output_path);
    return NULL;
}

static bool noc__replace_output(const char *temporary_path, const char *output_path)
{
#ifdef _WIN32
    return MoveFileExA(temporary_path,
                       output_path,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary_path, output_path) == 0;
#endif
}

static bool noc__write_output_atomic(Noc_Context *context,
                                     const char *output_path,
                                     const void *data,
                                     size_t count,
                                     Noc_Location location)
{
    Noc_Buffer temporary_path = {0};
    FILE *file = NULL;
    bool ok = false;
    bool temporary_created = false;
    file = noc__open_unique_output(context, output_path, &temporary_path, location);
    if (!file) goto done;
    temporary_created = true;
    if (count > 0 && fwrite(data, 1, count, file) != count) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not write '%s': %s",
                    temporary_path.items,
                    strerror(errno));
        goto done;
    }
    if (fclose(file) != 0) {
        file = NULL;
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not close '%s': %s",
                    temporary_path.items,
                    strerror(errno));
        goto done;
    }
    file = NULL;
    if (!noc__replace_output(temporary_path.items, output_path)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not rename '%s' to '%s': %s",
                    temporary_path.items,
                    output_path,
                    strerror(errno));
        goto done;
    }
    temporary_created = false;
    ok = true;

done:
    if (file) fclose(file);
    if (temporary_created) (void)remove(temporary_path.items);
    noc_buffer_free(&temporary_path);
    return ok;
}

NOCDEF bool noc_transform_file_with_result(Noc_Context *context,
                                           const char *input_path,
                                           const char *output_path,
                                           Noc_Transform_Result *result)
{
    Noc_Buffer source = {0};
    Noc_Location no_location = {0};
    bool ok = false;
    if (!context || !input_path || !output_path || !result) return false;
    memset(result, 0, sizeof(*result));
    if (noc__paths_refer_to_same_file(input_path, output_path)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "input and output paths must differ: '%s'",
                    input_path);
        goto done;
    }
    if (!noc__read_file(context, input_path, &source, no_location)) goto done;
    if (!noc_transform_source(context,
                              input_path,
                              source.items ? source.items : "",
                              source.count,
                              result)) {
        goto done;
    }
    ok = noc__write_output_atomic(context,
                                  output_path,
                                  result->output,
                                  result->output_count,
                                  no_location);

done:
    if (!ok) noc_transform_result_free(result);
    noc_buffer_free(&source);
    return ok;
}

NOCDEF bool noc_transform_file(Noc_Context *context,
                               const char *input_path,
                               const char *output_path)
{
    Noc_Transform_Result result = {0};
    bool ok = noc_transform_file_with_result(context,
                                             input_path,
                                             output_path,
                                             &result);
    noc_transform_result_free(&result);
    return ok;
}

static bool noc__path_is_absolute(const char *path)
{
    if (!path[0]) return false;
#ifdef _WIN32
    if (path[0] == '/' || path[0] == '\\') return true;
    return isalpha((unsigned char)path[0]) && path[1] == ':' &&
           (path[2] == '/' || path[2] == '\\');
#else
    return path[0] == '/';
#endif
}

static bool noc__path_is_separator(char c)
{
    return c == '/' || c == '\\';
}

static bool noc__path_character_equal(char left, char right)
{
    if (noc__path_is_separator(left) && noc__path_is_separator(right)) return true;
#ifdef _WIN32
    return tolower((unsigned char)left) == tolower((unsigned char)right);
#else
    return left == right;
#endif
}

static bool noc__paths_equal_for_output(const char *left, const char *right)
{
    while (*left && *right && noc__path_character_equal(*left, *right)) {
        left += 1;
        right += 1;
    }
    return *left == '\0' && *right == '\0';
}

static bool noc__batch_platform_path_is_supported(const char *path)
{
#ifdef _WIN32
    const unsigned char *cursor = (const unsigned char *)path;
    if (noc__path_is_separator(path[0]) && noc__path_is_separator(path[1])) {
        return false;
    }
    if (isalpha((unsigned char)path[0]) && path[1] == ':' &&
        !noc__path_is_separator(path[2])) {
        return false;
    }
    while (*cursor) {
        if (*cursor >= 128) return false;
        cursor += 1;
    }
#else
    (void)path;
#endif
    return true;
}

static bool noc__batch_map_output(Noc_Context *context,
                                  const Noc_Batch_Options *options,
                                  const char *input_path,
                                  Noc_Buffer *output)
{
    const char *root = options->input_root;
    const char *suffix;
    const char *cursor;
    size_t root_count;
    size_t relative_components = 0;
    Noc_Buffer relative = {0};
    Noc_Location location = {0};
    bool ok = false;
    location.path = input_path;
    if (!noc__depfile_path_is_valid(root) ||
        !noc__depfile_path_is_valid(options->output_root) ||
        !noc__depfile_path_is_valid(input_path)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "batch paths must be non-empty and cannot contain newlines");
        goto done;
    }
    if (!noc__batch_platform_path_is_supported(root) ||
        !noc__batch_platform_path_is_supported(options->output_root) ||
        !noc__batch_platform_path_is_supported(input_path)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "batch path is not supported by the Windows ANSI path backend");
        goto done;
    }
    root_count = strlen(root);
    while (root_count > 1 && noc__path_is_separator(root[root_count - 1])) {
#ifdef _WIN32
        if (root_count == 3 && root[1] == ':') break;
#endif
        root_count -= 1;
    }
    if (root_count == 1 && root[0] == '.') {
        if (noc__path_is_absolute(input_path)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "absolute batch input is outside relative root '.'");
            goto done;
        }
        suffix = input_path;
        while (suffix[0] == '.' && noc__path_is_separator(suffix[1])) suffix += 2;
    } else {
        size_t i;
        for (i = 0; i < root_count; ++i) {
            if (!input_path[i] || !noc__path_character_equal(root[i], input_path[i])) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            location,
                            "batch input is outside input root '%s'",
                            root);
                goto done;
            }
        }
        suffix = input_path + root_count;
        if (root_count == 0 || !noc__path_is_separator(root[root_count - 1])) {
            if (!noc__path_is_separator(*suffix)) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            location,
                            "batch input is outside input root '%s'",
                            root);
                goto done;
            }
        }
        while (noc__path_is_separator(*suffix)) suffix += 1;
    }
    cursor = suffix;
    while (*cursor) {
        const char *component;
        size_t component_count;
        while (noc__path_is_separator(*cursor)) cursor += 1;
        if (!*cursor) break;
        component = cursor;
        while (*cursor && !noc__path_is_separator(*cursor)) cursor += 1;
        component_count = (size_t)(cursor - component);
        if ((component_count == 1 && component[0] == '.') ||
            (component_count == 2 && component[0] == '.' && component[1] == '.') ||
            memchr(component, ':', component_count) != NULL) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "batch input contains a non-portable or escaping path component");
            goto done;
        }
        if ((relative_components > 0 && !noc_buffer_append_cstr(&relative, "/")) ||
            !noc_buffer_append(&relative, component, component_count)) {
            goto memory_failed;
        }
        relative_components += 1;
    }
    if (relative.count < 2 || relative.items[relative.count - 2] != '.' ||
        (relative.items[relative.count - 1] != 'c' &&
         relative.items[relative.count - 1] != 'h')) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "batch inputs must use ordinary .c or .h suffixes");
        goto done;
    }
    {
        size_t output_root_count = strlen(options->output_root);
        size_t i;
        while (output_root_count > 1 &&
               noc__path_is_separator(options->output_root[output_root_count - 1])) {
#ifdef _WIN32
            if (output_root_count == 3 && options->output_root[1] == ':') break;
#endif
            output_root_count -= 1;
        }
        for (i = 0; i < output_root_count; ++i) {
            char c = noc__path_is_separator(options->output_root[i])
                         ? '/'
                         : options->output_root[i];
            if (!noc_buffer_append(output, &c, 1)) goto memory_failed;
        }
        if ((output_root_count > 0 && output->items[output->count - 1] != '/' &&
             !noc_buffer_append_cstr(output, "/")) ||
            !noc_buffer_append(output, relative.items, relative.count) ||
            !noc_buffer_terminate(output)) {
            goto memory_failed;
        }
    }
    ok = true;
    goto done;

memory_failed:
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                location,
                "out of memory while mapping batch output path");
done:
    noc_buffer_free(&relative);
    return ok;
}

static bool noc__directory_exists(const char *path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

static bool noc__make_directory(const char *path)
{
    if (noc__directory_exists(path)) return true;
#ifdef _WIN32
    return _mkdir(path) == 0 || (errno == EEXIST && noc__directory_exists(path));
#else
    return mkdir(path, 0777) == 0 || (errno == EEXIST && noc__directory_exists(path));
#endif
}

static bool noc__ensure_parent_directories(Noc_Context *context, const char *path)
{
    Noc_Buffer copy = {0};
    Noc_Location location = {0};
    size_t i;
    bool ok = false;
    location.path = path;
    if (!noc_buffer_append_cstr(&copy, path) || !noc_buffer_terminate(&copy)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "out of memory while creating output directories");
        goto done;
    }
    for (i = 0; i < copy.count; ++i) {
        bool separator = copy.items[i] == '/';
#ifdef _WIN32
        separator = separator || copy.items[i] == '\\';
#endif
        if (!separator || i == 0) continue;
#ifdef _WIN32
        if (i == 2 && copy.items[1] == ':') continue;
#endif
        copy.items[i] = '\0';
        if (!noc__make_directory(copy.items)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "could not create output directory '%s': %s",
                        copy.items,
                        strerror(errno));
            copy.items[i] = '/';
            goto done;
        }
        copy.items[i] = '/';
    }
    ok = true;

done:
    noc_buffer_free(&copy);
    return ok;
}

NOCDEF bool noc_transform_files(Noc_Context *context,
                                const Noc_Batch_Options *options,
                                const char *const *input_paths,
                                size_t input_count)
{
    Noc_Buffer *outputs = NULL;
    Noc_Location no_location = {0};
    size_t i;
    size_t j;
    bool ok = false;
    if (!context || !options || !input_paths || input_count == 0 ||
        input_count > SIZE_MAX / sizeof(*outputs)) {
        if (context) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "batch transformation requires at least one input");
        }
        return false;
    }
    outputs = (Noc_Buffer *)calloc(input_count, sizeof(*outputs));
    if (!outputs) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while preparing batch transformation");
        return false;
    }
    for (i = 0; i < input_count; ++i) {
        if (!input_paths[i]) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "batch input path cannot be NULL");
            goto done;
        }
        if (!noc__batch_map_output(context, options, input_paths[i], &outputs[i])) {
            goto done;
        }
        for (j = 0; j < i; ++j) {
            if (noc__paths_equal_for_output(outputs[j].items, outputs[i].items)) {
                Noc_Location location = {0};
                location.path = input_paths[i];
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            location,
                            "batch inputs map to the same output '%s'",
                            outputs[i].items);
                goto done;
            }
        }
    }
    for (i = 0; i < input_count; ++i) {
        for (j = 0; j < input_count; ++j) {
            if (noc__paths_equal_for_output(outputs[i].items, input_paths[j]) ||
                noc__paths_refer_to_same_file(outputs[i].items, input_paths[j])) {
                Noc_Location location = {0};
                location.path = input_paths[j];
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            location,
                            "batch output '%s' aliases input '%s'",
                            outputs[i].items,
                            input_paths[j]);
                goto done;
            }
        }
    }
    for (i = 0; i < input_count; ++i) {
        if (!noc__ensure_parent_directories(context, outputs[i].items)) goto done;
        if (options->emit_depfiles) {
            Noc_Transform_Result result = {0};
            Noc_Buffer depfile = {0};
            Noc_Buffer depfile_path = {0};
            size_t errors_before = context->error_count;
            bool transformed = noc_transform_file_with_result(context,
                                                               input_paths[i],
                                                               outputs[i].items,
                                                               &result);
            if (transformed) {
                transformed = noc_buffer_append_cstr(&depfile_path, outputs[i].items) &&
                              noc_buffer_append_cstr(&depfile_path, ".d") &&
                              noc_buffer_terminate(&depfile_path) &&
                              noc_generate_depfile(context,
                                                   outputs[i].items,
                                                   input_paths[i],
                                                   &result,
                                                   &depfile) &&
                              noc__write_output_atomic(context,
                                                       depfile_path.items,
                                                       depfile.items,
                                                       depfile.count,
                                                       no_location);
                if (!transformed && context->error_count == errors_before) {
                    noc__report(context,
                                NOC_DIAGNOSTIC_ERROR,
                                no_location,
                                "could not generate batch depfile");
                }
            }
            noc_buffer_free(&depfile_path);
            noc_buffer_free(&depfile);
            noc_transform_result_free(&result);
            if (!transformed) goto done;
        } else if (!noc_transform_file(context, input_paths[i], outputs[i].items)) {
            goto done;
        }
    }
    ok = true;

done:
    for (i = 0; i < input_count; ++i) noc_buffer_free(&outputs[i]);
    free(outputs);
    return ok;
}

static bool noc__resolve_path(const char *source_path,
                              const char *referenced_path,
                              Noc_Buffer *resolved)
{
    const char *slash;
#ifdef _WIN32
    const char *backslash;
#endif
    const char *separator;
    if (noc__path_is_absolute(referenced_path)) {
        return noc_buffer_append_cstr(resolved, referenced_path) &&
               noc_buffer_terminate(resolved);
    }
    slash = strrchr(source_path, '/');
#ifdef _WIN32
    backslash = strrchr(source_path, '\\');
    separator = slash;
    if (!separator || (backslash && backslash > separator)) separator = backslash;
#else
    separator = slash;
#endif
    if (separator &&
        !noc_buffer_append(resolved, source_path, (size_t)(separator - source_path + 1))) {
        return false;
    }
    return noc_buffer_append_cstr(resolved, referenced_path) &&
           noc_buffer_terminate(resolved);
}

static bool noc__expand_embed(Noc_Rewriter *rewriter,
                              const Noc_Rule *rule,
                              void *user_data)
{
    const Noc_Token *path_token;
    Noc_Buffer decoded_path = {0};
    Noc_Buffer resolved_path = {0};
    Noc_Buffer contents = {0};
    Noc_Token consumed;
    bool ok = false;
    (void)rule;
    (void)user_data;
    if (!noc_rw_expect_punct(rewriter, "(")) goto done;
    noc_rw_skip_trivia(rewriter);
    path_token = noc_rw_peek_raw(rewriter, 0);
    if (!path_token || path_token->kind != NOC_TOKEN_STRING) {
        if (path_token) {
            noc_rw_error_at(rewriter,
                            path_token->location,
                            "@%s expects one ordinary string literal path",
                            rewriter->rule->name);
        } else {
            noc_rw_error(rewriter,
                         "@%s expects one ordinary string literal path",
                         rewriter->rule->name);
        }
        goto done;
    }
    if (!noc_decode_string_token(*path_token, &decoded_path) ||
        !noc_buffer_terminate(&decoded_path) ||
        memchr(decoded_path.items, '\0', decoded_path.count) != NULL) {
        noc_rw_error_at(rewriter,
                        path_token->location,
                        "@%s path is not a supported C string literal",
                        rewriter->rule->name);
        goto done;
    }
    (void)noc_rw_take_raw(rewriter, &consumed);
    if (!noc_rw_expect_punct(rewriter, ")")) goto done;
    if (!noc__resolve_path(rewriter->path, decoded_path.items, &resolved_path)) {
        noc_rw_error(rewriter, "out of memory while resolving embedded file path");
        goto done;
    }
    if (!noc__read_file(rewriter->context,
                        resolved_path.items,
                        &contents,
                        path_token->location)) {
        rewriter->failed = true;
        goto done;
    }
    if (!noc_rw_add_dependency(rewriter, resolved_path.items)) goto done;
    if (!noc_rw_emit_c_string(rewriter, contents.items, contents.count)) goto done;
    ok = true;

done:
    noc_buffer_free(&contents);
    noc_buffer_free(&resolved_path);
    noc_buffer_free(&decoded_path);
    return ok;
}

NOCDEF bool noc_register_embed_rule(Noc_Context *context, const char *name)
{
    Noc_Rule rule;
    rule.name = name;
    rule.scope = NOC_RULE_EXPRESSION;
    rule.syntax = "@<registered-name>(\"path\")";
    rule.description = "Embed a file as a C string literal; paths are relative to the source file.";
    rule.expand = noc__expand_embed;
    rule.user_data = NULL;
    return noc_register_rule(context, rule);
}

static void noc__print_usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "Usage:\n"
            "  %s [options] INPUT.[ch] -o OUTPUT.[ch]\n"
            "  %s --batch INPUT_ROOT OUTPUT_ROOT INPUT.[ch]...\n"
            "  %s --batch-depfiles INPUT_ROOT OUTPUT_ROOT INPUT.[ch]...\n"
            "  %s --describe\n"
            "  %s --ide-metadata OUTPUT.h\n"
            "  %s --command-signature OUTPUT -- COMMAND [ARG...]\n\n"
            "Options:\n"
            "  -o PATH               Write transformed C source/header to PATH\n"
            "  --depfile PATH        Write Make/Ninja dependencies to PATH\n"
            "  --dep-target PATH     Override the depfile target (default: -o PATH)\n"
            "  --describe            Describe all registered dialect rules\n"
            "  --ide-metadata PATH    Write a default IDE metadata header\n"
            "  --no-line-directives  Do not prepend a #line directive\n"
            "  -h, --help            Show this help\n",
            program,
            program,
            program,
            program,
            program,
            program);
}

NOCDEF int noc_run_cli(Noc_Context *context, int argc, char **argv)
{
    const char *input = NULL;
    const char *output = NULL;
    const char *depfile = NULL;
    const char *dep_target = NULL;
    int i;
    if (argc > 1 && strcmp(argv[1], "--command-signature") == 0) {
        const char **command_arguments;
        size_t command_argument_count;
        Noc_Buffer signature = {0};
        Noc_Location no_location = {0};
        bool ok;
        if (argc < 5 || strcmp(argv[3], "--") != 0) {
            noc__print_usage(stderr, argv[0]);
            return 2;
        }
        command_argument_count = (size_t)(argc - 4);
        command_arguments = (const char **)malloc(command_argument_count *
                                                   sizeof(*command_arguments));
        if (!command_arguments) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while preparing command signature arguments");
            return 1;
        }
        for (i = 0; i < (int)command_argument_count; ++i) {
            command_arguments[i] = argv[i + 4];
        }
        ok = noc_generate_command_signature(context,
                                            command_arguments,
                                            command_argument_count,
                                            &signature) &&
             noc__write_output_atomic(context,
                                      argv[2],
                                      signature.items,
                                      signature.count,
                                      no_location);
        free(command_arguments);
        noc_buffer_free(&signature);
        return ok ? 0 : 1;
    }
    if (argc > 1 &&
        (strcmp(argv[1], "--batch") == 0 ||
         strcmp(argv[1], "--batch-depfiles") == 0)) {
        Noc_Batch_Options options;
        const char **batch_inputs;
        size_t batch_input_count;
        bool ok;
        if (argc < 5) {
            noc__print_usage(stderr, argv[0]);
            return 2;
        }
        options.input_root = argv[2];
        options.output_root = argv[3];
        options.emit_depfiles = strcmp(argv[1], "--batch-depfiles") == 0;
        batch_input_count = (size_t)(argc - 4);
        if (batch_input_count > SIZE_MAX / sizeof(*batch_inputs)) return 1;
        batch_inputs = (const char **)malloc(batch_input_count * sizeof(*batch_inputs));
        if (!batch_inputs) {
            Noc_Location no_location = {0};
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while preparing batch CLI inputs");
            return 1;
        }
        for (i = 0; i < (int)batch_input_count; ++i) batch_inputs[i] = argv[i + 4];
        ok = noc_transform_files(context, &options, batch_inputs, batch_input_count);
        free(batch_inputs);
        return ok ? 0 : 1;
    }
    for (i = 1; i < argc; ++i) {
        const char *argument = argv[i];
        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            noc__print_usage(stdout, argv[0]);
            return 0;
        }
        if (strcmp(argument, "--describe") == 0) {
            noc_describe(context, stdout);
            return 0;
        }
        if (strcmp(argument, "--ide-metadata") == 0) {
            Noc_Buffer metadata = {0};
            Noc_Location no_location = {0};
            bool ok;
            if (i + 1 >= argc) {
                fprintf(stderr, "noc: error: --ide-metadata requires a path\n");
                return 2;
            }
            if (i != 1 || argc != 3) {
                fprintf(stderr, "noc: error: --ide-metadata must be used alone\n");
                return 2;
            }
            ok = noc_generate_ide_metadata_header(context, NULL, &metadata) &&
                 noc__write_output_atomic(context,
                                          argv[i + 1],
                                          metadata.items,
                                          metadata.count,
                                          no_location);
            noc_buffer_free(&metadata);
            return ok ? 0 : 1;
        }
        if (strcmp(argument, "--no-line-directives") == 0) {
            context->options.emit_line_directives = false;
            continue;
        }
        if (strcmp(argument, "--depfile") == 0 ||
            strcmp(argument, "--dep-target") == 0) {
            const char **destination = strcmp(argument, "--depfile") == 0
                                           ? &depfile
                                           : &dep_target;
            if (i + 1 >= argc) {
                fprintf(stderr, "noc: error: %s requires a path\n", argument);
                return 2;
            }
            if (*destination) {
                fprintf(stderr, "noc: error: %s may be specified only once\n", argument);
                return 2;
            }
            *destination = argv[++i];
            continue;
        }
        if (strcmp(argument, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "noc: error: -o requires a path\n");
                return 2;
            }
            output = argv[++i];
            continue;
        }
        if (argument[0] == '-') {
            fprintf(stderr, "noc: error: unknown option '%s'\n", argument);
            return 2;
        }
        if (input) {
            fprintf(stderr, "noc: error: only one input is currently supported\n");
            return 2;
        }
        input = argument;
    }
    if (!input || !output) {
        noc__print_usage(stderr, argv[0]);
        return 2;
    }
    if (dep_target && !depfile) {
        fprintf(stderr, "noc: error: --dep-target requires --depfile\n");
        return 2;
    }
    if (depfile &&
        (noc__paths_refer_to_same_file(depfile, input) ||
         noc__paths_refer_to_same_file(depfile, output))) {
        fprintf(stderr, "noc: error: depfile path must differ from input and output\n");
        return 2;
    }
    if (depfile) {
        Noc_Transform_Result result = {0};
        Noc_Buffer generated = {0};
        Noc_Location no_location = {0};
        bool ok = noc_transform_file_with_result(context, input, output, &result);
        if (ok && noc__paths_refer_to_same_file(depfile, output)) {
            fprintf(stderr, "noc: error: depfile path resolves to the output path\n");
            ok = false;
        }
        if (ok) {
            ok = noc_generate_depfile(context,
                                      dep_target ? dep_target : output,
                                      input,
                                      &result,
                                      &generated) &&
                 noc__write_output_atomic(context,
                                          depfile,
                                          generated.items,
                                          generated.count,
                                          no_location);
        }
        noc_buffer_free(&generated);
        noc_transform_result_free(&result);
        return ok ? 0 : 1;
    }
    return noc_transform_file(context, input, output) ? 0 : 1;
}

#endif /* NOC_EMIT_C_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */

#undef NOC__PRIVATE

/*
   Original Noc code is free and unencumbered software released into the public
   domain. Embedded third-party components retain the licenses reproduced in
   their generated implementation payload above.

   Anyone is free to copy, modify, publish, use, compile, sell, or distribute
   the original Noc code, either in source code form or as a compiled binary,
   for any purpose, commercial or non-commercial, and by any means.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
*/
