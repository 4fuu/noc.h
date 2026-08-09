/* noc.h - a single-header toolkit for building explicit, project-local C dialects

   Original Noc code is released into the public domain. Embedded third-party
   parser components retain their licenses; their complete notices and pinned
   versions are included in the generated implementation payload.

   Quick start:

       #define NOC_IMPLEMENTATION
       #include "noc.h"

       int main(int argc, char **argv)
       {
           Noc_Context noc;
           noc_context_init(&noc);
           noc_register_embed_rule(&noc, "embed");
           int result = noc_run_cli(&noc, argc, argv);
           noc_context_deinit(&noc);
           return result;
       }

   A source file can then use an explicitly registered extension while retaining
   its normal .c or .h suffix:

       static const char text[] = @embed("message.txt");

   Compile the transformed output, not the dialect source itself.
*/

#ifndef NOC_H_INCLUDED
#define NOC_H_INCLUDED

#define NOC_VERSION_MAJOR 0
#define NOC_VERSION_MINOR 42
#define NOC_VERSION_PATCH 8
#define NOC_VERSION "0.42.8"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define NOC_TOKEN_INDEX_NONE ((size_t)-1)
#define NOC_SYNTAX_NONE NOC_TOKEN_INDEX_NONE
#define NOC_C_PARSE_NODE_NONE ((size_t)-1)
#define NOC_C_AST_NODE_NONE ((size_t)-1)

#ifndef NOCDEF
#define NOCDEF extern
#endif

#if defined(__GNUC__) || defined(__clang__)
#define NOC_PRINTF_FORMAT(format_index, first_arg) \
    __attribute__((format(printf, format_index, first_arg)))
#else
#define NOC_PRINTF_FORMAT(format_index, first_arg)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *data;
    size_t count;
} Noc_Slice;

typedef struct {
    const char *path;
    size_t offset;
    size_t line;
    size_t column;
} Noc_Location;

typedef enum {
    NOC_TOKEN_EOF = 0,
    NOC_TOKEN_WHITESPACE,
    NOC_TOKEN_NEWLINE,
    NOC_TOKEN_IDENTIFIER,
    NOC_TOKEN_NUMBER,
    NOC_TOKEN_STRING,
    NOC_TOKEN_CHARACTER,
    NOC_TOKEN_LINE_COMMENT,
    NOC_TOKEN_BLOCK_COMMENT,
    NOC_TOKEN_PREPROCESSOR,
    NOC_TOKEN_HEADER_NAME,
    NOC_TOKEN_PUNCTUATOR,
    NOC_TOKEN_OTHER,
    NOC_TOKEN_INVALID,
} Noc_Token_Kind;

typedef struct {
    Noc_Token_Kind kind;
    Noc_Slice text;
    Noc_Location location;
} Noc_Token;

typedef struct {
    const char *path;
    const char *source;
    size_t source_count;
    size_t cursor;
    size_t line;
    size_t column;
    bool beginning_of_line;
    bool continuing_block_comment;
} Noc_Lexer;

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} Noc_Buffer;

typedef struct {
    Noc_Token *items;
    size_t count;
    size_t capacity;
    char *source;
    size_t source_count;
    char *path;
    size_t generation;
} Noc_Token_Stream;

/* Token ranges are half-open: [begin, end). A general range may include the
   terminal EOF token. Parsers that accept ranges exclude EOF from results. */
typedef struct {
    size_t begin;
    size_t end;
} Noc_Token_Range;

typedef struct {
    const Noc_Token_Stream *stream;
    size_t begin;
    size_t index;
    size_t end;
} Noc_Token_Cursor;

typedef enum {
    NOC_PREPROCESSOR_ACTIVITY_UNKNOWN = 0,
    NOC_PREPROCESSOR_ACTIVITY_ACTIVE,
    NOC_PREPROCESSOR_ACTIVITY_INACTIVE,
} Noc_Preprocessor_Activity;

typedef struct {
    const Noc_Token_Stream *stream;
    size_t stream_generation;
    Noc_Preprocessor_Activity *items;
    size_t count;
} Noc_Preprocessor_Map;

typedef struct {
    Noc_Token_Range *items;
    size_t count;
    size_t capacity;
} Noc_Argument_List;

typedef enum {
    NOC_SYNTAX_ROOT = 0,
    NOC_SYNTAX_TOKEN,
    NOC_SYNTAX_PAREN_GROUP,
    NOC_SYNTAX_BRACKET_GROUP,
    NOC_SYNTAX_BRACE_GROUP,
} Noc_Syntax_Kind;

typedef struct {
    Noc_Syntax_Kind kind;
    Noc_Token_Range range;
    size_t parent;
    size_t first_child;
    size_t last_child;
    size_t next_sibling;
} Noc_Syntax_Node;

typedef struct {
    const Noc_Token_Stream *stream;
    size_t stream_generation;
    Noc_Syntax_Node *items;
    size_t count;
    size_t capacity;
} Noc_Syntax_Tree;

typedef enum {
    NOC_C_EXTERNAL_UNKNOWN = 0,
    NOC_C_EXTERNAL_DECLARATION,
    NOC_C_EXTERNAL_FUNCTION_DEFINITION,
} Noc_C_External_Kind;

typedef enum {
    NOC_C_DECLARATION_UNKNOWN = 0,
    NOC_C_DECLARATION_OBJECT,
    NOC_C_DECLARATION_FUNCTION,
    NOC_C_DECLARATION_TYPEDEF,
    NOC_C_DECLARATION_TAG,
} Noc_C_Declaration_Kind;

typedef struct {
    Noc_C_External_Kind kind;
    Noc_C_Declaration_Kind declaration_kind;
    Noc_Token_Range range;
    Noc_Token_Range signature;
    size_t name_token;
    Noc_Token_Range parameters;
    Noc_Token_Range body;
} Noc_C_External_Item;

typedef struct {
    const Noc_Token_Stream *stream;
    size_t stream_generation;
    Noc_C_External_Item *items;
    size_t count;
    size_t capacity;
} Noc_C_Translation_Unit;

typedef struct {
    Noc_Token_Range range;
    size_t name_token;
    bool is_variadic;
} Noc_C_Parameter;

typedef struct {
    Noc_C_Parameter *items;
    size_t count;
    size_t capacity;
} Noc_C_Parameter_List;

typedef struct {
    Noc_Token_Range range;
    char *replacement;
    size_t replacement_count;
} Noc_Edit;

typedef struct {
    const Noc_Token_Stream *stream;
    size_t stream_generation;
    Noc_Edit *items;
    size_t count;
    size_t capacity;
} Noc_Edit_Set;

typedef enum {
    NOC_DIAGNOSTIC_NOTE = 0,
    NOC_DIAGNOSTIC_WARNING,
    NOC_DIAGNOSTIC_ERROR,
} Noc_Diagnostic_Severity;

typedef struct {
    Noc_Diagnostic_Severity severity;
    Noc_Location location;
    const char *message;
} Noc_Diagnostic;

typedef void (*Noc_Diagnostic_Fn)(void *user_data, const Noc_Diagnostic *diagnostic);

typedef enum {
    NOC_RULE_TOKEN = 0,
    NOC_RULE_EXPRESSION,
    NOC_RULE_STATEMENT,
    NOC_RULE_DECLARATION,
    NOC_RULE_ATTRIBUTE,
    NOC_RULE_DIRECTIVE,
} Noc_Rule_Scope;

typedef enum {
    NOC_RULE_TRIGGER_AT_NAME = 0,
    NOC_RULE_TRIGGER_PATTERN,
} Noc_Rule_Trigger_Kind;

typedef struct Noc_Context Noc_Context;
typedef struct Noc_Rewriter Noc_Rewriter;
typedef struct Noc_Rule Noc_Rule;

typedef bool (*Noc_Expand_Fn)(Noc_Rewriter *rewriter,
                              const Noc_Rule *rule,
                              void *user_data);

struct Noc_Rule {
    const char *name;
    Noc_Rule_Scope scope;
    const char *syntax;
    const char *description;
    Noc_Expand_Fn expand;
    void *user_data;
};

typedef struct {
    bool emit_line_directives;
    bool unknown_rule_is_error;
    bool skip_inactive_preprocessor_branches;
    bool disabled_rule_is_error;
} Noc_Options;

typedef struct {
    const char *include_guard;
    const char *macro_prefix;
    const char *dialect_name;
    bool omit_descriptions;
} Noc_Ide_Metadata_Options;

typedef struct {
    const char *input_root;
    const char *output_root;
    bool emit_depfiles;
} Noc_Batch_Options;

struct Noc_Context {
    Noc_Rule *rules;
    const char **rule_patterns;
    bool *rule_enabled;
    size_t rules_count;
    size_t rules_capacity;
    size_t active_transforms;
    Noc_Diagnostic_Fn diagnostic;
    void *diagnostic_user_data;
    Noc_Options options;
    size_t error_count;
};

typedef struct {
    char *output;
    size_t output_count;
    char **dependencies;
    size_t dependency_count;
    size_t error_count;
} Noc_Transform_Result;

/* Incremental source workspace. Paths are copied and compared as exact,
   case-sensitive byte strings without filesystem canonicalization. File IDs are
   stable only within one live workspace and are never reused by another path.

   Noc_Workspace and Noc_Document_Snapshot are owning C handles initialized with
   {0}; do not shallow-copy an initialized handle. Use
   noc_document_snapshot_clone() to create another owning snapshot handle. */
typedef size_t Noc_File_Id;
#define NOC_FILE_ID_NONE ((Noc_File_Id)-1)

typedef enum {
    NOC_SOURCE_CLASS_PROJECT = 0,
    NOC_SOURCE_CLASS_TRUSTED,
    NOC_SOURCE_CLASS_SYSTEM,
    NOC_SOURCE_CLASS_GENERATED,
} Noc_Source_Class;

typedef enum {
    NOC_WORKSPACE_OK = 0,
    NOC_WORKSPACE_INVALID_ARGUMENT,
    NOC_WORKSPACE_ALREADY_OPEN,
    NOC_WORKSPACE_NOT_CURRENT,
    NOC_WORKSPACE_NOT_FOUND,
    NOC_WORKSPACE_OUT_OF_RANGE,
    NOC_WORKSPACE_INVALID_EDIT,
    NOC_WORKSPACE_OUT_OF_MEMORY,
    NOC_WORKSPACE_LIMIT_EXCEEDED,
} Noc_Workspace_Status;

typedef struct Noc_Workspace_Impl Noc_Workspace_Impl;
typedef struct Noc_Document_Snapshot_Impl Noc_Document_Snapshot_Impl;

typedef struct {
    Noc_Workspace_Impl *impl;
} Noc_Workspace;

typedef struct {
    Noc_Document_Snapshot_Impl *impl;
} Noc_Document_Snapshot;

/* Text edit ranges are half-open byte offsets in one expected snapshot. The
   replacement is borrowed for the duration of noc_workspace_edit_document(). */
typedef struct {
    size_t begin;
    size_t end;
    Noc_Slice replacement;
} Noc_Text_Edit;

/* Stable syntax-expectation vocabulary shared by parser-state candidate hints
   and normalized-AST MISSING nodes. Exact punctuation and keywords carry a
   spelling; structural categories such as TYPE or EXPRESSION do not. */
typedef enum {
    NOC_C_AST_EXPECTED_NONE = 0,
    NOC_C_AST_EXPECTED_UNKNOWN,
    NOC_C_AST_EXPECTED_PUNCTUATOR,
    NOC_C_AST_EXPECTED_KEYWORD,
    NOC_C_AST_EXPECTED_IDENTIFIER,
    NOC_C_AST_EXPECTED_TYPE,
    NOC_C_AST_EXPECTED_DECLARATION,
    NOC_C_AST_EXPECTED_STATEMENT,
    NOC_C_AST_EXPECTED_EXPRESSION,
} Noc_C_Ast_Expected_Kind;

/* Recoverable physical C syntax for compiler and IDE clients. This parser
   recognizes the embedded tree-sitter-c grammar over one immutable document
   snapshot. It does not preprocess macros, select conditional branches, or
   claim that every grammar extension is enabled by a Noc feature profile;
   preprocessing, feature validation, semantic analysis, and lowering remain
   separate phases.

   Byte ranges are half-open [begin,end) positions in the retained physical
   snapshot. They deliberately are not Noc_Token_Range values: error recovery
   and zero-width missing nodes need not align with physical tokens, and future
   macro-expanded ASTs use a separate logical coordinate/provenance domain.
   Ordinary whitespace is available through the retained snapshot but is not a
   syntax node. Comments are EXTRA nodes. The translation-unit root spans the
   complete snapshot even when recovery skips an otherwise unrecognized edge
   byte; grammar children retain their exact recognized ranges and the root
   receives SKIPPED_SOURCE so callers never mistake normalization for complete
   recognition.

   Noc_C_Parse_Tree is an owning handle initialized with {0}. Do not shallow
   copy it. A successful build retains its own snapshot and transactionally
   replaces output; invalid input, cancellation, limits, adapter allocation
   failure, and engine failure preserve the previous tree and generation.
   Recoverable malformed/incomplete editor input is a successful build whose
   nodes carry ERROR, MISSING, and HAS_ERROR flags. A successful rebuild
   increments tree.generation and invalidates all earlier node pointers and
   indices; each published node records the generation in which it is valid.

   Builds use a fresh parser and have no mutable global parser state, but one
   owning tree must not be read while another thread rebuilds or frees it. The
   node limit bounds the published flat tree, not Tree-sitter's private tree
   allocation before flattening. Cancellation is cooperative. Allocation
   failure in Noc's snapshot/node adapter returns OUT_OF_MEMORY; the pinned
   upstream runtime deliberately retains its fail-fast policy for its own
   internal positive-size allocations. */
typedef struct {
    size_t begin;
    size_t end;
} Noc_Byte_Range;

typedef enum {
    NOC_C_PARSE_OK = 0,
    NOC_C_PARSE_INVALID_ARGUMENT,
    NOC_C_PARSE_CANCELLED,
    NOC_C_PARSE_LIMIT_EXCEEDED,
    NOC_C_PARSE_GENERATION_EXHAUSTED,
    NOC_C_PARSE_OUT_OF_MEMORY,
    NOC_C_PARSE_ENGINE_FAILURE,
} Noc_C_Parse_Status;

typedef bool (*Noc_C_Parse_Cancel_Fn)(void *user_data);

typedef struct {
    /* Both limits must be nonzero. Source bytes are also limited by the
       embedded engine's uint32_t input ABI. */
    size_t max_source_bytes;
    size_t max_nodes;
    Noc_C_Parse_Cancel_Fn should_cancel;
    void *cancel_user_data;
} Noc_C_Parse_Options;

enum {
    /* The candidate was normalized from the retained grammar parse state. */
    NOC_C_GRAMMAR_CANDIDATE_LOOKAHEAD = 1u << 0,
    /* A parser-materialized MISSING node at the query offset also requested it. */
    NOC_C_GRAMMAR_CANDIDATE_MISSING = 1u << 1,
    /* Exact spelling is accepted by the permissive embedded grammar but is
       not an ISO C11 spelling. Feature policy still decides permission. */
    NOC_C_GRAMMAR_CANDIDATE_NON_C11 = 1u << 2,
};

typedef struct {
    Noc_C_Ast_Expected_Kind kind;
    /* Owned by the containing Noc_C_Grammar_Candidates result. Exact keywords
       and punctuators are nonempty; structural categories are empty. */
    Noc_Slice spelling;
    unsigned int flags;
} Noc_C_Grammar_Candidate;

enum {
    /* A usable retained parser state contributed lookahead candidates. */
    NOC_C_GRAMMAR_CANDIDATES_STATE_AVAILABLE = 1u << 0,
    /* ERROR/MISSING recovery or a fallback state selected the anchor. */
    NOC_C_GRAMMAR_CANDIDATES_RECOVERY_HEURISTIC = 1u << 1,
    /* OFFSET is inside one physical grammar leaf. REPLACEMENT is that whole
       leaf; candidates describe replacing it, not lexing its suffix. */
    NOC_C_GRAMMAR_CANDIDATES_TOKEN_REPLACEMENT = 1u << 2,
    /* More distinct normalized candidates existed than max_candidates. */
    NOC_C_GRAMMAR_CANDIDATES_TRUNCATED = 1u << 3,
};

typedef struct {
    /* All three limits must be nonzero. max_candidates bounds published
       candidates after stable sorting and deduplication. */
    size_t max_candidates;
    /* Total flat CST node visits, including a possible ERROR-subtree pass. */
    size_t max_nodes_examined;
    /* Maximum raw symbols consumed from one grammar lookahead iterator. */
    size_t max_symbols_examined;
    Noc_C_Parse_Cancel_Fn should_cancel;
    void *cancel_user_data;
} Noc_C_Grammar_Candidate_Options;

/* Owning retained-CST grammar hints for one physical cursor offset. Initialize
   with {0}, do not shallow-copy, and free with
   noc_c_grammar_candidates_free(). Candidate storage remains valid after the
   source tree is rebuilt or freed; file/document/tree generations identify the
   immutable revision from which it was derived. REPLACEMENT is an empty range
   at ordinary token boundaries and in trivia, or a whole grammar leaf when
   TOKEN_REPLACEMENT is set.

   Candidate membership is grammar-derived and non-exhaustive. It does not
   account for typedef symbols, macro state, target/feature policy, external
   scanner constraints, or semantic visibility. Parser upgrades may change
   membership, while Noc's kinds, flags, sorting, and deduplication remain the
   stable contract. */
typedef struct {
    Noc_C_Grammar_Candidate *items;
    size_t count;
    size_t capacity;
    char *spelling_storage;
    size_t spelling_storage_count;
    size_t offset;
    Noc_Byte_Range replacement;
    Noc_File_Id file_id;
    size_t document_generation;
    size_t parse_tree_generation;
    size_t generation;
    unsigned int flags;
} Noc_C_Grammar_Candidates;

enum {
    NOC_C_PARSE_NODE_NAMED = 1u << 0,
    NOC_C_PARSE_NODE_EXTRA = 1u << 1,
    NOC_C_PARSE_NODE_ERROR = 1u << 2,
    NOC_C_PARSE_NODE_MISSING = 1u << 3,
    NOC_C_PARSE_NODE_HAS_ERROR = 1u << 4,
    /* The engine did not include one or more physical edge bytes in its root.
       Noc still publishes a document-wide root but records that recovery. */
    NOC_C_PARSE_NODE_SKIPPED_SOURCE = 1u << 5,
};

typedef struct {
    Noc_Byte_Range bytes;
    size_t parent;
    size_t first_child;
    size_t last_child;
    size_t next_sibling;
    size_t child_count;
    /* Grammar-layer labels such as "function_definition" and parent fields
       such as "body". They are immutable borrowed strings owned by Noc's
       embedded grammar, not stable numeric semantic classifications. */
    Noc_Slice kind;
    Noc_Slice field;
    size_t generation;
    unsigned int flags;
} Noc_C_Parse_Node;

