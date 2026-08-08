#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_AST_IMPLEMENTATION_INCLUDED
#define NOC_AST_IMPLEMENTATION_INCLUDED

typedef struct {
    size_t *items;
    size_t count;
    size_t capacity;
} Noc__Index_Stack;

static bool noc__index_stack_append(Noc__Index_Stack *stack, size_t item)
{
    size_t *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (stack->count > stack->capacity) return false;
    if (stack->count == stack->capacity) {
        if (stack->capacity >= maximum) return false;
        if (stack->capacity == 0) {
            capacity = maximum < 16 ? maximum : 16;
        } else if (stack->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = stack->capacity * 2;
        }
        if (capacity <= stack->capacity) return false;
        items = (size_t *)realloc(stack->items, capacity * sizeof(*items));
        if (!items) return false;
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count++] = item;
    return true;
}

static bool noc__syntax_append_node(Noc_Syntax_Tree *tree,
                                    Noc_Syntax_Node node,
                                    size_t *node_index)
{
    Noc_Syntax_Node *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (tree->count > tree->capacity) return false;
    if (tree->count == tree->capacity) {
        if (tree->capacity >= maximum) return false;
        if (tree->capacity == 0) {
            capacity = maximum < 256 ? maximum : 256;
        } else if (tree->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = tree->capacity * 2;
        }
        if (capacity <= tree->capacity) return false;
        items = (Noc_Syntax_Node *)realloc(tree->items, capacity * sizeof(*items));
        if (!items) return false;
        tree->items = items;
        tree->capacity = capacity;
    }
    if (node_index) *node_index = tree->count;
    tree->items[tree->count++] = node;
    return true;
}

static bool noc__syntax_add_child(Noc_Syntax_Tree *tree,
                                  size_t parent,
                                  Noc_Syntax_Node node,
                                  size_t *node_index)
{
    size_t child;
    size_t previous;
    node.parent = parent;
    if (!noc__syntax_append_node(tree, node, &child)) return false;
    previous = tree->items[parent].last_child;
    if (previous == NOC_SYNTAX_NONE) {
        tree->items[parent].first_child = child;
    } else {
        tree->items[previous].next_sibling = child;
    }
    tree->items[parent].last_child = child;
    if (node_index) *node_index = child;
    return true;
}

static Noc_Syntax_Kind noc__syntax_open_kind(Noc_Token token, char *expected_close)
{
    if (noc_token_is_punct(token, "(")) {
        *expected_close = ')';
        return NOC_SYNTAX_PAREN_GROUP;
    }
    if (noc_token_is_punct(token, "[")) {
        *expected_close = ']';
        return NOC_SYNTAX_BRACKET_GROUP;
    }
    if (noc_token_is_punct(token, "{")) {
        *expected_close = '}';
        return NOC_SYNTAX_BRACE_GROUP;
    }
    *expected_close = 0;
    return NOC_SYNTAX_TOKEN;
}

static char noc__syntax_expected_close(Noc_Syntax_Kind kind)
{
    switch (kind) {
    case NOC_SYNTAX_PAREN_GROUP: return ')';
    case NOC_SYNTAX_BRACKET_GROUP: return ']';
    case NOC_SYNTAX_BRACE_GROUP: return '}';
    default: return 0;
    }
}

static bool noc__syntax_is_close(Noc_Token token)
{
    return noc_token_is_punct(token, ")") || noc_token_is_punct(token, "]") ||
           noc_token_is_punct(token, "}");
}

NOCDEF void noc_syntax_tree_free(Noc_Syntax_Tree *tree)
{
    free(tree->items);
    memset(tree, 0, sizeof(*tree));
}

NOCDEF bool noc_syntax_tree_is_valid(const Noc_Syntax_Tree *tree)
{
    const Noc_Syntax_Node *root;
    if (!tree || !noc_token_stream_is_valid(tree->stream) ||
        tree->stream_generation != tree->stream->generation || !tree->items ||
        tree->count == 0 || tree->count > tree->capacity) {
        return false;
    }
    root = &tree->items[0];
    return root->kind == NOC_SYNTAX_ROOT && root->parent == NOC_SYNTAX_NONE &&
           noc_token_range_is_valid(tree->stream, root->range);
}

