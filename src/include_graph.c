#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_INCLUDE_GRAPH_IMPLEMENTATION_INCLUDED
#define NOC_INCLUDE_GRAPH_IMPLEMENTATION_INCLUDED

typedef struct {
    Noc_Include_Graph_Node view;
    Noc_Document_Snapshot snapshot;
    Noc_Preprocessor_Unit unit;
    Noc_Preprocessor_Conditional_Groups groups;
    size_t parent_node_index;
    size_t *outgoing_edges;
    size_t outgoing_edge_count;
    size_t outgoing_edge_capacity;
} Noc__Include_Graph_Node_Impl;

typedef struct {
    Noc_Include_Graph_Edge view;
    Noc_Include_Operand operand;
    Noc_Include_Expansion expansion;
} Noc__Include_Graph_Edge_Impl;

struct Noc_Include_Graph_Impl {
    Noc__Include_Graph_Node_Impl **nodes;
    size_t node_count;
    size_t node_capacity;
    Noc__Include_Graph_Edge_Impl **edges;
    size_t edge_count;
    size_t edge_capacity;
    size_t generation;
    unsigned int limit_flags;
};

typedef struct {
    Noc_Context *context;
    Noc_Include_Resolver resolver;
    Noc_Include_Graph_Options options;
    Noc_Include_Graph_Impl *graph;
} Noc__Include_Graph_Builder;

typedef struct {
    size_t node_index;
    size_t next_directive_index;
    size_t pending_child_index;
    bool macro_state_poisoned;
} Noc__Include_Graph_Frame;

NOCDEF const char *noc_include_graph_status_name(Noc_Include_Graph_Status status)
{
    switch (status) {
    case NOC_INCLUDE_GRAPH_OK: return "ok";
    case NOC_INCLUDE_GRAPH_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_INCLUDE_GRAPH_STALE: return "stale";
    case NOC_INCLUDE_GRAPH_CANCELLED: return "cancelled";
    case NOC_INCLUDE_GRAPH_GENERATION_EXHAUSTED:
        return "generation-exhausted";
    case NOC_INCLUDE_GRAPH_OUT_OF_MEMORY: return "out-of-memory";
    case NOC_INCLUDE_GRAPH_PREPROCESSOR_FAILED: return "preprocessor-failed";
    case NOC_INCLUDE_GRAPH_INVALID_RESULT: return "invalid-result";
    }
    return "unknown";
}

NOCDEF const char *noc_include_graph_edge_status_name(
    Noc_Include_Graph_Edge_Status status)
{
    switch (status) {
    case NOC_INCLUDE_GRAPH_EDGE_RESOLVED: return "resolved";
    case NOC_INCLUDE_GRAPH_EDGE_CYCLE: return "cycle";
    case NOC_INCLUDE_GRAPH_EDGE_INACTIVE: return "inactive";
    case NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_ACTIVITY: return "unknown-activity";
    case NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE:
        return "unknown-macro-state";
    case NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND: return "invalid-operand";
    case NOC_INCLUDE_GRAPH_EDGE_EXPANSION_FAILED: return "expansion-failed";
    case NOC_INCLUDE_GRAPH_EDGE_NOT_FOUND: return "not-found";
    case NOC_INCLUDE_GRAPH_EDGE_AMBIGUOUS: return "ambiguous";
    case NOC_INCLUDE_GRAPH_EDGE_DENIED: return "denied";
    case NOC_INCLUDE_GRAPH_EDGE_RESOLVER_FAILED: return "resolver-failed";
    case NOC_INCLUDE_GRAPH_EDGE_DEPTH_LIMIT: return "depth-limit";
    case NOC_INCLUDE_GRAPH_EDGE_NODE_LIMIT: return "node-limit";
    }
    return "unknown";
}

NOCDEF Noc_Include_Graph_Options noc_include_graph_default_options(void)
{
    Noc_Include_Graph_Options options;
    memset(&options, 0, sizeof(options));
    options.macro_policy = NOC_MACROS_TRUSTED_ONLY;
    options.macro_expansion_options = noc_macro_expansion_default_options();
    options.max_depth = 128;
    options.max_nodes = 1024;
    options.max_edges = 8192;
    return options;
}

static void noc__include_graph_edge_destroy(Noc__Include_Graph_Edge_Impl *edge)
{
    if (!edge) return;
    noc_include_expansion_free(&edge->expansion);
    noc_include_operand_free(&edge->operand);
    free(edge);
}

static void noc__include_graph_node_destroy(Noc__Include_Graph_Node_Impl *node)
{
    if (!node) return;
    free(node->outgoing_edges);
    noc_preprocessor_conditional_groups_free(&node->groups);
    noc_preprocessor_unit_free(&node->unit);
    noc_document_snapshot_free(&node->snapshot);
    free(node);
}