typedef struct Noc_C_Parse_Tree_Impl Noc_C_Parse_Tree_Impl;

typedef struct {
    Noc_C_Parse_Tree_Impl *impl;
    size_t generation;
} Noc_C_Parse_Tree;

NOCDEF const char *noc_workspace_status_name(Noc_Workspace_Status status);
/* init requires an uninitialized or deinitialized workspace. Workspace and
   snapshot operations are not safe concurrently on a shared object graph;
   disjoint workspaces have no mutable global workspace state. */
NOCDEF void noc_workspace_init(Noc_Workspace *workspace);
NOCDEF void noc_workspace_deinit(Noc_Workspace *workspace);

/* These operations copy input bytes. Owning outputs are replaced only on
   success and otherwise remain unchanged. update accepts output == expected;
   its expected snapshot must be the exact current revision. */
NOCDEF Noc_Workspace_Status noc_workspace_open_document(
    Noc_Workspace *workspace,
    const char *path,
    const char *source,
    size_t source_count,
    Noc_Source_Class source_class,
    Noc_Document_Snapshot *output);
NOCDEF Noc_Workspace_Status noc_workspace_update_document(
    Noc_Workspace *workspace,
    const Noc_Document_Snapshot *expected,
    const char *source,
    size_t source_count,
    Noc_Document_Snapshot *output);
/* Edits must be ordered by begin and non-overlapping in expected-snapshot byte
   coordinates. Adjacent edits and multiple insertions at one offset are valid. */
NOCDEF Noc_Workspace_Status noc_workspace_edit_document(
    Noc_Workspace *workspace,
    const Noc_Document_Snapshot *expected,
    const Noc_Text_Edit *edits,
    size_t edits_count,
    Noc_Document_Snapshot *output);
NOCDEF Noc_Workspace_Status noc_workspace_close_document(
    Noc_Workspace *workspace,
    const Noc_Document_Snapshot *expected);
NOCDEF Noc_Workspace_Status noc_workspace_current_document(
    const Noc_Workspace *workspace,
    Noc_File_Id file_id,
    Noc_Document_Snapshot *output);
NOCDEF Noc_Workspace_Status noc_workspace_find_document(
    const Noc_Workspace *workspace,
    const char *path,
    Noc_Document_Snapshot *output);
NOCDEF bool noc_document_snapshot_is_current(
    const Noc_Workspace *workspace,
    const Noc_Document_Snapshot *snapshot);

NOCDEF Noc_Workspace_Status noc_document_snapshot_clone(
    const Noc_Document_Snapshot *source,
    Noc_Document_Snapshot *output);
NOCDEF void noc_document_snapshot_free(Noc_Document_Snapshot *snapshot);
NOCDEF bool noc_document_snapshot_is_valid(const Noc_Document_Snapshot *snapshot);
NOCDEF Noc_File_Id noc_document_snapshot_file_id(
    const Noc_Document_Snapshot *snapshot);
NOCDEF size_t noc_document_snapshot_generation(
    const Noc_Document_Snapshot *snapshot);
NOCDEF const char *noc_document_snapshot_path(
    const Noc_Document_Snapshot *snapshot);
NOCDEF Noc_Slice noc_document_snapshot_source(
    const Noc_Document_Snapshot *snapshot);
NOCDEF Noc_Source_Class noc_document_snapshot_source_class(
    const Noc_Document_Snapshot *snapshot);

/* Physical offsets include EOF (0..source_count). Lines and byte columns are
   1-based. CRLF counts as one newline but both bytes retain distinct columns;
   tabs and multibyte text are counted as original bytes. Returned paths borrow
   storage from the retained snapshot. Scalar outputs are preserved on failure. */
NOCDEF Noc_Workspace_Status noc_document_snapshot_location(
    const Noc_Document_Snapshot *snapshot,
    size_t offset,
    Noc_Location *output);
NOCDEF Noc_Workspace_Status noc_document_snapshot_offset(
    const Noc_Document_Snapshot *snapshot,
    size_t line,
    size_t byte_column,
    size_t *output);

NOCDEF Noc_C_Parse_Options noc_c_parse_default_options(void);
NOCDEF const char *noc_c_parse_status_name(Noc_C_Parse_Status status);
NOCDEF Noc_C_Parse_Status noc_c_parse_tree_build(
    const Noc_Document_Snapshot *snapshot,
    Noc_C_Parse_Options options,
    Noc_C_Parse_Tree *output);
NOCDEF void noc_c_parse_tree_free(Noc_C_Parse_Tree *tree);
NOCDEF bool noc_c_parse_tree_is_valid(const Noc_C_Parse_Tree *tree);
NOCDEF size_t noc_c_parse_tree_generation(const Noc_C_Parse_Tree *tree);
/* The returned snapshot is a borrowed immutable view retained by tree. */
NOCDEF const Noc_Document_Snapshot *noc_c_parse_tree_snapshot(
    const Noc_C_Parse_Tree *tree);
NOCDEF size_t noc_c_parse_tree_node_count(const Noc_C_Parse_Tree *tree);
NOCDEF size_t noc_c_parse_tree_root(const Noc_C_Parse_Tree *tree);
/* Node pointers remain valid only until a successful rebuild or free. */
NOCDEF const Noc_C_Parse_Node *noc_c_parse_tree_node_at(
    const Noc_C_Parse_Tree *tree,
    size_t node_index);
NOCDEF bool noc_c_parse_tree_has_error(const Noc_C_Parse_Tree *tree);
/* Invalid tree/index returns {0}; a valid zero-width node returns an empty slice
   whose data points at its insertion byte in the retained snapshot. */
NOCDEF Noc_Slice noc_c_parse_node_source(const Noc_C_Parse_Tree *tree,
                                         size_t node_index);
NOCDEF Noc_Location noc_c_parse_node_location(const Noc_C_Parse_Tree *tree,
                                              size_t node_index);
NOCDEF Noc_C_Grammar_Candidate_Options
noc_c_grammar_candidate_default_options(void);
NOCDEF void noc_c_grammar_candidates_free(
    Noc_C_Grammar_Candidates *candidates);
NOCDEF bool noc_c_grammar_candidates_is_valid(
    const Noc_C_Grammar_Candidates *candidates);
/* Build is transactional. OFFSET accepts BOF through EOF. max_candidates is a
   successful truncation bound; exhausting node/symbol work, cancellation,
   invalid input, allocation failure, or generation exhaustion preserves the
   previous output. The stable result order is kind then spelling bytes, and
   duplicate lookahead/MISSING candidates are merged by OR-ing origin flags. */
NOCDEF Noc_C_Parse_Status noc_c_parse_grammar_candidates_build(
    const Noc_C_Parse_Tree *tree,
    size_t offset,
    Noc_C_Grammar_Candidate_Options options,
    Noc_C_Grammar_Candidates *output);
NOCDEF const Noc_C_Grammar_Candidate *noc_c_grammar_candidate_at(
    const Noc_C_Grammar_Candidates *candidates,
    size_t index);

/* Noc-owned normalized physical C AST. This is a stable compiler-facing
   classification layer, not a public view of the embedded parser: nodes carry
   Noc enums rather than grammar symbol IDs or borrowed grammar strings. Named
   non-trivia CST nodes are retained, and anonymous ERROR/MISSING recovery nodes
   are represented explicitly. Operators and keyword spellings that the
   concrete grammar stores in anonymous children are normalized into typed
   side queries instead of being lost.

   Every range is a half-open physical spelling range in the AST's independently
   retained immutable snapshot. Macro expansion, include provenance, and future
   logical ranges are deliberately separate; recognizing GNU, Microsoft, or C23
   syntax does not grant permission to use it under a feature profile.

   Noc_C_Ast is an owning handle initialized with {0}; do not shallow-copy it.
   A successful build clones the parse tree snapshot and then has no lifetime
   dependency on that tree. Failed builds preserve the prior AST exactly. Each
   successful rebuild increments the AST generation and invalidates prior node
   pointers and indices. The document generation identifies the retained source
   revision and is independent of the AST generation. Shared-handle rebuild/free
   and reads require external synchronization. */
typedef enum {
    NOC_C_AST_KIND_UNKNOWN = 0,
    NOC_C_AST_KIND_ABSTRACT_DECLARATOR,
    NOC_C_AST_KIND_DECLARATOR,
    NOC_C_AST_KIND_FIELD_DECLARATOR,
    NOC_C_AST_KIND_TYPE_DECLARATOR,
    NOC_C_AST_KIND_EXPRESSION,
    NOC_C_AST_KIND_STATEMENT,
    NOC_C_AST_KIND_TYPE_SPECIFIER,
    NOC_C_AST_KIND_ABSTRACT_ARRAY_DECLARATOR,
    NOC_C_AST_KIND_ABSTRACT_FUNCTION_DECLARATOR,
    NOC_C_AST_KIND_ABSTRACT_PARENTHESIZED_DECLARATOR,
    NOC_C_AST_KIND_ABSTRACT_POINTER_DECLARATOR,
    NOC_C_AST_KIND_ALIGNAS_QUALIFIER,
    NOC_C_AST_KIND_ALIGNOF_EXPRESSION,
    NOC_C_AST_KIND_ARGUMENT_LIST,
    NOC_C_AST_KIND_ARRAY_DECLARATOR,
    NOC_C_AST_KIND_ASSIGNMENT_EXPRESSION,
    NOC_C_AST_KIND_ATTRIBUTE,
    NOC_C_AST_KIND_ATTRIBUTE_DECLARATION,
    NOC_C_AST_KIND_ATTRIBUTE_SPECIFIER,
    NOC_C_AST_KIND_ATTRIBUTED_DECLARATOR,
    NOC_C_AST_KIND_ATTRIBUTED_STATEMENT,
    NOC_C_AST_KIND_BINARY_EXPRESSION,
    NOC_C_AST_KIND_BITFIELD_CLAUSE,
    NOC_C_AST_KIND_BREAK_STATEMENT,
    NOC_C_AST_KIND_CALL_EXPRESSION,
    NOC_C_AST_KIND_CASE_STATEMENT,
    NOC_C_AST_KIND_CAST_EXPRESSION,
    NOC_C_AST_KIND_CHAR_LITERAL,
    NOC_C_AST_KIND_COMMA_EXPRESSION,
    NOC_C_AST_KIND_COMPOUND_LITERAL_EXPRESSION,
    NOC_C_AST_KIND_COMPOUND_STATEMENT,
    NOC_C_AST_KIND_CONCATENATED_STRING,
    NOC_C_AST_KIND_CONDITIONAL_EXPRESSION,
    NOC_C_AST_KIND_CONTINUE_STATEMENT,
    NOC_C_AST_KIND_DECLARATION,
    NOC_C_AST_KIND_DECLARATION_LIST,
    NOC_C_AST_KIND_DO_STATEMENT,
    NOC_C_AST_KIND_ELSE_CLAUSE,
    NOC_C_AST_KIND_ENUM_SPECIFIER,
    NOC_C_AST_KIND_ENUMERATOR,
    NOC_C_AST_KIND_ENUMERATOR_LIST,
    NOC_C_AST_KIND_EXPRESSION_STATEMENT,
    NOC_C_AST_KIND_EXTENSION_EXPRESSION,
    NOC_C_AST_KIND_FIELD_DECLARATION,
    NOC_C_AST_KIND_FIELD_DECLARATION_LIST,
    NOC_C_AST_KIND_FIELD_DESIGNATOR,
    NOC_C_AST_KIND_FIELD_EXPRESSION,
    NOC_C_AST_KIND_FOR_STATEMENT,
    NOC_C_AST_KIND_FUNCTION_DECLARATOR,
    NOC_C_AST_KIND_FUNCTION_DEFINITION,
    NOC_C_AST_KIND_GENERIC_EXPRESSION,
    NOC_C_AST_KIND_GNU_ASM_CLOBBER_LIST,
    NOC_C_AST_KIND_GNU_ASM_EXPRESSION,
    NOC_C_AST_KIND_GNU_ASM_GOTO_LIST,
    NOC_C_AST_KIND_GNU_ASM_INPUT_OPERAND,
    NOC_C_AST_KIND_GNU_ASM_INPUT_OPERAND_LIST,
    NOC_C_AST_KIND_GNU_ASM_OUTPUT_OPERAND,
    NOC_C_AST_KIND_GNU_ASM_OUTPUT_OPERAND_LIST,
    NOC_C_AST_KIND_GNU_ASM_QUALIFIER,
    NOC_C_AST_KIND_GOTO_STATEMENT,
    NOC_C_AST_KIND_IF_STATEMENT,
    NOC_C_AST_KIND_INIT_DECLARATOR,
    NOC_C_AST_KIND_INITIALIZER_LIST,
    NOC_C_AST_KIND_INITIALIZER_PAIR,
    NOC_C_AST_KIND_LABELED_STATEMENT,
    NOC_C_AST_KIND_LINKAGE_SPECIFICATION,
    NOC_C_AST_KIND_MACRO_TYPE_SPECIFIER,
    NOC_C_AST_KIND_MS_BASED_MODIFIER,
    NOC_C_AST_KIND_MS_CALL_MODIFIER,
    NOC_C_AST_KIND_MS_DECLSPEC_MODIFIER,
    NOC_C_AST_KIND_MS_POINTER_MODIFIER,
    NOC_C_AST_KIND_MS_UNALIGNED_PTR_MODIFIER,
    NOC_C_AST_KIND_NULL,
    NOC_C_AST_KIND_OFFSETOF_EXPRESSION,
    NOC_C_AST_KIND_PARAMETER_DECLARATION,
    NOC_C_AST_KIND_PARAMETER_LIST,
    NOC_C_AST_KIND_PARENTHESIZED_DECLARATOR,
    NOC_C_AST_KIND_PARENTHESIZED_EXPRESSION,
    NOC_C_AST_KIND_POINTER_DECLARATOR,
    NOC_C_AST_KIND_POINTER_EXPRESSION,
    NOC_C_AST_KIND_PREPROC_CALL,
    NOC_C_AST_KIND_PREPROC_DEF,
    NOC_C_AST_KIND_PREPROC_DEFINED,
    NOC_C_AST_KIND_PREPROC_ELIF,
    NOC_C_AST_KIND_PREPROC_ELIFDEF,
    NOC_C_AST_KIND_PREPROC_ELSE,
    NOC_C_AST_KIND_PREPROC_FUNCTION_DEF,
    NOC_C_AST_KIND_PREPROC_IF,
    NOC_C_AST_KIND_PREPROC_IFDEF,
    NOC_C_AST_KIND_PREPROC_INCLUDE,
    NOC_C_AST_KIND_PREPROC_PARAMS,
    NOC_C_AST_KIND_RETURN_STATEMENT,
    NOC_C_AST_KIND_SEH_EXCEPT_CLAUSE,
    NOC_C_AST_KIND_SEH_FINALLY_CLAUSE,
    NOC_C_AST_KIND_SEH_LEAVE_STATEMENT,
    NOC_C_AST_KIND_SEH_TRY_STATEMENT,
    NOC_C_AST_KIND_SIZED_TYPE_SPECIFIER,
    NOC_C_AST_KIND_SIZEOF_EXPRESSION,
    NOC_C_AST_KIND_STORAGE_CLASS_SPECIFIER,
    NOC_C_AST_KIND_STRING_LITERAL,
    NOC_C_AST_KIND_STRUCT_SPECIFIER,
    NOC_C_AST_KIND_SUBSCRIPT_DESIGNATOR,
    NOC_C_AST_KIND_SUBSCRIPT_EXPRESSION,
    NOC_C_AST_KIND_SUBSCRIPT_RANGE_DESIGNATOR,
    NOC_C_AST_KIND_SWITCH_STATEMENT,
    NOC_C_AST_KIND_TRANSLATION_UNIT,
    NOC_C_AST_KIND_TYPE_DEFINITION,
    NOC_C_AST_KIND_TYPE_DESCRIPTOR,
    NOC_C_AST_KIND_TYPE_QUALIFIER,
    NOC_C_AST_KIND_UNARY_EXPRESSION,
    NOC_C_AST_KIND_UNION_SPECIFIER,
    NOC_C_AST_KIND_UPDATE_EXPRESSION,
    NOC_C_AST_KIND_VARIADIC_PARAMETER,
    NOC_C_AST_KIND_WHILE_STATEMENT,
    NOC_C_AST_KIND_CHARACTER,
    NOC_C_AST_KIND_COMMENT,
    NOC_C_AST_KIND_ESCAPE_SEQUENCE,
    NOC_C_AST_KIND_FALSE,
    NOC_C_AST_KIND_FIELD_IDENTIFIER,
    NOC_C_AST_KIND_IDENTIFIER,
    NOC_C_AST_KIND_MS_RESTRICT_MODIFIER,
    NOC_C_AST_KIND_MS_SIGNED_PTR_MODIFIER,
    NOC_C_AST_KIND_MS_UNSIGNED_PTR_MODIFIER,
    NOC_C_AST_KIND_NUMBER_LITERAL,
    NOC_C_AST_KIND_PREPROC_ARG,
    NOC_C_AST_KIND_PREPROC_DIRECTIVE,
    NOC_C_AST_KIND_PRIMITIVE_TYPE,
    NOC_C_AST_KIND_STATEMENT_IDENTIFIER,
    NOC_C_AST_KIND_STRING_CONTENT,
    NOC_C_AST_KIND_SYSTEM_LIB_STRING,
    NOC_C_AST_KIND_TRUE,
    NOC_C_AST_KIND_TYPE_IDENTIFIER,
    /* ISO C11 `_Static_assert(condition, "message");`; the recognized C23
       `static_assert` alias has the same shape and a distinct extension tag. */
    NOC_C_AST_KIND_STATIC_ASSERT_DECLARATION,
    /* ISO C11 atomic type specifier `_Atomic(type-name)`. Its TYPE field is a
       TYPE_DESCRIPTOR; qualifier spelling `_Atomic T` remains TYPE_QUALIFIER. */
    NOC_C_AST_KIND_ATOMIC_TYPE_SPECIFIER,
    NOC_C_AST_KIND_ERROR,
    NOC_C_AST_KIND_MISSING,
} Noc_C_Ast_Kind;

