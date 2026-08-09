#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_LOGICAL_C_PARSE_IMPLEMENTATION_INCLUDED
#define NOC_LOGICAL_C_PARSE_IMPLEMENTATION_INCLUDED

/* This adapter deliberately owns only stable public grammar labels and a copy
   of the flat topology. The temporary physical-shaped parser core is destroyed
   after conversion, preventing engine-private state or physical byte types from
   leaking into the logical coordinate contract. */
struct Noc_Logical_C_Parse_Tree_Impl {
    Noc_Logical_Source source;
    Noc_Logical_C_Parse_Node *nodes;
    size_t node_count;
};

static void noc__logical_c_parse_impl_free(
    Noc_Logical_C_Parse_Tree_Impl *implementation)
{
    if (!implementation) return;
    free(implementation->nodes);
    noc_logical_source_free(&implementation->source);
    free(implementation);
}

static bool noc__logical_c_parse_should_cancel(
    const Noc_C_Parse_Options *options)
{
    return options->should_cancel &&
           options->should_cancel(options->cancel_user_data);
}

NOCDEF Noc_C_Parse_Status noc_logical_c_parse_tree_build(
    const Noc_Logical_Source *source,
    Noc_C_Parse_Options options,
    Noc_Logical_C_Parse_Tree *output)
{
    Noc_C_Parse_Tree_Impl *temporary = NULL;
    Noc_Logical_C_Parse_Tree_Impl *built = NULL;
    Noc_Logical_C_Parse_Tree_Impl *previous;
    Noc_Logical_Source retained = {0};
    Noc_Logical_Source_Status clone_status;
    Noc_C_Parse_Status status;
    Noc_Slice text;
    size_t node_count;
    size_t generation;
    size_t index;

    if (!output || options.max_source_bytes == 0 || options.max_nodes == 0 ||
        (output->impl && !noc_logical_c_parse_tree_is_valid(output))) {
        return NOC_C_PARSE_INVALID_ARGUMENT;
    }
    text = noc_logical_source_text(source);
    if (!text.data) return NOC_C_PARSE_INVALID_ARGUMENT;
    if (output->generation == SIZE_MAX) {
        return NOC_C_PARSE_GENERATION_EXHAUSTED;
    }
    generation = output->generation + 1;
    /* Retain before the parser's first cancellation callback. The caller may
       rebuild its handle from that callback without invalidating parser input. */
    clone_status = noc_logical_source_clone(source, &retained);
    if (clone_status != NOC_LOGICAL_SOURCE_OK) {
        return clone_status == NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED
                   ? NOC_C_PARSE_LIMIT_EXCEEDED
                   : NOC_C_PARSE_INVALID_ARGUMENT;
    }
    text = noc_logical_source_text(&retained);
    status = noc__c_parse_source_build(text,
                                       options,
                                       generation,
                                       &temporary);
    if (status != NOC_C_PARSE_OK) {
        noc_logical_source_free(&retained);
        return status;
    }

    node_count = noc__c_parse_impl_node_count(temporary);
    if (node_count == 0 ||
        node_count > SIZE_MAX / sizeof(*built->nodes)) {
        noc__c_parse_impl_free(temporary);
        noc_logical_source_free(&retained);
        return NOC_C_PARSE_LIMIT_EXCEEDED;
    }
    built = (Noc_Logical_C_Parse_Tree_Impl *)calloc(1, sizeof(*built));
    if (!built) {
        noc__c_parse_impl_free(temporary);
        noc_logical_source_free(&retained);
        return NOC_C_PARSE_OUT_OF_MEMORY;
    }
    built->nodes = (Noc_Logical_C_Parse_Node *)malloc(
        node_count * sizeof(*built->nodes));
    if (!built->nodes) {
        noc__c_parse_impl_free(temporary);
        noc_logical_source_free(&retained);
        noc__logical_c_parse_impl_free(built);
        return NOC_C_PARSE_OUT_OF_MEMORY;
    }
    built->node_count = node_count;

    for (index = 0; index < node_count; ++index) {
        const Noc_C_Parse_Node *source_node =
            noc__c_parse_impl_node_at(temporary, index);
        Noc_Logical_C_Parse_Node *node = &built->nodes[index];
        if (!source_node) {
            noc__c_parse_impl_free(temporary);
            noc_logical_source_free(&retained);
            noc__logical_c_parse_impl_free(built);
            return NOC_C_PARSE_ENGINE_FAILURE;
        }
        if ((index & 1023u) == 0 &&
            noc__logical_c_parse_should_cancel(&options)) {
            noc__c_parse_impl_free(temporary);
            noc_logical_source_free(&retained);
            noc__logical_c_parse_impl_free(built);
            return NOC_C_PARSE_CANCELLED;
        }
        node->bytes.begin = source_node->bytes.begin;
        node->bytes.end = source_node->bytes.end;
        node->parent = source_node->parent;
        node->first_child = source_node->first_child;
        node->last_child = source_node->last_child;
        node->next_sibling = source_node->next_sibling;
        node->child_count = source_node->child_count;
        node->kind = source_node->kind;
        node->field = source_node->field;
        node->generation = generation;
        node->flags = source_node->flags;
    }
    noc__c_parse_impl_free(temporary);
    /* Internal move: built owns the retained logical revision from here. */
    built->source = retained;
    memset(&retained, 0, sizeof(retained));
    if (noc__logical_c_parse_should_cancel(&options)) {
        noc__logical_c_parse_impl_free(built);
        return NOC_C_PARSE_CANCELLED;
    }

    previous = output->impl;
    output->impl = built;
    output->generation = generation;
    noc__logical_c_parse_impl_free(previous);
    return NOC_C_PARSE_OK;
}

