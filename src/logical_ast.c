#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_LOGICAL_AST_IMPLEMENTATION_INCLUDED
#define NOC_LOGICAL_AST_IMPLEMENTATION_INCLUDED

/* Logical and physical ASTs share normalization in ast.c but own coordinate-
   specific public node arrays. Keeping the logical source here makes every
   source/provenance query independent of the input CST after publication. */
struct Noc_Logical_C_Ast_Impl {
    Noc_Logical_Source source;
    Noc_Logical_C_Ast_Node *nodes;
    Noc__C_Ast_Detail *details;
    size_t count;
    unsigned int issues;
};

static void noc__logical_c_ast_impl_free(
    Noc_Logical_C_Ast_Impl *implementation)
{
    size_t index;
    if (!implementation) return;
    for (index = 0; index < implementation->count; ++index) {
        free(implementation->details[index].expected_spelling);
    }
    free(implementation->details);
    free(implementation->nodes);
    noc_logical_source_free(&implementation->source);
    free(implementation);
}

static bool noc__logical_c_ast_input_query(
    const void *context,
    size_t node_index,
    Noc__C_Ast_Input_Node *output)
{
    const Noc_Logical_C_Parse_Tree *tree =
        (const Noc_Logical_C_Parse_Tree *)context;
    const Noc_Logical_C_Parse_Node *node =
        noc_logical_c_parse_tree_node_at(tree, node_index);
    if (!node || !output) return false;
    output->kind = node->kind;
    output->field = node->field;
    output->spelling = noc_logical_c_parse_node_source(tree, node_index);
    output->bytes_begin = node->bytes.begin;
    output->bytes_end = node->bytes.end;
    output->parent = node->parent;
    output->flags = node->flags;
    return true;
}

NOCDEF Noc_C_Ast_Status noc_logical_c_ast_build(
    const Noc_Logical_C_Parse_Tree *tree,
    Noc_C_Ast_Options options,
    Noc_Logical_C_Ast *output)
{
    Noc_Logical_C_Ast_Impl *built = NULL;
    Noc_Logical_C_Ast_Impl *previous;
    Noc_Logical_Source retained = {0};
    Noc_Logical_Source_Status clone_status;
    Noc__C_Ast_Normalized normalized = {0};
    Noc__C_Ast_Input input;
    Noc_C_Ast_Status status;
    size_t generation;
    size_t index;

    if (!noc_logical_c_parse_tree_is_valid(tree) || !output ||
        options.max_nodes == 0) {
        return NOC_C_AST_INVALID_ARGUMENT;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_C_AST_GENERATION_EXHAUSTED;
    }
    /* A freed handle retains its generation so borrowing IDE contexts cannot
       become valid again when the same handle address is rebuilt. */
    if (output->impl && !noc_logical_c_ast_is_valid(output)) {
        return NOC_C_AST_INVALID_ARGUMENT;
    }
    if (options.should_cancel &&
        options.should_cancel(options.cancel_user_data)) {
        return NOC_C_AST_CANCELLED;
    }

    /* Retain the immutable coordinate/provenance revision before the first
       periodic caller callback, matching the physical AST's retained-input
       contract after the required immediate cancellation poll. */
    clone_status = noc_logical_source_clone(
        noc_logical_c_parse_tree_source(tree),
        &retained);
    if (clone_status != NOC_LOGICAL_SOURCE_OK) {
        return clone_status == NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED
                   ? NOC_C_AST_LIMIT_EXCEEDED
               : clone_status == NOC_LOGICAL_SOURCE_OUT_OF_MEMORY
                   ? NOC_C_AST_OUT_OF_MEMORY
                   : NOC_C_AST_INVALID_ARGUMENT;
    }

    generation = output->generation + 1;
    input.context = tree;
    input.count = noc_logical_c_parse_tree_node_count(tree);
    input.query = noc__logical_c_ast_input_query;
    status = noc__c_ast_normalize(input, options, generation, &normalized);
    if (status != NOC_C_AST_OK) {
        noc_logical_source_free(&retained);
        return status;
    }
    if (normalized.count > SIZE_MAX / sizeof(*built->nodes)) {
        noc__c_ast_normalized_free(&normalized);
        noc_logical_source_free(&retained);
        return NOC_C_AST_LIMIT_EXCEEDED;
    }
    built = (Noc_Logical_C_Ast_Impl *)calloc(1, sizeof(*built));
    if (!built) {
        noc__c_ast_normalized_free(&normalized);
        noc_logical_source_free(&retained);
        return NOC_C_AST_OUT_OF_MEMORY;
    }
    built->nodes = (Noc_Logical_C_Ast_Node *)malloc(
        normalized.count * sizeof(*built->nodes));
    if (!built->nodes) {
        noc__c_ast_normalized_free(&normalized);
        noc_logical_source_free(&retained);
        noc__logical_c_ast_impl_free(built);
        return NOC_C_AST_OUT_OF_MEMORY;
    }
    for (index = 0; index < normalized.count; ++index) {
        const Noc__C_Ast_Normalized_Node *source = &normalized.nodes[index];
        Noc_Logical_C_Ast_Node *node = &built->nodes[index];
        if ((index & 255u) == 0 && options.should_cancel &&
            options.should_cancel(options.cancel_user_data)) {
            noc__c_ast_normalized_free(&normalized);
            noc_logical_source_free(&retained);
            noc__logical_c_ast_impl_free(built);
            return NOC_C_AST_CANCELLED;
        }
        node->kind = source->kind;
        node->field = source->field;
        node->bytes.begin = source->bytes_begin;
        node->bytes.end = source->bytes_end;
        node->parent = source->parent;
        node->first_child = source->first_child;
        node->last_child = source->last_child;
        node->next_sibling = source->next_sibling;
        node->child_count = source->child_count;
        node->generation = source->generation;
        node->flags = source->flags;
    }
    if (options.should_cancel &&
        options.should_cancel(options.cancel_user_data)) {
        noc__c_ast_normalized_free(&normalized);
        noc_logical_source_free(&retained);
        noc__logical_c_ast_impl_free(built);
        return NOC_C_AST_CANCELLED;
    }
    built->source = retained;
    memset(&retained, 0, sizeof(retained));
    built->details = normalized.details;
    built->count = normalized.count;
    built->issues = normalized.issues;
    normalized.details = NULL;
    free(normalized.nodes);
    normalized.nodes = NULL;

    previous = output->impl;
    output->impl = built;
    output->generation = generation;
    noc__logical_c_ast_impl_free(previous);
    return NOC_C_AST_OK;
}