typedef enum {
    NOC_C_AST_FIELD_NONE = 0,
    NOC_C_AST_FIELD_UNKNOWN,
    NOC_C_AST_FIELD_ALTERNATIVE,
    NOC_C_AST_FIELD_ARGUMENT,
    NOC_C_AST_FIELD_ARGUMENTS,
    NOC_C_AST_FIELD_ASSEMBLY_CODE,
    NOC_C_AST_FIELD_BODY,
    NOC_C_AST_FIELD_CLOBBERS,
    NOC_C_AST_FIELD_CONDITION,
    NOC_C_AST_FIELD_CONSEQUENCE,
    NOC_C_AST_FIELD_CONSTRAINT,
    NOC_C_AST_FIELD_DECLARATOR,
    NOC_C_AST_FIELD_DESIGNATOR,
    NOC_C_AST_FIELD_DIRECTIVE,
    NOC_C_AST_FIELD_END,
    NOC_C_AST_FIELD_FIELD,
    NOC_C_AST_FIELD_FILTER,
    NOC_C_AST_FIELD_FUNCTION,
    NOC_C_AST_FIELD_GOTO_LABELS,
    NOC_C_AST_FIELD_INDEX,
    NOC_C_AST_FIELD_INITIALIZER,
    NOC_C_AST_FIELD_INPUT_OPERANDS,
    NOC_C_AST_FIELD_LABEL,
    NOC_C_AST_FIELD_LEFT,
    NOC_C_AST_FIELD_MEMBER,
    NOC_C_AST_FIELD_NAME,
    NOC_C_AST_FIELD_OPERAND,
    NOC_C_AST_FIELD_OPERATOR,
    NOC_C_AST_FIELD_OUTPUT_OPERANDS,
    NOC_C_AST_FIELD_PARAMETERS,
    NOC_C_AST_FIELD_PATH,
    NOC_C_AST_FIELD_PREFIX,
    NOC_C_AST_FIELD_REGISTER,
    NOC_C_AST_FIELD_RIGHT,
    NOC_C_AST_FIELD_SIZE,
    NOC_C_AST_FIELD_START,
    NOC_C_AST_FIELD_SYMBOL,
    NOC_C_AST_FIELD_TYPE,
    NOC_C_AST_FIELD_UNDERLYING_TYPE,
    NOC_C_AST_FIELD_UPDATE,
    NOC_C_AST_FIELD_VALUE,
    /* String-literal child of a static assertion; absent for one-argument C23. */
    NOC_C_AST_FIELD_MESSAGE,
} Noc_C_Ast_Field;

/* Operators are classified by syntactic role. In particular, unary positive
   and negative plus pointer-expression address/dereference are distinct from
   arithmetic and bitwise forms. Updates preserve prefix/postfix placement. */
typedef enum {
    NOC_C_AST_OPERATOR_NONE = 0,
    NOC_C_AST_OPERATOR_UNKNOWN,
    NOC_C_AST_OPERATOR_ADD,
    NOC_C_AST_OPERATOR_SUBTRACT,
    NOC_C_AST_OPERATOR_MULTIPLY,
    NOC_C_AST_OPERATOR_DIVIDE,
    NOC_C_AST_OPERATOR_REMAINDER,
    NOC_C_AST_OPERATOR_ASSIGN,
    NOC_C_AST_OPERATOR_ADD_ASSIGN,
    NOC_C_AST_OPERATOR_SUBTRACT_ASSIGN,
    NOC_C_AST_OPERATOR_MULTIPLY_ASSIGN,
    NOC_C_AST_OPERATOR_DIVIDE_ASSIGN,
    NOC_C_AST_OPERATOR_REMAINDER_ASSIGN,
    NOC_C_AST_OPERATOR_SHIFT_LEFT_ASSIGN,
    NOC_C_AST_OPERATOR_SHIFT_RIGHT_ASSIGN,
    NOC_C_AST_OPERATOR_BIT_AND_ASSIGN,
    NOC_C_AST_OPERATOR_BIT_XOR_ASSIGN,
    NOC_C_AST_OPERATOR_BIT_OR_ASSIGN,
    NOC_C_AST_OPERATOR_BIT_AND,
    NOC_C_AST_OPERATOR_BIT_OR,
    NOC_C_AST_OPERATOR_BIT_XOR,
    NOC_C_AST_OPERATOR_LOGICAL_AND,
    NOC_C_AST_OPERATOR_LOGICAL_OR,
    NOC_C_AST_OPERATOR_EQUAL,
    NOC_C_AST_OPERATOR_NOT_EQUAL,
    NOC_C_AST_OPERATOR_LESS,
    NOC_C_AST_OPERATOR_LESS_EQUAL,
    NOC_C_AST_OPERATOR_GREATER,
    NOC_C_AST_OPERATOR_GREATER_EQUAL,
    NOC_C_AST_OPERATOR_SHIFT_LEFT,
    NOC_C_AST_OPERATOR_SHIFT_RIGHT,
    NOC_C_AST_OPERATOR_LOGICAL_NOT,
    NOC_C_AST_OPERATOR_BIT_NOT,
    NOC_C_AST_OPERATOR_POSITIVE,
    NOC_C_AST_OPERATOR_NEGATIVE,
    NOC_C_AST_OPERATOR_ADDRESS,
    NOC_C_AST_OPERATOR_DEREFERENCE,
    NOC_C_AST_OPERATOR_MEMBER,
    NOC_C_AST_OPERATOR_POINTER_MEMBER,
    NOC_C_AST_OPERATOR_PREFIX_INCREMENT,
    NOC_C_AST_OPERATOR_PREFIX_DECREMENT,
    NOC_C_AST_OPERATOR_POSTFIX_INCREMENT,
    NOC_C_AST_OPERATOR_POSTFIX_DECREMENT,
} Noc_C_Ast_Operator;

/* C storage classes and function specifiers share this spelling category.
   TYPEDEF is reported on TYPE_DEFINITION; extension spellings also have a
   matching Noc_C_Ast_Extension policy classification. */
typedef enum {
    NOC_C_AST_SPECIFIER_NONE = 0,
    NOC_C_AST_SPECIFIER_UNKNOWN,
    NOC_C_AST_SPECIFIER_EXTERN,
    NOC_C_AST_SPECIFIER_STATIC,
    NOC_C_AST_SPECIFIER_AUTO,
    NOC_C_AST_SPECIFIER_REGISTER,
    NOC_C_AST_SPECIFIER_TYPEDEF,
    NOC_C_AST_SPECIFIER_INLINE,
    NOC_C_AST_SPECIFIER_GNU_INLINE,
    NOC_C_AST_SPECIFIER_GNU_INLINE_ALT,
    NOC_C_AST_SPECIFIER_MS_FORCE_INLINE,
    NOC_C_AST_SPECIFIER_C11_THREAD_LOCAL,
    NOC_C_AST_SPECIFIER_C23_THREAD_LOCAL,
    NOC_C_AST_SPECIFIER_GNU_THREAD_LOCAL,
} Noc_C_Ast_Specifier;

/* Type/function qualifiers preserve standard and extension spellings rather
   than treating semantically similar GNU, Clang, C11, and C23 forms as equal. */
typedef enum {
    NOC_C_AST_QUALIFIER_NONE = 0,
    NOC_C_AST_QUALIFIER_UNKNOWN,
    NOC_C_AST_QUALIFIER_CONST,
    NOC_C_AST_QUALIFIER_VOLATILE,
    NOC_C_AST_QUALIFIER_RESTRICT,
    NOC_C_AST_QUALIFIER_ATOMIC,
    NOC_C_AST_QUALIFIER_NORETURN,
    NOC_C_AST_QUALIFIER_C11_ALIGNAS,
    NOC_C_AST_QUALIFIER_C23_CONSTEXPR,
    NOC_C_AST_QUALIFIER_C23_NORETURN,
    NOC_C_AST_QUALIFIER_C23_ALIGNAS,
    NOC_C_AST_QUALIFIER_GNU_RESTRICT,
    NOC_C_AST_QUALIFIER_GNU_EXTENSION,
    NOC_C_AST_QUALIFIER_CLANG_NONNULL,
} Noc_C_Ast_Qualifier;

typedef enum {
    NOC_C_AST_PRIMITIVE_NONE = 0,
    NOC_C_AST_PRIMITIVE_UNKNOWN,
    NOC_C_AST_PRIMITIVE_VOID,
    NOC_C_AST_PRIMITIVE_CHAR,
    NOC_C_AST_PRIMITIVE_INT,
    NOC_C_AST_PRIMITIVE_FLOAT,
    NOC_C_AST_PRIMITIVE_DOUBLE,
    NOC_C_AST_PRIMITIVE_C11_BOOL,
    NOC_C_AST_PRIMITIVE_C23_BOOL,
    NOC_C_AST_PRIMITIVE_IMPLEMENTATION_TYPE,
} Noc_C_Ast_Primitive;

enum {
    NOC_C_AST_TYPE_SIGNED = 1u << 0,
    NOC_C_AST_TYPE_UNSIGNED = 1u << 1,
    NOC_C_AST_TYPE_SHORT = 1u << 2,
    /* ISO C11 `_Complex` combines with float/double and therefore remains a
       spelling flag instead of replacing the underlying primitive category. */
    NOC_C_AST_TYPE_COMPLEX = 1u << 3,
};

typedef struct {
    /* This records source spelling only. Conflicting flags and implementation
       types remain available to the later semantic/type-validation phase. */
    Noc_C_Ast_Primitive primitive;
    unsigned int flags;
    size_t long_count;
} Noc_C_Ast_Type_Spelling;

typedef enum {
    NOC_C_AST_ARRAY_SIZE_NONE = 0,
    NOC_C_AST_ARRAY_SIZE_UNKNOWN,
    NOC_C_AST_ARRAY_SIZE_EXPRESSION,
    NOC_C_AST_ARRAY_SIZE_STAR,
} Noc_C_Ast_Array_Size;

typedef struct {
    /* C99 parameter-array `static` promises the caller supplies at least the
       written bound; it is not a storage-class specifier in this context. */
    bool has_static_minimum;
    Noc_C_Ast_Array_Size size;
} Noc_C_Ast_Array_Detail;

typedef enum {
    NOC_C_AST_EXTENSION_NONE = 0,
    NOC_C_AST_EXTENSION_UNKNOWN,
    NOC_C_AST_EXTENSION_GNU_ATTRIBUTE,
    NOC_C_AST_EXTENSION_C23_ATTRIBUTE,
    NOC_C_AST_EXTENSION_GNU_EXPRESSION,
    NOC_C_AST_EXTENSION_GNU_ASM,
    NOC_C_AST_EXTENSION_GNU_ASM_VOLATILE,
    NOC_C_AST_EXTENSION_GNU_ASM_VOLATILE_ALT,
    NOC_C_AST_EXTENSION_GNU_ASM_INLINE,
    NOC_C_AST_EXTENSION_GNU_ASM_GOTO,
    NOC_C_AST_EXTENSION_GNU_SUBSCRIPT_RANGE,
    NOC_C_AST_EXTENSION_GNU_ALIGNOF,
    NOC_C_AST_EXTENSION_GNU_ALIGNOF_ALT,
    NOC_C_AST_EXTENSION_C23_ALIGNOF,
    NOC_C_AST_EXTENSION_GNU_RESTRICT,
    NOC_C_AST_EXTENSION_GNU_EXTENSION_QUALIFIER,
    NOC_C_AST_EXTENSION_GNU_INLINE,
    NOC_C_AST_EXTENSION_GNU_INLINE_ALT,
    NOC_C_AST_EXTENSION_GNU_THREAD_LOCAL,
    NOC_C_AST_EXTENSION_MS_DECLSPEC,
    NOC_C_AST_EXTENSION_MS_BASED,
    NOC_C_AST_EXTENSION_MS_CDECL,
    NOC_C_AST_EXTENSION_MS_CLRCALL,
    NOC_C_AST_EXTENSION_MS_STDCALL,
    NOC_C_AST_EXTENSION_MS_FASTCALL,
    NOC_C_AST_EXTENSION_MS_THISCALL,
    NOC_C_AST_EXTENSION_MS_VECTORCALL,
    NOC_C_AST_EXTENSION_MS_FORCE_INLINE,
    NOC_C_AST_EXTENSION_MS_RESTRICT,
    NOC_C_AST_EXTENSION_MS_UNSIGNED_POINTER,
    NOC_C_AST_EXTENSION_MS_SIGNED_POINTER,
    NOC_C_AST_EXTENSION_MS_UNALIGNED_POINTER,
    NOC_C_AST_EXTENSION_MS_UNALIGNED_POINTER_ALT,
    NOC_C_AST_EXTENSION_MS_SEH_TRY,
    NOC_C_AST_EXTENSION_MS_SEH_EXCEPT,
    NOC_C_AST_EXTENSION_MS_SEH_FINALLY,
    NOC_C_AST_EXTENSION_MS_SEH_LEAVE,
    NOC_C_AST_EXTENSION_CXX_LINKAGE,
    NOC_C_AST_EXTENSION_C23_THREAD_LOCAL,
    NOC_C_AST_EXTENSION_C23_CONSTEXPR,
    NOC_C_AST_EXTENSION_C23_NORETURN,
    NOC_C_AST_EXTENSION_C23_ALIGNAS,
    NOC_C_AST_EXTENSION_C23_BOOL,
    NOC_C_AST_EXTENSION_C23_TRUE,
    NOC_C_AST_EXTENSION_C23_FALSE,
    NOC_C_AST_EXTENSION_C23_NULL,
    NOC_C_AST_EXTENSION_CLANG_NONNULL,
    NOC_C_AST_EXTENSION_MACRO_TYPE,
    /* The C23 `static_assert` alias; ISO C11 `_Static_assert` reports NONE. */
    NOC_C_AST_EXTENSION_C23_STATIC_ASSERT,
} Noc_C_Ast_Extension;

typedef struct {
    Noc_C_Ast_Expected_Kind kind;
    /* Borrowed from the AST. Punctuation/keywords are exact expected spelling;
       category-only recovery may have an empty spelling. */
    Noc_Slice spelling;
} Noc_C_Ast_Expected;

enum {
    NOC_C_AST_NODE_ERROR = 1u << 0,
    NOC_C_AST_NODE_MISSING = 1u << 1,
    NOC_C_AST_NODE_HAS_ERROR = 1u << 2,
    NOC_C_AST_NODE_UNKNOWN_KIND = 1u << 3,
    NOC_C_AST_NODE_UNKNOWN_FIELD = 1u << 4,
    NOC_C_AST_NODE_UNKNOWN_DETAIL = 1u << 5,
};

enum {
    NOC_C_AST_ISSUE_PARSE_ERROR = 1u << 0,
    NOC_C_AST_ISSUE_MISSING = 1u << 1,
    NOC_C_AST_ISSUE_SKIPPED_SOURCE = 1u << 2,
    NOC_C_AST_ISSUE_UNKNOWN_KIND = 1u << 3,
    NOC_C_AST_ISSUE_UNKNOWN_FIELD = 1u << 4,
    NOC_C_AST_ISSUE_UNKNOWN_DETAIL = 1u << 5,
};

typedef struct {
    Noc_C_Ast_Kind kind;
    Noc_C_Ast_Field field;
    Noc_Byte_Range bytes;
    size_t parent;
    size_t first_child;
    size_t last_child;
    size_t next_sibling;
    size_t child_count;
    size_t generation;
    unsigned int flags;
} Noc_C_Ast_Node;

typedef struct Noc_C_Ast_Impl Noc_C_Ast_Impl;

typedef struct {
    Noc_C_Ast_Impl *impl;
    size_t generation;
} Noc_C_Ast;

/* Borrowing syntactic context for one physical insertion position. The offset
   may be any position from the beginning of the document through EOF. LEFT and
   RIGHT own the adjacent source bytes when those bytes exist. When zero-width
   recovery expectations exist at OFFSET, NODE is their common parent context;
   otherwise it is the adjacent nodes' smallest common ancestor, with the
   translation-unit root used at document edges. EXPECTED_COUNT includes every
   parser-materialized MISSING node at OFFSET, in AST preorder. These recovery
   expectations are diagnostic hints and candidate-ranking inputs, not an
   exhaustive semantic or grammar-lookahead completion set; their absence must
   not suppress a completion candidate.

   The context owns no memory and must be treated as read-only. OWNER binds it
   to the AST handle that created it. FILE_ID, GENERATION, and
   DOCUMENT_GENERATION describe the retained immutable revision. Updating the
   workspace does not mutate an old AST/context; clients compare this metadata
   to their current snapshot, while a successful OWNER rebuild invalidates the
   context automatically. */
typedef struct {
    const Noc_C_Ast *owner;
    size_t offset;
    size_t node;
    size_t left_node;
    size_t right_node;
    size_t expected_count;
    Noc_File_Id file_id;
    size_t generation;
    size_t document_generation;
} Noc_C_Ast_Completion_Context;

typedef bool (*Noc_C_Ast_Cancel_Fn)(void *user_data);

typedef struct {
    /* Maximum number of selected normalized nodes, including recovery nodes. */
    size_t max_nodes;
    /* Polled before work and periodically while flattening. */
    Noc_C_Ast_Cancel_Fn should_cancel;
    void *cancel_user_data;
} Noc_C_Ast_Options;

typedef enum {
    NOC_C_AST_OK = 0,
    NOC_C_AST_INVALID_ARGUMENT,
    NOC_C_AST_CANCELLED,
    NOC_C_AST_LIMIT_EXCEEDED,
    NOC_C_AST_GENERATION_EXHAUSTED,
    NOC_C_AST_OUT_OF_MEMORY,
} Noc_C_Ast_Status;

NOCDEF Noc_C_Ast_Options noc_c_ast_default_options(void);
NOCDEF const char *noc_c_ast_status_name(Noc_C_Ast_Status status);
/* Build is transactional: limit, cancellation, allocation, and invalid-input
   failures leave an existing valid output AST unchanged. */
NOCDEF Noc_C_Ast_Status noc_c_ast_build(const Noc_C_Parse_Tree *tree,
                                         Noc_C_Ast_Options options,
                                         Noc_C_Ast *output);
NOCDEF void noc_c_ast_free(Noc_C_Ast *ast);
NOCDEF bool noc_c_ast_is_valid(const Noc_C_Ast *ast);
/* Syntax completeness excludes parser recovery and unknown adapter mappings.
   It does not imply preprocessing, feature, type, or semantic validity. */
NOCDEF bool noc_c_ast_is_syntax_complete(const Noc_C_Ast *ast);
NOCDEF unsigned int noc_c_ast_issues(const Noc_C_Ast *ast);
NOCDEF size_t noc_c_ast_generation(const Noc_C_Ast *ast);
NOCDEF size_t noc_c_ast_document_generation(const Noc_C_Ast *ast);
NOCDEF const Noc_Document_Snapshot *noc_c_ast_snapshot(const Noc_C_Ast *ast);
NOCDEF size_t noc_c_ast_node_count(const Noc_C_Ast *ast);
NOCDEF size_t noc_c_ast_root(const Noc_C_Ast *ast);
/* Node pointers and expected spellings remain valid until successful rebuild
   or free. Invalid/inapplicable typed queries return their NONE value. */
