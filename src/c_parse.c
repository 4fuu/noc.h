#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_C_PARSE_IMPLEMENTATION_INCLUDED
#define NOC_C_PARSE_IMPLEMENTATION_INCLUDED

#ifndef NOC__VENDOR_TREE_SITTER_DECLARATIONS_INCLUDED
#include "../third_party/tree-sitter/tree_sitter_private.h"
#endif

enum {
    NOC__C_PARSE_DEFAULT_MAX_SOURCE_BYTES = 16 * 1024 * 1024,
    NOC__C_PARSE_DEFAULT_MAX_NODES = 1024 * 1024,
};

typedef struct {
    Noc__Vendor_TSNode node;
    size_t output_index;
    uint32_t next_child;
    uint32_t child_count;
} Noc__C_Parse_Frame;

typedef struct {
    Noc_Slice source;
} Noc__C_Parse_Input;

typedef struct {
    Noc_C_Parse_Cancel_Fn should_cancel;
    void *user_data;
    bool cancelled;
} Noc__C_Parse_Control;

struct Noc_C_Parse_Tree_Impl {
    Noc_Document_Snapshot snapshot;
    Noc__Vendor_TSTree *engine_tree;
    Noc_C_Parse_Node *nodes;
    size_t nodes_count;
    size_t nodes_capacity;
};

static void noc__c_parse_impl_free(Noc_C_Parse_Tree_Impl *implementation)
{
    if (!implementation) return;
    free(implementation->nodes);
    noc__vendor_ts_tree_delete(implementation->engine_tree);
    noc_document_snapshot_free(&implementation->snapshot);
    free(implementation);
}

static bool noc__c_parse_should_cancel(Noc__C_Parse_Control *control)
{
    if (!control->should_cancel ||
        !control->should_cancel(control->user_data)) {
        return false;
    }
    control->cancelled = true;
    return true;
}

static bool noc__c_parse_progress(Noc__Vendor_TSParseState *state)
{
    Noc__C_Parse_Control *control = (Noc__C_Parse_Control *)state->payload;
    return noc__c_parse_should_cancel(control);
}

static const char *noc__c_parse_read(void *payload,
                                     uint32_t byte_index,
                                     Noc__Vendor_TSPoint point,
                                     uint32_t *bytes_read)
{
    const Noc__C_Parse_Input *input = (const Noc__C_Parse_Input *)payload;
    size_t remaining;
    (void)point;
    if ((size_t)byte_index >= input->source.count) {
        *bytes_read = 0;
        return input->source.data + input->source.count;
    }
    remaining = input->source.count - (size_t)byte_index;
    *bytes_read = (uint32_t)remaining;
    return input->source.data + byte_index;
}

static Noc_C_Parse_Status noc__c_parse_nodes_reserve(
    Noc_C_Parse_Tree_Impl *implementation,
    size_t max_nodes)
{
    Noc_C_Parse_Node *nodes;
    size_t capacity;
    if (implementation->nodes_count < implementation->nodes_capacity) {
        return NOC_C_PARSE_OK;
    }
    if (implementation->nodes_count >= max_nodes) {
        return NOC_C_PARSE_LIMIT_EXCEEDED;
    }
    if (implementation->nodes_capacity == 0) {
        capacity = max_nodes < 256 ? max_nodes : 256;
    } else if (implementation->nodes_capacity > max_nodes / 2) {
        capacity = max_nodes;
    } else {
        capacity = implementation->nodes_capacity * 2;
    }
    if (capacity <= implementation->nodes_capacity ||
        capacity > SIZE_MAX / sizeof(*nodes)) {
        return NOC_C_PARSE_LIMIT_EXCEEDED;
    }
    nodes = (Noc_C_Parse_Node *)realloc(
        implementation->nodes,
        capacity * sizeof(*nodes));
    if (!nodes) return NOC_C_PARSE_OUT_OF_MEMORY;
    implementation->nodes = nodes;
    implementation->nodes_capacity = capacity;
    return NOC_C_PARSE_OK;
}