NOCDEF bool noc_syntax_tree_build(Noc_Context *context,
                                  const Noc_Token_Stream *stream,
                                  Noc_Syntax_Tree *tree)
{
    Noc_Syntax_Tree parsed = {0};
    Noc__Index_Stack parents = {0};
    Noc_Location no_location = {0};
    Noc_Syntax_Node root;
    size_t eof_index;
    size_t i;
    bool ok = false;
    if (!noc_token_stream_is_valid(stream)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "cannot build a syntax tree from an invalid token stream");
        return false;
    }
    parsed.stream = stream;
    parsed.stream_generation = stream->generation;
    eof_index = stream->count - 1;
    memset(&root, 0, sizeof(root));
    root.kind = NOC_SYNTAX_ROOT;
    root.range.begin = 0;
    root.range.end = eof_index;
    root.parent = NOC_SYNTAX_NONE;
    root.first_child = NOC_SYNTAX_NONE;
    root.last_child = NOC_SYNTAX_NONE;
    root.next_sibling = NOC_SYNTAX_NONE;
    if (!noc__syntax_append_node(&parsed, root, NULL) ||
        !noc__index_stack_append(&parents, 0)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while starting syntax tree");
        goto done;
    }

    for (i = 0; i < eof_index; ++i) {
        Noc_Token token = stream->items[i];
        Noc_Syntax_Node node;
        Noc_Syntax_Kind open_kind;
        size_t parent = parents.items[parents.count - 1];
        size_t node_index;
        char expected_close;
        memset(&node, 0, sizeof(node));
        open_kind = noc__syntax_open_kind(token, &expected_close);
        if (expected_close != 0) {
            node.kind = open_kind;
            node.range.begin = i;
            node.range.end = NOC_TOKEN_INDEX_NONE;
            node.first_child = NOC_SYNTAX_NONE;
            node.last_child = NOC_SYNTAX_NONE;
            node.next_sibling = NOC_SYNTAX_NONE;
            if (!noc__syntax_add_child(&parsed, parent, node, &node_index) ||
                !noc__index_stack_append(&parents, node_index)) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "out of memory while building syntax tree");
                goto done;
            }
            continue;
        }
        if (noc__syntax_is_close(token)) {
            Noc_Syntax_Node *group;
            char expected;
            if (parents.count == 1) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "unexpected closing delimiter '%.*s'",
                            (int)token.text.count,
                            token.text.data);
                goto done;
            }
            group = &parsed.items[parents.items[parents.count - 1]];
            expected = noc__syntax_expected_close(group->kind);
            if (token.text.count != 1 || token.text.data[0] != expected) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "expected closing delimiter '%c', got '%.*s'",
                            expected,
                            (int)token.text.count,
                            token.text.data);
                goto done;
            }
            group->range.end = i + 1;
            parents.count -= 1;
            continue;
        }
        node.kind = NOC_SYNTAX_TOKEN;
        node.range.begin = i;
        node.range.end = i + 1;
        node.first_child = NOC_SYNTAX_NONE;
        node.last_child = NOC_SYNTAX_NONE;
        node.next_sibling = NOC_SYNTAX_NONE;
        if (!noc__syntax_add_child(&parsed, parent, node, NULL)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "out of memory while building syntax tree");
            goto done;
        }
    }
    if (parents.count != 1) {
        const Noc_Syntax_Node *group = &parsed.items[parents.items[parents.count - 1]];
        Noc_Location location = stream->items[group->range.begin].location;
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "unclosed '%.*s'; expected '%c'",
                    (int)stream->items[group->range.begin].text.count,
                    stream->items[group->range.begin].text.data,
                    noc__syntax_expected_close(group->kind));
        goto done;
    }
    noc_syntax_tree_free(tree);
    *tree = parsed;
    memset(&parsed, 0, sizeof(parsed));
    ok = true;

done:
    free(parents.items);
    noc_syntax_tree_free(&parsed);
    return ok;
}

NOCDEF const char *noc_syntax_kind_name(Noc_Syntax_Kind kind)
{
    switch (kind) {
    case NOC_SYNTAX_ROOT: return "root";
    case NOC_SYNTAX_TOKEN: return "token";
    case NOC_SYNTAX_PAREN_GROUP: return "parenthesis group";
    case NOC_SYNTAX_BRACKET_GROUP: return "bracket group";
    case NOC_SYNTAX_BRACE_GROUP: return "brace group";
    }
    return "unknown syntax";
}

NOCDEF size_t noc_syntax_root(const Noc_Syntax_Tree *tree)
{
    return noc_syntax_tree_is_valid(tree) ? 0 : NOC_SYNTAX_NONE;
}

NOCDEF const Noc_Syntax_Node *noc_syntax_node(const Noc_Syntax_Tree *tree,
                                              size_t node)
{
    if (!noc_syntax_tree_is_valid(tree) || node >= tree->count) return NULL;
    return &tree->items[node];
}