static void noc__include_graph_impl_destroy(Noc_Include_Graph_Impl *implementation)
{
    size_t index;
    if (!implementation) return;
    for (index = 0; index < implementation->edge_count; ++index) {
        noc__include_graph_edge_destroy(implementation->edges[index]);
    }
    index = implementation->node_count;
    while (index > 0) {
        noc__include_graph_node_destroy(implementation->nodes[--index]);
    }
    free(implementation->edges);
    free(implementation->nodes);
    free(implementation);
}

NOCDEF void noc_include_graph_free(Noc_Include_Graph *graph)
{
    size_t generation;
    if (!graph) return;
    generation = graph->generation;
    noc__include_graph_impl_destroy(graph->impl);
    memset(graph, 0, sizeof(*graph));
    graph->generation = generation;
}

static bool noc__include_graph_is_basic_valid(const Noc_Include_Graph *graph)
{
    return graph && graph->impl && graph->generation != 0 &&
           graph->impl->generation == graph->generation;
}

NOCDEF bool noc_include_graph_is_valid(const Noc_Include_Graph *graph)
{
    size_t node_index;
    size_t edge_index;
    if (!noc__include_graph_is_basic_valid(graph) ||
        graph->impl->node_count == 0 ||
        graph->impl->node_count > graph->impl->node_capacity ||
        graph->impl->edge_count > graph->impl->edge_capacity ||
        !graph->impl->nodes ||
        (graph->impl->edge_capacity == 0) != (graph->impl->edges == NULL) ||
        (graph->impl->limit_flags &
         ~(NOC_INCLUDE_GRAPH_LIMIT_DEPTH |
           NOC_INCLUDE_GRAPH_LIMIT_NODES |
           NOC_INCLUDE_GRAPH_LIMIT_EDGES)) != 0) {
        return false;
    }
    for (node_index = 0; node_index < graph->impl->node_count; ++node_index) {
        const Noc__Include_Graph_Node_Impl *node =
            graph->impl->nodes[node_index];
        size_t outgoing_index;
        if (!node || node->view.index != node_index ||
            !noc_document_snapshot_is_valid(&node->snapshot) ||
            !noc_preprocessor_unit_is_valid(&node->unit) ||
            !noc_preprocessor_conditional_groups_is_valid(&node->groups) ||
            node->groups.unit != &node->unit ||
            node->view.path != noc_document_snapshot_path(&node->snapshot) ||
            node->view.file_id != noc_document_snapshot_file_id(&node->snapshot) ||
            node->view.document_generation !=
                noc_document_snapshot_generation(&node->snapshot) ||
            node->view.source_class !=
                noc_document_snapshot_source_class(&node->snapshot) ||
            node->view.outgoing_edge_count != node->outgoing_edge_count ||
            node->outgoing_edge_count > node->outgoing_edge_capacity ||
            ((node->outgoing_edge_capacity == 0) !=
             (node->outgoing_edges == NULL)) ||
            (node_index == 0 && node->parent_node_index != NOC_TOKEN_INDEX_NONE) ||
            (node_index != 0 && node->parent_node_index >= node_index)) {
            return false;
        }
        for (outgoing_index = 0;
             outgoing_index < node->outgoing_edge_count;
             ++outgoing_index) {
            edge_index = node->outgoing_edges[outgoing_index];
            if (edge_index >= graph->impl->edge_count ||
                graph->impl->edges[edge_index]->view.source_node_index !=
                    node_index) {
                return false;
            }
        }
    }
    for (edge_index = 0; edge_index < graph->impl->edge_count; ++edge_index) {
        const Noc__Include_Graph_Edge_Impl *edge =
            graph->impl->edges[edge_index];
        Noc_Slice expected_name;
        Noc_Include_Form expected_form;
        if (!edge || edge->view.index != edge_index ||
            edge->view.source_node_index >= graph->impl->node_count ||
            edge->view.directive_index >=
                graph->impl->nodes[edge->view.source_node_index]->unit.count ||
            edge->view.status < NOC_INCLUDE_GRAPH_EDGE_RESOLVED ||
            edge->view.status > NOC_INCLUDE_GRAPH_EDGE_NODE_LIMIT ||
            edge->view.conditional_activity < NOC_PREPROCESSOR_ACTIVITY_UNKNOWN ||
            edge->view.conditional_activity > NOC_PREPROCESSOR_ACTIVITY_INACTIVE ||
            !noc_include_operand_is_valid(&edge->operand) ||
            edge->operand.directive_index != edge->view.directive_index ||
            (edge->view.target_node_index != NOC_TOKEN_INDEX_NONE &&
             edge->view.target_node_index >= graph->impl->node_count) ||
            (edge->view.macro_expanded &&
             !noc_include_expansion_is_valid(&edge->expansion))) {
            return false;
        }
        if (edge->view.macro_expanded) {
            expected_name = edge->expansion.logical_name;
            expected_form = edge->expansion.form;
        } else {
            expected_name = edge->operand.logical_name;
            expected_form = edge->operand.form;
        }
        if (edge->view.logical_name.data != expected_name.data ||
            edge->view.logical_name.count != expected_name.count ||
            edge->view.form != expected_form ||
            ((edge->view.status == NOC_INCLUDE_GRAPH_EDGE_RESOLVED ||
              edge->view.status == NOC_INCLUDE_GRAPH_EDGE_CYCLE) !=
             (edge->view.target_node_index != NOC_TOKEN_INDEX_NONE))) {
            return false;
        }
    }
    return true;
}

