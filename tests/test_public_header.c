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
    Noc_C_Grammar_Candidates grammar_candidates;
    Noc_C_Grammar_Candidate_Options grammar_options;
    Noc_C_Ast ast;
    Noc_C_Ast_Options ast_options;
    Noc_Logical_C_Parse_Tree logical_parse_tree;
    Noc_Logical_C_Parse_Node logical_parse_node;
    Noc_Logical_C_Ast logical_ast;
    Noc_Logical_C_Ast_Node logical_ast_node;
    Noc_Logical_Location logical_location;
    Noc_Logical_Token_Range logical_tokens;
    memset(&pragma_once, 0, sizeof(pragma_once));
    memset(&guard, 0, sizeof(guard));
    memset(&expansion, 0, sizeof(expansion));
    memset(&graph, 0, sizeof(graph));
    memset(&grammar_candidates, 0, sizeof(grammar_candidates));
    memset(&grammar_options, 0, sizeof(grammar_options));
    memset(&ast, 0, sizeof(ast));
    memset(&ast_options, 0, sizeof(ast_options));
    memset(&logical_parse_tree, 0, sizeof(logical_parse_tree));
    memset(&logical_parse_node, 0, sizeof(logical_parse_node));
    memset(&logical_ast, 0, sizeof(logical_ast));
    memset(&logical_ast_node, 0, sizeof(logical_ast_node));
    memset(&logical_location, 0, sizeof(logical_location));
    memset(&logical_tokens, 0, sizeof(logical_tokens));
    resolver.resolve = public_header_resolver;
    resolver.user_data = NULL;
    return version.count == strlen(NOC_VERSION) &&
                   empty.begin == empty.end &&
                   pragma_once.generation == 0 &&
                   guard.generation == 0 &&
                   expansion.generation == 0 &&
                   graph.generation == 0 &&
                   grammar_candidates.generation == 0 &&
                   grammar_options.max_candidates == 0 &&
                   ast.generation == 0 &&
                   ast_options.max_nodes == 0 &&
                   logical_parse_tree.generation == 0 &&
                   logical_parse_node.bytes.begin ==
                       logical_parse_node.bytes.end &&
                   logical_ast.generation == 0 &&
                   logical_ast_node.bytes.begin ==
                       logical_ast_node.bytes.end &&
                   logical_location.line == 0 &&
                   logical_tokens.begin == logical_tokens.end &&
                   resolver.resolve != NULL &&
                   NOC_INCLUDE_FORM_QUOTED != NOC_INCLUDE_FORM_ANGLED &&
                   NOC_VERSION_MAJOR == 0 &&
                   NOC_VERSION_MINOR == 42 &&
                   NOC_VERSION_PATCH == 10
               ? 0
               : 1;
}