NOCDEF const Noc_C_Ast_Node *noc_c_ast_node_at(const Noc_C_Ast *ast,
                                               size_t node_index);
NOCDEF Noc_Slice noc_c_ast_node_source(const Noc_C_Ast *ast,
                                       size_t node_index);
NOCDEF Noc_Location noc_c_ast_node_location(const Noc_C_Ast *ast,
                                            size_t node_index);
/* Physical-source navigation for editor and compiler clients. Offset queries
   accept an existing source byte (offset < source_count), not the EOF insertion
   position. Range queries accept a non-empty half-open physical byte range.
   Both return the deepest normalized node that contains the requested bytes;
   trivia and anonymous punctuation therefore resolve to their nearest retained
   AST ancestor. Zero-width recovery nodes do not own source bytes and are
   queried separately with noc_c_ast_node_expected(). Invalid ASTs, offsets,
   ranges, and node indices return NOC_C_AST_NODE_NONE. */
NOCDEF size_t noc_c_ast_node_at_offset(const Noc_C_Ast *ast, size_t offset);
NOCDEF size_t noc_c_ast_node_covering_range(const Noc_C_Ast *ast,
                                            Noc_Byte_Range range);
/* The translation-unit root has depth zero. Common-ancestor results and all
   input node indices belong to the current AST generation. */
NOCDEF size_t noc_c_ast_depth(const Noc_C_Ast *ast, size_t node_index);
NOCDEF size_t noc_c_ast_common_ancestor(const Noc_C_Ast *ast,
                                        size_t left,
                                        size_t right);
/* Completion-context construction is allocation-free and transactionally
   preserves output on invalid ASTs, out-of-range positions, or NULL output.
   next_expected_node returns expectations in AST preorder. Pass
   NOC_C_AST_NODE_NONE as previous to begin; subsequent calls must pass the
   prior returned node. A stale context, an invalid previous node, or the end of
   the sequence returns NOC_C_AST_NODE_NONE. Successful results can be passed
   to noc_c_ast_node_expected(). Complete iteration is linear in AST size. */
NOCDEF bool noc_c_ast_completion_context(
    const Noc_C_Ast *ast,
    size_t offset,
    Noc_C_Ast_Completion_Context *output);
NOCDEF size_t noc_c_ast_completion_next_expected_node(
    const Noc_C_Ast *ast,
    const Noc_C_Ast_Completion_Context *context,
    size_t previous);
NOCDEF Noc_C_Ast_Operator noc_c_ast_node_operator(const Noc_C_Ast *ast,
                                                  size_t node_index);
NOCDEF Noc_C_Ast_Specifier noc_c_ast_node_specifier(const Noc_C_Ast *ast,
                                                    size_t node_index);
NOCDEF Noc_C_Ast_Qualifier noc_c_ast_node_qualifier(const Noc_C_Ast *ast,
                                                    size_t node_index);
NOCDEF bool noc_c_ast_node_type_spelling(const Noc_C_Ast *ast,
                                         size_t node_index,
                                         Noc_C_Ast_Type_Spelling *output);
NOCDEF bool noc_c_ast_node_array_detail(const Noc_C_Ast *ast,
                                        size_t node_index,
                                        Noc_C_Ast_Array_Detail *output);
NOCDEF Noc_C_Ast_Extension noc_c_ast_node_extension(const Noc_C_Ast *ast,
                                                    size_t node_index);
NOCDEF Noc_C_Ast_Expected noc_c_ast_node_expected(const Noc_C_Ast *ast,
                                                  size_t node_index);
/* Stable diagnostic/serialization names; unknown enum values return
   "unknown". These names are Noc API strings, not borrowed parser metadata. */
NOCDEF const char *noc_c_ast_kind_name(Noc_C_Ast_Kind kind);
NOCDEF const char *noc_c_ast_field_name(Noc_C_Ast_Field field);
NOCDEF const char *noc_c_ast_operator_name(Noc_C_Ast_Operator operator_kind);
NOCDEF const char *noc_c_ast_specifier_name(Noc_C_Ast_Specifier specifier);
NOCDEF const char *noc_c_ast_qualifier_name(Noc_C_Ast_Qualifier qualifier);
NOCDEF const char *noc_c_ast_primitive_name(Noc_C_Ast_Primitive primitive);
NOCDEF const char *noc_c_ast_array_size_name(Noc_C_Ast_Array_Size size);
NOCDEF const char *noc_c_ast_extension_name(Noc_C_Ast_Extension extension);
NOCDEF const char *noc_c_ast_expected_kind_name(Noc_C_Ast_Expected_Kind kind);

/* Policy-aware preprocessing frontend. Building a unit recognizes source
   directives regardless of whether policy permits them and also publishes a
   lossless preprocessing-token view. Validation is a separate operation so
   IDEs can still inspect disabled constructs. Macro expansion itself is
   implemented by later preprocessor milestones. */
typedef enum {
    NOC_MACROS_DISABLED = 0,
    NOC_MACROS_TRUSTED_ONLY,
    NOC_MACROS_PROJECT,
    NOC_MACROS_FULL,
} Noc_Macro_Policy;

typedef enum {
    NOC_PREPROCESSOR_DIRECTIVE_NULL = 0,
    NOC_PREPROCESSOR_DIRECTIVE_DEFINE,
    NOC_PREPROCESSOR_DIRECTIVE_UNDEF,
    NOC_PREPROCESSOR_DIRECTIVE_INCLUDE,
    NOC_PREPROCESSOR_DIRECTIVE_IF,
    NOC_PREPROCESSOR_DIRECTIVE_IFDEF,
    NOC_PREPROCESSOR_DIRECTIVE_IFNDEF,
    NOC_PREPROCESSOR_DIRECTIVE_ELIF,
    NOC_PREPROCESSOR_DIRECTIVE_ELIFDEF,
    NOC_PREPROCESSOR_DIRECTIVE_ELIFNDEF,
    NOC_PREPROCESSOR_DIRECTIVE_ELSE,
    NOC_PREPROCESSOR_DIRECTIVE_ENDIF,
    NOC_PREPROCESSOR_DIRECTIVE_LINE,
    NOC_PREPROCESSOR_DIRECTIVE_ERROR,
    NOC_PREPROCESSOR_DIRECTIVE_WARNING,
    NOC_PREPROCESSOR_DIRECTIVE_PRAGMA,
    NOC_PREPROCESSOR_DIRECTIVE_UNKNOWN,
} Noc_Preprocessor_Directive_Kind;

typedef enum {
    NOC_PREPROCESSING_TOKEN_SOURCE = 0,
    NOC_PREPROCESSING_TOKEN_DIRECTIVE_MARKER,
    NOC_PREPROCESSING_TOKEN_DIRECTIVE_KEYWORD,
    NOC_PREPROCESSING_TOKEN_DIRECTIVE_BODY,
    NOC_PREPROCESSING_TOKEN_DIRECTIVE_TRIVIA,
} Noc_Preprocessing_Token_Role;

typedef struct {
    Noc_Token token;
    Noc_Preprocessing_Token_Role role;
    /* NOC_TOKEN_INDEX_NONE for tokens outside preprocessing directives. */
    size_t directive_index;
} Noc_Preprocessing_Token;

typedef enum {
    NOC_MACRO_DIRECTIVE_DEFINE_OBJECT = 0,
    NOC_MACRO_DIRECTIVE_DEFINE_FUNCTION,
    NOC_MACRO_DIRECTIVE_UNDEF,
} Noc_Macro_Directive_Kind;

typedef enum {
    NOC_MACRO_DIRECTIVE_STATUS_VALID = 0,
    NOC_MACRO_DIRECTIVE_STATUS_INCOMPLETE,
    NOC_MACRO_DIRECTIVE_STATUS_MALFORMED,
} Noc_Macro_Directive_Status;

typedef struct {
    /* Identifier token, or the ellipsis token for an unnamed variadic slot. */
    size_t token_index;
    bool variadic;
} Noc_Macro_Parameter;

typedef struct {
    Noc_Macro_Directive_Kind kind;
    Noc_Macro_Directive_Status status;
    size_t directive_index;
    size_t name_token_index;
    /* Function-like parameter contents, excluding parentheses. Object-like
       definitions and #undef use {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE}. */
    Noc_Token_Range parameter_tokens;
    /* Significant replacement span with internal trivia preserved. #undef uses
       the absent range above; an empty definition uses {position, position}. */
    Noc_Token_Range replacement_tokens;
    size_t parameter_begin;
    size_t parameter_count;
    size_t problem_token_index;
    bool variadic;
} Noc_Macro_Directive;

typedef struct {
    Noc_Preprocessor_Directive_Kind kind;
    /* Index of the original opaque directive in stream. */
    size_t token_index;
    /* Half-open range in Noc_Preprocessor_Unit.preprocessing_tokens. */
    Noc_Token_Range preprocessing_tokens;
    /* Index in Noc_Preprocessor_Unit.macro_directives, or NONE. */
    size_t macro_directive_index;
    Noc_Slice spelling;
    Noc_Slice keyword;
    Noc_Slice payload;
    Noc_Location location;
    bool macro_definition_allowed;
} Noc_Preprocessor_Directive;

typedef enum {
    NOC_MACRO_INVOCATION_NOT_INVOKED = 0,
    NOC_MACRO_INVOCATION_COMPLETE,
    NOC_MACRO_INVOCATION_INCOMPLETE,
} Noc_Macro_Invocation_Status;

typedef enum {
    NOC_MACRO_INVOCATION_BUILD_OK = 0,
    NOC_MACRO_INVOCATION_BUILD_INVALID_ARGUMENT,
    NOC_MACRO_INVOCATION_BUILD_STALE,
    NOC_MACRO_INVOCATION_BUILD_GENERATION_EXHAUSTED,
    NOC_MACRO_INVOCATION_BUILD_OUT_OF_MEMORY,
} Noc_Macro_Invocation_Build_Status;

typedef struct {
    /* Exact half-open token range between a top-level separator and the
       invocation parentheses. Leading/trailing trivia is intentionally kept. */
    Noc_Token_Range tokens;
} Noc_Macro_Argument;

/* Owning handle: initialize to {0}, do not shallow-copy, and release with
   noc_preprocessor_unit_free. Successful rebuild invalidates pointers and
   derived views borrowed from the old unit; failed rebuild preserves them. */
typedef struct {
    Noc_Token_Stream stream;
    size_t token_stream_generation;
    Noc_Preprocessing_Token *preprocessing_tokens;
    size_t preprocessing_token_count;
    size_t preprocessing_token_capacity;
    Noc_Macro_Directive *macro_directives;
    size_t macro_directive_count;
    size_t macro_directive_capacity;
    Noc_Macro_Parameter *macro_parameters;
    size_t macro_parameter_count;
    size_t macro_parameter_capacity;
    Noc_Preprocessor_Directive *items;
    size_t count;
    size_t capacity;
    Noc_File_Id file_id;
    size_t document_generation;
    Noc_Source_Class source_class;
    Noc_Macro_Policy macro_policy;
    size_t disabled_macro_definition_count;
    size_t invalid_macro_directive_count;
} Noc_Preprocessor_Unit;

/* Recoverable syntax for one physical #include operand:
   - DIRECT is exactly one nonempty physical HEADER_NAME token.
   - EXPANSION_REQUIRED means macro replacement or expanded end-of-directive
     checking can still determine validity. This includes ordinary macro bodies,
     <NAME without a physical close, and a direct header followed by an
     identifier that may expand away.
   - EMPTY is exactly "" or <>; MISSING has no significant directive body.
   - MALFORMED is known invalid without expansion, such as a direct header
     followed by a number or second header name.
   - INCOMPLETE contains an invalid/unterminated lexical token. In particular,
     an unterminated quoted name is INCOMPLETE while <NAME can require expansion.
   Every source state is published for diagnostics/IDE recovery, but only DIRECT
   can be passed to noc_include_resolve. */
typedef enum {
    NOC_INCLUDE_OPERAND_DIRECT = 0,
    NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED,
    NOC_INCLUDE_OPERAND_EMPTY,
    NOC_INCLUDE_OPERAND_MISSING,
    NOC_INCLUDE_OPERAND_MALFORMED,
    NOC_INCLUDE_OPERAND_INCOMPLETE,
} Noc_Include_Operand_Status;

typedef enum {
    NOC_INCLUDE_FORM_NONE = 0,
    NOC_INCLUDE_FORM_QUOTED,
    NOC_INCLUDE_FORM_ANGLED,
} Noc_Include_Form;

typedef enum {
    NOC_INCLUDE_OPERAND_BUILD_OK = 0,
    NOC_INCLUDE_OPERAND_BUILD_INVALID_ARGUMENT,
    NOC_INCLUDE_OPERAND_BUILD_STALE,
    NOC_INCLUDE_OPERAND_BUILD_GENERATION_EXHAUSTED,
    NOC_INCLUDE_OPERAND_BUILD_OUT_OF_MEMORY,
} Noc_Include_Operand_Build_Status;

/* Owning logical-name query borrowing one preprocessing unit. Initialize to
   {0}, do not shallow-copy, treat fields as read-only, and free with
   noc_include_operand_free. The unit object must remain alive; a successful
   unit rebuild makes this result stale.

   body_tokens is {NONE,NONE} for MISSING; otherwise it spans the first through
   last significant body token and retains internal trivia. DIRECT and EMPTY
   publish header_token_index/form. DIRECT alone owns a nonempty logical_name and
   has no problem token; EMPTY points problem_token_index at its header. Other
   statuses use form NONE, header NONE, and an empty name; MISSING has no problem
   token while the other states identify the first token needing attention.

   logical_name is a counted byte slice with delimiters and phase-2 line splices
   removed. C escapes are not decoded, and no encoding validation, separator
   conversion, or path normalization occurs. Hosts must honor count rather than
   relying on NUL termination. Recoverable source states are successful builds;
   operational failure preserves the previous output. */
typedef struct {
    const Noc_Preprocessor_Unit *unit;
    size_t unit_stream_generation;
    size_t directive_index;
    Noc_Token_Range body_tokens;
    size_t header_token_index;
    size_t problem_token_index;
    Noc_Slice logical_name;
    Noc_Include_Operand_Status status;
    Noc_Include_Form form;
    size_t generation;
} Noc_Include_Operand;

/* A resolver request borrows every pointer for one callback invocation. The
   host decides quote/angle search order, path normalization, overlays, virtual
   filesystems, and source classification; Noc never infers these from its own
   process or accesses the filesystem here. File IDs are local to the workspace
   selected by resolver.user_data rather than globally meaningful. */
typedef struct {
    const char *including_path;
    Noc_File_Id including_file_id;
    size_t including_document_generation;
    Noc_Source_Class including_source_class;
    size_t directive_index;
    Noc_Location directive_location;
    Noc_Include_Form form;
    Noc_Slice logical_name;
} Noc_Include_Request;

typedef enum {
    NOC_INCLUDE_RESOLVE_FOUND = 0,
    NOC_INCLUDE_RESOLVE_NOT_FOUND,
    NOC_INCLUDE_RESOLVE_AMBIGUOUS,
    NOC_INCLUDE_RESOLVE_DENIED,
    NOC_INCLUDE_RESOLVE_CANCELLED,
    NOC_INCLUDE_RESOLVE_FAILED,
    NOC_INCLUDE_RESOLVE_INVALID_ARGUMENT,
    NOC_INCLUDE_RESOLVE_STALE,
    NOC_INCLUDE_RESOLVE_OUT_OF_MEMORY,
    NOC_INCLUDE_RESOLVE_INVALID_RESULT,
} Noc_Include_Resolve_Status;

/* The callback output is initially empty. On FOUND, the callback transfers one
   valid owning snapshot to the wrapper. NOT_FOUND, AMBIGUOUS, DENIED,
   CANCELLED, FAILED, and OUT_OF_MEMORY must leave it empty. The wrapper frees a
   snapshot returned in violation of that rule and preserves the caller's
   destination. INVALID_ARGUMENT, STALE, and INVALID_RESULT are wrapper-only.

   The target snapshot path and source class are host-authoritative. Noc neither
   inherits class from the including file nor infers trust from quote/angle form
   or path spelling; later macro policy consumes the returned class verbatim. */
typedef Noc_Include_Resolve_Status (*Noc_Include_Resolve_Fn)(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *resolved_snapshot);

typedef struct {
    Noc_Include_Resolve_Fn resolve;
    void *user_data;
} Noc_Include_Resolver;

/* Owning, lossless physical-source function-like invocation syntax. Initialize
   to {0}, do not shallow-copy, and release with noc_macro_invocation_free. The
   source unit must remain alive and unchanged. Complete and incomplete editor
   input are both successful query results; operational failures preserve the
   previous result. Expanded invocations assembled from multiple provenance
   owners require a logical-token representation rather than this source range. */
typedef struct {
    const Noc_Preprocessor_Unit *unit;
    size_t unit_stream_generation;
    size_t name_token_index;
    size_t open_token_index;
    size_t close_token_index;
    Noc_Token_Range tokens;
    Noc_Macro_Argument *arguments;
    size_t argument_count;
    size_t argument_capacity;
    size_t problem_token_index;
    Noc_Macro_Invocation_Status status;
    size_t generation;
} Noc_Macro_Invocation;

typedef enum {
    NOC_MACRO_ENVIRONMENT_OK = 0,
    NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT,
    NOC_MACRO_ENVIRONMENT_INVALID_DIRECTIVE,
    NOC_MACRO_ENVIRONMENT_DISABLED,
    NOC_MACRO_ENVIRONMENT_STALE,
    NOC_MACRO_ENVIRONMENT_GENERATION_EXHAUSTED,
    NOC_MACRO_ENVIRONMENT_OUT_OF_MEMORY,
} Noc_Macro_Environment_Status;

/* One caller-selected effective #define/#undef event. The referenced unit and
   its owning object must outlive the environment and must not be rebuilt. */
typedef struct {
    const Noc_Preprocessor_Unit *unit;
    size_t unit_stream_generation;
    size_t macro_directive_index;
    /* Prior applied event for the same phase-2 logical name, or NONE. */
    size_t previous_entry_index;
} Noc_Macro_Environment_Entry;

/* Owning, caller-driven macro state. Initialize to {0}; do not shallow-copy.
   Apply directives in active preprocessing order, including include traversal.
   This layer deliberately does not guess conditional activity. Mutation
   invalidates returned entry pointers; failure preserves all prior state. */
typedef struct {
    Noc_Macro_Environment_Entry *items;
    size_t count;
    size_t capacity;
    size_t generation;
} Noc_Macro_Environment;