static bool noc__include_graph_cancelled(
    const Noc__Include_Graph_Builder *builder)
{
    return builder->options.should_cancel &&
           builder->options.should_cancel(builder->options.cancel_user_data);
}

static bool noc__include_graph_reserve_pointers(void ***items,
                                                size_t *capacity,
                                                size_t count)
{
    void **resized;
    size_t new_capacity;
    if (count < *capacity) return true;
    new_capacity = *capacity == 0 ? 8 : *capacity * 2;
    if (new_capacity < *capacity ||
        new_capacity > SIZE_MAX / sizeof(*resized)) {
        return false;
    }
    resized = (void **)realloc(*items, new_capacity * sizeof(*resized));
    if (!resized) return false;
    *items = resized;
    *capacity = new_capacity;
    return true;
}

static bool noc__include_graph_append_node(
    Noc_Include_Graph_Impl *graph,
    Noc__Include_Graph_Node_Impl *node)
{
    if (!noc__include_graph_reserve_pointers((void ***)&graph->nodes,
                                             &graph->node_capacity,
                                             graph->node_count)) {
        return false;
    }
    node->view.index = graph->node_count;
    graph->nodes[graph->node_count++] = node;
    return true;
}

static bool noc__include_graph_append_edge(
    Noc_Include_Graph_Impl *graph,
    Noc__Include_Graph_Node_Impl *node,
    Noc__Include_Graph_Edge_Impl *edge)
{
    size_t *resized;
    size_t capacity;
    if (!noc__include_graph_reserve_pointers((void ***)&graph->edges,
                                             &graph->edge_capacity,
                                             graph->edge_count)) {
        return false;
    }
    if (node->outgoing_edge_count == node->outgoing_edge_capacity) {
        capacity = node->outgoing_edge_capacity == 0 ?
                       8 : node->outgoing_edge_capacity * 2;
        if (capacity < node->outgoing_edge_capacity ||
            capacity > SIZE_MAX / sizeof(*resized)) {
            return false;
        }
        resized = (size_t *)realloc(node->outgoing_edges,
                                    capacity * sizeof(*resized));
        if (!resized) return false;
        node->outgoing_edges = resized;
        node->outgoing_edge_capacity = capacity;
    }
    edge->view.index = graph->edge_count;
    graph->edges[graph->edge_count] = edge;
    node->outgoing_edges[node->outgoing_edge_count++] = graph->edge_count;
    node->view.outgoing_edge_count = node->outgoing_edge_count;
    graph->edge_count += 1;
    return true;
}

static Noc_Include_Graph_Status noc__include_graph_groups_status(
    Noc_Conditional_Groups_Build_Status status)
{
    switch (status) {
    case NOC_CONDITIONAL_GROUPS_OK: return NOC_INCLUDE_GRAPH_OK;
    case NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT:
        return NOC_INCLUDE_GRAPH_INVALID_RESULT;
    case NOC_CONDITIONAL_GROUPS_STALE: return NOC_INCLUDE_GRAPH_STALE;
    case NOC_CONDITIONAL_GROUPS_GENERATION_EXHAUSTED:
        return NOC_INCLUDE_GRAPH_GENERATION_EXHAUSTED;
    case NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY:
        return NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
    }
    return NOC_INCLUDE_GRAPH_INVALID_RESULT;
}

