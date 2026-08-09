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
    NOC__C_GRAMMAR_DEFAULT_MAX_CANDIDATES = 256,
    NOC__C_GRAMMAR_DEFAULT_MAX_NODES_EXAMINED = 2 * 1024 * 1024,
    NOC__C_GRAMMAR_DEFAULT_MAX_SYMBOLS_EXAMINED = 4096,
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

typedef struct {
    Noc__Vendor_TSStateId parse_state;
    /* Exclusive flat-preorder end of this node's complete subtree. */
    size_t subtree_end;
} Noc__C_Parse_Node_Metadata;

struct Noc_C_Parse_Tree_Impl {
    Noc_Document_Snapshot snapshot;
    Noc__Vendor_TSTree *engine_tree;
    Noc_C_Parse_Node *nodes;
    Noc__C_Parse_Node_Metadata *metadata;
    size_t nodes_count;
    size_t nodes_capacity;
};

static void noc__c_parse_impl_free(Noc_C_Parse_Tree_Impl *implementation)
{
    if (!implementation) return;
    free(implementation->metadata);
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

static bool noc__c_grammar_slice_has_suffix(Noc_Slice value,
                                            const char *suffix)
{
    size_t suffix_count = strlen(suffix);
    return value.count >= suffix_count &&
           memcmp(value.data + value.count - suffix_count,
                  suffix,
                  suffix_count) == 0;
}

NOC__PRIVATE Noc_C_Ast_Expected_Kind noc__c_grammar_expected_kind(
    Noc_Slice spelling,
    bool named)
{
    if (!spelling.data || spelling.count == 0) {
        return NOC_C_AST_EXPECTED_UNKNOWN;
    }
    if (named) {
        if (noc_slice_equal_cstr(spelling, "identifier") ||
            noc_slice_equal_cstr(spelling, "field_identifier") ||
            noc_slice_equal_cstr(spelling, "statement_identifier") ||
            noc_slice_equal_cstr(spelling, "_declarator") ||
            noc_slice_equal_cstr(spelling, "_field_declarator") ||
            noc_slice_equal_cstr(spelling, "_type_declarator") ||
            noc_slice_equal_cstr(spelling, "_abstract_declarator")) {
            return NOC_C_AST_EXPECTED_IDENTIFIER;
        }
        if (noc_slice_equal_cstr(spelling, "type_identifier") ||
            noc_slice_equal_cstr(spelling, "type_specifier") ||
            noc_slice_equal_cstr(spelling, "primitive_type") ||
            noc_slice_equal_cstr(spelling, "sized_type_specifier") ||
            noc_slice_equal_cstr(spelling, "type_descriptor")) {
            return NOC_C_AST_EXPECTED_TYPE;
        }
        if (noc_slice_equal_cstr(spelling, "declaration") ||
            noc__c_grammar_slice_has_suffix(spelling, "_declaration")) {
            return NOC_C_AST_EXPECTED_DECLARATION;
        }
        if (noc_slice_equal_cstr(spelling, "statement") ||
            noc__c_grammar_slice_has_suffix(spelling, "_statement")) {
            return NOC_C_AST_EXPECTED_STATEMENT;
        }
        if (noc_slice_equal_cstr(spelling, "expression") ||
            noc__c_grammar_slice_has_suffix(spelling, "_expression") ||
            noc_slice_equal_cstr(spelling, "char_literal") ||
            noc_slice_equal_cstr(spelling, "concatenated_string") ||
            noc_slice_equal_cstr(spelling, "number_literal") ||
            noc_slice_equal_cstr(spelling, "string_literal")) {
            return NOC_C_AST_EXPECTED_EXPRESSION;
        }
        return NOC_C_AST_EXPECTED_UNKNOWN;
    }
    if (ispunct((unsigned char)spelling.data[0])) {
        return NOC_C_AST_EXPECTED_PUNCTUATOR;
    }
    if (isalpha((unsigned char)spelling.data[0]) || spelling.data[0] == '_') {
        return NOC_C_AST_EXPECTED_KEYWORD;
    }
    return NOC_C_AST_EXPECTED_UNKNOWN;
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
    Noc__C_Parse_Node_Metadata *metadata;
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
    metadata = (Noc__C_Parse_Node_Metadata *)realloc(
        implementation->metadata,
        capacity * sizeof(*metadata));
    if (!metadata) return NOC_C_PARSE_OUT_OF_MEMORY;
    implementation->metadata = metadata;
    implementation->nodes_capacity = capacity;
    return NOC_C_PARSE_OK;
}

static Noc_C_Parse_Status noc__c_parse_append_node(
    Noc_C_Parse_Tree_Impl *implementation,
    size_t max_nodes,
    Noc_C_Parse_Node node,
    Noc__Vendor_TSStateId parse_state,
    size_t *node_index)
{
    Noc_C_Parse_Status status = noc__c_parse_nodes_reserve(implementation,
                                                           max_nodes);
    if (status != NOC_C_PARSE_OK) return status;
    *node_index = implementation->nodes_count;
    implementation->nodes[implementation->nodes_count] = node;
    implementation->metadata[implementation->nodes_count].parse_state =
        parse_state;
    implementation->metadata[implementation->nodes_count].subtree_end =
        implementation->nodes_count + 1;
    implementation->nodes_count += 1;
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
                                      noc__vendor_ts_node_parse_state(root),
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
            implementation->metadata[frame->output_index].subtree_end =
                implementation->nodes_count;
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
                                              noc__vendor_ts_node_parse_state(
                                                  child),
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
        !tree->impl->nodes || !tree->impl->metadata ||
        tree->impl->nodes_count == 0 ||
        tree->impl->nodes_count > tree->impl->nodes_capacity) {
        return false;
    }
    root = &tree->impl->nodes[0];
    source = noc_document_snapshot_source(&tree->impl->snapshot);
    return root->parent == NOC_C_PARSE_NODE_NONE &&
           root->generation == tree->generation &&
           root->bytes.begin == 0 && root->bytes.end <= source.count &&
           tree->impl->metadata[0].subtree_end == tree->impl->nodes_count;
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

typedef struct {
    Noc_C_Ast_Expected_Kind kind;
    Noc_Slice spelling;
    unsigned int flags;
} Noc__C_Grammar_Temporary_Candidate;

static bool noc__c_grammar_candidate_spelling_equal(Noc_Slice left,
                                                    Noc_Slice right)
{
    return left.count == right.count &&
           (left.count == 0 ||
            memcmp(left.data, right.data, left.count) == 0);
}

static int noc__c_grammar_candidate_compare(const void *left_pointer,
                                            const void *right_pointer)
{
    const Noc__C_Grammar_Temporary_Candidate *left =
        (const Noc__C_Grammar_Temporary_Candidate *)left_pointer;
    const Noc__C_Grammar_Temporary_Candidate *right =
        (const Noc__C_Grammar_Temporary_Candidate *)right_pointer;
    size_t common;
    int compared;
    if (left->kind != right->kind) {
        return left->kind < right->kind ? -1 : 1;
    }
    common = left->spelling.count < right->spelling.count
                 ? left->spelling.count
                 : right->spelling.count;
    compared = common == 0
                   ? 0
                   : memcmp(left->spelling.data,
                            right->spelling.data,
                            common);
    if (compared != 0) return compared;
    if (left->spelling.count == right->spelling.count) return 0;
    return left->spelling.count < right->spelling.count ? -1 : 1;
}

static bool noc__c_grammar_exact_is_word(Noc_Slice spelling)
{
    size_t index = 0;
    if (!spelling.data || spelling.count == 0) return false;
    if (spelling.data[index] == '#') {
        index += 1;
        if (index == spelling.count) return false;
    }
    if (!isalpha((unsigned char)spelling.data[index]) &&
        spelling.data[index] != '_') {
        return false;
    }
    for (index += 1; index < spelling.count; ++index) {
        if (!isalnum((unsigned char)spelling.data[index]) &&
            spelling.data[index] != '_') {
            return false;
        }
    }
    return true;
}

static bool noc__c_grammar_exact_is_punctuation(Noc_Slice spelling)
{
    size_t index;
    if (!spelling.data || spelling.count == 0) return false;
    for (index = 0; index < spelling.count; ++index) {
        if (!ispunct((unsigned char)spelling.data[index])) return false;
    }
    return true;
}

static bool noc__c_grammar_exact_is_c11(Noc_Slice spelling)
{
    static const char *const spellings[] = {
        "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex",
        "_Generic", "_Imaginary", "_Noreturn", "_Pragma",
        "_Static_assert", "_Thread_local", "auto", "break", "case",
        "char", "const", "continue", "default", "defined", "do",
        "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "inline", "int", "long", "register", "restrict", "return",
        "short", "signed", "sizeof", "static", "struct", "switch",
        "typedef", "union", "unsigned", "void", "volatile", "while",
        "#define", "#elif", "#else", "#endif", "#error", "#if",
        "#ifdef", "#ifndef", "#include", "#line", "#pragma", "#undef",
        "[", "]", "(", ")", "{", "}", ".", "->", "++", "--",
        "&", "*", "+", "-", "~", "!", "/", "%", "<<", ">>",
        "<", ">", "<=", ">=", "==", "!=", "^", "|", "&&", "||",
        "?", ":", ";", "...", "=", "*=", "/=", "%=", "+=", "-=",
        "<<=", ">>=", "&=", "^=", "|=", ",", "#", "##", "<:",
        ":>", "<%", "%>", "%:", "%:%:",
    };
    size_t index;
    for (index = 0; index < sizeof(spellings) / sizeof(spellings[0]); ++index) {
        if (noc_slice_equal_cstr(spelling, spellings[index])) return true;
    }
    return false;
}

static Noc_C_Parse_Status noc__c_grammar_candidate_append(
    Noc__C_Grammar_Temporary_Candidate **items,
    size_t *count,
    size_t *capacity,
    Noc_C_Ast_Expected_Kind kind,
    Noc_Slice spelling,
    unsigned int flags)
{
    Noc__C_Grammar_Temporary_Candidate *grown;
    size_t index;
    size_t new_capacity;
    if (kind == NOC_C_AST_EXPECTED_NONE ||
        kind == NOC_C_AST_EXPECTED_UNKNOWN) {
        return NOC_C_PARSE_OK;
    }
    if (spelling.count != 0 && !noc__c_grammar_exact_is_c11(spelling)) {
        flags |= NOC_C_GRAMMAR_CANDIDATE_NON_C11;
    }
    for (index = 0; index < *count; ++index) {
        if ((*items)[index].kind == kind &&
            noc__c_grammar_candidate_spelling_equal((*items)[index].spelling,
                                                     spelling)) {
            (*items)[index].flags |= flags;
            return NOC_C_PARSE_OK;
        }
    }
    if (*count == *capacity) {
        new_capacity = *capacity == 0 ? 32 : *capacity * 2;
        if (new_capacity <= *capacity ||
            new_capacity > SIZE_MAX / sizeof(*grown)) {
            return NOC_C_PARSE_LIMIT_EXCEEDED;
        }
        grown = (Noc__C_Grammar_Temporary_Candidate *)realloc(
            *items,
            new_capacity * sizeof(*grown));
        if (!grown) return NOC_C_PARSE_OUT_OF_MEMORY;
        *items = grown;
        *capacity = new_capacity;
    }
    (*items)[*count].kind = kind;
    (*items)[*count].spelling = spelling;
    (*items)[*count].flags = flags;
    *count += 1;
    return NOC_C_PARSE_OK;
}

static Noc_C_Parse_Status noc__c_grammar_work_step(
    size_t *examined,
    size_t maximum,
    Noc_C_Parse_Cancel_Fn should_cancel,
    void *user_data)
{
    if (*examined >= maximum) return NOC_C_PARSE_LIMIT_EXCEEDED;
    *examined += 1;
    if (should_cancel && ((*examined & 255u) == 0) &&
        should_cancel(user_data)) {
        return NOC_C_PARSE_CANCELLED;
    }
    return NOC_C_PARSE_OK;
}

static bool noc__c_grammar_physical_leaf(const Noc_C_Parse_Node *node)
{
    return node->child_count == 0 &&
           (node->flags &
            (NOC_C_PARSE_NODE_EXTRA | NOC_C_PARSE_NODE_MISSING)) == 0 &&
           node->bytes.begin < node->bytes.end;
}

static bool noc__c_grammar_output_is_zero(
    const Noc_C_Grammar_Candidates *candidates)
{
    return candidates && !candidates->items && candidates->count == 0 &&
           candidates->capacity == 0 && !candidates->spelling_storage &&
           candidates->spelling_storage_count == 0 &&
           candidates->offset == 0 && candidates->replacement.begin == 0 &&
           candidates->replacement.end == 0 &&
           candidates->file_id == 0 &&
           candidates->document_generation == 0 &&
           candidates->parse_tree_generation == 0 &&
           candidates->generation == 0 && candidates->flags == 0;
}

NOCDEF Noc_C_Grammar_Candidate_Options
noc_c_grammar_candidate_default_options(void)
{
    Noc_C_Grammar_Candidate_Options options;
    options.max_candidates = NOC__C_GRAMMAR_DEFAULT_MAX_CANDIDATES;
    options.max_nodes_examined = NOC__C_GRAMMAR_DEFAULT_MAX_NODES_EXAMINED;
    options.max_symbols_examined = NOC__C_GRAMMAR_DEFAULT_MAX_SYMBOLS_EXAMINED;
    options.should_cancel = NULL;
    options.cancel_user_data = NULL;
    return options;
}

NOCDEF void noc_c_grammar_candidates_free(
    Noc_C_Grammar_Candidates *candidates)
{
    if (!candidates) return;
    free(candidates->spelling_storage);
    free(candidates->items);
    memset(candidates, 0, sizeof(*candidates));
}

NOCDEF bool noc_c_grammar_candidates_is_valid(
    const Noc_C_Grammar_Candidates *candidates)
{
    const unsigned int candidate_flags =
        NOC_C_GRAMMAR_CANDIDATE_LOOKAHEAD |
        NOC_C_GRAMMAR_CANDIDATE_MISSING |
        NOC_C_GRAMMAR_CANDIDATE_NON_C11;
    const unsigned int result_flags =
        NOC_C_GRAMMAR_CANDIDATES_STATE_AVAILABLE |
        NOC_C_GRAMMAR_CANDIDATES_RECOVERY_HEURISTIC |
        NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT |
        NOC_C_GRAMMAR_CANDIDATES_TRUNCATED;
    uintptr_t storage_begin;
    uintptr_t storage_end;
    size_t index;
    if (!candidates || candidates->generation == 0 ||
        candidates->file_id == NOC_FILE_ID_NONE ||
        candidates->document_generation == 0 ||
        candidates->parse_tree_generation == 0 ||
        candidates->count > candidates->capacity ||
        ((candidates->capacity == 0) != (candidates->items == NULL)) ||
        ((candidates->spelling_storage_count == 0) !=
         (candidates->spelling_storage == NULL)) ||
        (candidates->flags & ~result_flags) != 0 ||
        candidates->replacement.begin > candidates->replacement.end) {
        return false;
    }
    if ((candidates->flags &
         NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT) != 0) {
        if (!(candidates->replacement.begin < candidates->offset &&
              candidates->offset < candidates->replacement.end)) {
            return false;
        }
    } else if (candidates->replacement.begin != candidates->offset ||
               candidates->replacement.end != candidates->offset) {
        return false;
    }
    storage_begin = (uintptr_t)candidates->spelling_storage;
    if (candidates->spelling_storage_count > UINTPTR_MAX - storage_begin) {
        return false;
    }
    storage_end = storage_begin + candidates->spelling_storage_count;
    for (index = 0; index < candidates->count; ++index) {
        const Noc_C_Grammar_Candidate *candidate = &candidates->items[index];
        uintptr_t spelling_begin = (uintptr_t)candidate->spelling.data;
        if (candidate->kind <= NOC_C_AST_EXPECTED_UNKNOWN ||
            candidate->kind > NOC_C_AST_EXPECTED_EXPRESSION ||
            (candidate->flags & ~candidate_flags) != 0 ||
            (candidate->flags &
             (NOC_C_GRAMMAR_CANDIDATE_LOOKAHEAD |
              NOC_C_GRAMMAR_CANDIDATE_MISSING)) == 0) {
            return false;
        }
        if (candidate->spelling.count == 0) {
            if (candidate->spelling.data != NULL) return false;
        } else if (spelling_begin < storage_begin ||
                   spelling_begin >= storage_end ||
                   candidate->spelling.count > storage_end - spelling_begin) {
            return false;
        }
        if (candidate->kind == NOC_C_AST_EXPECTED_PUNCTUATOR) {
            if (candidate->spelling.count == 0 ||
                !noc__c_grammar_exact_is_punctuation(candidate->spelling)) {
                return false;
            }
        } else if (candidate->kind == NOC_C_AST_EXPECTED_KEYWORD) {
            if (candidate->spelling.count == 0 ||
                !noc__c_grammar_exact_is_word(candidate->spelling)) {
                return false;
            }
        } else if (candidate->spelling.count != 0 ||
                   (candidate->flags &
                    NOC_C_GRAMMAR_CANDIDATE_NON_C11) != 0) {
            return false;
        }
        if (candidate->spelling.count != 0 &&
            (((candidate->flags & NOC_C_GRAMMAR_CANDIDATE_NON_C11) != 0) ==
             noc__c_grammar_exact_is_c11(candidate->spelling))) {
            return false;
        }
        if (index != 0) {
            Noc__C_Grammar_Temporary_Candidate left;
            Noc__C_Grammar_Temporary_Candidate right;
            left.kind = candidates->items[index - 1].kind;
            left.spelling = candidates->items[index - 1].spelling;
            right.kind = candidate->kind;
            right.spelling = candidate->spelling;
            if (noc__c_grammar_candidate_compare(&left, &right) >= 0) {
                return false;
            }
        }
    }
    return true;
}

static Noc_C_Parse_Status noc__c_grammar_candidate_from_symbol(
    const Noc__Vendor_TSLanguage *language,
    Noc__Vendor_TSSymbol symbol,
    Noc__C_Grammar_Temporary_Candidate **items,
    size_t *count,
    size_t *capacity)
{
    Noc__Vendor_TSSymbolType symbol_type =
        noc__vendor_ts_language_symbol_type(language, symbol);
    const char *name = noc__vendor_ts_language_symbol_name(language, symbol);
    Noc_C_Ast_Expected_Kind kind;
    Noc_Slice spelling = {0};
    Noc_Slice name_slice;
    if (!name || name[0] == '\0' || strcmp(name, "ERROR") == 0 ||
        strcmp(name, "end") == 0 ||
        symbol_type == Noc__Vendor_TSSymbolTypeAuxiliary) {
        return NOC_C_PARSE_OK;
    }
    name_slice.data = name;
    name_slice.count = strlen(name);
    if (symbol_type == Noc__Vendor_TSSymbolTypeAnonymous) {
        if (noc__c_grammar_exact_is_word(name_slice)) {
            kind = NOC_C_AST_EXPECTED_KEYWORD;
        } else if (noc__c_grammar_exact_is_punctuation(name_slice)) {
            kind = NOC_C_AST_EXPECTED_PUNCTUATOR;
        } else {
            return NOC_C_PARSE_OK;
        }
        spelling = name_slice;
    } else {
        kind = noc__c_grammar_expected_kind(name_slice, true);
    }
    return noc__c_grammar_candidate_append(
        items,
        count,
        capacity,
        kind,
        spelling,
        NOC_C_GRAMMAR_CANDIDATE_LOOKAHEAD);
}

NOCDEF Noc_C_Parse_Status noc_c_parse_grammar_candidates_build(
    const Noc_C_Parse_Tree *tree,
    size_t offset,
    Noc_C_Grammar_Candidate_Options options,
    Noc_C_Grammar_Candidates *output)
{
    Noc__C_Grammar_Temporary_Candidate *temporary = NULL;
    size_t temporary_count = 0;
    size_t temporary_capacity = 0;
    Noc_C_Grammar_Candidates result;
    Noc_C_Grammar_Candidates previous;
    const Noc__Vendor_TSLanguage *language;
    Noc__Vendor_TSLookaheadIterator *iterator = NULL;
    Noc_Slice source;
    size_t nodes_examined = 0;
    size_t symbols_examined = 0;
    size_t error_node = NOC_C_PARSE_NODE_NONE;
    size_t inside_leaf = NOC_C_PARSE_NODE_NONE;
    size_t left_leaf = NOC_C_PARSE_NODE_NONE;
    size_t right_leaf = NOC_C_PARSE_NODE_NONE;
    size_t anchor = NOC_C_PARSE_NODE_NONE;
    size_t anchor_candidates[4];
    size_t anchor_candidate_count = 0;
    size_t index;
    size_t publish_count;
    size_t spelling_count = 0;
    size_t spelling_cursor = 0;
    bool has_missing = false;
    Noc_C_Parse_Status status = NOC_C_PARSE_OK;

    if (!noc_c_parse_tree_is_valid(tree) || !output ||
        options.max_candidates == 0 || options.max_nodes_examined == 0 ||
        options.max_symbols_examined == 0 ||
        (!noc__c_grammar_output_is_zero(output) &&
         !noc_c_grammar_candidates_is_valid(output))) {
        return NOC_C_PARSE_INVALID_ARGUMENT;
    }
    source = noc_document_snapshot_source(&tree->impl->snapshot);
    if (offset > source.count) return NOC_C_PARSE_INVALID_ARGUMENT;
    if (output->generation == SIZE_MAX) {
        return NOC_C_PARSE_GENERATION_EXHAUSTED;
    }
    if (options.should_cancel &&
        options.should_cancel(options.cancel_user_data)) {
        return NOC_C_PARSE_CANCELLED;
    }
    memset(&result, 0, sizeof(result));
    result.offset = offset;
    result.replacement.begin = offset;
    result.replacement.end = offset;
    result.file_id = noc_document_snapshot_file_id(&tree->impl->snapshot);
    result.document_generation =
        noc_document_snapshot_generation(&tree->impl->snapshot);
    result.parse_tree_generation = tree->generation;
    result.generation = output->generation + 1;

    for (index = 0; index < tree->impl->nodes_count; ++index) {
        const Noc_C_Parse_Node *node = &tree->impl->nodes[index];
        status = noc__c_grammar_work_step(&nodes_examined,
                                          options.max_nodes_examined,
                                          options.should_cancel,
                                          options.cancel_user_data);
        if (status != NOC_C_PARSE_OK) goto fail;
        if ((node->flags & NOC_C_PARSE_NODE_MISSING) != 0 &&
            node->bytes.begin == offset && node->bytes.end == offset) {
            Noc_C_Ast_Expected_Kind kind = noc__c_grammar_expected_kind(
                node->kind,
                (node->flags & NOC_C_PARSE_NODE_NAMED) != 0);
            Noc_Slice spelling = {0};
            if ((node->flags & NOC_C_PARSE_NODE_NAMED) == 0) {
                spelling = node->kind;
            }
            status = noc__c_grammar_candidate_append(
                &temporary,
                &temporary_count,
                &temporary_capacity,
                kind,
                spelling,
                NOC_C_GRAMMAR_CANDIDATE_MISSING);
            if (status != NOC_C_PARSE_OK) goto fail;
            has_missing = true;
        }
        if ((node->flags & NOC_C_PARSE_NODE_ERROR) != 0 &&
            node->bytes.begin <= offset && offset < node->bytes.end) {
            /* Matching nodes are nested at one physical byte; preorder makes
               the last match the deepest retained ERROR node. */
            error_node = index;
        }
        if (noc__c_grammar_physical_leaf(node)) {
            if (node->bytes.begin < offset && offset < node->bytes.end) {
                inside_leaf = index;
            }
            if (node->bytes.end <= offset &&
                (left_leaf == NOC_C_PARSE_NODE_NONE ||
                 node->bytes.end >=
                     tree->impl->nodes[left_leaf].bytes.end)) {
                left_leaf = index;
            }
            if (node->bytes.begin >= offset &&
                (right_leaf == NOC_C_PARSE_NODE_NONE ||
                 node->bytes.begin <
                     tree->impl->nodes[right_leaf].bytes.begin)) {
                right_leaf = index;
            }
        }
    }

    if (error_node != NOC_C_PARSE_NODE_NONE) {
        size_t subtree_end = tree->impl->metadata[error_node].subtree_end;
        result.flags |= NOC_C_GRAMMAR_CANDIDATES_RECOVERY_HEURISTIC;
        for (index = error_node; index < subtree_end; ++index) {
            const Noc_C_Parse_Node *node = &tree->impl->nodes[index];
            status = noc__c_grammar_work_step(&nodes_examined,
                                              options.max_nodes_examined,
                                              options.should_cancel,
                                              options.cancel_user_data);
            if (status != NOC_C_PARSE_OK) goto fail;
            if (node->child_count == 0 &&
                (node->flags & NOC_C_PARSE_NODE_MISSING) == 0 &&
                node->bytes.begin < node->bytes.end) {
                anchor = index;
                break;
            }
        }
    } else if (has_missing) {
        anchor = left_leaf;
        result.flags |= NOC_C_GRAMMAR_CANDIDATES_RECOVERY_HEURISTIC;
    } else if (inside_leaf != NOC_C_PARSE_NODE_NONE) {
        anchor = inside_leaf;
    } else if (right_leaf != NOC_C_PARSE_NODE_NONE) {
        anchor = right_leaf;
    } else if (left_leaf != NOC_C_PARSE_NODE_NONE) {
        anchor = left_leaf;
    } else {
        anchor = 0;
    }
    /* ERROR and MISSING affect parser-state selection, not physical ownership.
       A later leaf inside a multi-leaf ERROR still has replacement semantics
       even though the ERROR's first leaf supplies the documented state. */
    if (inside_leaf != NOC_C_PARSE_NODE_NONE) {
        result.replacement = tree->impl->nodes[inside_leaf].bytes;
        result.flags |= NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT;
    }

    anchor_candidates[anchor_candidate_count++] = anchor;
    if (right_leaf != NOC_C_PARSE_NODE_NONE && right_leaf != anchor) {
        anchor_candidates[anchor_candidate_count++] = right_leaf;
    }
    if (left_leaf != NOC_C_PARSE_NODE_NONE && left_leaf != anchor &&
        left_leaf != right_leaf) {
        anchor_candidates[anchor_candidate_count++] = left_leaf;
    }
    if (anchor != 0 && right_leaf != 0 && left_leaf != 0) {
        anchor_candidates[anchor_candidate_count++] = 0;
    }

    language = noc__vendor_tree_sitter_c();
    for (index = 0; index < anchor_candidate_count; ++index) {
        size_t candidate_anchor = anchor_candidates[index];
        Noc__Vendor_TSStateId state;
        if (candidate_anchor == NOC_C_PARSE_NODE_NONE ||
            candidate_anchor >= tree->impl->nodes_count) {
            continue;
        }
        state = tree->impl->metadata[candidate_anchor].parse_state;
        iterator = noc__vendor_ts_lookahead_iterator_new(language, state);
        if (iterator) {
            if (index != 0) {
                result.flags |=
                    NOC_C_GRAMMAR_CANDIDATES_RECOVERY_HEURISTIC;
            }
            break;
        }
    }
    if (iterator) {
        result.flags |= NOC_C_GRAMMAR_CANDIDATES_STATE_AVAILABLE;
        while (noc__vendor_ts_lookahead_iterator_next(iterator)) {
            Noc__Vendor_TSSymbol symbol;
            status = noc__c_grammar_work_step(&symbols_examined,
                                              options.max_symbols_examined,
                                              options.should_cancel,
                                              options.cancel_user_data);
            if (status != NOC_C_PARSE_OK) goto fail;
            symbol = noc__vendor_ts_lookahead_iterator_current_symbol(iterator);
            status = noc__c_grammar_candidate_from_symbol(
                language,
                symbol,
                &temporary,
                &temporary_count,
                &temporary_capacity);
            if (status != NOC_C_PARSE_OK) goto fail;
        }
        noc__vendor_ts_lookahead_iterator_delete(iterator);
        iterator = NULL;
    }

    if (temporary_count > 1) {
        qsort(temporary,
              temporary_count,
              sizeof(*temporary),
              noc__c_grammar_candidate_compare);
    }
    publish_count = temporary_count;
    if (publish_count > options.max_candidates) {
        publish_count = options.max_candidates;
        result.flags |= NOC_C_GRAMMAR_CANDIDATES_TRUNCATED;
    }
    for (index = 0; index < publish_count; ++index) {
        if (temporary[index].spelling.count != 0) {
            if (spelling_count == SIZE_MAX ||
                temporary[index].spelling.count >
                    SIZE_MAX - spelling_count - 1) {
                status = NOC_C_PARSE_LIMIT_EXCEEDED;
                goto fail;
            }
            spelling_count += temporary[index].spelling.count + 1;
        }
    }
    if (publish_count != 0) {
        if (publish_count > SIZE_MAX / sizeof(*result.items)) {
            status = NOC_C_PARSE_LIMIT_EXCEEDED;
            goto fail;
        }
        result.items = (Noc_C_Grammar_Candidate *)calloc(
            publish_count,
            sizeof(*result.items));
        if (!result.items) {
            status = NOC_C_PARSE_OUT_OF_MEMORY;
            goto fail;
        }
        result.capacity = publish_count;
    }
    if (spelling_count != 0) {
        result.spelling_storage = (char *)malloc(spelling_count);
        if (!result.spelling_storage) {
            status = NOC_C_PARSE_OUT_OF_MEMORY;
            goto fail;
        }
        result.spelling_storage_count = spelling_count;
    }
    for (index = 0; index < publish_count; ++index) {
        result.items[index].kind = temporary[index].kind;
        result.items[index].flags = temporary[index].flags;
        if (temporary[index].spelling.count != 0) {
            result.items[index].spelling.data =
                result.spelling_storage + spelling_cursor;
            result.items[index].spelling.count = temporary[index].spelling.count;
            memcpy(result.spelling_storage + spelling_cursor,
                   temporary[index].spelling.data,
                   temporary[index].spelling.count);
            spelling_cursor += temporary[index].spelling.count;
            result.spelling_storage[spelling_cursor++] = '\0';
        }
    }
    result.count = publish_count;
    free(temporary);
    previous = *output;
    *output = result;
    noc_c_grammar_candidates_free(&previous);
    return NOC_C_PARSE_OK;

fail:
    noc__vendor_ts_lookahead_iterator_delete(iterator);
    free(temporary);
    noc_c_grammar_candidates_free(&result);
    return status;
}

NOCDEF const Noc_C_Grammar_Candidate *noc_c_grammar_candidate_at(
    const Noc_C_Grammar_Candidates *candidates,
    size_t index)
{
    if (!noc_c_grammar_candidates_is_valid(candidates) ||
        index >= candidates->count) {
        return NULL;
    }
    return &candidates->items[index];
}

#endif /* NOC_C_PARSE_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