static Noc_C_Parse_Status noc__c_parse_append_node(
    Noc_C_Parse_Tree_Impl *implementation,
    size_t max_nodes,
    Noc_C_Parse_Node node,
    size_t *node_index)
{
    Noc_C_Parse_Status status = noc__c_parse_nodes_reserve(implementation,
                                                           max_nodes);
    if (status != NOC_C_PARSE_OK) return status;
    *node_index = implementation->nodes_count;
    implementation->nodes[implementation->nodes_count++] = node;
    return NOC_C_PARSE_OK;
}

static Noc_C_Parse_Status noc__c_parse_frames_append(
    Noc__C_Parse_Frame **frames,
    size_t *count,
    size_t *capacity,
    size_t max_nodes,
    Noc__C_Parse_Frame frame)
{
    Noc__C_Parse_Frame *items;
    size_t new_capacity;
    if (*count == *capacity) {
        if (*count >= max_nodes) return NOC_C_PARSE_LIMIT_EXCEEDED;
        if (*capacity == 0) {
            new_capacity = max_nodes < 64 ? max_nodes : 64;
        } else if (*capacity > max_nodes / 2) {
            new_capacity = max_nodes;
        } else {
            new_capacity = *capacity * 2;
        }
        if (new_capacity <= *capacity ||
            new_capacity > SIZE_MAX / sizeof(*items)) {
            return NOC_C_PARSE_LIMIT_EXCEEDED;
        }
        items = (Noc__C_Parse_Frame *)realloc(
            *frames,
            new_capacity * sizeof(*items));
        if (!items) return NOC_C_PARSE_OUT_OF_MEMORY;
        *frames = items;
        *capacity = new_capacity;
    }
    (*frames)[(*count)++] = frame;
    return NOC_C_PARSE_OK;
}

static Noc_C_Parse_Status noc__c_parse_make_node(
    Noc__Vendor_TSNode engine_node,
    const char *field,
    size_t parent,
    size_t source_count,
    size_t generation,
    Noc_C_Parse_Node *node)
{
    const char *kind;
    size_t begin;
    size_t end;
    memset(node, 0, sizeof(*node));
    if (noc__vendor_ts_node_is_null(engine_node)) {
        return NOC_C_PARSE_ENGINE_FAILURE;
    }
    kind = noc__vendor_ts_node_type(engine_node);
    if (!kind) return NOC_C_PARSE_ENGINE_FAILURE;
    begin = (size_t)noc__vendor_ts_node_start_byte(engine_node);
    end = (size_t)noc__vendor_ts_node_end_byte(engine_node);
    if (begin > end || end > source_count) return NOC_C_PARSE_ENGINE_FAILURE;
    node->bytes.begin = begin;
    node->bytes.end = end;
    node->parent = parent;
    node->first_child = NOC_C_PARSE_NODE_NONE;
    node->last_child = NOC_C_PARSE_NODE_NONE;
    node->next_sibling = NOC_C_PARSE_NODE_NONE;
    node->kind.data = kind;
    node->kind.count = strlen(kind);
    if (field) {
        node->field.data = field;
        node->field.count = strlen(field);
    }
    node->generation = generation;
    if (noc__vendor_ts_node_is_named(engine_node)) {
        node->flags |= NOC_C_PARSE_NODE_NAMED;
    }
    if (noc__vendor_ts_node_is_extra(engine_node)) {
        node->flags |= NOC_C_PARSE_NODE_EXTRA;
    }
    if (noc__vendor_ts_node_is_error(engine_node)) {
        node->flags |= NOC_C_PARSE_NODE_ERROR;
    }
    if (noc__vendor_ts_node_is_missing(engine_node)) {
        node->flags |= NOC_C_PARSE_NODE_MISSING;
    }
    if (noc__vendor_ts_node_has_error(engine_node)) {
        node->flags |= NOC_C_PARSE_NODE_HAS_ERROR;
    }
    return NOC_C_PARSE_OK;
}