static Noc_Include_Graph_Status noc__include_graph_create_node(
    Noc__Include_Graph_Builder *builder,
    Noc_Document_Snapshot *snapshot,
    const Noc_Macro_Environment *initial_environment,
    size_t initial_entry_limit,
    size_t parent_node_index,
    size_t depth,
    Noc__Include_Graph_Node_Impl **output)
{
    Noc__Include_Graph_Node_Impl *node;
    Noc_Conditional_Groups_Build_Status groups_status;
    node = (Noc__Include_Graph_Node_Impl *)calloc(1, sizeof(*node));
    if (!node) return NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
    node->snapshot = *snapshot;
    snapshot->impl = NULL;
    node->parent_node_index = parent_node_index;
    if (!noc_preprocessor_unit_build(builder->context,
                                     &node->snapshot,
                                     builder->options.macro_policy,
                                     &node->unit)) {
        noc__include_graph_node_destroy(node);
        return NOC_INCLUDE_GRAPH_PREPROCESSOR_FAILED;
    }
    groups_status = noc_preprocessor_conditional_groups_build_with_options(
        initial_environment,
        initial_entry_limit,
        &node->unit,
        builder->options.macro_expansion_options,
        &node->groups);
    if (groups_status != NOC_CONDITIONAL_GROUPS_OK) {
        Noc_Include_Graph_Status status =
            noc__include_graph_groups_status(groups_status);
        noc__include_graph_node_destroy(node);
        return status;
    }
    node->view.path = noc_document_snapshot_path(&node->snapshot);
    node->view.file_id = noc_document_snapshot_file_id(&node->snapshot);
    node->view.document_generation =
        noc_document_snapshot_generation(&node->snapshot);
    node->view.source_class =
        noc_document_snapshot_source_class(&node->snapshot);
    node->view.depth = depth;
    node->view.traversal_complete = true;
    *output = node;
    return NOC_INCLUDE_GRAPH_OK;
}

static bool noc__include_graph_node_has_local_macro_effect(
    const Noc__Include_Graph_Node_Impl *node)
{
    size_t index;
    for (index = 0; index < node->unit.macro_directive_count; ++index) {
        const Noc_Macro_Directive *macro = &node->unit.macro_directives[index];
        const Noc_Preprocessor_Directive *directive;
        Noc_Preprocessor_Activity activity;
        if (macro->status != NOC_MACRO_DIRECTIVE_STATUS_VALID ||
            macro->directive_index >= node->unit.count) {
            continue;
        }
        directive = &node->unit.items[macro->directive_index];
        if (!directive->macro_definition_allowed) continue;
        activity = noc_preprocessor_conditional_token_activity(
            &node->groups, directive->preprocessing_tokens.begin);
        if (activity != NOC_PREPROCESSOR_ACTIVITY_INACTIVE) return true;
    }
    return false;
}

static size_t noc__include_graph_ancestor(
    const Noc_Include_Graph_Impl *graph,
    size_t node_index,
    const Noc_Document_Snapshot *snapshot)
{
    while (node_index != NOC_TOKEN_INDEX_NONE) {
        const Noc__Include_Graph_Node_Impl *node = graph->nodes[node_index];
        if (node->snapshot.impl == snapshot->impl) {
            return node_index;
        }
        node_index = node->parent_node_index;
    }
    return NOC_TOKEN_INDEX_NONE;
}

static Noc_Include_Graph_Status noc__include_graph_expansion_failure(
    Noc_Macro_Expansion_Status status)
{
    if (status == NOC_MACRO_EXPANSION_STALE) {
        return NOC_INCLUDE_GRAPH_STALE;
    }
    if (status == NOC_MACRO_EXPANSION_GENERATION_EXHAUSTED) {
        return NOC_INCLUDE_GRAPH_GENERATION_EXHAUSTED;
    }
    if (status == NOC_MACRO_EXPANSION_OUT_OF_MEMORY) {
        return NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
    }
    if (status == NOC_MACRO_EXPANSION_INVALID_ARGUMENT) {
        return NOC_INCLUDE_GRAPH_INVALID_RESULT;
    }
    return NOC_INCLUDE_GRAPH_OK;
}

static bool noc__include_graph_push_frame(
    Noc__Include_Graph_Builder *builder,
    Noc__Include_Graph_Frame **frames,
    size_t *frame_count,
    size_t *frame_capacity,
    size_t node_index)
{
    Noc__Include_Graph_Frame *resized;
    Noc__Include_Graph_Frame frame;
    size_t capacity;
    if (*frame_count == *frame_capacity) {
        capacity = *frame_capacity == 0 ? 16 : *frame_capacity * 2;
        if (capacity < *frame_capacity ||
            capacity > SIZE_MAX / sizeof(*resized)) {
            return false;
        }
        resized = (Noc__Include_Graph_Frame *)realloc(
            *frames, capacity * sizeof(*resized));
        if (!resized) return false;
        *frames = resized;
        *frame_capacity = capacity;
    }
    memset(&frame, 0, sizeof(frame));
    frame.node_index = node_index;
    frame.pending_child_index = NOC_TOKEN_INDEX_NONE;
    (*frames)[(*frame_count)++] = frame;
    builder->graph->nodes[node_index]->view.may_mutate_macros =
        noc__include_graph_node_has_local_macro_effect(
            builder->graph->nodes[node_index]);
    return true;
}