typedef enum {
    NOC_MACRO_EXPANSION_OK = 0,
    NOC_MACRO_EXPANSION_INVALID_ARGUMENT,
    NOC_MACRO_EXPANSION_STALE,
    NOC_MACRO_EXPANSION_INCOMPLETE_INVOCATION,
    NOC_MACRO_EXPANSION_ARGUMENT_COUNT_MISMATCH,
    NOC_MACRO_EXPANSION_INVALID_DEFINITION,
    NOC_MACRO_EXPANSION_DEPTH_LIMIT,
    NOC_MACRO_EXPANSION_OUTPUT_LIMIT,
    NOC_MACRO_EXPANSION_COUNT_LIMIT,
    NOC_MACRO_EXPANSION_UNSUPPORTED_OPERATOR,
    NOC_MACRO_EXPANSION_UNSUPPORTED_VARIADIC,
    NOC_MACRO_EXPANSION_GENERATION_EXHAUSTED,
    NOC_MACRO_EXPANSION_OUT_OF_MEMORY,
    /* A ##/%:%: application did not form exactly one preprocessing token. */
    NOC_MACRO_EXPANSION_INVALID_PASTE,
} Noc_Macro_Expansion_Status;

typedef enum {
    NOC_PREPROCESSOR_EXPRESSION_OK = 0,
    NOC_PREPROCESSOR_EXPRESSION_INVALID_ARGUMENT,
    NOC_PREPROCESSOR_EXPRESSION_STALE,
    NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
    NOC_PREPROCESSOR_EXPRESSION_DIVISION_BY_ZERO,
    NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW,
    NOC_PREPROCESSOR_EXPRESSION_SHIFT_OUT_OF_RANGE,
    NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT,
    NOC_PREPROCESSOR_EXPRESSION_DEPTH_LIMIT,
    NOC_PREPROCESSOR_EXPRESSION_OUT_OF_MEMORY,
} Noc_Preprocessor_Expression_Status;

typedef enum {
    NOC_CONDITIONAL_GROUP_COMPLETE = 0,
    NOC_CONDITIONAL_GROUP_INCOMPLETE,
    NOC_CONDITIONAL_GROUP_MALFORMED,
    NOC_CONDITIONAL_GROUP_MALFORMED_INCOMPLETE,
} Noc_Conditional_Group_Status;
typedef enum {
    NOC_CONDITIONAL_CONDITION_NOT_APPLICABLE = 0,
    NOC_CONDITIONAL_CONDITION_EVALUATED,
    NOC_CONDITIONAL_CONDITION_NOT_EVALUATED,
    NOC_CONDITIONAL_CONDITION_MALFORMED,
    NOC_CONDITIONAL_CONDITION_EXPANSION_FAILED,
    NOC_CONDITIONAL_CONDITION_EVALUATION_FAILED,
} Noc_Conditional_Condition_Status;
typedef enum {
    NOC_CONDITIONAL_ISSUE_UNMATCHED_ELIF = 0,
    NOC_CONDITIONAL_ISSUE_UNMATCHED_ELSE,
    NOC_CONDITIONAL_ISSUE_UNMATCHED_ENDIF,
    NOC_CONDITIONAL_ISSUE_ELIF_AFTER_ELSE,
    NOC_CONDITIONAL_ISSUE_DUPLICATE_ELSE,
    NOC_CONDITIONAL_ISSUE_MISSING_ENDIF,
    NOC_CONDITIONAL_ISSUE_UNEXPECTED_TOKENS,
    NOC_CONDITIONAL_ISSUE_UNRESOLVED_CONDITION,
    NOC_CONDITIONAL_ISSUE_UNSUPPORTED_DIRECTIVE,
} Noc_Conditional_Issue_Kind;
typedef enum {
    NOC_CONDITIONAL_GROUPS_OK = 0,
    NOC_CONDITIONAL_GROUPS_INVALID_ARGUMENT,
    NOC_CONDITIONAL_GROUPS_STALE,
    NOC_CONDITIONAL_GROUPS_GENERATION_EXHAUSTED,
    NOC_CONDITIONAL_GROUPS_OUT_OF_MEMORY,
} Noc_Conditional_Groups_Build_Status;

typedef struct {
    /* The branch containing this nested group, or NONE at file scope. */
    size_t parent_branch_index;
    size_t opener_directive_index;
    size_t closer_directive_index;
    size_t first_branch_index;
    size_t last_branch_index;
    /* Includes the opening and matched closing directives. An incomplete
       group's range ends immediately before the terminal EOF token. */
    Noc_Token_Range preprocessing_tokens;
    Noc_Conditional_Group_Status status;
} Noc_Preprocessor_Conditional_Group;

typedef struct {
    size_t group_index;
    size_t previous_branch_index;
    size_t next_branch_index;
    size_t directive_index;
    Noc_Preprocessor_Directive_Kind directive_kind;
    Noc_Token_Range condition_tokens;
    /* Excludes this branch directive and the next peer/closing directive. */
    Noc_Token_Range content_tokens;
    /* Concrete macro prefix used to evaluate the condition, or NONE when the
       branch was not evaluated or macro state was already uncertain. */
    size_t condition_environment_entry_limit;
    Noc_Preprocessor_Activity condition_activity;
    Noc_Preprocessor_Activity content_activity;
    Noc_Conditional_Condition_Status condition_status;
    Noc_Macro_Expansion_Status expansion_status;
    Noc_Preprocessor_Expression_Status expression_status;
    /* Optional stable source coordinate for an expression problem. Expansion
       failures may only identify the condition's physical anchor. */
    const Noc_Preprocessor_Unit *problem_unit;
    size_t problem_unit_stream_generation;
    size_t problem_token_index;
} Noc_Preprocessor_Conditional_Branch;

typedef struct {
    Noc_Conditional_Issue_Kind kind;
    size_t directive_index;
    size_t group_index;
    size_t branch_index;
    size_t problem_token_index;
} Noc_Preprocessor_Conditional_Issue;

/* Owning, recoverable balanced analysis. Initialize to {0}, do not
   shallow-copy, and release with noc_preprocessor_conditional_groups_free. It
   borrows the input unit and units referenced by its cloned initial macro
   prefix. The inline environment records definitely-active events; it is a
   complete concrete macro state only while macro_state_complete is true.
   Successful rebuild replaces the result and invalidates every borrowed
   pointer/expansion; operational failure preserves it. */
typedef struct {
    const Noc_Preprocessor_Unit *unit;
    size_t unit_stream_generation;
    Noc_Preprocessor_Conditional_Group *groups;
    size_t group_count;
    size_t group_capacity;
    Noc_Preprocessor_Conditional_Branch *branches;
    size_t branch_count;
    size_t branch_capacity;
    Noc_Preprocessor_Conditional_Issue *issues;
    size_t issue_count;
    size_t issue_capacity;
    size_t *directive_owned_groups;
    size_t *directive_introduced_branches;
    Noc_Preprocessor_Activity *token_activities;
    /* Each entry is a concrete prefix in environment, or NONE after macro
       state becomes uncertain due to an unknown-path macro event. */
    size_t *token_macro_entry_limits;
    size_t directive_count;
    size_t preprocessing_token_count;
    Noc_Macro_Environment environment;
    size_t published_environment_generation;
    size_t published_environment_count;
    size_t generation;
    bool macro_state_complete;
} Noc_Preprocessor_Conditional_Groups;

/* Include-control recognition is a read-only IDE/preprocessing phase. It does
   not suppress an include, mutate a macro environment, or access a filesystem.
   Initialize each output to {0}. Results own no allocation and need no free;
   they borrow one immutable preprocessing unit (and, for include guards, its
   matching conditional analysis). Shallow copies are safe but become stale if
   either owner is rebuilt or freed. Every token index/range addresses
   unit.preprocessing_tokens, and every range is half-open [begin,end).

   #pragma once is a widespread non-ISO extension. Noc recognizes only a direct,
   case-sensitive `once` identifier with no other significant body token;
   comments and phase-2 splices remain accepted through normal preprocessing
   tokenization. VALID identifies that exact spelling, OTHER is a nonempty valid
   pragma body owned by another extension, MISSING has no significant body,
   MALFORMED has a direct `once` followed by another significant token, and
   INCOMPLETE contains an invalid preprocessing token. Recognition alone never
   grants pragma semantics or records that a file was previously included. */
typedef enum {
    NOC_PRAGMA_ONCE_VALID = 0,
    NOC_PRAGMA_ONCE_OTHER,
    NOC_PRAGMA_ONCE_MISSING,
    NOC_PRAGMA_ONCE_MALFORMED,
    NOC_PRAGMA_ONCE_INCOMPLETE,
} Noc_Pragma_Once_Status;

typedef struct {
    const Noc_Preprocessor_Unit *unit;
    size_t unit_stream_generation;
    size_t directive_index;
    Noc_Token_Range body_tokens;
    size_t once_token_index;
    size_t problem_token_index;
    Noc_Pragma_Once_Status status;
    size_t generation;
} Noc_Pragma_Once;

/* A canonical macro guard is the structural convention

       #ifndef NAME
       #define NAME [replacement]
       ...
       #endif

   with comments/trivia allowed around directives, the object-like #define as
   the first significant construct inside the group, no peer #elif/#else branch,
   and no significant source outside the outer group. Equivalent forms such as
   `#if !defined(NAME)`, function-like definitions, delayed definitions, and
   nested lookalikes are deliberately not canonical. NAME is a borrowed raw
   identifier slice whose logical spelling applies phase-2 splice deletion. The
   #define may have any object-like replacement; definition_allowed reports
   whether the unit's configured macro policy permits that event.

   CANONICAL describes syntax only: callers must not suppress a file when
   definition_allowed is false, and recognition does not prove the guard is
   currently defined. NONE means the first significant file token does not start
   the strict form. Other statuses retain guard-shaped incomplete or malformed
   editor input without presenting it as a usable include guard. The referenced
   conditional analysis supplies balanced nesting and must have been built from
   the same unchanged unit. */
typedef enum {
    NOC_INCLUDE_GUARD_NONE = 0,
    NOC_INCLUDE_GUARD_CANONICAL,
    NOC_INCLUDE_GUARD_INCOMPLETE,
    NOC_INCLUDE_GUARD_MALFORMED,
    NOC_INCLUDE_GUARD_MISSING_DEFINE,
    NOC_INCLUDE_GUARD_NAME_MISMATCH,
    NOC_INCLUDE_GUARD_HAS_PEER_BRANCH,
    NOC_INCLUDE_GUARD_NOT_FILE_ENCLOSING,
} Noc_Include_Guard_Status;

typedef struct {
    const Noc_Preprocessor_Unit *unit;
    size_t unit_stream_generation;
    const Noc_Preprocessor_Conditional_Groups *groups;
    size_t groups_generation;
    size_t group_index;
    size_t branch_index;
    size_t opener_directive_index;
    size_t define_directive_index;
    size_t closer_directive_index;
    size_t guard_name_token_index;
    size_t problem_directive_index;
    size_t problem_token_index;
    Noc_Token_Range preprocessing_tokens;
    Noc_Slice guard_name;
    Noc_Include_Guard_Status status;
    size_t generation;
    bool definition_allowed;
} Noc_Include_Guard;

typedef enum {
    NOC_INCLUDE_CONTROL_BUILD_OK = 0,
    NOC_INCLUDE_CONTROL_BUILD_INVALID_ARGUMENT,
    NOC_INCLUDE_CONTROL_BUILD_STALE,
    NOC_INCLUDE_CONTROL_BUILD_GENERATION_EXHAUSTED,
} Noc_Include_Control_Build_Status;

typedef enum {
    /* No predefined name. This value is never present in an availability set. */
    NOC_MACRO_BUILTIN_NONE = 0,
    /* Always-available C11 fallbacks in a default Noc expansion. */
    NOC_MACRO_BUILTIN_FILE,
    NOC_MACRO_BUILTIN_LINE,
    NOC_MACRO_BUILTIN_STDC,
    NOC_MACRO_BUILTIN_STDC_VERSION,
    /* Available only when the corresponding translation input is configured. */
    NOC_MACRO_BUILTIN_STDC_HOSTED,
    NOC_MACRO_BUILTIN_DATE,
    NOC_MACRO_BUILTIN_TIME,
} Noc_Macro_Builtin_Kind;

/* The C execution-environment choice used to synthesize __STDC_HOSTED__.
   This describes the output translation, never the machine running Noc. */
typedef enum {
    /* Leaves the fallback unavailable rather than guessing from the host. */
    NOC_EXECUTION_ENVIRONMENT_UNSPECIFIED = 0,
    /* Expands __STDC_HOSTED__ to 0. */
    NOC_EXECUTION_ENVIRONMENT_FREESTANDING,
    /* Expands __STDC_HOSTED__ to 1. */
    NOC_EXECUTION_ENVIRONMENT_HOSTED,
} Noc_Execution_Environment;

typedef enum {
    NOC_MACRO_EXPANSION_TOKEN_INPUT = 0,
    NOC_MACRO_EXPANSION_TOKEN_ARGUMENT,
    NOC_MACRO_EXPANSION_TOKEN_REPLACEMENT,
    NOC_MACRO_EXPANSION_TOKEN_STRINGIFICATION,
    NOC_MACRO_EXPANSION_TOKEN_PASTE,
    NOC_MACRO_EXPANSION_TOKEN_BUILTIN,
} Noc_Macro_Expansion_Token_Origin;

typedef struct {
    /* Maximum nested provenance frames along one expansion path. */
    size_t max_depth;
    /* Maximum tokens in any live logical sequence, including the result. */
    size_t max_output_tokens;
    /* Maximum object/function expansion frames created by one build. */
    size_t max_expansions;
} Noc_Macro_Expansion_Limits;

/* Per-translation expansion inputs. Start from
   noc_macro_expansion_default_options rather than zero-initializing: zero
   limits are invalid. Date is an unquoted "Mmm dd yyyy" C11 value and time is
   an unquoted "hh:mm:ss" value; validation checks this lexical form, day 1..31,
   hour 0..23, minute 0..59, and second 0..60, but deliberately does not apply a
   calendar or timezone. Empty slices leave those predefined fallbacks
   unavailable. Configured slices are borrowed only for the build call and all
   emitted spellings are copied into the owning expansion. Reuse the same
   options throughout one preprocessing translation so every file/condition
   observes one reproducible translation environment. */
typedef struct {
    Noc_Macro_Expansion_Limits limits;
    Noc_Execution_Environment execution_environment;
    Noc_Slice translation_date;
    Noc_Slice translation_time;
} Noc_Macro_Expansion_Options;

typedef struct {
    Noc_Token token;
    const Noc_Preprocessor_Unit *unit;
    size_t unit_stream_generation;
    size_t preprocessing_token_index;
    size_t frame_index;
    /* Index in expansion.generated_spellings for synthesized tokens, or NONE. */
    size_t generated_spelling_index;
    /* Non-NONE only when origin is BUILTIN. */
    Noc_Macro_Builtin_Kind builtin_kind;
    Noc_Macro_Expansion_Token_Origin origin;
} Noc_Macro_Expansion_Token;

typedef struct {
    size_t environment_entry_index;
    size_t parent_frame_index;
    const Noc_Preprocessor_Unit *invocation_unit;
    size_t invocation_unit_stream_generation;
    size_t invocation_token_index;
} Noc_Macro_Expansion_Frame;

/* Owning expansion view with borrowed source/provenance. Initialize to {0} and
   do not shallow-copy. The environment, input unit, and every definition unit
   referenced by the environment must remain alive and unchanged. Synthesized
   token spellings are owned by this expansion and remain stable until it is
   rebuilt or freed. */
typedef struct {
    const Noc_Macro_Environment *environment;
    size_t environment_generation;
    size_t environment_entry_count;
    size_t environment_entry_limit;
    const Noc_Preprocessor_Unit *input_unit;
    size_t input_unit_stream_generation;
    Noc_Macro_Expansion_Token *items;
    size_t count;
    size_t capacity;
    Noc_Macro_Expansion_Frame *frames;
    size_t frame_count;
    size_t frame_capacity;
    /* Stable, owned spelling arena for synthesized and intermediate tokens. */
    Noc_Slice *generated_spellings;
    size_t generated_spelling_count;
    size_t generated_spelling_capacity;
    /* Private bit representation; query with
       noc_macro_expansion_builtin_is_available rather than interpreting it. */
    uint32_t available_builtin_mask;
    size_t generation;
} Noc_Macro_Expansion;

/* Owning canonical logical source for later preprocessing-aware C parsing.
   This layer gives macro-expanded tokens their own byte coordinate domain
   without changing Noc_Byte_Range or any physical CST/AST query. It currently
   builds one expansion fragment; it does not execute directives, remove
   conditional branches, traverse includes, or claim to be a complete
   preprocessed translation unit.

   Stored token spellings have translation-phase-2 line splices removed. Every
   expansion token is retained in order, including trivia and zero-width
   splice-only trivia. When two non-trivia preprocessing tokens would otherwise
   become adjacent with no nonempty logical trivia, Noc inserts one ASCII space
   token marked GENERATED_SEPARATOR. This conservative canonical spelling keeps
   separately produced tokens from accidentally re-lexing as one token; only
   macro ##/%:%: expansion may intentionally create a combined token.

   Noc_Logical_Source is initialized with {0}, is fully owning, and must not be
   shallow-copied. Text, token pointers, file/path records, and frame pointers
   remain valid until a successful rebuild or free. Successful builds copy
   rendered bytes, physical source identity/sites, and normalized macro frames;
   the expansion, environment, preprocessing units, snapshots, and workspace may
   all then be rebuilt or freed. File IDs retain only their original workspace-
   local meaning, while the copied path and document generation identify the
   physical revision for durable diagnostics and IDE/LSP source lookup. Failed
   builds preserve the previous result and generation. */
typedef struct {
    size_t begin;
    size_t end;
} Noc_Logical_Byte_Range;

typedef struct {
    size_t begin;
    size_t end;
} Noc_Logical_Token_Range;

typedef struct {
    size_t offset;
    /* One-based logical line and byte column. */
    size_t line;
    size_t byte_column;
} Noc_Logical_Location;

enum {
    /* One owned ASCII space inserted solely to preserve a token boundary. It
       has no physical anchor or macro provenance. */
    NOC_LOGICAL_TOKEN_GENERATED_SEPARATOR = 1u << 0,
};

typedef struct {
    Noc_Token_Kind kind;
    Noc_Logical_Byte_Range bytes;
    size_t generation;
    unsigned int flags;
} Noc_Logical_Token;

typedef struct {
    Noc_File_Id file_id;
    size_t document_generation;
    Noc_Source_Class source_class;
    /* Owned by the logical source and NUL-terminated; count excludes NUL. */
    Noc_Slice path;
} Noc_Logical_Source_File;

typedef struct {
    /* Index in this logical source's file table. */
    size_t file_index;
    /* Half-open physical spelling range in that exact document revision. */
    Noc_Byte_Range bytes;
    /* One-based physical line and byte column at bytes.begin. */
    size_t line;
    size_t byte_column;
} Noc_Logical_Physical_Site;