NOCDEF void noc_logical_c_ast_free(Noc_Logical_C_Ast *ast)
{
    Noc_Logical_C_Ast_Impl *implementation;
    size_t generation;
    if (!ast) return;
    implementation = ast->impl;
    generation = ast->generation;
    memset(ast, 0, sizeof(*ast));
    ast->generation = generation;
    noc__logical_c_ast_impl_free(implementation);
}

NOCDEF bool noc_logical_c_ast_is_valid(const Noc_Logical_C_Ast *ast)
{
    const Noc_Logical_C_Ast_Node *root;
    Noc_Slice text;
    if (!ast || !ast->impl || ast->generation == 0 ||
        !ast->impl->nodes || !ast->impl->details || ast->impl->count == 0) {
        return false;
    }
    root = &ast->impl->nodes[0];
    text = noc_logical_source_text(&ast->impl->source);
    return root->parent == NOC_C_AST_NODE_NONE &&
           root->generation == ast->generation && root->bytes.begin == 0 &&
           root->bytes.end == text.count;
}

NOCDEF bool noc_logical_c_ast_is_syntax_complete(
    const Noc_Logical_C_Ast *ast)
{
    return noc_logical_c_ast_is_valid(ast) && ast->impl->issues == 0;
}

NOCDEF unsigned int noc_logical_c_ast_issues(
    const Noc_Logical_C_Ast *ast)
{
    return noc_logical_c_ast_is_valid(ast) ? ast->impl->issues : 0;
}

NOCDEF size_t noc_logical_c_ast_generation(
    const Noc_Logical_C_Ast *ast)
{
    return noc_logical_c_ast_is_valid(ast) ? ast->generation : 0;
}

NOCDEF size_t noc_logical_c_ast_source_generation(
    const Noc_Logical_C_Ast *ast)
{
    return noc_logical_c_ast_is_valid(ast)
               ? ast->impl->source.generation
               : 0;
}

NOCDEF const Noc_Logical_Source *noc_logical_c_ast_source(
    const Noc_Logical_C_Ast *ast)
{
    return noc_logical_c_ast_is_valid(ast) ? &ast->impl->source : NULL;
}

NOCDEF size_t noc_logical_c_ast_node_count(
    const Noc_Logical_C_Ast *ast)
{
    return noc_logical_c_ast_is_valid(ast) ? ast->impl->count : 0;
}