static Noc_Include_Graph_Status noc__include_graph_process_node(
    Noc__Include_Graph_Builder *builder,
    size_t root_node_index)
{
    Noc__Include_Graph_Frame *frames = NULL;
    size_t frame_count = 0;
    size_t frame_capacity = 0;
    Noc_Include_Graph_Status status = NOC_INCLUDE_GRAPH_OK;
    if (!noc__include_graph_push_frame(builder,
                                       &frames,
                                       &frame_count,
                                       &frame_capacity,
                                       root_node_index)) {
        return NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
    }
    while (frame_count > 0) {
        Noc__Include_Graph_Frame *frame = &frames[frame_count - 1];
        Noc__Include_Graph_Node_Impl *node =
            builder->graph->nodes[frame->node_index];
        const Noc_Preprocessor_Directive *directive;
        Noc__Include_Graph_Edge_Impl *edge;
        Noc_Include_Operand_Build_Status operand_status;
        size_t directive_index;
        size_t entry_limit;
        if (frame->pending_child_index != NOC_TOKEN_INDEX_NONE) {
            Noc__Include_Graph_Node_Impl *child =
                builder->graph->nodes[frame->pending_child_index];
            if (child->view.may_mutate_macros) {
                frame->macro_state_poisoned = true;
                node->view.may_mutate_macros = true;
            }
            frame->pending_child_index = NOC_TOKEN_INDEX_NONE;
            continue;
        }
        while (frame->next_directive_index < node->unit.count &&
               node->unit.items[frame->next_directive_index].kind !=
                   NOC_PREPROCESSOR_DIRECTIVE_INCLUDE) {
            frame->next_directive_index += 1;
        }
        if (frame->next_directive_index == node->unit.count) {
            frame_count -= 1;
            continue;
        }
        directive_index = frame->next_directive_index++;
        directive = &node->unit.items[directive_index];
        if (noc__include_graph_cancelled(builder)) {
            status = NOC_INCLUDE_GRAPH_CANCELLED;
            goto done;
        }
        if (builder->graph->edge_count >= builder->options.max_edges) {
            builder->graph->limit_flags |= NOC_INCLUDE_GRAPH_LIMIT_EDGES;
            node->view.traversal_complete = false;
            node->view.may_mutate_macros = true;
            frame_count -= 1;
            continue;
        }
        edge = (Noc__Include_Graph_Edge_Impl *)calloc(1, sizeof(*edge));
        if (!edge) {
            status = NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
            goto done;
        }
        edge->view.source_node_index = frame->node_index;
        edge->view.target_node_index = NOC_TOKEN_INDEX_NONE;
        edge->view.directive_index = directive_index;
        edge->view.expansion_status = NOC_MACRO_EXPANSION_INVALID_ARGUMENT;
        edge->view.resolve_status = NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT;
        edge->view.conditional_activity =
            noc_preprocessor_conditional_token_activity(
                &node->groups, directive->preprocessing_tokens.begin);
        operand_status = noc_include_operand_build(&node->unit,
                                                   directive_index,
                                                   &edge->operand);
        if (operand_status != NOC_INCLUDE_OPERAND_BUILD_OK) {
            noc__include_graph_edge_destroy(edge);
            if (operand_status == NOC_INCLUDE_OPERAND_BUILD_STALE) {
                status = NOC_INCLUDE_GRAPH_STALE;
                goto done;
            }
            if (operand_status ==
                NOC_INCLUDE_OPERAND_BUILD_GENERATION_EXHAUSTED) {
                status = NOC_INCLUDE_GRAPH_GENERATION_EXHAUSTED;
                goto done;
            }
            if (operand_status == NOC_INCLUDE_OPERAND_BUILD_OUT_OF_MEMORY) {
                status = NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
                goto done;
            }
            status = NOC_INCLUDE_GRAPH_INVALID_RESULT;
            goto done;
        }
        edge->view.logical_name = edge->operand.logical_name;
        edge->view.form = edge->operand.form;
        if (!noc__include_graph_append_edge(builder->graph, node, edge)) {
            noc__include_graph_edge_destroy(edge);
            status = NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
            goto done;
        }
        if (frame->macro_state_poisoned) {
            edge->view.conditional_activity =
                NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
            edge->view.status = NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE;
            node->view.may_mutate_macros = true;
            continue;
        }
        if (edge->view.conditional_activity ==
            NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
            edge->view.status = NOC_INCLUDE_GRAPH_EDGE_INACTIVE;
            continue;
        }
        if (edge->view.conditional_activity ==
            NOC_PREPROCESSOR_ACTIVITY_UNKNOWN) {
            edge->view.status = NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_ACTIVITY;
            frame->macro_state_poisoned = true;
            node->view.may_mutate_macros = true;
            continue;
        }
        entry_limit = noc_preprocessor_conditional_token_macro_entry_limit(
            &node->groups, directive->preprocessing_tokens.begin);
        if (entry_limit == NOC_TOKEN_INDEX_NONE) {
            edge->view.status = NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE;
            node->view.may_mutate_macros = true;
            continue;
        }
        if (edge->operand.status == NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED) {
            Noc_Include_Graph_Status expansion_failure;
            edge->view.expansion_status =
                noc_include_expansion_build_with_options(
                    &node->groups.environment,
                    entry_limit,
                    &edge->operand,
                    builder->options.macro_expansion_options,
                    &edge->expansion);
            expansion_failure = noc__include_graph_expansion_failure(
                edge->view.expansion_status);
            if (expansion_failure != NOC_INCLUDE_GRAPH_OK) {
                status = expansion_failure;
                goto done;
            }
            if (edge->view.expansion_status != NOC_MACRO_EXPANSION_OK) {
                edge->view.status = NOC_INCLUDE_GRAPH_EDGE_EXPANSION_FAILED;
                continue;
            }
            edge->view.macro_expanded = true;
            edge->view.logical_name = edge->expansion.logical_name;
            edge->view.form = edge->expansion.form;
            if (edge->expansion.status != NOC_INCLUDE_OPERAND_DIRECT) {
                edge->view.status = NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND;
                continue;
            }
        } else if (edge->operand.status != NOC_INCLUDE_OPERAND_DIRECT) {
            edge->view.status = NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND;
            continue;
        }
        {
            Noc_Document_Snapshot target = {0};
            Noc_Include_Resolve_Status resolve_status;
            size_t ancestor;
            Noc__Include_Graph_Node_Impl *child;
            Noc_Include_Graph_Status child_status;
            if (edge->view.macro_expanded) {
                resolve_status = noc_include_expansion_resolve(
                    builder->resolver, &edge->expansion, &target);
            } else {
                resolve_status = noc_include_resolve(builder->resolver,
                                                     &edge->operand,
                                                     &target);
            }
            edge->view.resolve_status = resolve_status;
            if (resolve_status != NOC_INCLUDE_RESOLVE_FOUND) {
                if (resolve_status == NOC_INCLUDE_RESOLVE_NOT_FOUND) {
                    edge->view.status = NOC_INCLUDE_GRAPH_EDGE_NOT_FOUND;
                } else if (resolve_status == NOC_INCLUDE_RESOLVE_AMBIGUOUS) {
                    edge->view.status = NOC_INCLUDE_GRAPH_EDGE_AMBIGUOUS;
                } else if (resolve_status == NOC_INCLUDE_RESOLVE_DENIED) {
                    edge->view.status = NOC_INCLUDE_GRAPH_EDGE_DENIED;
                } else if (resolve_status == NOC_INCLUDE_RESOLVE_FAILED) {
                    edge->view.status = NOC_INCLUDE_GRAPH_EDGE_RESOLVER_FAILED;
                } else if (resolve_status == NOC_INCLUDE_RESOLVE_CANCELLED) {
                    status = NOC_INCLUDE_GRAPH_CANCELLED;
                    goto done;
                } else if (resolve_status ==
                           NOC_INCLUDE_RESOLVE_OUT_OF_MEMORY) {
                    status = NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
                    goto done;
                } else if (resolve_status == NOC_INCLUDE_RESOLVE_STALE) {
                    status = NOC_INCLUDE_GRAPH_STALE;
                    goto done;
                } else {
                    status = NOC_INCLUDE_GRAPH_INVALID_RESULT;
                    goto done;
                }
                continue;
            }
            ancestor = noc__include_graph_ancestor(builder->graph,
                                                   frame->node_index,
                                                   &target);
            if (ancestor != NOC_TOKEN_INDEX_NONE) {
                edge->view.target_node_index = ancestor;
                edge->view.status = NOC_INCLUDE_GRAPH_EDGE_CYCLE;
                node->view.may_mutate_macros = true;
                frame->macro_state_poisoned = true;
                noc_document_snapshot_free(&target);
                continue;
            }
            if (node->view.depth >= builder->options.max_depth) {
                edge->view.status = NOC_INCLUDE_GRAPH_EDGE_DEPTH_LIMIT;
                builder->graph->limit_flags |= NOC_INCLUDE_GRAPH_LIMIT_DEPTH;
                node->view.traversal_complete = false;
                node->view.may_mutate_macros = true;
                frame->macro_state_poisoned = true;
                noc_document_snapshot_free(&target);
                continue;
            }
            if (builder->graph->node_count >= builder->options.max_nodes) {
                edge->view.status = NOC_INCLUDE_GRAPH_EDGE_NODE_LIMIT;
                builder->graph->limit_flags |= NOC_INCLUDE_GRAPH_LIMIT_NODES;
                node->view.traversal_complete = false;
                node->view.may_mutate_macros = true;
                frame->macro_state_poisoned = true;
                noc_document_snapshot_free(&target);
                continue;
            }
            child_status = noc__include_graph_create_node(
                builder,
                &target,
                &node->groups.environment,
                entry_limit,
                frame->node_index,
                node->view.depth + 1,
                &child);
            noc_document_snapshot_free(&target);
            if (child_status != NOC_INCLUDE_GRAPH_OK) {
                status = child_status;
                goto done;
            }
            if (!noc__include_graph_append_node(builder->graph, child)) {
                noc__include_graph_node_destroy(child);
                status = NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
                goto done;
            }
            edge->view.target_node_index = child->view.index;
            edge->view.status = NOC_INCLUDE_GRAPH_EDGE_RESOLVED;
            frame->pending_child_index = child->view.index;
            if (!noc__include_graph_push_frame(builder,
                                               &frames,
                                               &frame_count,
                                               &frame_capacity,
                                               child->view.index)) {
                status = NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
                goto done;
            }
        }
    }

done:
    free(frames);
    return status;
}