typedef struct {
    /* Immediate physical source token. For stringification, paste, and
       built-ins this is an operator/name anchor, not a claim that generated
       bytes have a physical spelling range. */
    Noc_Logical_Physical_Site anchor;
    /* Index in this logical source's normalized frame array, or
       NOC_TOKEN_INDEX_NONE. */
    size_t macro_frame_index;
    Noc_Macro_Expansion_Token_Origin macro_origin;
    Noc_Macro_Builtin_Kind builtin_kind;
} Noc_Logical_Token_Macro_Provenance;

typedef struct {
    /* The macro-name token in its definition and at this invocation. */
    Noc_Logical_Physical_Site definition;
    Noc_Logical_Physical_Site invocation;
    /* Earlier enclosing frame in this source, or NOC_TOKEN_INDEX_NONE. */
    size_t parent_macro_frame_index;
} Noc_Logical_Macro_Frame;

typedef bool (*Noc_Logical_Source_Cancel_Fn)(void *user_data);

typedef struct {
    /* All limits must be nonzero, and max_source_bytes must be less than
       SIZE_MAX. Source bytes include generated separators but exclude the
       terminating NUL; max_tokens includes separator tokens.
       max_input_bytes_examined bounds physical/generated spelling bytes scanned
       before phase-2 normalization. Path bytes include one NUL per unique source
       file. */
    size_t max_source_bytes;
    size_t max_input_bytes_examined;
    size_t max_tokens;
    size_t max_macro_frames;
    size_t max_source_files;
    size_t max_path_bytes;
    Noc_Logical_Source_Cancel_Fn should_cancel;
    void *cancel_user_data;
} Noc_Logical_Source_Options;

typedef enum {
    NOC_LOGICAL_SOURCE_OK = 0,
    NOC_LOGICAL_SOURCE_INVALID_ARGUMENT,
    NOC_LOGICAL_SOURCE_STALE,
    NOC_LOGICAL_SOURCE_CANCELLED,
    NOC_LOGICAL_SOURCE_LIMIT_EXCEEDED,
    NOC_LOGICAL_SOURCE_GENERATION_EXHAUSTED,
    NOC_LOGICAL_SOURCE_OUT_OF_MEMORY,
} Noc_Logical_Source_Status;

typedef struct Noc_Logical_Source_Impl Noc_Logical_Source_Impl;

typedef struct {
    Noc_Logical_Source_Impl *impl;
    size_t generation;
} Noc_Logical_Source;

/* Owning result of applying normal C11 macro replacement to one physical
   EXPANSION_REQUIRED #include operand and then interpreting the complete token
   sequence as a header name. Initialize to {0}, do not shallow-copy, treat all
   fields as read-only, and release with noc_include_expansion_free.

   macro_expansion owns synthesized token spellings and retains per-token macro
   definition/invocation provenance. body_tokens indexes the original physical
   macro_expansion.input_unit->preprocessing_tokens; header_tokens and
   problem_token_index index macro_expansion.items. All ranges are half-open.
   logical_name is separately owned, stable until rebuild/free, phase-2-splice
   normalized, excludes delimiters, and is not path-normalized or escape-decoded;
   callers must honor its count rather than rely on NUL termination.

   During angle reconstruction, a contiguous input-trivia run between two
   surviving input tokens becomes one space; input trivia bordering or mixed
   with macro-produced output is omitted. Replacement/argument whitespace is
   retained, and every retained comment becomes one space. This deterministic
   preprocessing policy is separate from filesystem normalization.

   A successful build publishes DIRECT, EMPTY, MALFORMED, or INCOMPLETE. EMPTY
   may have no delimiters when expansion produces no significant tokens.
   MALFORMED means the final expansion cannot be one header name; INCOMPLETE
   means it contains an invalid token or an unmatched opening '<'. Problem and
   header indices are expansion-relative. The environment object, input unit,
   and definition units referenced by the environment must remain alive and
   unchanged; rebuilding any of them makes this result stale. */
typedef struct {
    Noc_Macro_Expansion macro_expansion;
    Noc_Token_Range body_tokens;
    Noc_Token_Range header_tokens;
    size_t directive_index;
    size_t problem_token_index;
    Noc_Slice logical_name;
    Noc_Include_Operand_Status status;
    Noc_Include_Form form;
    size_t generation;
} Noc_Include_Expansion;

/* A Noc include graph is a bounded, owning discovery result rather than a
   hidden filesystem preprocessor. It owns its snapshots, preprocessing units,
   conditional analyses, physical operands, expanded operands, logical-name
   bytes, and resolved edges. Initialize the handle to {0}, never shallow-copy
   it, and release it with noc_include_graph_free. Returned node, edge,
   snapshot, unit, conditional, operand, and expansion pointers are borrowed,
   read-only, and stable until the graph is successfully rebuilt or freed.

   Node zero is the root. Traversal is deterministic depth-first source order:
   each source node's include directives retain source order, and a resolved
   child is visited before the next directive in its parent. Traversal frames use
   bounded heap storage rather than one C call frame per include. Every occurrence
   that resolves to a non-ancestor snapshot creates a separate context;
   the same immutable file may therefore appear repeatedly under distinct macro
   prefixes. Resolving to the same retained immutable snapshot revision as an
   ancestor creates a CYCLE edge targeting that existing ancestor; equal
   workspace-local file ids, generations, or paths alone do not establish
   identity.

   Physical classification, optional normal macro expansion, host resolution,
   and recursion remain separately queryable phases. Definitely INACTIVE and
   UNKNOWN conditional edges are recorded but never sent to the resolver.
   Definitely active edges use the concrete local macro prefix visible at their
   directive. Local definitely-active macro events are passed into child
   analysis. This layer deliberately does not splice macro side effects from a
   child back into its parent. A child, cycle, bound, or unknown branch that may
   change macros therefore makes affected later edges UNKNOWN_MACRO_STATE rather
   than evaluating them against an unsound prefix; their published conditional
   activity is UNKNOWN even if the parent-only analysis had predicted ACTIVE or
   INACTIVE. may_mutate_macros propagates this conservative state through
   descendants. This is explicit IDE recovery, not a claim of complete
   translation-unit preprocessing.

   Paths, file identities, snapshots, and source classes are host-authoritative.
   Source class controls configured macro permission but does not establish
   filesystem trust. No graph operation reads the filesystem, searches or
   canonicalizes paths, infers trust, handles include guards or pragma once, or
   suppresses repeated includes. Hosts implement all resolution through the
   required Noc_Include_Resolver callback. */
typedef struct Noc_Include_Graph_Impl Noc_Include_Graph_Impl;
typedef struct {
    Noc_Include_Graph_Impl *impl;
    size_t generation;
} Noc_Include_Graph;

typedef enum {
    NOC_INCLUDE_GRAPH_OK = 0,
    NOC_INCLUDE_GRAPH_INVALID_ARGUMENT,
    NOC_INCLUDE_GRAPH_STALE,
    NOC_INCLUDE_GRAPH_CANCELLED,
    NOC_INCLUDE_GRAPH_GENERATION_EXHAUSTED,
    NOC_INCLUDE_GRAPH_OUT_OF_MEMORY,
    NOC_INCLUDE_GRAPH_PREPROCESSOR_FAILED,
    NOC_INCLUDE_GRAPH_INVALID_RESULT,
} Noc_Include_Graph_Status;

typedef enum {
    NOC_INCLUDE_GRAPH_EDGE_RESOLVED = 0,
    NOC_INCLUDE_GRAPH_EDGE_CYCLE,
    NOC_INCLUDE_GRAPH_EDGE_INACTIVE,
    NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_ACTIVITY,
    NOC_INCLUDE_GRAPH_EDGE_UNKNOWN_MACRO_STATE,
    NOC_INCLUDE_GRAPH_EDGE_INVALID_OPERAND,
    NOC_INCLUDE_GRAPH_EDGE_EXPANSION_FAILED,
    NOC_INCLUDE_GRAPH_EDGE_NOT_FOUND,
    NOC_INCLUDE_GRAPH_EDGE_AMBIGUOUS,
    NOC_INCLUDE_GRAPH_EDGE_DENIED,
    NOC_INCLUDE_GRAPH_EDGE_RESOLVER_FAILED,
    NOC_INCLUDE_GRAPH_EDGE_DEPTH_LIMIT,
    NOC_INCLUDE_GRAPH_EDGE_NODE_LIMIT,
} Noc_Include_Graph_Edge_Status;

enum {
    NOC_INCLUDE_GRAPH_LIMIT_NONE = 0,
    NOC_INCLUDE_GRAPH_LIMIT_DEPTH = 1u << 0,
    NOC_INCLUDE_GRAPH_LIMIT_NODES = 1u << 1,
    NOC_INCLUDE_GRAPH_LIMIT_EDGES = 1u << 2,
};

typedef bool (*Noc_Include_Graph_Cancel_Fn)(void *user_data);

/* max_depth counts edges from the root (whose depth is zero). All three limits
   must be nonzero. Reaching depth/nodes records a recoverable edge and flag;
   reaching max_edges records the flag but cannot allocate an edge for the
   omitted directive. should_cancel is optional and is called only during build;
   returning true aborts transactionally without publishing a partial graph. */
typedef struct {
    Noc_Macro_Policy macro_policy;
    Noc_Macro_Expansion_Options macro_expansion_options;
    size_t max_depth;
    size_t max_nodes;
    size_t max_edges;
    Noc_Include_Graph_Cancel_Fn should_cancel;
    void *cancel_user_data;
} Noc_Include_Graph_Options;

typedef struct {
    size_t index;
    /* NUL-terminated path borrowed from this node's owned snapshot. */
    const char *path;
    Noc_File_Id file_id;
    size_t document_generation;
    Noc_Source_Class source_class;
    /* Number of resolved parent-to-child edges from root node zero. */
    size_t depth;
    size_t outgoing_edge_count;
    /* False only when a graph bound prevented complete local edge discovery or
       traversal. Recoverable syntax/resolver states remain explicit edges. */
    bool traversal_complete;
    /* Conservative transitive answer: including this context can or might
       change the caller's macro state. */
    bool may_mutate_macros;
} Noc_Include_Graph_Node;

typedef struct {
    size_t index;
    size_t source_node_index;
    /* A node index only for RESOLVED and CYCLE; otherwise TOKEN_INDEX_NONE. */
    size_t target_node_index;
    size_t directive_index;
    /* Owned by the graph. Empty when no valid final logical name exists. */
    Noc_Slice logical_name;
    Noc_Preprocessor_Activity conditional_activity;
    Noc_Include_Graph_Edge_Status status;
    Noc_Include_Form form;
    /* INVALID_ARGUMENT when expansion was unnecessary or not attempted. */
    Noc_Macro_Expansion_Status expansion_status;
    /* INVALID_ARGUMENT when host resolution was not attempted. */
    Noc_Include_Resolve_Status resolve_status;
    /* True only when a successful expansion object supplies the final fields. */
    bool macro_expanded;
} Noc_Include_Graph_Edge;

NOCDEF const char *noc_source_class_name(Noc_Source_Class source_class);
NOCDEF const char *noc_macro_policy_name(Noc_Macro_Policy policy);
NOCDEF const char *noc_preprocessor_directive_kind_name(
    Noc_Preprocessor_Directive_Kind kind);
NOCDEF const char *noc_preprocessing_token_role_name(
    Noc_Preprocessing_Token_Role role);
NOCDEF const char *noc_macro_directive_kind_name(Noc_Macro_Directive_Kind kind);
NOCDEF const char *noc_macro_directive_status_name(
    Noc_Macro_Directive_Status status);
NOCDEF bool noc_macro_policy_allows_definition(Noc_Macro_Policy policy,
                                               Noc_Source_Class source_class);
NOCDEF bool noc_preprocessor_unit_build(Noc_Context *context,
                                        const Noc_Document_Snapshot *snapshot,
                                        Noc_Macro_Policy macro_policy,
                                        Noc_Preprocessor_Unit *unit);
NOCDEF void noc_preprocessor_unit_free(Noc_Preprocessor_Unit *unit);
NOCDEF bool noc_preprocessor_unit_is_valid(const Noc_Preprocessor_Unit *unit);
NOCDEF const Noc_Preprocessor_Directive *noc_preprocessor_directive_at(
    const Noc_Preprocessor_Unit *unit,
    size_t index);
/* Significant directive-body span with internal trivia preserved. Directives
   without a body return {NONE, NONE}. */
NOCDEF Noc_Token_Range noc_preprocessor_directive_body_tokens(
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index);
NOCDEF const Noc_Preprocessing_Token *noc_preprocessor_token_at(
    const Noc_Preprocessor_Unit *unit,
    size_t index);
NOCDEF const char *noc_include_operand_status_name(
    Noc_Include_Operand_Status status);
NOCDEF const char *noc_include_form_name(Noc_Include_Form form);
NOCDEF const char *noc_include_operand_build_status_name(
    Noc_Include_Operand_Build_Status status);
NOCDEF const char *noc_include_resolve_status_name(
    Noc_Include_Resolve_Status status);
NOCDEF void noc_include_operand_free(Noc_Include_Operand *operand);
NOCDEF bool noc_include_operand_is_valid(const Noc_Include_Operand *operand);
/* Parse one inventoried #include directive. Exactly one nonempty physical
   HEADER_NAME token produces DIRECT; other statuses retain enough range/problem
   information for diagnostics, highlighting, and completion. */
NOCDEF Noc_Include_Operand_Build_Status noc_include_operand_build(
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index,
    Noc_Include_Operand *output);
/* Resolve a valid DIRECT operand through host policy. FOUND transactionally
   replaces resolved_snapshot. Every other status preserves it. The wrapper
   rejects stale operands and callback/result contract violations. This API
   resolves one edge only: it does not expand an operand, recursively traverse
   includes, detect cycles/guards, or apply conditional activity. */
NOCDEF Noc_Include_Resolve_Status noc_include_resolve(
    Noc_Include_Resolver resolver,
    const Noc_Include_Operand *operand,
    Noc_Document_Snapshot *resolved_snapshot);
NOCDEF void noc_include_expansion_free(Noc_Include_Expansion *expansion);
NOCDEF bool noc_include_expansion_is_valid(
    const Noc_Include_Expansion *expansion);
/* Expand one valid physical EXPANSION_REQUIRED operand with environment entries
   [0,entry_limit), then classify the final include spelling. The limits wrapper
   uses otherwise-default translation inputs; the options wrapper accepts the
   same reproducible predefined inputs as normal macro expansion. Both return
   Noc_Macro_Expansion_Status so expansion failures retain their exact cause.
   Success, including recoverable final syntax, transactionally replaces output;
   every failure preserves it. Physical DIRECT operands intentionally use
   noc_include_resolve without a redundant expansion. */
NOCDEF Noc_Macro_Expansion_Status noc_include_expansion_build(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Include_Operand *operand,
    Noc_Macro_Expansion_Limits limits,
    Noc_Include_Expansion *output);
NOCDEF Noc_Macro_Expansion_Status noc_include_expansion_build_with_options(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Include_Operand *operand,
    Noc_Macro_Expansion_Options options,
    Noc_Include_Expansion *output);
/* Resolve one valid expanded DIRECT result through the same host policy and
   transactional snapshot contract as noc_include_resolve. No filesystem access,
   recursive traversal, conditional-activity decision, or guard handling occurs. */
NOCDEF Noc_Include_Resolve_Status noc_include_expansion_resolve(
    Noc_Include_Resolver resolver,
    const Noc_Include_Expansion *expansion,
    Noc_Document_Snapshot *resolved_snapshot);
NOCDEF const char *noc_include_graph_status_name(Noc_Include_Graph_Status status);
NOCDEF const char *noc_include_graph_edge_status_name(
    Noc_Include_Graph_Edge_Status status);
NOCDEF Noc_Include_Graph_Options noc_include_graph_default_options(void);
NOCDEF void noc_include_graph_free(Noc_Include_Graph *graph);
NOCDEF bool noc_include_graph_is_valid(const Noc_Include_Graph *graph);
/* Build one graph using options.macro_policy for every node. initial_environment
   may be NULL only when initial_entry_limit is zero; that pair starts from an
   empty macro environment. Otherwise [0,initial_entry_limit) is cloned into the
   root analysis. Definition units named by those cloned entries remain borrowed
   and must stay alive and unchanged for the graph lifetime. Root and resolver
   snapshots are retained or moved into the graph, so caller snapshot handles and
   their workspace may be released after success. A nonempty initial prefix may
   not borrow units owned by the same output graph being rebuilt in place; use an
   externally owned prelude environment instead.

   INVALID_OPERAND, EXPANSION_FAILED, ordinary resolver failures, cycles, and
   graph bounds are recoverable published edge states. A cancellation requested
   either by options or the resolver, an allocation/preprocessor failure, stale
   borrowed input, generation exhaustion, or a resolver contract violation is a
   fatal build status and preserves the previous output graph exactly. A
   successful rebuild increments output.generation and invalidates every pointer
   borrowed from the previous graph. */
NOCDEF Noc_Include_Graph_Status noc_include_graph_build(
    Noc_Context *context,
    const Noc_Document_Snapshot *root_snapshot,
    const Noc_Macro_Environment *initial_environment,
    size_t initial_entry_limit,
    Noc_Include_Resolver resolver,
    Noc_Include_Graph_Options options,
    Noc_Include_Graph *output);
NOCDEF size_t noc_include_graph_node_count(const Noc_Include_Graph *graph);
NOCDEF size_t noc_include_graph_edge_count(const Noc_Include_Graph *graph);
NOCDEF unsigned int noc_include_graph_limit_flags(
    const Noc_Include_Graph *graph);
NOCDEF const Noc_Include_Graph_Node *noc_include_graph_node_at(
    const Noc_Include_Graph *graph,
    size_t node_index);
NOCDEF const Noc_Include_Graph_Edge *noc_include_graph_edge_at(
    const Noc_Include_Graph *graph,
    size_t edge_index);
NOCDEF const Noc_Include_Graph_Edge *noc_include_graph_node_edge_at(
    const Noc_Include_Graph *graph,
    size_t node_index,
    size_t outgoing_edge_index);
NOCDEF const Noc_Document_Snapshot *noc_include_graph_node_snapshot(
    const Noc_Include_Graph *graph,
    size_t node_index);
NOCDEF const Noc_Preprocessor_Unit *noc_include_graph_node_preprocessor_unit(
    const Noc_Include_Graph *graph,
    size_t node_index);
NOCDEF const Noc_Preprocessor_Conditional_Groups *
noc_include_graph_node_conditional_groups(const Noc_Include_Graph *graph,
                                          size_t node_index);
