#include <noc/noc.h>

#include <cstddef>

int main()
{
    static const char source[] = "int called_from_cpp(void) { return 7; }\n";
    Noc_Workspace workspace = {};
    Noc_Document_Snapshot snapshot = {};
    Noc_C_Parse_Tree tree = {};
    Noc_C_Ast ast = {};
    int result = 1;

    noc_workspace_init(&workspace);
    if (noc_workspace_open_document(&workspace,
                                    "parse/cpp-client.c",
                                    source,
                                    sizeof(source) - 1,
                                    NOC_SOURCE_CLASS_PROJECT,
                                    &snapshot) == NOC_WORKSPACE_OK &&
        noc_c_parse_tree_build(&snapshot,
                               noc_c_parse_default_options(),
                               &tree) == NOC_C_PARSE_OK &&
        noc_c_parse_tree_is_valid(&tree) &&
        !noc_c_parse_tree_has_error(&tree) &&
        noc_c_parse_tree_node_count(&tree) > 1 &&
        noc_c_ast_build(&tree, noc_c_ast_default_options(), &ast) ==
            NOC_C_AST_OK &&
        noc_c_ast_is_syntax_complete(&ast) &&
        noc_c_ast_node_at(&ast, noc_c_ast_root(&ast))->kind ==
            NOC_C_AST_KIND_TRANSLATION_UNIT) {
        result = 0;
    }
    noc_c_ast_free(&ast);
    noc_c_parse_tree_free(&tree);
    noc_document_snapshot_free(&snapshot);
    noc_workspace_deinit(&workspace);
    return result;
}