static bool noc__include_graph_options_are_valid(
    Noc_Include_Graph_Options options)
{
    return options.macro_policy >= NOC_MACROS_DISABLED &&
           options.macro_policy <= NOC_MACROS_FULL &&
           noc__macro_expansion_options_are_valid(
               options.macro_expansion_options) &&
           options.max_depth > 0 && options.max_nodes > 0 &&
           options.max_edges > 0;
}

static bool noc__include_graph_prefix_borrows_output(
    const Noc_Include_Graph *output,
    const Noc_Macro_Environment *environment,
    size_t entry_limit)
{
    size_t entry_index;
    if (entry_limit == 0 || !noc__include_graph_is_basic_valid(output)) {
        return false;
    }
    for (entry_index = 0; entry_index < entry_limit; ++entry_index) {
        size_t node_index;
        for (node_index = 0;
             node_index < output->impl->node_count;
             ++node_index) {
            if (environment->items[entry_index].unit ==
                &output->impl->nodes[node_index]->unit) {
                return true;
            }
        }
    }
    return false;
}

NOCDEF Noc_Include_Graph_Status noc_include_graph_build(
    Noc_Context *context,
    const Noc_Document_Snapshot *root_snapshot,
    const Noc_Macro_Environment *initial_environment,
    size_t initial_entry_limit,
    Noc_Include_Resolver resolver,
    Noc_Include_Graph_Options options,
    Noc_Include_Graph *output)
{
    Noc_Macro_Environment empty_environment = {0};
    Noc_Document_Snapshot root = {0};
    Noc_Include_Graph_Impl *implementation;
    Noc__Include_Graph_Node_Impl *root_node;
    Noc__Include_Graph_Builder builder;
    Noc_Include_Graph_Status status;
    size_t generation;
    if (!context || !root_snapshot || !resolver.resolve || !output ||
        (!initial_environment && initial_entry_limit != 0) ||
        !noc__include_graph_options_are_valid(options)) {
        return NOC_INCLUDE_GRAPH_INVALID_ARGUMENT;
    }
    if (!noc_document_snapshot_is_valid(root_snapshot)) {
        return NOC_INCLUDE_GRAPH_STALE;
    }
    if (!initial_environment) initial_environment = &empty_environment;
    if (initial_entry_limit > initial_environment->count) {
        return NOC_INCLUDE_GRAPH_INVALID_ARGUMENT;
    }
    if (!noc_macro_environment_is_valid(initial_environment)) {
        return NOC_INCLUDE_GRAPH_STALE;
    }
    if (noc__include_graph_prefix_borrows_output(output,
                                                 initial_environment,
                                                 initial_entry_limit)) {
        return NOC_INCLUDE_GRAPH_INVALID_ARGUMENT;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_INCLUDE_GRAPH_GENERATION_EXHAUSTED;
    }
    memset(&builder, 0, sizeof(builder));
    builder.context = context;
    builder.resolver = resolver;
    builder.options = options;
    if (noc__include_graph_cancelled(&builder)) {
        return NOC_INCLUDE_GRAPH_CANCELLED;
    }
    implementation = (Noc_Include_Graph_Impl *)calloc(1,
                                                       sizeof(*implementation));
    if (!implementation) return NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
    builder.graph = implementation;
    if (noc_document_snapshot_clone(root_snapshot, &root) != NOC_WORKSPACE_OK) {
        noc__include_graph_impl_destroy(implementation);
        return NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
    }
    status = noc__include_graph_create_node(&builder,
                                            &root,
                                            initial_environment,
                                            initial_entry_limit,
                                            NOC_TOKEN_INDEX_NONE,
                                            0,
                                            &root_node);
    noc_document_snapshot_free(&root);
    if (status != NOC_INCLUDE_GRAPH_OK) {
        noc__include_graph_impl_destroy(implementation);
        return status;
    }
    if (!noc__include_graph_append_node(implementation, root_node)) {
        noc__include_graph_node_destroy(root_node);
        noc__include_graph_impl_destroy(implementation);
        return NOC_INCLUDE_GRAPH_OUT_OF_MEMORY;
    }
    status = noc__include_graph_process_node(&builder, 0);
    if (status != NOC_INCLUDE_GRAPH_OK) {
        noc__include_graph_impl_destroy(implementation);
        return status;
    }
    generation = output->generation + 1;
    implementation->generation = generation;
    noc_include_graph_free(output);
    output->impl = implementation;
    output->generation = generation;
    return NOC_INCLUDE_GRAPH_OK;
}