NOCDEF size_t noc_logical_c_ast_root(const Noc_Logical_C_Ast *ast)
{
    return noc_logical_c_ast_is_valid(ast) ? 0 : NOC_C_AST_NODE_NONE;
}

NOCDEF const Noc_Logical_C_Ast_Node *noc_logical_c_ast_node_at(
    const Noc_Logical_C_Ast *ast,
    size_t node_index)
{
    if (!noc_logical_c_ast_is_valid(ast) || node_index >= ast->impl->count) {
        return NULL;
    }
    return &ast->impl->nodes[node_index];
}

NOCDEF Noc_Slice noc_logical_c_ast_node_source(
    const Noc_Logical_C_Ast *ast,
    size_t node_index)
{
    Noc_Slice result = {0};
    const Noc_Logical_C_Ast_Node *node =
        noc_logical_c_ast_node_at(ast, node_index);
    Noc_Slice text;
    if (!node) return result;
    text = noc_logical_source_text(&ast->impl->source);
    if (node->bytes.begin > node->bytes.end || node->bytes.end > text.count) {
        return result;
    }
    result.data = text.data + node->bytes.begin;
    result.count = node->bytes.end - node->bytes.begin;
    return result;
}

NOCDEF Noc_Logical_Location noc_logical_c_ast_node_location(
    const Noc_Logical_C_Ast *ast,
    size_t node_index)
{
    Noc_Logical_Location location = {0};
    const Noc_Logical_C_Ast_Node *node =
        noc_logical_c_ast_node_at(ast, node_index);
    if (node) {
        (void)noc_logical_source_location(&ast->impl->source,
                                          node->bytes.begin,
                                          &location);
    }
    return location;
}

NOCDEF size_t noc_logical_c_ast_node_covering_range(
    const Noc_Logical_C_Ast *ast,
    Noc_Logical_Byte_Range range)
{
    Noc_Slice text;
    size_t node_index;
    if (!noc_logical_c_ast_is_valid(ast) || range.begin >= range.end) {
        return NOC_C_AST_NODE_NONE;
    }
    text = noc_logical_source_text(&ast->impl->source);
    if (range.end > text.count) return NOC_C_AST_NODE_NONE;
    node_index = noc_logical_c_ast_root(ast);
    for (;;) {
        const Noc_Logical_C_Ast_Node *node =
            noc_logical_c_ast_node_at(ast, node_index);
        size_t child;
        size_t covering = NOC_C_AST_NODE_NONE;
        if (!node || range.begin < node->bytes.begin ||
            range.end > node->bytes.end) {
            return NOC_C_AST_NODE_NONE;
        }
        child = node->first_child;
        while (child != NOC_C_AST_NODE_NONE) {
            const Noc_Logical_C_Ast_Node *candidate =
                noc_logical_c_ast_node_at(ast, child);
            if (!candidate) return NOC_C_AST_NODE_NONE;
            if (candidate->bytes.begin <= range.begin &&
                range.end <= candidate->bytes.end) {
                covering = child;
                break;
            }
            child = candidate->next_sibling;
        }
        if (covering == NOC_C_AST_NODE_NONE) return node_index;
        node_index = covering;
    }
}

NOCDEF size_t noc_logical_c_ast_node_at_offset(
    const Noc_Logical_C_Ast *ast,
    size_t offset)
{
    Noc_Logical_Byte_Range range;
    Noc_Slice text;
    if (!noc_logical_c_ast_is_valid(ast)) return NOC_C_AST_NODE_NONE;
    text = noc_logical_source_text(&ast->impl->source);
    if (offset >= text.count) return NOC_C_AST_NODE_NONE;
    range.begin = offset;
    range.end = offset + 1;
    return noc_logical_c_ast_node_covering_range(ast, range);
}

NOCDEF size_t noc_logical_c_ast_depth(
    const Noc_Logical_C_Ast *ast,
    size_t node_index)
{
    const Noc_Logical_C_Ast_Node *node =
        noc_logical_c_ast_node_at(ast, node_index);
    size_t depth = 0;
    size_t count = noc_logical_c_ast_node_count(ast);
    if (!node) return NOC_C_AST_NODE_NONE;
    while (node->parent != NOC_C_AST_NODE_NONE) {
        if (depth >= count) return NOC_C_AST_NODE_NONE;
        depth += 1;
        node = noc_logical_c_ast_node_at(ast, node->parent);
        if (!node) return NOC_C_AST_NODE_NONE;
    }
    return depth;
}