static Noc_C_Parse_Status noc__c_parse_flatten(
    Noc_C_Parse_Tree_Impl *implementation,
    size_t generation,
    size_t max_nodes,
    Noc__C_Parse_Control *control)
{
    Noc__C_Parse_Frame *frames = NULL;
    size_t frames_count = 0;
    size_t frames_capacity = 0;
    Noc__Vendor_TSNode root =
        noc__vendor_ts_tree_root_node(implementation->engine_tree);
    Noc__C_Parse_Frame root_frame;
    Noc_C_Parse_Node root_node;
    Noc_C_Parse_Status status;
    size_t root_index;

    if (noc__c_parse_should_cancel(control)) return NOC_C_PARSE_CANCELLED;
    status = noc__c_parse_make_node(root,
                                    NULL,
                                    NOC_C_PARSE_NODE_NONE,
                                    noc_document_snapshot_source(
                                        &implementation->snapshot).count,
                                    generation,
                                    &root_node);
    if (status != NOC_C_PARSE_OK) return status;
    /* Tree-sitter can exclude an unrecognized leading or trailing byte from
       its root range while still publishing a recoverable tree. Noc's root is
       the physical document view, so retain those skipped bytes at the root;
       grammar children keep their exact engine ranges and error flags. */
    if (root_node.bytes.begin != 0 ||
        root_node.bytes.end != noc_document_snapshot_source(
                                   &implementation->snapshot).count) {
        root_node.flags |= NOC_C_PARSE_NODE_SKIPPED_SOURCE;
    }
    root_node.bytes.begin = 0;
    root_node.bytes.end = noc_document_snapshot_source(
                              &implementation->snapshot).count;
    status = noc__c_parse_append_node(implementation,
                                      max_nodes,
                                      root_node,
                                      &root_index);
    if (status != NOC_C_PARSE_OK) return status;
    root_frame.node = root;
    root_frame.output_index = root_index;
    root_frame.next_child = 0;
    root_frame.child_count = noc__vendor_ts_node_child_count(root);
    status = noc__c_parse_frames_append(&frames,
                                        &frames_count,
                                        &frames_capacity,
                                        max_nodes,
                                        root_frame);

    while (status == NOC_C_PARSE_OK && frames_count != 0) {
        Noc__C_Parse_Frame *frame = &frames[frames_count - 1];
        if (frame->next_child == frame->child_count) {
            frames_count -= 1;
            continue;
        }
        {
            uint32_t child_number = frame->next_child++;
            size_t parent_index = frame->output_index;
            Noc__Vendor_TSNode child =
                noc__vendor_ts_node_child(frame->node, child_number);
            const char *field = noc__vendor_ts_node_field_name_for_child(
                frame->node,
                child_number);
            Noc_C_Parse_Node child_node;
            Noc__C_Parse_Frame child_frame;
            size_t child_index;
            size_t previous_child;

            status = noc__c_parse_make_node(
                child,
                field,
                parent_index,
                noc_document_snapshot_source(&implementation->snapshot).count,
                generation,
                &child_node);
            if (status != NOC_C_PARSE_OK) break;
            status = noc__c_parse_append_node(implementation,
                                              max_nodes,
                                              child_node,
                                              &child_index);
            if (status != NOC_C_PARSE_OK) break;
            previous_child = implementation->nodes[parent_index].last_child;
            if (previous_child == NOC_C_PARSE_NODE_NONE) {
                implementation->nodes[parent_index].first_child = child_index;
            } else {
                implementation->nodes[previous_child].next_sibling = child_index;
            }
            implementation->nodes[parent_index].last_child = child_index;
            implementation->nodes[parent_index].child_count += 1;

            child_frame.node = child;
            child_frame.output_index = child_index;
            child_frame.next_child = 0;
            child_frame.child_count = noc__vendor_ts_node_child_count(child);
            status = noc__c_parse_frames_append(&frames,
                                                &frames_count,
                                                &frames_capacity,
                                                max_nodes,
                                                child_frame);
            if (status == NOC_C_PARSE_OK &&
                (implementation->nodes_count & 1023u) == 0 &&
                noc__c_parse_should_cancel(control)) {
                status = NOC_C_PARSE_CANCELLED;
            }
        }
    }
    free(frames);
    return status;
}

NOCDEF Noc_C_Parse_Options noc_c_parse_default_options(void)
{
    Noc_C_Parse_Options options;
    options.max_source_bytes = NOC__C_PARSE_DEFAULT_MAX_SOURCE_BYTES;
    options.max_nodes = NOC__C_PARSE_DEFAULT_MAX_NODES;
    options.should_cancel = NULL;
    options.cancel_user_data = NULL;
    return options;
}