NOCDEF size_t noc_include_graph_node_count(const Noc_Include_Graph *graph)
{
    return noc__include_graph_is_basic_valid(graph) ? graph->impl->node_count : 0;
}

NOCDEF size_t noc_include_graph_edge_count(const Noc_Include_Graph *graph)
{
    return noc__include_graph_is_basic_valid(graph) ? graph->impl->edge_count : 0;
}

NOCDEF unsigned int noc_include_graph_limit_flags(
    const Noc_Include_Graph *graph)
{
    return noc__include_graph_is_basic_valid(graph) ?
               graph->impl->limit_flags : NOC_INCLUDE_GRAPH_LIMIT_NONE;
}

NOCDEF const Noc_Include_Graph_Node *noc_include_graph_node_at(
    const Noc_Include_Graph *graph,
    size_t node_index)
{
    if (!noc__include_graph_is_basic_valid(graph) ||
        node_index >= graph->impl->node_count) {
        return NULL;
    }
    return &graph->impl->nodes[node_index]->view;
}

NOCDEF const Noc_Include_Graph_Edge *noc_include_graph_edge_at(
    const Noc_Include_Graph *graph,
    size_t edge_index)
{
    if (!noc__include_graph_is_basic_valid(graph) ||
        edge_index >= graph->impl->edge_count) {
        return NULL;
    }
    return &graph->impl->edges[edge_index]->view;
}

