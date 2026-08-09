#include <noc/noc.h>

#include <string.h>

static Noc_Include_Resolve_Status public_header_resolver(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *output)
{
    (void)user_data;
    (void)request;
    (void)output;
    return NOC_INCLUDE_RESOLVE_NOT_FOUND;
}

int main(void)
{
    Noc_Slice version = {NOC_VERSION, sizeof(NOC_VERSION) - 1};
    Noc_Token_Range empty = {0, 0};
    Noc_Pragma_Once pragma_once;
    Noc_Include_Guard guard;
    Noc_Include_Expansion expansion;
    Noc_Include_Graph graph;
    Noc_Include_Resolver resolver;
    Noc_C_Ast ast;
    Noc_C_Ast_Options ast_options;
    memset(&pragma_once, 0, sizeof(pragma_once));
    memset(&guard, 0, sizeof(guard));
    memset(&expansion, 0, sizeof(expansion));
    memset(&graph, 0, sizeof(graph));
    memset(&ast, 0, sizeof(ast));
    memset(&ast_options, 0, sizeof(ast_options));
    resolver.resolve = public_header_resolver;
    resolver.user_data = NULL;
    return version.count == strlen(NOC_VERSION) &&
                   empty.begin == empty.end &&
                   pragma_once.generation == 0 &&
                   guard.generation == 0 &&
                   expansion.generation == 0 &&
                   graph.generation == 0 &&
                   ast.generation == 0 &&
                   ast_options.max_nodes == 0 &&
                   resolver.resolve != NULL &&
                   NOC_INCLUDE_FORM_QUOTED != NOC_INCLUDE_FORM_ANGLED &&
                   NOC_VERSION_MAJOR == 0 &&
                   NOC_VERSION_MINOR == 42 &&
                   NOC_VERSION_PATCH == 4
               ? 0
               : 1;
}