NOCDEF size_t noc_syntax_parent(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? syntax->parent : NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_first_child(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? syntax->first_child : NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_next_sibling(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? syntax->next_sibling : NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_child_count(const Noc_Syntax_Tree *tree, size_t node)
{
    size_t count = 0;
    size_t child = noc_syntax_first_child(tree, node);
    while (child != NOC_SYNTAX_NONE) {
        count += 1;
        child = noc_syntax_next_sibling(tree, child);
    }
    return count;
}

NOCDEF size_t noc_syntax_first_child_of_kind(const Noc_Syntax_Tree *tree,
                                             size_t node,
                                             Noc_Syntax_Kind kind)
{
    size_t child = noc_syntax_first_child(tree, node);
    while (child != NOC_SYNTAX_NONE) {
        const Noc_Syntax_Node *syntax = noc_syntax_node(tree, child);
        if (syntax && syntax->kind == kind) return child;
        child = noc_syntax_next_sibling(tree, child);
    }
    return NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_next_preorder(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax) return NOC_SYNTAX_NONE;
    if (syntax->first_child != NOC_SYNTAX_NONE) return syntax->first_child;
    while (node != NOC_SYNTAX_NONE) {
        syntax = noc_syntax_node(tree, node);
        if (!syntax) return NOC_SYNTAX_NONE;
        if (syntax->next_sibling != NOC_SYNTAX_NONE) return syntax->next_sibling;
        node = syntax->parent;
    }
    return NOC_SYNTAX_NONE;
}

NOCDEF size_t noc_syntax_node_covering_range(const Noc_Syntax_Tree *tree,
                                             Noc_Token_Range range)
{
    size_t node;
    const Noc_Syntax_Node *syntax;
    if (!noc_syntax_tree_is_valid(tree) || range.begin >= range.end ||
        !noc_token_range_is_valid(tree->stream, range)) {
        return NOC_SYNTAX_NONE;
    }
    node = noc_syntax_root(tree);
    syntax = noc_syntax_node(tree, node);
    if (!syntax || range.begin < syntax->range.begin || range.end > syntax->range.end) {
        return NOC_SYNTAX_NONE;
    }
    for (;;) {
        size_t child = syntax->first_child;
        size_t covering = NOC_SYNTAX_NONE;
        while (child != NOC_SYNTAX_NONE) {
            const Noc_Syntax_Node *candidate = noc_syntax_node(tree, child);
            if (!candidate) return NOC_SYNTAX_NONE;
            if (candidate->range.begin <= range.begin &&
                range.end <= candidate->range.end) {
                covering = child;
                break;
            }
            if (candidate->range.begin > range.begin) break;
            child = candidate->next_sibling;
        }
        if (covering == NOC_SYNTAX_NONE) return node;
        node = covering;
        syntax = &tree->items[node];
    }
}

NOCDEF size_t noc_syntax_node_at_token(const Noc_Syntax_Tree *tree,
                                       size_t token_index)
{
    Noc_Token_Range range;
    if (!noc_syntax_tree_is_valid(tree) || token_index >= tree->stream->count - 1) {
        return NOC_SYNTAX_NONE;
    }
    range.begin = token_index;
    range.end = token_index + 1;
    return noc_syntax_node_covering_range(tree, range);
}

NOCDEF size_t noc_syntax_depth(const Noc_Syntax_Tree *tree, size_t node)
{
    size_t depth = 0;
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax) return NOC_SYNTAX_NONE;
    while (syntax->parent != NOC_SYNTAX_NONE) {
        if (depth >= tree->count) return NOC_SYNTAX_NONE;
        depth += 1;
        syntax = noc_syntax_node(tree, syntax->parent);
        if (!syntax) return NOC_SYNTAX_NONE;
    }
    return depth;
}

NOCDEF size_t noc_syntax_common_ancestor(const Noc_Syntax_Tree *tree,
                                         size_t left,
                                         size_t right)
{
    size_t left_depth = noc_syntax_depth(tree, left);
    size_t right_depth = noc_syntax_depth(tree, right);
    if (left_depth == NOC_SYNTAX_NONE || right_depth == NOC_SYNTAX_NONE) {
        return NOC_SYNTAX_NONE;
    }
    while (left_depth > right_depth) {
        left = tree->items[left].parent;
        left_depth -= 1;
    }
    while (right_depth > left_depth) {
        right = tree->items[right].parent;
        right_depth -= 1;
    }
    while (left != right) {
        if (left == NOC_SYNTAX_NONE || right == NOC_SYNTAX_NONE) {
            return NOC_SYNTAX_NONE;
        }
        left = tree->items[left].parent;
        right = tree->items[right].parent;
    }
    return left;
}

NOCDEF Noc_Token_Range noc_syntax_inner_range(const Noc_Syntax_Tree *tree,
                                              size_t node)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax || (syntax->kind != NOC_SYNTAX_PAREN_GROUP &&
                    syntax->kind != NOC_SYNTAX_BRACKET_GROUP &&
                    syntax->kind != NOC_SYNTAX_BRACE_GROUP) ||
        syntax->range.end < syntax->range.begin + 2) {
        return invalid;
    }
    invalid.begin = syntax->range.begin + 1;
    invalid.end = syntax->range.end - 1;
    return invalid;
}

NOCDEF Noc_Slice noc_syntax_source(const Noc_Syntax_Tree *tree, size_t node)
{
    Noc_Slice empty = {0};
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? noc_token_range_source(tree->stream, syntax->range) : empty;
}

NOCDEF Noc_Location noc_syntax_location(const Noc_Syntax_Tree *tree, size_t node)
{
    Noc_Location empty = {0};
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax ? noc_token_range_location(tree->stream, syntax->range) : empty;
}

NOCDEF const Noc_Token *noc_syntax_token(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax || syntax->kind != NOC_SYNTAX_TOKEN ||
        syntax->range.end != syntax->range.begin + 1) {
        return NULL;
    }
    return &tree->stream->items[syntax->range.begin];
}

typedef struct {
    bool has_typedef;
    bool has_equal;
    bool has_comma;
    bool has_tag_specifier;
    bool starts_with_static_assert;
    size_t tag_name_token;
    size_t name_token;
    bool has_declarator;
    bool declarator_is_function;
    Noc_Token_Range parameters;
} Noc__C_External_Analysis;

typedef enum {
    NOC__C_DECLARATOR_NONE = 0,
    NOC__C_DECLARATOR_POINTER,
    NOC__C_DECLARATOR_ARRAY,
    NOC__C_DECLARATOR_FUNCTION,
} Noc__C_Declarator_Operator;

typedef struct {
    size_t name_token;
    Noc__C_Declarator_Operator first_operator;
    Noc_Token_Range parameters;
} Noc__C_Declarator;

static bool noc__c_token_is_keyword(Noc_Token token)
{
    static const char *const keywords[] = {
        "_Alignas",       "_Alignof",      "_Atomic",       "_BitInt",
        "_Bool",          "_Complex",      "_Decimal128",   "_Decimal32",
        "_Decimal64",     "_Generic",      "_Imaginary",    "_Noreturn",
        "_Static_assert", "_Thread_local", "alignas",       "alignof",
        "auto",           "bool",          "break",         "case",
        "char",           "const",         "constexpr",     "continue",
        "default",        "do",            "double",        "else",
        "enum",           "extern",        "false",         "float",
        "for",            "goto",          "if",            "inline",
        "int",            "long",          "nullptr",       "register",
        "restrict",       "return",        "short",         "signed",
        "sizeof",         "static",        "static_assert", "struct",
        "switch",         "thread_local",  "true",          "typedef",
        "typeof",         "typeof_unqual", "union",         "unsigned",
        "void",           "volatile",      "while",
    };
    size_t i;
    if (token.kind != NOC_TOKEN_IDENTIFIER) return false;
    for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (noc_token_is_identifier(token, keywords[i])) return true;
    }
    return false;
}

static bool noc__c_token_is_attribute_name(Noc_Token token)
{
    return noc_token_is_identifier(token, "__attribute__") ||
           noc_token_is_identifier(token, "__declspec") ||
           noc_token_is_identifier(token, "_Alignas") ||
           noc_token_is_identifier(token, "alignas");
}

