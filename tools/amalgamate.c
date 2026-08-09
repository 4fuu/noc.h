/* Portable deterministic source amalgamator. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static int is_redundant_local_include(const unsigned char *line, size_t count)
{
    static const char *const includes[] = {
        "#include \"../include/noc/noc.h\"",
        "#include \"internal.h\"",
        "#include \"../third_party/tree-sitter/tree_sitter_private.h\"",
    };
    size_t begin = 0;
    size_t end = count;
    size_t index;
    while (begin < end && (line[begin] == ' ' || line[begin] == '\t')) begin += 1;
    while (end > begin &&
           (line[end - 1] == ' ' || line[end - 1] == '\t' ||
            line[end - 1] == '\r' || line[end - 1] == '\n')) {
        end -= 1;
    }
    for (index = 0; index < sizeof(includes) / sizeof(includes[0]); ++index) {
        size_t include_count = strlen(includes[index]);
        if (end - begin == include_count &&
            memcmp(line + begin, includes[index], include_count) == 0) {
            return 1;
        }
    }
    return 0;
}

static int copy_file(FILE *output, const char *path)
{
    FILE *input = fopen(path, "rb");
    unsigned char *line = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int character;
    int last_written = '\n';
    int ok = 1;
    if (!input) {
        fprintf(stderr, "amalgamate: cannot open %s: %s\n", path, strerror(errno));
        return 0;
    }
    while ((character = fgetc(input)) != EOF) {
        if (count == capacity) {
            size_t next_capacity = capacity == 0 ? 4096 : capacity * 2;
            unsigned char *next;
            if (next_capacity <= capacity) {
                ok = 0;
                break;
            }
            next = (unsigned char *)realloc(line, next_capacity);
            if (!next) {
                ok = 0;
                break;
            }
            line = next;
            capacity = next_capacity;
        }
        line[count++] = (unsigned char)character;
        if (character == '\n') {
            /* Repository attributes keep generated inputs on LF, but normalize
               defensively so an archive export or foreign checkout still
               produces the same release bytes on every host. */
            if (count >= 2 && line[count - 2] == '\r') {
                line[count - 2] = '\n';
                count -= 1;
            }
            if (!is_redundant_local_include(line, count)) {
                if (fwrite(line, 1, count, output) != count) {
                    ok = 0;
                    break;
                }
                last_written = '\n';
            }
            count = 0;
        }
    }
    if (ok && ferror(input)) {
        fprintf(stderr, "amalgamate: cannot read %s: %s\n", path, strerror(errno));
        ok = 0;
    }
    if (ok && count != 0 && !is_redundant_local_include(line, count)) {
        if (fwrite(line, 1, count, output) != count) {
            ok = 0;
        } else {
            last_written = line[count - 1];
        }
    }
    free(line);
    if (fclose(input) != 0) ok = 0;
    if (!ok) {
        fprintf(stderr, "amalgamate: failed while copying %s\n", path);
        return 0;
    }
    return last_written == '\n' || fputc('\n', output) != EOF;
}

static int files_equal(const char *left_path, const char *right_path)
{
    unsigned char left_buffer[16384];
    unsigned char right_buffer[16384];
    FILE *left = fopen(left_path, "rb");
    FILE *right;
    int equal = 1;
    if (!left) return -1;
    right = fopen(right_path, "rb");
    if (!right) {
        int missing = errno == ENOENT;
        fclose(left);
        return missing ? 0 : -1;
    }
    while (equal) {
        size_t left_count = fread(left_buffer, 1, sizeof(left_buffer), left);
        size_t right_count = fread(right_buffer, 1, sizeof(right_buffer), right);
        if (left_count != right_count ||
            (left_count != 0 && memcmp(left_buffer, right_buffer, left_count) != 0)) {
            equal = 0;
        }
        if (left_count < sizeof(left_buffer) || right_count < sizeof(right_buffer)) {
            if (ferror(left) || ferror(right)) equal = -1;
            break;
        }
    }
    if (fclose(left) != 0 || fclose(right) != 0) equal = -1;
    return equal;
}

static int replace_if_changed(const char *temporary, const char *output)
{
    int equal = files_equal(temporary, output);
    if (equal < 0) {
        fprintf(stderr, "amalgamate: cannot compare %s and %s: %s\n",
                temporary,
                output,
                strerror(errno));
        return 0;
    }
    if (equal) return remove(temporary) == 0;

#ifdef _WIN32
    if (MoveFileExA(temporary,
                    output,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return 1;
    }
    fprintf(stderr,
            "amalgamate: cannot replace %s: Windows error %lu\n",
            output,
            (unsigned long)GetLastError());
    return 0;
#else
    /* POSIX rename replaces atomically. A failure leaves the old output in
       place and must never be followed by deleting that known-good file. */
    if (rename(temporary, output) == 0) return 1;
    fprintf(stderr, "amalgamate: cannot replace %s: %s\n", output, strerror(errno));
    return 0;
#endif
}

int main(int argc, char **argv)
{
    static const char banner[] =
        "/* Generated by `./nob header` from the sources listed in nob.c.\n"
        "   Do not edit this file directly. */\n\n";
    char *temporary;
    FILE *output;
    size_t length;
    int index;
    int ok = 1;
    if (argc < 3) {
        fprintf(stderr, "Usage: %s OUTPUT INPUT...\n", argv[0]);
        return 2;
    }
    length = strlen(argv[1]);
    temporary = (char *)malloc(length + sizeof(".tmp"));
    if (!temporary) return 1;
    memcpy(temporary, argv[1], length);
    memcpy(temporary + length, ".tmp", sizeof(".tmp"));
    output = fopen(temporary, "wb");
    if (!output) {
        fprintf(stderr, "amalgamate: cannot create %s: %s\n", temporary, strerror(errno));
        free(temporary);
        return 1;
    }
    ok = fwrite(banner, 1, sizeof(banner) - 1, output) == sizeof(banner) - 1;
    for (index = 2; ok && index < argc; ++index) ok = copy_file(output, argv[index]);
    if (fclose(output) != 0) ok = 0;
    if (ok) ok = replace_if_changed(temporary, argv[1]);
    if (!ok) remove(temporary);
    free(temporary);
    return ok ? 0 : 1;
}