NOCDEF const Noc_Include_Operand *noc_include_graph_edge_operand(
    const Noc_Include_Graph *graph,
    size_t edge_index);
/* NULL unless macro expansion completed successfully for this edge, including
   recoverable EMPTY/MALFORMED/INCOMPLETE final syntax. */
NOCDEF const Noc_Include_Expansion *noc_include_graph_edge_expansion(
    const Noc_Include_Graph *graph,
    size_t edge_index);
NOCDEF const Noc_Macro_Directive *noc_macro_directive_at(
    const Noc_Preprocessor_Unit *unit,
    size_t index);
NOCDEF const Noc_Macro_Parameter *noc_macro_parameter_at(
    const Noc_Preprocessor_Unit *unit,
    const Noc_Macro_Directive *directive,
    size_t index);
NOCDEF const char *noc_macro_invocation_status_name(
    Noc_Macro_Invocation_Status status);
NOCDEF const char *noc_macro_invocation_build_status_name(
    Noc_Macro_Invocation_Build_Status status);
NOCDEF void noc_macro_invocation_free(Noc_Macro_Invocation *invocation);
NOCDEF bool noc_macro_invocation_is_valid(
    const Noc_Macro_Invocation *invocation);
/* Tests whether the identifier at name_token_index is followed (after trivia)
   by a function-like invocation and, if so, collects comma-separated arguments
   while balancing nested parentheses. token_limit is an exclusive caller-owned
   boundary and may include the terminal EOF token; this function does not infer
   directive or replacement-list boundaries. Empty parentheses produce zero
   arguments; expansion later interprets that syntax against the selected macro
   definition. NOC_TOKEN_INVALID or a missing close produces INCOMPLETE syntax. */
NOCDEF Noc_Macro_Invocation_Build_Status noc_macro_invocation_parse(
    const Noc_Preprocessor_Unit *unit,
    size_t name_token_index,
    size_t token_limit,
    Noc_Macro_Invocation *output);
NOCDEF const Noc_Macro_Argument *noc_macro_invocation_argument_at(
    const Noc_Macro_Invocation *invocation,
    size_t index);
/* Reports every disabled #define/#undef through context but leaves unit valid. */
NOCDEF bool noc_preprocessor_unit_validate_macro_policy(
    Noc_Context *context,
    const Noc_Preprocessor_Unit *unit);
/* Reports malformed/incomplete #define/#undef records but preserves the unit. */
NOCDEF bool noc_preprocessor_unit_validate_macro_directives(
    Noc_Context *context,
    const Noc_Preprocessor_Unit *unit);
NOCDEF const char *noc_macro_environment_status_name(
    Noc_Macro_Environment_Status status);
NOCDEF void noc_macro_environment_free(Noc_Macro_Environment *environment);
NOCDEF bool noc_macro_environment_is_valid(
    const Noc_Macro_Environment *environment);
/* Applies one structurally valid, policy-enabled directive by index. #undef of
   an absent name is still recorded as an event. Successful apply increments
   environment.generation; every failure leaves the environment unchanged. */
NOCDEF Noc_Macro_Environment_Status noc_macro_environment_apply(
    Noc_Macro_Environment *environment,
    const Noc_Preprocessor_Unit *unit,
    size_t macro_directive_index);
NOCDEF const Noc_Macro_Environment_Entry *noc_macro_environment_entry_at(
    const Noc_Macro_Environment *environment,
    size_t index);
NOCDEF const Noc_Macro_Directive *noc_macro_environment_entry_directive(
    const Noc_Macro_Environment *environment,
    size_t index);
/* Returns the active definition after considering entries [0, entry_limit).
   An absent name or a latest matching #undef returns NULL. */
NOCDEF const Noc_Macro_Environment_Entry *noc_macro_environment_lookup_before(
    const Noc_Macro_Environment *environment,
    Noc_Slice logical_name,
    size_t entry_limit);
NOCDEF const Noc_Macro_Environment_Entry *noc_macro_environment_lookup(
    const Noc_Macro_Environment *environment,
    Noc_Slice logical_name);
NOCDEF const char *noc_conditional_group_status_name(
    Noc_Conditional_Group_Status status);
NOCDEF const char *noc_conditional_condition_status_name(
    Noc_Conditional_Condition_Status status);
NOCDEF const char *noc_conditional_issue_kind_name(
    Noc_Conditional_Issue_Kind kind);
NOCDEF const char *noc_conditional_groups_build_status_name(
    Noc_Conditional_Groups_Build_Status status);
NOCDEF void noc_preprocessor_conditional_groups_free(
    Noc_Preprocessor_Conditional_Groups *groups);
NOCDEF bool noc_preprocessor_conditional_groups_is_valid(
    const Noc_Preprocessor_Conditional_Groups *groups);
/* True only for a valid result with complete, well-formed structure, no
   unresolved condition, and a concrete final macro state. */
NOCDEF bool noc_preprocessor_conditional_groups_is_fully_resolved(
    const Noc_Preprocessor_Conditional_Groups *groups);
/* Clone initial_environment entries [0, initial_entry_limit), scan unit in
   source order, and append only definitely-active local macro events. The
   inline environment no longer borrows the initial environment object itself.
   Recoverable source issues are published in a successful result. */
NOCDEF Noc_Conditional_Groups_Build_Status noc_preprocessor_conditional_groups_build(
    const Noc_Macro_Environment *initial_environment,
    size_t initial_entry_limit,
    const Noc_Preprocessor_Unit *unit,
    Noc_Macro_Expansion_Limits limits,
    Noc_Preprocessor_Conditional_Groups *output);
/* As above, with explicit per-translation predefined-macro inputs. Invalid
   options preserve output just like every other operational failure. */
NOCDEF Noc_Conditional_Groups_Build_Status
noc_preprocessor_conditional_groups_build_with_options(
    const Noc_Macro_Environment *initial_environment,
    size_t initial_entry_limit,
    const Noc_Preprocessor_Unit *unit,
    Noc_Macro_Expansion_Options options,
    Noc_Preprocessor_Conditional_Groups *output);
NOCDEF const Noc_Preprocessor_Conditional_Group *
noc_preprocessor_conditional_group_at(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t index);
NOCDEF const Noc_Preprocessor_Conditional_Branch *
noc_preprocessor_conditional_branch_at(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t index);
NOCDEF const Noc_Preprocessor_Conditional_Issue *
noc_preprocessor_conditional_issue_at(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t index);
/* Openers, peers, and matched closers map to their group. */
NOCDEF size_t noc_preprocessor_conditional_owned_group(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t directive_index);
/* Openers, #elif, and #else map to the branch they introduce. */
NOCDEF size_t noc_preprocessor_conditional_introduced_branch(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t directive_index);
NOCDEF Noc_Preprocessor_Activity noc_preprocessor_conditional_token_activity(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t preprocessing_token_index);
/* Returns NONE when no sound concrete macro state exists at the token. */
NOCDEF size_t noc_preprocessor_conditional_token_macro_entry_limit(
    const Noc_Preprocessor_Conditional_Groups *groups,
    size_t preprocessing_token_index);
/* Read-only: mutating this environment invalidates the enclosing result. */
NOCDEF const Noc_Macro_Environment *noc_preprocessor_conditional_environment(
    const Noc_Preprocessor_Conditional_Groups *groups);
NOCDEF const char *noc_pragma_once_status_name(Noc_Pragma_Once_Status status);
NOCDEF const char *noc_include_guard_status_name(Noc_Include_Guard_Status status);
NOCDEF const char *noc_include_control_build_status_name(
    Noc_Include_Control_Build_Status status);
NOCDEF bool noc_pragma_once_is_valid(const Noc_Pragma_Once *pragma_once);
/* Classify one #pragma directive. Success, including OTHER/recovery syntax,
   transactionally replaces output; operational failure preserves it. */
NOCDEF Noc_Include_Control_Build_Status noc_pragma_once_build(
    const Noc_Preprocessor_Unit *unit,
    size_t directive_index,
    Noc_Pragma_Once *output);
NOCDEF bool noc_include_guard_is_valid(const Noc_Include_Guard *guard);
/* Inspect the first significant file token and, only when it starts #ifndef,
   publish canonical or recoverable guard structure. The conditional analysis
   must belong to unit. Success transactionally replaces output; operational
   failure preserves it. */
NOCDEF Noc_Include_Control_Build_Status noc_include_guard_build(
    const Noc_Preprocessor_Unit *unit,
    const Noc_Preprocessor_Conditional_Groups *groups,
    Noc_Include_Guard *output);
NOCDEF const char *noc_macro_builtin_kind_name(Noc_Macro_Builtin_Kind kind);
/* Classifies phase-2 logical predefined names independently of whether a
   particular translation configured that predefined fallback. Classification
   is useful for syntax highlighting; use the expansion availability query for
   the semantic state of one translation. */
NOCDEF Noc_Macro_Builtin_Kind noc_macro_builtin_kind_from_name(Noc_Slice name);
NOCDEF const char *noc_macro_expansion_status_name(
    Noc_Macro_Expansion_Status status);
NOCDEF const char *noc_preprocessor_expression_status_name(
    Noc_Preprocessor_Expression_Status status);
NOCDEF const char *noc_macro_expansion_token_origin_name(
    Noc_Macro_Expansion_Token_Origin origin);
NOCDEF Noc_Macro_Expansion_Limits noc_macro_expansion_default_limits(void);
NOCDEF Noc_Macro_Expansion_Options noc_macro_expansion_default_options(void);
NOCDEF void noc_macro_expansion_free(Noc_Macro_Expansion *expansion);
NOCDEF bool noc_macro_expansion_is_valid(const Noc_Macro_Expansion *expansion);
/* Queries configured predefined fallback availability. This does not inspect
   explicit #define/#undef precedence and returns false for an invalid result. */
NOCDEF bool noc_macro_expansion_builtin_is_available(
    const Noc_Macro_Expansion *expansion,
    Noc_Macro_Builtin_Kind kind);
/* Expands object-like, fixed-arity, and strict C11 variadic function-like macros
   using environment entries [0, entry_limit). Arguments are collected from the
   logical token stream, prescanned once, substituted, and rescanned with
   provenance retained. # and %: stringify the raw, unprescanned argument using
   C11 whitespace and literal-escaping rules. ## and %:%: paste raw adjacent
   arguments using C11 placemarkers, re-tokenize the result, and rescan it. Noc
   resolves paste chains deterministically from left to right; C11 leaves their
   evaluation order unspecified. __FILE__, __LINE__, __STDC__, and
   __STDC_VERSION__ expand deterministically. Options-aware builds additionally
   provide configured __STDC_HOSTED__, __DATE__, and __TIME__; defaults never
   infer these values from the host or wall clock. C11 requires all seven in a
   conforming implementation, so an unconfigured fallback is a deliberate Noc
   analysis state, not a complete C11 translation environment. File/line use the
   nearest physical token or invocation in the expansion input until #line
   semantics are implemented. An active explicit definition in the selected
   environment prefix takes precedence over predefined fallback; after an
   effective #undef, fallback is eligible again. For F(x, ...), F(value) omits
   the required variable argument and is rejected, while F(value,) supplies it
   explicitly as empty; V() is valid for V(...) and supplies one empty variable
   argument.
   The token limit bounds every live logical sequence, including raw input and
   argument-prescan sequences, rather than only the final rendered result.
   Success replaces output; every failure preserves the prior expansion. */
NOCDEF Noc_Macro_Expansion_Status noc_macro_expansion_build(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Limits limits,
    Noc_Macro_Expansion *output);
/* As above, with explicit per-translation predefined-macro inputs. Options are
   validated before allocation; invalid options preserve an existing output. */
NOCDEF Noc_Macro_Expansion_Status noc_macro_expansion_build_with_options(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Options options,
    Noc_Macro_Expansion *output);
/* As above, but preserves the defined operator and its identifier operand for
   #if/#elif evaluation while expanding every other eligible macro normally.
   If macro replacement generates defined, it is treated as the operator; this
   is a deterministic GCC/Clang-compatible extension to undefined C11 input. */
NOCDEF Noc_Macro_Expansion_Status noc_macro_expansion_build_condition(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Limits limits,
    Noc_Macro_Expansion *output);
NOCDEF Noc_Macro_Expansion_Status
noc_macro_expansion_build_condition_with_options(
    const Noc_Macro_Environment *environment,
    size_t entry_limit,
    const Noc_Preprocessor_Unit *input_unit,
    Noc_Token_Range input_tokens,
    Noc_Macro_Expansion_Options options,
    Noc_Macro_Expansion *output);
NOCDEF const Noc_Macro_Expansion_Token *noc_macro_expansion_token_at(
    const Noc_Macro_Expansion *expansion,
    size_t index);
NOCDEF const Noc_Macro_Expansion_Frame *noc_macro_expansion_frame_at(
    const Noc_Macro_Expansion *expansion,
    size_t index);
/* Concatenates each expansion token's stored spelling into a NUL-terminated
   buffer. Source-backed spellings are physical; synthesized spellings are
   owned by the expansion. Success replaces output; failure preserves it. */
NOCDEF bool noc_macro_expansion_render(const Noc_Macro_Expansion *expansion,
                                       Noc_Buffer *output);
NOCDEF Noc_Logical_Source_Options noc_logical_source_default_options(void);
NOCDEF const char *noc_logical_source_status_name(
    Noc_Logical_Source_Status status);
NOCDEF void noc_logical_source_free(Noc_Logical_Source *source);
/* Validity checks only owned storage and topology; successful results have no
   lifetime dependency on their build inputs. */
NOCDEF bool noc_logical_source_is_valid(const Noc_Logical_Source *source);
NOCDEF size_t noc_logical_source_generation(
    const Noc_Logical_Source *source);
/* Build polls cancellation before work and while copying tokens/frames. Limits,
   cancellation, stale input, allocation failure, invalid input, and exhausted
   generation preserve an existing output exactly. */
NOCDEF Noc_Logical_Source_Status noc_logical_source_build_macro_expansion(
    const Noc_Macro_Expansion *expansion,
    Noc_Logical_Source_Options options,
    Noc_Logical_Source *output);
/* Owned, NUL-terminated text; the NUL is excluded from count. Invalid or stale
   sources return {0}. */
NOCDEF Noc_Slice noc_logical_source_text(const Noc_Logical_Source *source);
NOCDEF size_t noc_logical_source_token_count(
    const Noc_Logical_Source *source);
NOCDEF const Noc_Logical_Token *noc_logical_source_token_at(
    const Noc_Logical_Source *source,
    size_t token_index);
NOCDEF Noc_Slice noc_logical_source_token_text(
    const Noc_Logical_Source *source,
    size_t token_index);
NOCDEF size_t noc_logical_source_line_count(
    const Noc_Logical_Source *source);
/* Logical offsets include EOF (0..text.count). CRLF is one line break while
   both bytes retain separate columns. Scalar outputs are preserved on failure.
   These coordinates deliberately have no path: use token provenance to reach
   one or more exact physical source revisions. */
NOCDEF bool noc_logical_source_location(
    const Noc_Logical_Source *source,
    size_t offset,
    Noc_Logical_Location *output);
NOCDEF bool noc_logical_source_offset(
    const Noc_Logical_Source *source,
    size_t line,
    size_t byte_column,
    size_t *output);
/* Return the smallest half-open token-index range containing every token that
   contributes bytes to BYTES. Zero-width tokens strictly inside a nonempty
   byte range remain visible; zero-width tokens at either boundary are excluded.
   An empty byte range returns an empty token range at the first token that
   contributes a byte at or after that insertion point. Failure preserves
   output. */
NOCDEF bool noc_logical_source_token_range_for_bytes(
    const Noc_Logical_Source *source,
    Noc_Logical_Byte_Range bytes,
    Noc_Logical_Token_Range *output);
/* Returns false and preserves output for generated separators, stale sources,
   invalid indices, or NULL output. */
NOCDEF bool noc_logical_source_token_macro_provenance(
    const Noc_Logical_Source *source,
    size_t token_index,
    Noc_Logical_Token_Macro_Provenance *output);
NOCDEF size_t noc_logical_source_file_count(
    const Noc_Logical_Source *source);
NOCDEF const Noc_Logical_Source_File *noc_logical_source_file_at(
    const Noc_Logical_Source *source,
    size_t file_index);
NOCDEF size_t noc_logical_source_macro_frame_count(
    const Noc_Logical_Source *source);
NOCDEF const Noc_Logical_Macro_Frame *noc_logical_source_macro_frame_at(
    const Noc_Logical_Source *source,
    size_t frame_index);
/* Evaluate a condition-mode macro expansion as a bounded C11 preprocessing
   integer constant expression. Remaining identifiers become zero and defined
   queries the expansion's selected environment prefix, including deterministic
   predefined macros. Character constants requiring execution-character-set
   semantics and negative signed right shifts are reported as target-dependent;
   bounded numeric octal/hexadecimal character escapes are deterministic. On
   success value is replaced and problem_token_index is NONE. Syntax/evaluation
   failures report an expansion-token index when one is available and preserve
   value. Nesting is limited to 256 parser frames. */
NOCDEF Noc_Preprocessor_Expression_Status noc_preprocessor_expression_evaluate(
    const Noc_Macro_Expansion *expansion,
    bool *value,
    size_t *problem_token_index);

/* Slices and tokens */
NOCDEF Noc_Slice noc_slice_from_cstr(const char *text);
NOCDEF bool noc_slice_equal(Noc_Slice left, Noc_Slice right);
NOCDEF bool noc_slice_equal_cstr(Noc_Slice slice, const char *text);
NOCDEF const char *noc_token_kind_name(Noc_Token_Kind kind);
NOCDEF bool noc_token_is_trivia(Noc_Token token);
NOCDEF bool noc_token_is_punct(Noc_Token token, const char *punctuator);
NOCDEF bool noc_token_is_identifier(Noc_Token token, const char *identifier);
/* Copy a token's spelling after C translation phase-2 line-splice deletion.
   The raw token.text remains an exact source slice. Initialize output to {0};
   success replaces it with NUL-terminated bytes, while failure preserves it. */
NOCDEF bool noc_token_logical_text(Noc_Token token, Noc_Buffer *output);

/* Standalone lexer */
NOCDEF void noc_lexer_init(Noc_Lexer *lexer,
                           const char *path,
                           const char *source,
                           size_t source_count);
NOCDEF Noc_Token noc_lexer_next(Noc_Lexer *lexer);

/* General-purpose growable byte buffer */
NOCDEF void noc_buffer_free(Noc_Buffer *buffer);
NOCDEF bool noc_buffer_reserve(Noc_Buffer *buffer, size_t additional_count);
NOCDEF bool noc_buffer_append(Noc_Buffer *buffer, const void *data, size_t count);
NOCDEF bool noc_buffer_append_slice(Noc_Buffer *buffer, Noc_Slice slice);
NOCDEF bool noc_buffer_append_cstr(Noc_Buffer *buffer, const char *text);
NOCDEF bool noc_buffer_appendf(Noc_Buffer *buffer, const char *format, ...)
    NOC_PRINTF_FORMAT(2, 3);