static bool noc__c_node_is_ignored(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Token *token = noc_syntax_token(tree, node);
    return token && (noc_token_is_trivia(*token) ||
                     token->kind == NOC_TOKEN_PREPROCESSOR);
}

static size_t noc__c_next_significant_node(const Noc_Syntax_Tree *tree,
                                           size_t node,
                                           size_t end)
{
    while (node != end && node != NOC_SYNTAX_NONE &&
           noc__c_node_is_ignored(tree, node)) {
        node = tree->items[node].next_sibling;
    }
    return node;
}

static bool noc__c_group_is_attribute(const Noc_Syntax_Tree *tree, size_t node)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    if (!syntax || syntax->kind != NOC_SYNTAX_BRACKET_GROUP ||
        syntax->range.end < syntax->range.begin + 4) {
        return false;
    }
    return noc_token_is_punct(tree->stream->items[syntax->range.begin + 1], "[") &&
           noc_token_is_punct(tree->stream->items[syntax->range.end - 2], "]");
}

static bool noc__c_parse_declarator(const Noc_Syntax_Tree *tree,
                                    size_t node,
                                    size_t end,
                                    size_t depth,
                                    Noc__C_Declarator *declarator,
                                    size_t *after)
{
    Noc__C_Declarator parsed;
    bool has_pointer = false;
    const Noc_Syntax_Node *syntax;
    const Noc_Token *token;
    if (depth > tree->count) return false;
    parsed.name_token = NOC_TOKEN_INDEX_NONE;
    parsed.first_operator = NOC__C_DECLARATOR_NONE;
    parsed.parameters.begin = NOC_TOKEN_INDEX_NONE;
    parsed.parameters.end = NOC_TOKEN_INDEX_NONE;
    node = noc__c_next_significant_node(tree, node, end);

    while (node != end && node != NOC_SYNTAX_NONE) {
        size_t next;
        syntax = &tree->items[node];
        token = noc_syntax_token(tree, node);
        if (token && noc_token_is_punct(*token, "*")) {
            has_pointer = true;
            node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            continue;
        }
        if (has_pointer && token &&
            (noc_token_is_identifier(*token, "const") ||
             noc_token_is_identifier(*token, "restrict") ||
             noc_token_is_identifier(*token, "volatile") ||
             noc_token_is_identifier(*token, "_Atomic"))) {
            node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            continue;
        }
        if (has_pointer && token && noc__c_token_is_attribute_name(*token)) {
            next = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            if (next != end && next != NOC_SYNTAX_NONE &&
                tree->items[next].kind == NOC_SYNTAX_PAREN_GROUP) {
                node = noc__c_next_significant_node(tree,
                                                    tree->items[next].next_sibling,
                                                    end);
                continue;
            }
        }
        break;
    }

    if (node == end || node == NOC_SYNTAX_NONE) return false;
    syntax = &tree->items[node];
    token = noc_syntax_token(tree, node);
    if (token && token->kind == NOC_TOKEN_IDENTIFIER &&
        !noc__c_token_is_keyword(*token) && !noc__c_token_is_attribute_name(*token)) {
        parsed.name_token = syntax->range.begin;
    } else if (syntax->kind == NOC_SYNTAX_PAREN_GROUP) {
        size_t inner_after;
        if (!noc__c_parse_declarator(tree,
                                     syntax->first_child,
                                     NOC_SYNTAX_NONE,
                                     depth + 1,
                                     &parsed,
                                     &inner_after) ||
            noc__c_next_significant_node(tree,
                                         inner_after,
                                         NOC_SYNTAX_NONE) != NOC_SYNTAX_NONE) {
            return false;
        }
    } else {
        return false;
    }
    node = noc__c_next_significant_node(tree, syntax->next_sibling, end);

    while (node != end && node != NOC_SYNTAX_NONE) {
        size_t next;
        syntax = &tree->items[node];
        token = noc_syntax_token(tree, node);
        if (syntax->kind == NOC_SYNTAX_PAREN_GROUP) {
            if (parsed.first_operator == NOC__C_DECLARATOR_NONE) {
                parsed.first_operator = NOC__C_DECLARATOR_FUNCTION;
                parsed.parameters = syntax->range;
            }
            node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            continue;
        }
        if (syntax->kind == NOC_SYNTAX_BRACKET_GROUP) {
            if (noc__c_group_is_attribute(tree, node)) {
                node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
                continue;
            }
            if (parsed.first_operator == NOC__C_DECLARATOR_NONE) {
                parsed.first_operator = NOC__C_DECLARATOR_ARRAY;
            }
            node = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            continue;
        }
        if (token && noc__c_token_is_attribute_name(*token)) {
            next = noc__c_next_significant_node(tree, syntax->next_sibling, end);
            if (next != end && next != NOC_SYNTAX_NONE &&
                tree->items[next].kind == NOC_SYNTAX_PAREN_GROUP) {
                node = noc__c_next_significant_node(tree,
                                                    tree->items[next].next_sibling,
                                                    end);
                continue;
            }
        }
        break;
    }
    if (has_pointer && parsed.first_operator == NOC__C_DECLARATOR_NONE) {
        parsed.first_operator = NOC__C_DECLARATOR_POINTER;
    }
    *declarator = parsed;
    *after = node;
    return true;
}

static bool noc__c_find_declarator(const Noc_Syntax_Tree *tree,
                                   size_t first,
                                   size_t end,
                                   Noc__C_Declarator *declarator)
{
    size_t node = noc__c_next_significant_node(tree, first, end);
    while (node != end && node != NOC_SYNTAX_NONE) {
        Noc__C_Declarator parsed;
        size_t after;
        if (noc__c_parse_declarator(tree, node, end, 0, &parsed, &after) &&
            noc__c_next_significant_node(tree, after, end) == end) {
            *declarator = parsed;
            return true;
        }
        node = noc__c_next_significant_node(tree,
                                            tree->items[node].next_sibling,
                                            end);
    }
    return false;
}