NOCDEF const char *noc_c_parse_status_name(Noc_C_Parse_Status status)
{
    switch (status) {
    case NOC_C_PARSE_OK: return "ok";
    case NOC_C_PARSE_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_C_PARSE_CANCELLED: return "cancelled";
    case NOC_C_PARSE_LIMIT_EXCEEDED: return "limit-exceeded";
    case NOC_C_PARSE_GENERATION_EXHAUSTED: return "generation-exhausted";
    case NOC_C_PARSE_OUT_OF_MEMORY: return "out-of-memory";
    case NOC_C_PARSE_ENGINE_FAILURE: return "engine-failure";
    }
    return "unknown";
}

NOCDEF Noc_C_Parse_Status noc_c_parse_tree_build(
    const Noc_Document_Snapshot *snapshot,
    Noc_C_Parse_Options options,
    Noc_C_Parse_Tree *output)
{
    Noc_C_Parse_Tree_Impl *parsed;
    Noc_C_Parse_Tree_Impl *previous;
    Noc__Vendor_TSParser *parser;
    Noc__Vendor_TSInput input;
    Noc__Vendor_TSParseOptions parse_options;
    Noc__C_Parse_Input input_payload;
    Noc__C_Parse_Control control;
    Noc_C_Parse_Status status;
    Noc_Workspace_Status snapshot_status;
    Noc_Slice source;
    size_t generation;

    if (!noc_document_snapshot_is_valid(snapshot) || !output ||
        options.max_source_bytes == 0 || options.max_nodes == 0 ||
        (output->impl && !noc_c_parse_tree_is_valid(output))) {
        return NOC_C_PARSE_INVALID_ARGUMENT;
    }
    source = noc_document_snapshot_source(snapshot);
    if (source.count > options.max_source_bytes || source.count > UINT32_MAX) {
        return NOC_C_PARSE_LIMIT_EXCEEDED;
    }
    if (output->generation == SIZE_MAX) {
        return NOC_C_PARSE_GENERATION_EXHAUSTED;
    }
    generation = output->generation + 1;
    control.should_cancel = options.should_cancel;
    control.user_data = options.cancel_user_data;
    control.cancelled = false;
    if (noc__c_parse_should_cancel(&control)) return NOC_C_PARSE_CANCELLED;

    parsed = (Noc_C_Parse_Tree_Impl *)calloc(1, sizeof(*parsed));
    if (!parsed) return NOC_C_PARSE_OUT_OF_MEMORY;
    snapshot_status = noc_document_snapshot_clone(snapshot, &parsed->snapshot);
    if (snapshot_status != NOC_WORKSPACE_OK) {
        noc__c_parse_impl_free(parsed);
        return snapshot_status == NOC_WORKSPACE_LIMIT_EXCEEDED
                   ? NOC_C_PARSE_LIMIT_EXCEEDED
                   : NOC_C_PARSE_INVALID_ARGUMENT;
    }

    parser = noc__vendor_ts_parser_new();
    if (!parser) {
        noc__c_parse_impl_free(parsed);
        return NOC_C_PARSE_ENGINE_FAILURE;
    }
    if (!noc__vendor_ts_parser_set_language(
            parser,
            noc__vendor_tree_sitter_c())) {
        noc__vendor_ts_parser_delete(parser);
        noc__c_parse_impl_free(parsed);
        return NOC_C_PARSE_ENGINE_FAILURE;
    }
    input_payload.source = source;
    input.payload = &input_payload;
    input.read = noc__c_parse_read;
    input.encoding = Noc__Vendor_TSInputEncodingUTF8;
    input.decode = NULL;
    parse_options.payload = &control;
    parse_options.progress_callback = noc__c_parse_progress;
    parsed->engine_tree = noc__vendor_ts_parser_parse_with_options(parser,
                                                                   NULL,
                                                                   input,
                                                                   parse_options);
    noc__vendor_ts_parser_delete(parser);
    if (!parsed->engine_tree) {
        status = control.cancelled
                     ? NOC_C_PARSE_CANCELLED
                     : NOC_C_PARSE_ENGINE_FAILURE;
        noc__c_parse_impl_free(parsed);
        return status;
    }
    status = noc__c_parse_flatten(parsed,
                                  generation,
                                  options.max_nodes,
                                  &control);
    if (status != NOC_C_PARSE_OK) {
        noc__c_parse_impl_free(parsed);
        return status;
    }

    previous = output->impl;
    output->impl = parsed;
    output->generation = generation;
    noc__c_parse_impl_free(previous);
    return NOC_C_PARSE_OK;
}