NOCDEF size_t noc_logical_c_ast_common_ancestor(
    const Noc_Logical_C_Ast *ast,
    size_t left,
    size_t right)
{
    size_t left_depth = noc_logical_c_ast_depth(ast, left);
    size_t right_depth = noc_logical_c_ast_depth(ast, right);
    if (left_depth == NOC_C_AST_NODE_NONE ||
        right_depth == NOC_C_AST_NODE_NONE) {
        return NOC_C_AST_NODE_NONE;
    }
    while (left_depth > right_depth) {
        left = ast->impl->nodes[left].parent;
        left_depth -= 1;
    }
    while (right_depth > left_depth) {
        right = ast->impl->nodes[right].parent;
        right_depth -= 1;
    }
    while (left != right) {
        if (left == NOC_C_AST_NODE_NONE || right == NOC_C_AST_NODE_NONE) {
            return NOC_C_AST_NODE_NONE;
        }
        left = ast->impl->nodes[left].parent;
        right = ast->impl->nodes[right].parent;
    }
    return left;
}

static bool noc__logical_c_ast_is_expected_at_offset(
    const Noc_Logical_C_Ast_Node *node,
    size_t offset)
{
    return node && (node->flags & NOC_C_AST_NODE_MISSING) != 0 &&
           node->bytes.begin == offset && node->bytes.end == offset;
}

NOCDEF bool noc_logical_c_ast_completion_context(
    const Noc_Logical_C_Ast *ast,
    size_t offset,
    Noc_Logical_C_Ast_Completion_Context *output)
{
    Noc_Logical_C_Ast_Completion_Context result;
    Noc_Slice text;
    size_t first_expected_parent = NOC_C_AST_NODE_NONE;
    size_t last_expected_parent = NOC_C_AST_NODE_NONE;
    size_t index;
    if (!noc_logical_c_ast_is_valid(ast) || !output) return false;
    text = noc_logical_source_text(&ast->impl->source);
    if (offset > text.count) return false;

    memset(&result, 0, sizeof(result));
    result.owner = ast;
    result.offset = offset;
    result.left_node = offset == 0
                           ? NOC_C_AST_NODE_NONE
                           : noc_logical_c_ast_node_at_offset(ast, offset - 1);
    result.right_node = offset == text.count
                            ? NOC_C_AST_NODE_NONE
                            : noc_logical_c_ast_node_at_offset(ast, offset);
    result.node = noc_logical_c_ast_root(ast);
    result.generation = noc_logical_c_ast_generation(ast);
    result.source_generation = noc_logical_c_ast_source_generation(ast);
    if (result.left_node != NOC_C_AST_NODE_NONE &&
        result.right_node != NOC_C_AST_NODE_NONE) {
        result.node = noc_logical_c_ast_common_ancestor(ast,
                                                        result.left_node,
                                                        result.right_node);
        if (result.node == NOC_C_AST_NODE_NONE) return false;
    }

    for (index = 0; index < ast->impl->count; ++index) {
        const Noc_Logical_C_Ast_Node *node = &ast->impl->nodes[index];
        size_t parent;
        if (!noc__logical_c_ast_is_expected_at_offset(node, offset)) continue;
        result.expected_count += 1;
        parent = node->parent == NOC_C_AST_NODE_NONE
                     ? noc_logical_c_ast_root(ast)
                     : node->parent;
        if (first_expected_parent == NOC_C_AST_NODE_NONE) {
            first_expected_parent = parent;
        }
        last_expected_parent = parent;
    }
    if (first_expected_parent != NOC_C_AST_NODE_NONE) {
        result.node = noc_logical_c_ast_common_ancestor(ast,
                                                        first_expected_parent,
                                                        last_expected_parent);
        if (result.node == NOC_C_AST_NODE_NONE) return false;
    }
    *output = result;
    return true;
}

NOCDEF size_t noc_logical_c_ast_completion_next_expected_node(
    const Noc_Logical_C_Ast *ast,
    const Noc_Logical_C_Ast_Completion_Context *context,
    size_t previous)
{
    Noc_Slice text;
    size_t index;
    if (!noc_logical_c_ast_is_valid(ast) || !context ||
        context->owner != ast ||
        context->generation != noc_logical_c_ast_generation(ast) ||
        context->source_generation != noc_logical_c_ast_source_generation(ast)) {
        return NOC_C_AST_NODE_NONE;
    }
    text = noc_logical_source_text(&ast->impl->source);
    if (context->offset > text.count) return NOC_C_AST_NODE_NONE;
    if (previous == NOC_C_AST_NODE_NONE) {
        index = 0;
    } else {
        const Noc_Logical_C_Ast_Node *previous_node =
            noc_logical_c_ast_node_at(ast, previous);
        if (!noc__logical_c_ast_is_expected_at_offset(previous_node,
                                                       context->offset)) {
            return NOC_C_AST_NODE_NONE;
        }
        index = previous + 1;
    }
    for (; index < ast->impl->count; ++index) {
        const Noc_Logical_C_Ast_Node *node = &ast->impl->nodes[index];
        if (noc__logical_c_ast_is_expected_at_offset(node, context->offset)) {
            return index;
        }
    }
    return NOC_C_AST_NODE_NONE;
}