static Noc__C_External_Analysis noc__c_analyze_external(
    const Noc_Syntax_Tree *tree,
    size_t first,
    size_t end)
{
    Noc__C_External_Analysis analysis;
    size_t node = first;
    size_t previous = NOC_SYNTAX_NONE;
    size_t declarator_end = end;
    size_t significant_count = 0;
    bool expect_tag_name = false;
    memset(&analysis, 0, sizeof(analysis));
    analysis.tag_name_token = NOC_TOKEN_INDEX_NONE;
    analysis.name_token = NOC_TOKEN_INDEX_NONE;
    analysis.parameters.begin = NOC_TOKEN_INDEX_NONE;
    analysis.parameters.end = NOC_TOKEN_INDEX_NONE;

    while (node != end && node != NOC_SYNTAX_NONE) {
        const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
        const Noc_Token *token;
        if (!syntax) break;
        if (noc__c_node_is_ignored(tree, node)) {
            node = syntax->next_sibling;
            continue;
        }
        token = noc_syntax_token(tree, node);
        if (token) {
            if (significant_count == 0) {
                analysis.starts_with_static_assert =
                    noc_token_is_identifier(*token, "_Static_assert") ||
                    noc_token_is_identifier(*token, "static_assert");
            }
            if (!analysis.has_equal &&
                (noc_token_is_identifier(*token, "struct") ||
                 noc_token_is_identifier(*token, "union") ||
                 noc_token_is_identifier(*token, "enum"))) {
                analysis.has_tag_specifier = true;
                expect_tag_name = true;
            } else if (expect_tag_name && noc__c_token_is_attribute_name(*token)) {
                /* The following group belongs to the attribute, not the tag. */
            } else if (expect_tag_name && token->kind == NOC_TOKEN_IDENTIFIER) {
                analysis.tag_name_token = syntax->range.begin;
                expect_tag_name = false;
            } else if (expect_tag_name && token->kind != NOC_TOKEN_IDENTIFIER) {
                expect_tag_name = false;
            }
            if (noc_token_is_identifier(*token, "typedef")) analysis.has_typedef = true;
            if (noc_token_is_punct(*token, "=")) analysis.has_equal = true;
            if (noc_token_is_punct(*token, ",")) analysis.has_comma = true;
            if ((noc_token_is_punct(*token, "=") || noc_token_is_punct(*token, ",")) &&
                declarator_end == end) {
                declarator_end = node;
            }
        } else if (expect_tag_name) {
            const Noc_Token *previous_token = noc_syntax_token(tree, previous);
            if (!noc__c_group_is_attribute(tree, node) &&
                !(syntax->kind == NOC_SYNTAX_PAREN_GROUP && previous_token &&
                  noc__c_token_is_attribute_name(*previous_token))) {
                expect_tag_name = false;
            }
        }
        previous = node;
        significant_count += 1;
        node = syntax->next_sibling;
    }
    {
        Noc__C_Declarator declarator;
        if (noc__c_find_declarator(tree, first, declarator_end, &declarator)) {
            analysis.has_declarator = true;
            analysis.name_token = declarator.name_token;
            analysis.declarator_is_function =
                declarator.first_operator == NOC__C_DECLARATOR_FUNCTION;
            analysis.parameters = declarator.parameters;
        }
    }
    return analysis;
}

static bool noc__c_external_append(Noc_C_Translation_Unit *unit,
                                   Noc_C_External_Item item)
{
    Noc_C_External_Item *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (unit->count > unit->capacity) return false;
    if (unit->count == unit->capacity) {
        if (unit->capacity >= maximum) return false;
        if (unit->capacity == 0) {
            capacity = maximum < 16 ? maximum : 16;
        } else if (unit->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = unit->capacity * 2;
        }
        if (capacity <= unit->capacity) return false;
        items = (Noc_C_External_Item *)realloc(unit->items,
                                               capacity * sizeof(*items));
        if (!items) return false;
        unit->items = items;
        unit->capacity = capacity;
    }
    unit->items[unit->count++] = item;
    return true;
}

static Noc_C_External_Item noc__c_make_external(const Noc_Syntax_Tree *tree,
                                                size_t first,
                                                size_t end,
                                                Noc_Token_Range range,
                                                bool function_definition,
                                                Noc_Token_Range body)
{
    Noc_C_External_Item item;
    Noc__C_External_Analysis analysis = noc__c_analyze_external(tree, first, end);
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    memset(&item, 0, sizeof(item));
    item.kind = function_definition ? NOC_C_EXTERNAL_FUNCTION_DEFINITION
                                    : NOC_C_EXTERNAL_DECLARATION;
    item.range = range;
    item.signature = range;
    item.name_token = NOC_TOKEN_INDEX_NONE;
    item.parameters = invalid;
    item.body = invalid;
    if (function_definition) {
        item.signature.end = body.begin;
        item.signature = noc_token_range_trim_trivia(tree->stream, item.signature);
        item.body = body;
        item.declaration_kind = NOC_C_DECLARATION_FUNCTION;
        item.name_token = analysis.name_token;
        item.parameters = analysis.parameters;
        return item;
    }
    if (item.signature.end > item.signature.begin &&
        noc_token_is_punct(tree->stream->items[item.signature.end - 1], ";")) {
        item.signature.end -= 1;
        item.signature = noc_token_range_trim_trivia(tree->stream, item.signature);
    }
    if (analysis.has_typedef) {
        item.declaration_kind = NOC_C_DECLARATION_TYPEDEF;
    } else if (analysis.starts_with_static_assert) {
        item.declaration_kind = NOC_C_DECLARATION_UNKNOWN;
    } else if (analysis.has_comma) {
        item.declaration_kind = NOC_C_DECLARATION_UNKNOWN;
    } else if (analysis.has_declarator && analysis.declarator_is_function &&
               !analysis.has_equal) {
        item.declaration_kind = NOC_C_DECLARATION_FUNCTION;
    } else if (analysis.has_tag_specifier && !analysis.has_equal &&
               (!analysis.has_declarator ||
                analysis.name_token == analysis.tag_name_token)) {
        item.declaration_kind = NOC_C_DECLARATION_TAG;
    } else {
        item.declaration_kind = NOC_C_DECLARATION_OBJECT;
    }
    if (analysis.declarator_is_function) item.parameters = analysis.parameters;
    if (!analysis.has_comma) {
        item.name_token = analysis.name_token;
        if (item.declaration_kind == NOC_C_DECLARATION_TAG &&
            item.name_token == NOC_TOKEN_INDEX_NONE) {
            item.name_token = analysis.tag_name_token;
        }
    }
    return item;
}