NOCDEF const Noc_Include_Graph_Edge *noc_include_graph_node_edge_at(
    const Noc_Include_Graph *graph,
    size_t node_index,
    size_t outgoing_edge_index)
{
    const Noc__Include_Graph_Node_Impl *node;
    if (!noc__include_graph_is_basic_valid(graph) ||
        node_index >= graph->impl->node_count) {
        return NULL;
    }
    node = graph->impl->nodes[node_index];
    if (outgoing_edge_index >= node->outgoing_edge_count) return NULL;
    return &graph->impl->edges[
        node->outgoing_edges[outgoing_edge_index]]->view;
}

NOCDEF const Noc_Document_Snapshot *noc_include_graph_node_snapshot(
    const Noc_Include_Graph *graph,
    size_t node_index)
{
    if (!noc__include_graph_is_basic_valid(graph) ||
        node_index >= graph->impl->node_count) {
        return NULL;
    }
    return &graph->impl->nodes[node_index]->snapshot;
}

NOCDEF const Noc_Preprocessor_Unit *noc_include_graph_node_preprocessor_unit(
    const Noc_Include_Graph *graph,
    size_t node_index)
{
    if (!noc__include_graph_is_basic_valid(graph) ||
        node_index >= graph->impl->node_count) {
        return NULL;
    }
    return &graph->impl->nodes[node_index]->unit;
}

NOCDEF const Noc_Preprocessor_Conditional_Groups *
noc_include_graph_node_conditional_groups(const Noc_Include_Graph *graph,
                                          size_t node_index)
{
    if (!noc__include_graph_is_basic_valid(graph) ||
        node_index >= graph->impl->node_count) {
        return NULL;
    }
    return &graph->impl->nodes[node_index]->groups;
}

NOCDEF const Noc_Include_Operand *noc_include_graph_edge_operand(
    const Noc_Include_Graph *graph,
    size_t edge_index)
{
    if (!noc__include_graph_is_basic_valid(graph) ||
        edge_index >= graph->impl->edge_count) {
        return NULL;
    }
    return &graph->impl->edges[edge_index]->operand;
}

NOCDEF const Noc_Include_Expansion *noc_include_graph_edge_expansion(
    const Noc_Include_Graph *graph,
    size_t edge_index)
{
    if (!noc__include_graph_is_basic_valid(graph) ||
        edge_index >= graph->impl->edge_count ||
        !graph->impl->edges[edge_index]->view.macro_expanded) {
        return NULL;
    }
    return &graph->impl->edges[edge_index]->expansion;
}

#endif /* NOC_INCLUDE_GRAPH_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