NOCDEF bool noc_buffer_terminate(Noc_Buffer *buffer);

/* Context and rule registry. Rule strings and explicit trigger patterns must
   outlive the context. noc_register_rule() uses the legacy @name trigger.
   Pattern triggers are significant C token sequences: source trivia between
   their tokens is ignored, and a leading @ is reserved for legacy triggers. */
NOCDEF void noc_context_init(Noc_Context *context);
NOCDEF void noc_context_deinit(Noc_Context *context);
NOCDEF void noc_context_set_diagnostic(Noc_Context *context,
                                       Noc_Diagnostic_Fn diagnostic,
                                       void *user_data);
NOCDEF bool noc_register_rule(Noc_Context *context, Noc_Rule rule);
NOCDEF bool noc_register_rule_pattern(Noc_Context *context,
                                      const char *pattern,
                                      Noc_Rule rule);
/* Rules start enabled. Registry mutation is rejected while any outer or nested
   transform is active. A missing name is diagnosed and returns false. */
NOCDEF bool noc_set_rule_enabled(Noc_Context *context, Noc_Slice name, bool enabled);
/* Missing rules and disabled rules both return false; use noc_find_rule() when
   the distinction matters. */
NOCDEF bool noc_rule_is_enabled(const Noc_Context *context, Noc_Slice name);
NOCDEF const Noc_Rule *noc_find_rule(const Noc_Context *context, Noc_Slice name);
NOCDEF void noc_describe(const Noc_Context *context, FILE *stream);
NOCDEF const char *noc_rule_scope_name(Noc_Rule_Scope scope);
/* Generate a standalone C header describing the registered dialect for IDE
   integrations. Options may be NULL or zero-initialized. Initialize output to
   {0}; it is replaced only on success. */
NOCDEF bool noc_generate_ide_metadata_header(
    Noc_Context *context,
    const Noc_Ide_Metadata_Options *options,
    Noc_Buffer *output);
/* Serialize a Make/Ninja-compatible depfile from a transform result. The source
   path is always the first dependency; duplicate source entries are omitted.
   Initialize output to {0}; it is replaced only on success. */
NOCDEF bool noc_generate_depfile(Noc_Context *context,
                                 const char *target_path,
                                 const char *source_path,
                                 const Noc_Transform_Result *result,
                                 Noc_Buffer *output);
/* Serialize an exact, length-prefixed command signature. Empty arguments are
   preserved and embedded newlines remain unambiguous. Initialize output to
   {0}; it is replaced only on success. */
NOCDEF bool noc_generate_command_signature(Noc_Context *context,
                                           const char *const *arguments,
                                           size_t argument_count,
                                           Noc_Buffer *output);

/* Owning token streams. Initialize the output to {0}. Tokens and their
   text/location pointers remain valid until successful retokenization or
   noc_token_stream_free(). Failed retokenization preserves the old stream. */
NOCDEF bool noc_tokenize(Noc_Context *context,
                         const char *path,
                         const char *source,
                         size_t source_count,
                         Noc_Token_Stream *stream);
NOCDEF void noc_token_stream_free(Noc_Token_Stream *stream);
NOCDEF bool noc_token_stream_is_valid(const Noc_Token_Stream *stream);
NOCDEF Noc_Slice noc_token_stream_source(const Noc_Token_Stream *stream);

/* Conservative conditional-compilation analysis. Exact #if 0/#if 1 and #elif
   literals are resolved; macro-dependent expressions remain UNKNOWN. The map
   has one activity per token and borrows the stream. Initialize it to {0}; it
   is replaced only after a successful structurally balanced build. */
NOCDEF bool noc_preprocessor_map_build(Noc_Context *context,
                                       const Noc_Token_Stream *stream,
                                       Noc_Preprocessor_Map *map);
NOCDEF void noc_preprocessor_map_free(Noc_Preprocessor_Map *map);
NOCDEF bool noc_preprocessor_map_is_valid(const Noc_Preprocessor_Map *map);
NOCDEF Noc_Preprocessor_Activity noc_preprocessor_activity_at(
    const Noc_Preprocessor_Map *map,
    size_t token_index);

/* Source ranges and standalone cursors */
NOCDEF bool noc_token_range_is_valid(const Noc_Token_Stream *stream,
                                     Noc_Token_Range range);
/* Invalid input returns {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE}. */
NOCDEF Noc_Token_Range noc_token_range_trim_trivia(const Noc_Token_Stream *stream,
                                                    Noc_Token_Range range);
NOCDEF Noc_Slice noc_token_range_source(const Noc_Token_Stream *stream,
                                        Noc_Token_Range range);
NOCDEF Noc_Location noc_token_range_location(const Noc_Token_Stream *stream,
                                             Noc_Token_Range range);

NOCDEF void noc_token_cursor_init(Noc_Token_Cursor *cursor,
                                  const Noc_Token_Stream *stream);
NOCDEF bool noc_token_cursor_init_range(Noc_Token_Cursor *cursor,
                                        const Noc_Token_Stream *stream,
                                        Noc_Token_Range range);
NOCDEF size_t noc_token_cursor_mark(const Noc_Token_Cursor *cursor);
NOCDEF bool noc_token_cursor_rewind(Noc_Token_Cursor *cursor, size_t mark);
NOCDEF bool noc_token_cursor_at_end(const Noc_Token_Cursor *cursor);
NOCDEF const Noc_Token *noc_token_cursor_peek_raw(const Noc_Token_Cursor *cursor,
                                                  size_t lookahead);
NOCDEF const Noc_Token *noc_token_cursor_peek(const Noc_Token_Cursor *cursor,
                                              size_t lookahead);
NOCDEF bool noc_token_cursor_take_raw(Noc_Token_Cursor *cursor, Noc_Token *token);
NOCDEF bool noc_token_cursor_take(Noc_Token_Cursor *cursor, Noc_Token *token);
NOCDEF void noc_token_cursor_skip_trivia(Noc_Token_Cursor *cursor);
NOCDEF bool noc_token_cursor_match_kind(Noc_Token_Cursor *cursor,
                                        Noc_Token_Kind kind,
                                        Noc_Token *token);
NOCDEF bool noc_token_cursor_match_punct(Noc_Token_Cursor *cursor,
                                         const char *punctuator,
                                         Noc_Token *token);
NOCDEF bool noc_token_cursor_match_identifier(Noc_Token_Cursor *cursor,
                                              const char *identifier,
                                              Noc_Token *token);
NOCDEF bool noc_token_cursor_take_balanced(Noc_Token_Cursor *cursor,
                                           const char *open,
                                           const char *close,
                                           Noc_Token_Range *whole,
                                           Noc_Token_Range *inside);

/* Initialize arguments to {0}. Success transactionally replaces its contents;
   failure preserves them. */
NOCDEF bool noc_parse_arguments(const Noc_Token_Stream *stream,
                                Noc_Token_Range range,
                                Noc_Argument_List *arguments);
NOCDEF void noc_argument_list_free(Noc_Argument_List *arguments);

/* Lossless delimiter syntax tree. Initialize the tree to {0}. It borrows the
   token stream, which must not be freed or successfully retokenized while the
   tree is in use. Failed builds preserve the old tree. */
NOCDEF bool noc_syntax_tree_build(Noc_Context *context,
                                  const Noc_Token_Stream *stream,
                                  Noc_Syntax_Tree *tree);
NOCDEF void noc_syntax_tree_free(Noc_Syntax_Tree *tree);
NOCDEF bool noc_syntax_tree_is_valid(const Noc_Syntax_Tree *tree);
NOCDEF const char *noc_syntax_kind_name(Noc_Syntax_Kind kind);
NOCDEF size_t noc_syntax_root(const Noc_Syntax_Tree *tree);
NOCDEF const Noc_Syntax_Node *noc_syntax_node(const Noc_Syntax_Tree *tree,
                                              size_t node);
NOCDEF size_t noc_syntax_parent(const Noc_Syntax_Tree *tree, size_t node);
NOCDEF size_t noc_syntax_first_child(const Noc_Syntax_Tree *tree, size_t node);
NOCDEF size_t noc_syntax_next_sibling(const Noc_Syntax_Tree *tree, size_t node);
NOCDEF size_t noc_syntax_child_count(const Noc_Syntax_Tree *tree, size_t node);
NOCDEF size_t noc_syntax_first_child_of_kind(const Noc_Syntax_Tree *tree,
                                             size_t node,
                                             Noc_Syntax_Kind kind);
NOCDEF size_t noc_syntax_next_preorder(const Noc_Syntax_Tree *tree, size_t node);
/* Return the deepest node that owns token_index. Opening and closing delimiter
   tokens belong to their group. EOF and invalid indices return NONE. */
NOCDEF size_t noc_syntax_node_at_token(const Noc_Syntax_Tree *tree,
                                       size_t token_index);
/* Return the deepest node covering a non-empty, EOF-free token range. */
NOCDEF size_t noc_syntax_node_covering_range(const Noc_Syntax_Tree *tree,
                                             Noc_Token_Range range);
NOCDEF size_t noc_syntax_depth(const Noc_Syntax_Tree *tree, size_t node);
NOCDEF size_t noc_syntax_common_ancestor(const Noc_Syntax_Tree *tree,
                                         size_t left,
                                         size_t right);
NOCDEF Noc_Token_Range noc_syntax_inner_range(const Noc_Syntax_Tree *tree,
                                              size_t node);
NOCDEF Noc_Slice noc_syntax_source(const Noc_Syntax_Tree *tree, size_t node);
NOCDEF Noc_Location noc_syntax_location(const Noc_Syntax_Tree *tree, size_t node);
NOCDEF const Noc_Token *noc_syntax_token(const Noc_Syntax_Tree *tree, size_t node);

/* Lightweight C external-item discovery over a lossless tree. This is
   deliberately syntactic and does not resolve macros or typedef names.
   Initialize the translation unit to {0}; failed builds preserve it. The
   result owns its item array and borrows only the tree's token stream. */
NOCDEF bool noc_c_translation_unit_build(Noc_Context *context,
                                         const Noc_Syntax_Tree *tree,
                                         Noc_C_Translation_Unit *unit);
NOCDEF void noc_c_translation_unit_free(Noc_C_Translation_Unit *unit);
NOCDEF bool noc_c_translation_unit_is_valid(const Noc_C_Translation_Unit *unit);
NOCDEF const Noc_C_External_Item *noc_c_external_item(const Noc_C_Translation_Unit *unit,
                                                      size_t item);
NOCDEF const char *noc_c_external_kind_name(Noc_C_External_Kind kind);
NOCDEF const char *noc_c_declaration_kind_name(Noc_C_Declaration_Kind kind);
/* Parameter and compound ranges include their outer delimiters. Parameter
   names are best-effort: typedef-dependent ambiguity is reported as no name. */
NOCDEF bool noc_c_parse_parameters(const Noc_Token_Stream *stream,
                                   Noc_Token_Range parameters,
                                   Noc_C_Parameter_List *list);
NOCDEF void noc_c_parameter_list_free(Noc_C_Parameter_List *list);
NOCDEF bool noc_c_compound_statement_is_valid(const Noc_Token_Stream *stream,
                                              Noc_Token_Range compound);
NOCDEF Noc_Token_Range noc_c_compound_statement_inner(const Noc_Token_Stream *stream,
                                                      Noc_Token_Range compound);

/* Transactional, non-overlapping source edits. The set owns replacement text
   and borrows its first edit's token stream. Empty ranges are insertions and
   conflict with another edit at the same boundary. Initialize sets/buffers to
   {0}; failed additions and applications preserve their destinations. */
NOCDEF bool noc_edit_set_is_valid(const Noc_Edit_Set *edits,
                                  const Noc_Token_Stream *stream);
NOCDEF bool noc_edit_set_add(Noc_Edit_Set *edits,
                             const Noc_Token_Stream *stream,
                             Noc_Token_Range range,
                             Noc_Slice replacement);
NOCDEF bool noc_edit_set_add_cstr(Noc_Edit_Set *edits,
                                  const Noc_Token_Stream *stream,
                                  Noc_Token_Range range,
                                  const char *replacement);
NOCDEF bool noc_edit_set_add_syntax(Noc_Edit_Set *edits,
                                    const Noc_Syntax_Tree *tree,
                                    size_t node,
                                    Noc_Slice replacement);
NOCDEF bool noc_edit_set_apply(const Noc_Edit_Set *edits,
                               const Noc_Token_Stream *stream,
                               Noc_Buffer *output);
NOCDEF void noc_edit_set_free(Noc_Edit_Set *edits);

/* Rewriter API available to expansion callbacks. The callback starts just after
   the final significant token of the selected trigger. Raw operations include
   trivia; significant operations skip whitespace and comments. */
NOCDEF const Noc_Token *noc_rw_peek_raw(const Noc_Rewriter *rewriter, size_t lookahead);
NOCDEF const Noc_Token *noc_rw_peek(const Noc_Rewriter *rewriter, size_t lookahead);
NOCDEF bool noc_rw_take_raw(Noc_Rewriter *rewriter, Noc_Token *token);
NOCDEF void noc_rw_skip_trivia(Noc_Rewriter *rewriter);
NOCDEF bool noc_rw_match_punct(Noc_Rewriter *rewriter, const char *punctuator);
NOCDEF bool noc_rw_match_identifier(Noc_Rewriter *rewriter, const char *identifier);
NOCDEF bool noc_rw_expect_punct(Noc_Rewriter *rewriter, const char *punctuator);
NOCDEF bool noc_rw_expect_identifier(Noc_Rewriter *rewriter,
                                     const char *identifier,
                                     Noc_Token *token);
NOCDEF bool noc_rw_capture_balanced(Noc_Rewriter *rewriter,
                                    const char *open,
                                    const char *close,
                                    Noc_Slice *inside);
NOCDEF const char *noc_rw_source_path(const Noc_Rewriter *rewriter);
NOCDEF Noc_Location noc_rw_trigger_location(const Noc_Rewriter *rewriter);
/* The trigger range includes trivia between matched trigger tokens, but not
   trivia before the first or after the last token. It borrows the callback's
   token stream and is valid only during that callback. */
NOCDEF Noc_Token_Range noc_rw_trigger_range(const Noc_Rewriter *rewriter);
/* The stream and lazily built tree are borrowed and valid only for the current
   expansion callback. They must not be modified, freed, retokenized, or
   retained. The remaining half-open range excludes EOF and may be empty. */
NOCDEF const Noc_Token_Stream *noc_rw_token_stream(const Noc_Rewriter *rewriter);
NOCDEF Noc_Token_Range noc_rw_remaining_range(const Noc_Rewriter *rewriter);
/* Consume a range beginning at the exact raw rewrite cursor. Invalid or
   out-of-order ranges are rejected without moving the cursor. */
NOCDEF bool noc_rw_consume_range(Noc_Rewriter *rewriter, Noc_Token_Range range);
NOCDEF const Noc_Syntax_Tree *noc_rw_syntax_tree(Noc_Rewriter *rewriter);
/* Skip trivia and consume the next complete lossless syntax node when its kind
   matches. A mismatch leaves the cursor unchanged. */
NOCDEF bool noc_rw_take_syntax(Noc_Rewriter *rewriter,
                               Noc_Syntax_Kind kind,
                               size_t *node);
/* Record a build input in the transform result. Paths are copied and duplicate
   strings are coalesced in first-seen order. */
NOCDEF bool noc_rw_add_dependency(Noc_Rewriter *rewriter, const char *path);

NOCDEF bool noc_rw_emit(Noc_Rewriter *rewriter, const void *data, size_t count);
NOCDEF bool noc_rw_emit_slice(Noc_Rewriter *rewriter, Noc_Slice slice);
NOCDEF bool noc_rw_emit_cstr(Noc_Rewriter *rewriter, const char *text);
/* Emit only the physical newline sequences present in source. This lets a
   replacement keep following source on its original physical line. */
NOCDEF bool noc_rw_preserve_newlines(Noc_Rewriter *rewriter, Noc_Slice source);
/* Start a #line directive and map the next output line to location.line. A
   NULL location.path uses the current source path; offset and column are not
   represented by C line directives. */
NOCDEF bool noc_rw_emit_line_directive(Noc_Rewriter *rewriter,
                                       Noc_Location location);
/* Transform dialect syntax inside a captured slice with the same registry,
   merge its dependencies, and emit the resulting standard C. */
NOCDEF bool noc_rw_emit_transformed(Noc_Rewriter *rewriter, Noc_Slice source);
NOCDEF bool noc_rw_emitf(Noc_Rewriter *rewriter, const char *format, ...)
    NOC_PRINTF_FORMAT(2, 3);
NOCDEF bool noc_rw_emit_c_string(Noc_Rewriter *rewriter,
                                 const void *data,
                                 size_t count);
NOCDEF void noc_rw_error(Noc_Rewriter *rewriter, const char *format, ...)
    NOC_PRINTF_FORMAT(2, 3);
NOCDEF void noc_rw_error_at(Noc_Rewriter *rewriter,
                            Noc_Location location,
                            const char *format,
                            ...) NOC_PRINTF_FORMAT(3, 4);

/* Decode one ordinary C string token. Prefixes and adjacent string literals are
   intentionally not accepted by this helper. */
NOCDEF bool noc_decode_string_token(Noc_Token token, Noc_Buffer *decoded);

/* Transformation and file/CLI front ends */
NOCDEF bool noc_transform_source(Noc_Context *context,
                                 const char *path,
                                 const char *source,
                                 size_t source_count,
                                 Noc_Transform_Result *result);
/* Atomically write a transformed file while retaining its owning transform
   result for dependency processing. Initialize result to {0}; failures publish
   neither output text nor dependencies. */
NOCDEF bool noc_transform_file_with_result(Noc_Context *context,
                                           const char *input_path,
                                           const char *output_path,
                                           Noc_Transform_Result *result);
NOCDEF bool noc_transform_file(Noc_Context *context,
                               const char *input_path,
                               const char *output_path);
/* Transform ordinary .c/.h inputs into a mirrored output tree. All mappings
   and collisions are validated before writing; completed earlier files remain
   if a later transformation fails. */
NOCDEF bool noc_transform_files(Noc_Context *context,
                                const Noc_Batch_Options *options,
                                const char *const *input_paths,
                                size_t input_count);
NOCDEF void noc_transform_result_free(Noc_Transform_Result *result);
NOCDEF int noc_run_cli(Noc_Context *context, int argc, char **argv);

/* Optional built-in modules */
NOCDEF bool noc_register_embed_rule(Noc_Context *context, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* NOC_H_INCLUDED */