NOCDEF void noc_c_translation_unit_free(Noc_C_Translation_Unit *unit)
{
    free(unit->items);
    memset(unit, 0, sizeof(*unit));
}

NOCDEF bool noc_c_translation_unit_is_valid(const Noc_C_Translation_Unit *unit)
{
    return unit && noc_token_stream_is_valid(unit->stream) &&
           unit->stream_generation == unit->stream->generation &&
           unit->count <= unit->capacity && (unit->count == 0 || unit->items);
}

NOCDEF bool noc_c_translation_unit_build(Noc_Context *context,
                                         const Noc_Syntax_Tree *tree,
                                         Noc_C_Translation_Unit *unit)
{
    Noc_C_Translation_Unit parsed = {0};
    Noc_Location no_location = {0};
    const Noc_Syntax_Node *root;
    size_t first = NOC_SYNTAX_NONE;
    size_t node;
    bool ok = false;
    if (!noc_syntax_tree_is_valid(tree)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "cannot analyze an invalid syntax tree");
        return false;
    }
    parsed.stream = tree->stream;
    parsed.stream_generation = tree->stream_generation;
    root = &tree->items[noc_syntax_root(tree)];
    node = root->first_child;
    while (node != NOC_SYNTAX_NONE) {
        const Noc_Syntax_Node *syntax = &tree->items[node];
        const Noc_Token *token = noc_syntax_token(tree, node);
        size_t next = syntax->next_sibling;
        if (first == NOC_SYNTAX_NONE) {
            if (noc__c_node_is_ignored(tree, node)) {
                node = next;
                continue;
            }
            first = node;
        }
        if (token && noc_token_is_punct(*token, ";")) {
            Noc_Token_Range range = {tree->items[first].range.begin, syntax->range.end};
            Noc_C_External_Item item = noc__c_make_external(tree,
                                                            first,
                                                            node,
                                                            range,
                                                            false,
                                                            (Noc_Token_Range){0, 0});
            if (!noc__c_external_append(&parsed, item)) goto out_of_memory;
            first = NOC_SYNTAX_NONE;
        } else if (syntax->kind == NOC_SYNTAX_BRACE_GROUP) {
            Noc__C_External_Analysis analysis = noc__c_analyze_external(tree,
                                                                        first,
                                                                        node);
            if (analysis.has_declarator && analysis.declarator_is_function &&
                !analysis.has_equal && !analysis.has_typedef) {
                Noc_Token_Range range = {tree->items[first].range.begin,
                                         syntax->range.end};
                Noc_C_External_Item item = noc__c_make_external(tree,
                                                                first,
                                                                node,
                                                                range,
                                                                true,
                                                                syntax->range);
                if (!noc__c_external_append(&parsed, item)) goto out_of_memory;
                first = NOC_SYNTAX_NONE;
            } else if (first == node ||
                       (!analysis.has_tag_specifier && !analysis.has_equal &&
                        !analysis.has_typedef)) {
                Noc_Token_Range range = {tree->items[first].range.begin,
                                         syntax->range.end};
                Noc_C_External_Item item = noc__c_make_external(tree,
                                                                first,
                                                                node,
                                                                range,
                                                                false,
                                                                (Noc_Token_Range){0, 0});
                item.kind = NOC_C_EXTERNAL_UNKNOWN;
                item.declaration_kind = NOC_C_DECLARATION_UNKNOWN;
                item.name_token = NOC_TOKEN_INDEX_NONE;
                item.parameters.begin = NOC_TOKEN_INDEX_NONE;
                item.parameters.end = NOC_TOKEN_INDEX_NONE;
                if (!noc__c_external_append(&parsed, item)) goto out_of_memory;
                first = NOC_SYNTAX_NONE;
            }
        }
        node = next;
    }
    if (first != NOC_SYNTAX_NONE) {
        Noc_Token_Range range = {tree->items[first].range.begin, root->range.end};
        range = noc_token_range_trim_trivia(tree->stream, range);
        if (range.begin != range.end) {
            Noc_C_External_Item item = noc__c_make_external(tree,
                                                            first,
                                                            NOC_SYNTAX_NONE,
                                                            range,
                                                            false,
                                                            (Noc_Token_Range){0, 0});
            item.kind = NOC_C_EXTERNAL_UNKNOWN;
            item.declaration_kind = NOC_C_DECLARATION_UNKNOWN;
            if (!noc__c_external_append(&parsed, item)) goto out_of_memory;
        }
    }
    noc_c_translation_unit_free(unit);
    *unit = parsed;
    memset(&parsed, 0, sizeof(parsed));
    ok = true;
    goto done;

out_of_memory:
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "out of memory while analyzing C translation unit");
done:
    noc_c_translation_unit_free(&parsed);
    return ok;
}

