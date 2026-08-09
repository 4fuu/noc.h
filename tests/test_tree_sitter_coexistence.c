#include <noc/noc.h>

#include <stdbool.h>
#include <stddef.h>

/* A consumer may link its own ordinary Tree-sitter. These deliberately named
   definitions turn any unprefixed embedded runtime symbol into a link error. */
typedef struct TSParser {
    int consumer_owned;
} TSParser;

TSParser *ts_parser_new(void)
{
    static TSParser parser;
    return &parser;
}

const void *tree_sitter_c(void)
{
    static const int language = 1;
    return &language;
}

int _ts_dup(int descriptor)
{
    return descriptor;
}

bool range_intersects(const void *left, const void *right)
{
    return left == right;
}

bool range_within(const void *left, const void *right)
{
    return left == right;
}

int main(void)
{
    static const char source[] = "int coexistence;\n";
    Noc_Workspace workspace = {0};
    Noc_Document_Snapshot snapshot = {0};
    Noc_C_Parse_Tree tree = {0};
    int result = 1;

    noc_workspace_init(&workspace);
    if (ts_parser_new() && tree_sitter_c() &&
        noc_workspace_open_document(&workspace,
                                    "coexistence.c",
                                    source,
                                    sizeof(source) - 1,
                                    NOC_SOURCE_CLASS_PROJECT,
                                    &snapshot) == NOC_WORKSPACE_OK &&
        noc_c_parse_tree_build(&snapshot,
                               noc_c_parse_default_options(),
                               &tree) == NOC_C_PARSE_OK &&
        noc_c_parse_tree_is_valid(&tree) &&
        !noc_c_parse_tree_has_error(&tree)) {
        result = 0;
    }
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    return result;
}
