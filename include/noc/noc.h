/* noc.h - a single-header toolkit for building explicit, project-local C dialects

   This software is released into the public domain. See the end of this file.

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
#define NOC_VERSION_MINOR 35
#define NOC_VERSION_PATCH 0
#define NOC_VERSION "0.35.0"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define NOC_TOKEN_INDEX_NONE ((size_t)-1)
#define NOC_SYNTAX_NONE NOC_TOKEN_INDEX_NONE

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

typedef enum {
    NOC_MACRO_BUILTIN_NONE = 0,
    NOC_MACRO_BUILTIN_FILE,
    NOC_MACRO_BUILTIN_LINE,
    NOC_MACRO_BUILTIN_STDC,
    NOC_MACRO_BUILTIN_STDC_VERSION,
} Noc_Macro_Builtin_Kind;

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
    size_t generation;
} Noc_Macro_Expansion;

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
NOCDEF const char *noc_macro_builtin_kind_name(Noc_Macro_Builtin_Kind kind);
/* Classifies phase-2 logical predefined names. NONE is returned for ordinary
   identifiers and built-ins that require target/translation configuration. */
NOCDEF Noc_Macro_Builtin_Kind noc_macro_builtin_kind_from_name(Noc_Slice name);
NOCDEF const char *noc_macro_expansion_status_name(
    Noc_Macro_Expansion_Status status);
NOCDEF const char *noc_preprocessor_expression_status_name(
    Noc_Preprocessor_Expression_Status status);
NOCDEF const char *noc_macro_expansion_token_origin_name(
    Noc_Macro_Expansion_Token_Origin origin);
NOCDEF Noc_Macro_Expansion_Limits noc_macro_expansion_default_limits(void);
NOCDEF void noc_macro_expansion_free(Noc_Macro_Expansion *expansion);
NOCDEF bool noc_macro_expansion_is_valid(const Noc_Macro_Expansion *expansion);
/* Expands object-like, fixed-arity, and strict C11 variadic function-like macros
   using environment entries [0, entry_limit). Arguments are collected from the
   logical token stream, prescanned once, substituted, and rescanned with
   provenance retained. # and %: stringify the raw, unprescanned argument using
   C11 whitespace and literal-escaping rules. ## and %:%: paste raw adjacent
   arguments using C11 placemarkers, re-tokenize the result, and rescan it. Noc
   resolves paste chains deterministically from left to right; C11 leaves their
   evaluation order unspecified. __FILE__, __LINE__, __STDC__, and
   __STDC_VERSION__ expand deterministically; file/line use the nearest physical
   token or invocation in the expansion input until #line semantics are
   implemented. An active explicit definition in the selected environment
   prefix takes precedence over these predefined macros; after an effective
   #undef, predefined fallback is eligible again. For F(x, ...), F(value) omits
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