NOCDEF const Noc_C_External_Item *noc_c_external_item(const Noc_C_Translation_Unit *unit,
                                                      size_t item)
{
    if (!noc_c_translation_unit_is_valid(unit) || item >= unit->count) return NULL;
    return &unit->items[item];
}

NOCDEF const char *noc_c_external_kind_name(Noc_C_External_Kind kind)
{
    switch (kind) {
    case NOC_C_EXTERNAL_UNKNOWN: return "unknown external item";
    case NOC_C_EXTERNAL_DECLARATION: return "declaration";
    case NOC_C_EXTERNAL_FUNCTION_DEFINITION: return "function definition";
    }
    return "unknown external item";
}

NOCDEF const char *noc_c_declaration_kind_name(Noc_C_Declaration_Kind kind)
{
    switch (kind) {
    case NOC_C_DECLARATION_UNKNOWN: return "unknown declaration";
    case NOC_C_DECLARATION_OBJECT: return "object declaration";
    case NOC_C_DECLARATION_FUNCTION: return "function declaration";
    case NOC_C_DECLARATION_TYPEDEF: return "typedef declaration";
    case NOC_C_DECLARATION_TAG: return "tag declaration";
    }
    return "unknown declaration";
}

static bool noc__c_delimited_range_is_valid(const Noc_Token_Stream *stream,
                                            Noc_Token_Range range,
                                            const char *open,
                                            const char *close)
{
    Noc_Token_Cursor cursor;
    Noc_Token_Range whole;
    if (!noc_token_cursor_init_range(&cursor, stream, range) ||
        !noc_token_cursor_take_balanced(&cursor, open, close, &whole, NULL)) {
        return false;
    }
    return whole.begin == range.begin && whole.end == range.end &&
           noc_token_cursor_at_end(&cursor);
}

static bool noc__c_token_is_builtin_type(Noc_Token token)
{
    return noc_token_is_identifier(token, "_Atomic") ||
           noc_token_is_identifier(token, "_BitInt") ||
           noc_token_is_identifier(token, "_Bool") ||
           noc_token_is_identifier(token, "_Complex") ||
           noc_token_is_identifier(token, "_Decimal128") ||
           noc_token_is_identifier(token, "_Decimal32") ||
           noc_token_is_identifier(token, "_Decimal64") ||
           noc_token_is_identifier(token, "_Imaginary") ||
           noc_token_is_identifier(token, "bool") ||
           noc_token_is_identifier(token, "char") ||
           noc_token_is_identifier(token, "double") ||
           noc_token_is_identifier(token, "enum") ||
           noc_token_is_identifier(token, "float") ||
           noc_token_is_identifier(token, "int") ||
           noc_token_is_identifier(token, "long") ||
           noc_token_is_identifier(token, "short") ||
           noc_token_is_identifier(token, "signed") ||
           noc_token_is_identifier(token, "struct") ||
           noc_token_is_identifier(token, "typeof") ||
           noc_token_is_identifier(token, "typeof_unqual") ||
           noc_token_is_identifier(token, "union") ||
           noc_token_is_identifier(token, "unsigned") ||
           noc_token_is_identifier(token, "void");
}

static size_t noc__c_next_significant_token(const Noc_Token_Stream *stream,
                                            size_t token,
                                            size_t end)
{
    while (token < end && noc_token_is_trivia(stream->items[token])) token += 1;
    return token;
}