NOCDEF bool noc_logical_c_ast_node_token_range(
    const Noc_Logical_C_Ast *ast,
    size_t node_index,
    Noc_Logical_Token_Range *output)
{
    const Noc_Logical_C_Ast_Node *node =
        noc_logical_c_ast_node_at(ast, node_index);
    if (!node) return false;
    return noc_logical_source_token_range_for_bytes(&ast->impl->source,
                                                     node->bytes,
                                                     output);
}

NOCDEF Noc_C_Ast_Operator noc_logical_c_ast_node_operator(
    const Noc_Logical_C_Ast *ast,
    size_t node_index)
{
    return noc_logical_c_ast_node_at(ast, node_index)
               ? ast->impl->details[node_index].operator_kind
               : NOC_C_AST_OPERATOR_NONE;
}

NOCDEF Noc_C_Ast_Specifier noc_logical_c_ast_node_specifier(
    const Noc_Logical_C_Ast *ast,
    size_t node_index)
{
    return noc_logical_c_ast_node_at(ast, node_index)
               ? ast->impl->details[node_index].specifier
               : NOC_C_AST_SPECIFIER_NONE;
}

NOCDEF Noc_C_Ast_Qualifier noc_logical_c_ast_node_qualifier(
    const Noc_Logical_C_Ast *ast,
    size_t node_index)
{
    return noc_logical_c_ast_node_at(ast, node_index)
               ? ast->impl->details[node_index].qualifier
               : NOC_C_AST_QUALIFIER_NONE;
}

NOCDEF bool noc_logical_c_ast_node_type_spelling(
    const Noc_Logical_C_Ast *ast,
    size_t node_index,
    Noc_C_Ast_Type_Spelling *output)
{
    const Noc_Logical_C_Ast_Node *node =
        noc_logical_c_ast_node_at(ast, node_index);
    if (!node || !output ||
        (node->kind != NOC_C_AST_KIND_PRIMITIVE_TYPE &&
         node->kind != NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER)) {
        return false;
    }
    *output = ast->impl->details[node_index].type_spelling;
    return true;
}

NOCDEF bool noc_logical_c_ast_node_array_detail(
    const Noc_Logical_C_Ast *ast,
    size_t node_index,
    Noc_C_Ast_Array_Detail *output)
{
    const Noc_Logical_C_Ast_Node *node =
        noc_logical_c_ast_node_at(ast, node_index);
    if (!node || !output ||
        (node->kind != NOC_C_AST_KIND_ARRAY_DECLARATOR &&
         node->kind != NOC_C_AST_KIND_ABSTRACT_ARRAY_DECLARATOR)) {
        return false;
    }
    *output = ast->impl->details[node_index].array_detail;
    return true;
}

NOCDEF Noc_C_Ast_Extension noc_logical_c_ast_node_extension(
    const Noc_Logical_C_Ast *ast,
    size_t node_index)
{
    return noc_logical_c_ast_node_at(ast, node_index)
               ? ast->impl->details[node_index].extension
               : NOC_C_AST_EXTENSION_NONE;
}

NOCDEF Noc_C_Ast_Expected noc_logical_c_ast_node_expected(
    const Noc_Logical_C_Ast *ast,
    size_t node_index)
{
    Noc_C_Ast_Expected expected = {0};
    const Noc_Logical_C_Ast_Node *node =
        noc_logical_c_ast_node_at(ast, node_index);
    if (node && (node->flags & NOC_C_AST_NODE_MISSING) != 0) {
        const Noc__C_Ast_Detail *detail = &ast->impl->details[node_index];
        expected.kind = detail->expected_kind;
        if (detail->expected_spelling) {
            expected.spelling.data = detail->expected_spelling;
            expected.spelling.count = strlen(detail->expected_spelling);
        }
    }
    return expected;
}

#endif /* NOC_LOGICAL_AST_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