NOCDEF void noc_logical_c_parse_tree_free(Noc_Logical_C_Parse_Tree *tree)
{
    Noc_Logical_C_Parse_Tree_Impl *implementation;
    if (!tree) return;
    implementation = tree->impl;
    memset(tree, 0, sizeof(*tree));
    noc__logical_c_parse_impl_free(implementation);
}

NOCDEF bool noc_logical_c_parse_tree_is_valid(
    const Noc_Logical_C_Parse_Tree *tree)
{
    const Noc_Logical_C_Parse_Node *root;
    Noc_Slice text;
    if (!tree || !tree->impl || tree->generation == 0 ||
        !tree->impl->nodes || tree->impl->node_count == 0) {
        return false;
    }
    root = &tree->impl->nodes[0];
    text = noc_logical_source_text(&tree->impl->source);
    return text.data && root->generation == tree->generation &&
           root->parent == NOC_C_PARSE_NODE_NONE && root->bytes.begin == 0 &&
           root->bytes.end == text.count;
}

NOCDEF size_t noc_logical_c_parse_tree_generation(
    const Noc_Logical_C_Parse_Tree *tree)
{
    return noc_logical_c_parse_tree_is_valid(tree) ? tree->generation : 0;
}

NOCDEF const Noc_Logical_Source *noc_logical_c_parse_tree_source(
    const Noc_Logical_C_Parse_Tree *tree)
{
    return noc_logical_c_parse_tree_is_valid(tree)
               ? &tree->impl->source
               : NULL;
}

NOCDEF size_t noc_logical_c_parse_tree_node_count(
    const Noc_Logical_C_Parse_Tree *tree)
{
    return noc_logical_c_parse_tree_is_valid(tree)
               ? tree->impl->node_count
               : 0;
}

NOCDEF size_t noc_logical_c_parse_tree_root(
    const Noc_Logical_C_Parse_Tree *tree)
{
    return noc_logical_c_parse_tree_is_valid(tree)
               ? 0
               : NOC_C_PARSE_NODE_NONE;
}

NOCDEF const Noc_Logical_C_Parse_Node *noc_logical_c_parse_tree_node_at(
    const Noc_Logical_C_Parse_Tree *tree,
    size_t node_index)
{
    if (!noc_logical_c_parse_tree_is_valid(tree) ||
        node_index >= tree->impl->node_count) {
        return NULL;
    }
    return &tree->impl->nodes[node_index];
}

NOCDEF bool noc_logical_c_parse_tree_has_error(
    const Noc_Logical_C_Parse_Tree *tree)
{
    const Noc_Logical_C_Parse_Node *root =
        noc_logical_c_parse_tree_node_at(tree, 0);
    return root &&
           (root->flags & (NOC_C_PARSE_NODE_HAS_ERROR |
                           NOC_C_PARSE_NODE_SKIPPED_SOURCE)) != 0;
}

NOCDEF Noc_Slice noc_logical_c_parse_node_source(
    const Noc_Logical_C_Parse_Tree *tree,
    size_t node_index)
{
    Noc_Slice result = {0};
    const Noc_Logical_C_Parse_Node *node =
        noc_logical_c_parse_tree_node_at(tree, node_index);
    Noc_Slice text;
    if (!node) return result;
    text = noc_logical_source_text(&tree->impl->source);
    if (node->bytes.begin > node->bytes.end || node->bytes.end > text.count) {
        return result;
    }
    result.data = text.data + node->bytes.begin;
    result.count = node->bytes.end - node->bytes.begin;
    return result;
}

NOCDEF Noc_Logical_Location noc_logical_c_parse_node_location(
    const Noc_Logical_C_Parse_Tree *tree,
    size_t node_index)
{
    Noc_Logical_Location location = {0};
    const Noc_Logical_C_Parse_Node *node =
        noc_logical_c_parse_tree_node_at(tree, node_index);
    if (node) {
        (void)noc_logical_source_location(&tree->impl->source,
                                          node->bytes.begin,
                                          &location);
    }
    return location;
}

NOCDEF bool noc_logical_c_parse_node_token_range(
    const Noc_Logical_C_Parse_Tree *tree,
    size_t node_index,
    Noc_Logical_Token_Range *output)
{
    const Noc_Logical_C_Parse_Node *node =
        noc_logical_c_parse_tree_node_at(tree, node_index);
    if (!node) return false;
    return noc_logical_source_token_range_for_bytes(&tree->impl->source,
                                                     node->bytes,
                                                     output);
}

#endif /* NOC_LOGICAL_C_PARSE_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