static size_t noc__c_parenthesized_pointer_name(const Noc_Token_Stream *stream,
                                                Noc_Token_Range range)
{
    size_t i;
    size_t parentheses = 0;
    size_t brackets = 0;
    size_t braces = 0;
    for (i = range.begin; i < range.end; ++i) {
        size_t token;
        Noc_Token current = stream->items[i];
        if (noc_token_is_punct(current, "(")) {
            if (parentheses == 0 && brackets == 0 && braces == 0) {
                token = noc__c_next_significant_token(stream, i + 1, range.end);
                if (token < range.end && noc_token_is_punct(stream->items[token], "*")) {
                    do {
                        token = noc__c_next_significant_token(stream,
                                                              token + 1,
                                                              range.end);
                    } while (token < range.end &&
                             noc_token_is_punct(stream->items[token], "*"));
                    while (token < range.end &&
                           (noc_token_is_identifier(stream->items[token], "const") ||
                            noc_token_is_identifier(stream->items[token], "restrict") ||
                            noc_token_is_identifier(stream->items[token], "volatile") ||
                            noc_token_is_identifier(stream->items[token], "_Atomic"))) {
                        token = noc__c_next_significant_token(stream,
                                                              token + 1,
                                                              range.end);
                    }
                    if (token < range.end &&
                        stream->items[token].kind == NOC_TOKEN_IDENTIFIER &&
                        !noc__c_token_is_keyword(stream->items[token]) &&
                        !noc__c_token_is_attribute_name(stream->items[token])) {
                        size_t close = noc__c_next_significant_token(stream,
                                                                    token + 1,
                                                                    range.end);
                        if (close < range.end &&
                            noc_token_is_punct(stream->items[close], ")")) {
                            return token;
                        }
                    }
                }
            }
            parentheses += 1;
        } else if (noc_token_is_punct(current, ")")) {
            if (parentheses > 0) parentheses -= 1;
        } else if (noc_token_is_punct(current, "[")) {
            brackets += 1;
        } else if (noc_token_is_punct(current, "]")) {
            if (brackets > 0) brackets -= 1;
        } else if (noc_token_is_punct(current, "{")) {
            braces += 1;
        } else if (noc_token_is_punct(current, "}")) {
            if (braces > 0) braces -= 1;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static size_t noc__c_parameter_name(const Noc_Token_Stream *stream,
                                    Noc_Token_Range range)
{
    size_t pointer_name = noc__c_parenthesized_pointer_name(stream, range);
    size_t candidate = NOC_TOKEN_INDEX_NONE;
    size_t candidate_depth = SIZE_MAX;
    size_t candidate_count = 0;
    size_t depth = 0;
    size_t i;
    bool has_builtin_type = false;
    bool expect_tag_name = false;
    if (pointer_name != NOC_TOKEN_INDEX_NONE) return pointer_name;
    for (i = range.begin; i < range.end; ++i) {
        Noc_Token token = stream->items[i];
        if (noc_token_is_punct(token, "[")) {
            size_t nested = noc__c_next_significant_token(stream, i + 1, range.end);
            if (nested < range.end && noc_token_is_punct(stream->items[nested], "[")) {
                Noc_Token_Cursor cursor;
                Noc_Token_Range whole;
                if (noc_token_cursor_init_range(&cursor,
                                                stream,
                                                (Noc_Token_Range){i, range.end}) &&
                    noc_token_cursor_take_balanced(&cursor, "[", "]", &whole, NULL) &&
                    whole.begin == i) {
                    i = whole.end - 1;
                    continue;
                }
            }
        }
        if (noc_token_is_punct(token, "(") || noc_token_is_punct(token, "[") ||
            noc_token_is_punct(token, "{")) {
            depth += 1;
            continue;
        }
        if (noc_token_is_punct(token, ")") || noc_token_is_punct(token, "]") ||
            noc_token_is_punct(token, "}")) {
            if (depth > 0) depth -= 1;
            continue;
        }
        if (token.kind != NOC_TOKEN_IDENTIFIER) continue;
        if (noc__c_token_is_attribute_name(token)) {
            size_t group = noc__c_next_significant_token(stream, i + 1, range.end);
            Noc_Token_Cursor cursor;
            Noc_Token_Range whole;
            if (group < range.end &&
                noc_token_cursor_init_range(&cursor,
                                            stream,
                                            (Noc_Token_Range){group, range.end}) &&
                noc_token_cursor_take_balanced(&cursor, "(", ")", &whole, NULL)) {
                i = whole.end - 1;
            }
            continue;
        }
        if (expect_tag_name) {
            expect_tag_name = false;
            continue;
        }
        if (noc_token_is_identifier(token, "struct") ||
            noc_token_is_identifier(token, "union") ||
            noc_token_is_identifier(token, "enum")) {
            has_builtin_type = true;
            expect_tag_name = true;
            continue;
        }
        if (noc__c_token_is_builtin_type(token)) has_builtin_type = true;
        if (noc__c_token_is_keyword(token)) continue;
        if (depth < candidate_depth) {
            candidate = i;
            candidate_depth = depth;
            candidate_count = 1;
        } else if (depth == candidate_depth) {
            candidate = i;
            candidate_count += 1;
        }
    }
    if (candidate_depth != 0 || (candidate_count == 1 && !has_builtin_type)) {
        return NOC_TOKEN_INDEX_NONE;
    }
    return candidate;
}

static bool noc__c_parameter_append(Noc_C_Parameter_List *list,
                                    Noc_C_Parameter parameter)
{
    Noc_C_Parameter *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (list->count > list->capacity) return false;
    if (list->count == list->capacity) {
        if (list->capacity >= maximum) return false;
        if (list->capacity == 0) {
            capacity = maximum < 8 ? maximum : 8;
        } else if (list->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = list->capacity * 2;
        }
        if (capacity <= list->capacity) return false;
        items = (Noc_C_Parameter *)realloc(list->items, capacity * sizeof(*items));
        if (!items) return false;
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = parameter;
    return true;
}

NOCDEF void noc_c_parameter_list_free(Noc_C_Parameter_List *list)
{
    free(list->items);
    memset(list, 0, sizeof(*list));
}

NOCDEF bool noc_c_parse_parameters(const Noc_Token_Stream *stream,
                                   Noc_Token_Range parameters,
                                   Noc_C_Parameter_List *list)
{
    Noc_C_Parameter_List parsed = {0};
    Noc_Argument_List arguments = {0};
    Noc_Token_Range inner;
    size_t i;
    bool ok = false;
    if (!noc__c_delimited_range_is_valid(stream, parameters, "(", ")")) return false;
    inner.begin = parameters.begin + 1;
    inner.end = parameters.end - 1;
    if (!noc_parse_arguments(stream, inner, &arguments)) return false;
    for (i = 0; i < arguments.count; ++i) {
        Noc_C_Parameter parameter;
        Noc_Token_Range trimmed = noc_token_range_trim_trivia(stream,
                                                               arguments.items[i]);
        parameter.range = trimmed;
        parameter.name_token = noc__c_parameter_name(stream, trimmed);
        parameter.is_variadic = trimmed.end == trimmed.begin + 1 &&
                                noc_token_is_punct(stream->items[trimmed.begin], "...");
        if (parameter.is_variadic) parameter.name_token = NOC_TOKEN_INDEX_NONE;
        if (!noc__c_parameter_append(&parsed, parameter)) goto done;
    }
    noc_c_parameter_list_free(list);
    *list = parsed;
    memset(&parsed, 0, sizeof(parsed));
    ok = true;

done:
    noc_argument_list_free(&arguments);
    noc_c_parameter_list_free(&parsed);
    return ok;
}

NOCDEF bool noc_c_compound_statement_is_valid(const Noc_Token_Stream *stream,
                                              Noc_Token_Range compound)
{
    return noc__c_delimited_range_is_valid(stream, compound, "{", "}");
}

NOCDEF Noc_Token_Range noc_c_compound_statement_inner(const Noc_Token_Stream *stream,
                                                      Noc_Token_Range compound)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    if (!noc_c_compound_statement_is_valid(stream, compound)) return invalid;
    compound.begin += 1;
    compound.end -= 1;
    return compound;
}

#endif /* NOC_AST_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