NOCDEF void noc_c_parse_tree_free(Noc_C_Parse_Tree *tree)
{
    Noc_C_Parse_Tree_Impl *implementation;
    if (!tree) return;
    implementation = tree->impl;
    memset(tree, 0, sizeof(*tree));
    noc__c_parse_impl_free(implementation);
}

NOCDEF bool noc_c_parse_tree_is_valid(const Noc_C_Parse_Tree *tree)
{
    const Noc_C_Parse_Node *root;
    Noc_Slice source;
    if (!tree || !tree->impl || tree->generation == 0 ||
        !tree->impl->engine_tree ||
        !noc_document_snapshot_is_valid(&tree->impl->snapshot) ||
        !tree->impl->nodes || tree->impl->nodes_count == 0 ||
        tree->impl->nodes_count > tree->impl->nodes_capacity) {
        return false;
    }
    root = &tree->impl->nodes[0];
    source = noc_document_snapshot_source(&tree->impl->snapshot);
    return root->parent == NOC_C_PARSE_NODE_NONE &&
           root->generation == tree->generation &&
           root->bytes.begin == 0 && root->bytes.end <= source.count;
}

NOCDEF size_t noc_c_parse_tree_generation(const Noc_C_Parse_Tree *tree)
{
    return noc_c_parse_tree_is_valid(tree) ? tree->generation : 0;
}

NOCDEF const Noc_Document_Snapshot *noc_c_parse_tree_snapshot(
    const Noc_C_Parse_Tree *tree)
{
    return noc_c_parse_tree_is_valid(tree) ? &tree->impl->snapshot : NULL;
}

NOCDEF size_t noc_c_parse_tree_node_count(const Noc_C_Parse_Tree *tree)
{
    return noc_c_parse_tree_is_valid(tree) ? tree->impl->nodes_count : 0;
}

NOCDEF size_t noc_c_parse_tree_root(const Noc_C_Parse_Tree *tree)
{
    return noc_c_parse_tree_is_valid(tree) ? 0 : NOC_C_PARSE_NODE_NONE;
}

NOCDEF const Noc_C_Parse_Node *noc_c_parse_tree_node_at(
    const Noc_C_Parse_Tree *tree,
    size_t node_index)
{
    if (!noc_c_parse_tree_is_valid(tree) ||
        node_index >= tree->impl->nodes_count) {
        return NULL;
    }
    return &tree->impl->nodes[node_index];
}

NOCDEF bool noc_c_parse_tree_has_error(const Noc_C_Parse_Tree *tree)
{
    const Noc_C_Parse_Node *root = noc_c_parse_tree_node_at(tree, 0);
    return root &&
           (root->flags & (NOC_C_PARSE_NODE_HAS_ERROR |
                           NOC_C_PARSE_NODE_SKIPPED_SOURCE)) != 0;
}

NOCDEF Noc_Slice noc_c_parse_node_source(const Noc_C_Parse_Tree *tree,
                                         size_t node_index)
{
    Noc_Slice result = {0};
    const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, node_index);
    Noc_Slice source;
    if (!node) return result;
    source = noc_document_snapshot_source(&tree->impl->snapshot);
    if (node->bytes.begin > node->bytes.end || node->bytes.end > source.count) {
        return result;
    }
    result.data = source.data + node->bytes.begin;
    result.count = node->bytes.end - node->bytes.begin;
    return result;
}

NOCDEF Noc_Location noc_c_parse_node_location(const Noc_C_Parse_Tree *tree,
                                              size_t node_index)
{
    Noc_Location location = {0};
    const Noc_C_Parse_Node *node = noc_c_parse_tree_node_at(tree, node_index);
    if (node) {
        (void)noc_document_snapshot_location(&tree->impl->snapshot,
                                             node->bytes.begin,
                                             &location);
    }
    return location;
}

#endif /* NOC_C_PARSE_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
