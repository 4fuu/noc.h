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
#define NOC_VERSION_MINOR 25
#define NOC_VERSION_PATCH 0
#define NOC_VERSION "0.25.0"

#include <stdbool.h>
#include <stddef.h>
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
/* Reports every disabled #define/#undef through context but leaves unit valid. */
NOCDEF bool noc_preprocessor_unit_validate_macro_policy(
    Noc_Context *context,
    const Noc_Preprocessor_Unit *unit);
/* Reports malformed/incomplete #define/#undef records but preserves the unit. */
NOCDEF bool noc_preprocessor_unit_validate_macro_directives(
    Noc_Context *context,
    const Noc_Preprocessor_Unit *unit);

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

#ifdef NOC_IMPLEMENTATION
#ifndef NOC_IMPLEMENTATION_INCLUDED
#define NOC_IMPLEMENTATION_INCLUDED

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

typedef Noc_Token_Stream Noc__Tokens;

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} Noc__String_List;

struct Noc_Rewriter {
    Noc_Context *context;
    const Noc_Rule *rule;
    const char *path;
    const char *source;
    size_t source_count;
    const Noc_Token *tokens;
    size_t tokens_count;
    size_t cursor;
    const Noc_Token_Stream *stream;
    Noc_Syntax_Tree syntax_tree;
    bool syntax_tree_attempted;
    Noc_Location trigger_location;
    Noc_Token_Range trigger_range;
    Noc_Buffer *output;
    Noc__String_List *dependencies;
    size_t expansion_depth;
    bool failed;
};

static bool noc__transform_source(Noc_Context *context,
                                  const char *path,
                                  const char *source,
                                  size_t source_count,
                                  Noc_Transform_Result *result,
                                  size_t expansion_depth,
                                  bool emit_line_directives,
                                  bool analyze_preprocessor_activity);

static bool noc__is_identifier_start(unsigned char c)
{
    return c == '_' || isalpha(c) || c >= 128;
}

static bool noc__is_identifier_continue(unsigned char c)
{
    return c == '_' || isalnum(c) || c >= 128;
}

static bool noc__is_horizontal_space(char c)
{
    return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

static bool noc__contains_newline(const char *data, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        if (data[i] == '\n' || data[i] == '\r') return true;
    }
    return false;
}

static size_t noc__splice_length(const char *source, size_t count, size_t position)
{
    if (position + 1 >= count || source[position] != '\\') return 0;
    if (source[position + 1] == '\n') return 2;
    if (source[position + 1] == '\r') {
        return position + 2 < count && source[position + 2] == '\n' ? 3 : 2;
    }
    return 0;
}

static size_t noc__skip_splices(const char *source, size_t count, size_t position)
{
    size_t splice;
    while ((splice = noc__splice_length(source, count, position)) != 0) {
        position += splice;
    }
    return position;
}

static size_t noc__universal_character_name_end(const char *source,
                                                size_t count,
                                                size_t start)
{
    size_t position;
    size_t digit_count;
    size_t index;
    if (start >= count || source[start] != '\\') return start;
    position = noc__skip_splices(source, count, start + 1);
    if (position >= count || (source[position] != 'u' && source[position] != 'U')) {
        return start;
    }
    digit_count = source[position] == 'u' ? 4 : 8;
    position += 1;
    for (index = 0; index < digit_count; ++index) {
        position = noc__skip_splices(source, count, position);
        if (position >= count ||
            !isxdigit((unsigned char)source[position])) {
            return start;
        }
        position += 1;
    }
    return position;
}

static bool noc__slice_logically_equal_cstr(Noc_Slice slice, const char *text)
{
    size_t source_index = 0;
    size_t text_index = 0;
    size_t text_count = text ? strlen(text) : 0;
    if (!slice.data && slice.count != 0) return false;
    while (source_index < slice.count) {
        size_t splice = noc__splice_length(slice.data, slice.count, source_index);
        if (splice != 0) {
            source_index += splice;
            continue;
        }
        if (text_index >= text_count || slice.data[source_index] != text[text_index]) {
            return false;
        }
        source_index += 1;
        text_index += 1;
    }
    return text_index == text_count;
}

static bool noc__slices_logically_equal(Noc_Slice left, Noc_Slice right)
{
    size_t left_index = 0;
    size_t right_index = 0;
    if ((!left.data && left.count != 0) || (!right.data && right.count != 0)) {
        return false;
    }
    for (;;) {
        size_t splice;
        while (left_index < left.count &&
               (splice = noc__splice_length(left.data, left.count, left_index)) != 0) {
            left_index += splice;
        }
        while (right_index < right.count &&
               (splice = noc__splice_length(right.data, right.count, right_index)) != 0) {
            right_index += splice;
        }
        if (left_index == left.count || right_index == right.count) {
            return left_index == left.count && right_index == right.count;
        }
        if (left.data[left_index] != right.data[right_index]) return false;
        left_index += 1;
        right_index += 1;
    }
}

static void noc__lexer_advance(Noc_Lexer *lexer, size_t end)
{
    while (lexer->cursor < end) {
        char c = lexer->source[lexer->cursor++];
        if (c == '\r') {
            if (lexer->cursor < end && lexer->source[lexer->cursor] == '\n') {
                lexer->cursor += 1;
            }
            lexer->line += 1;
            lexer->column = 1;
        } else if (c == '\n') {
            lexer->line += 1;
            lexer->column = 1;
        } else {
            lexer->column += 1;
        }
    }
}

static Noc_Token noc__make_token(Noc_Lexer *lexer,
                                 Noc_Token_Kind kind,
                                 size_t start,
                                 size_t end,
                                 Noc_Location location)
{
    Noc_Token token;
    token.kind = kind;
    token.text.data = lexer->source + start;
    token.text.count = end - start;
    token.location = location;
    noc__lexer_advance(lexer, end);
    return token;
}

NOCDEF Noc_Slice noc_slice_from_cstr(const char *text)
{
    Noc_Slice slice;
    slice.data = text;
    slice.count = text ? strlen(text) : 0;
    return slice;
}

NOCDEF bool noc_slice_equal(Noc_Slice left, Noc_Slice right)
{
    return left.count == right.count &&
           (left.count == 0 || memcmp(left.data, right.data, left.count) == 0);
}

NOCDEF bool noc_slice_equal_cstr(Noc_Slice slice, const char *text)
{
    return noc_slice_equal(slice, noc_slice_from_cstr(text));
}

NOCDEF const char *noc_token_kind_name(Noc_Token_Kind kind)
{
    switch (kind) {
    case NOC_TOKEN_EOF: return "end of file";
    case NOC_TOKEN_WHITESPACE: return "whitespace";
    case NOC_TOKEN_NEWLINE: return "newline";
    case NOC_TOKEN_IDENTIFIER: return "identifier";
    case NOC_TOKEN_NUMBER: return "number";
    case NOC_TOKEN_STRING: return "string";
    case NOC_TOKEN_CHARACTER: return "character";
    case NOC_TOKEN_LINE_COMMENT: return "line comment";
    case NOC_TOKEN_BLOCK_COMMENT: return "block comment";
    case NOC_TOKEN_PREPROCESSOR: return "preprocessor directive";
    case NOC_TOKEN_HEADER_NAME: return "header name";
    case NOC_TOKEN_PUNCTUATOR: return "punctuator";
    case NOC_TOKEN_OTHER: return "non-whitespace character";
    case NOC_TOKEN_INVALID: return "invalid token";
    }
    return "unknown token";
}

NOCDEF bool noc_token_is_trivia(Noc_Token token)
{
    return token.kind == NOC_TOKEN_WHITESPACE ||
           token.kind == NOC_TOKEN_NEWLINE ||
           token.kind == NOC_TOKEN_LINE_COMMENT ||
           token.kind == NOC_TOKEN_BLOCK_COMMENT;
}

NOCDEF bool noc_token_is_punct(Noc_Token token, const char *punctuator)
{
    return token.kind == NOC_TOKEN_PUNCTUATOR &&
           noc__slice_logically_equal_cstr(token.text, punctuator);
}

NOCDEF bool noc_token_is_identifier(Noc_Token token, const char *identifier)
{
    return token.kind == NOC_TOKEN_IDENTIFIER &&
           noc__slice_logically_equal_cstr(token.text, identifier);
}

NOCDEF void noc_lexer_init(Noc_Lexer *lexer,
                           const char *path,
                           const char *source,
                           size_t source_count)
{
    lexer->path = path;
    lexer->source = source;
    lexer->source_count = source_count;
    lexer->cursor = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->beginning_of_line = true;
    lexer->continuing_block_comment = false;
}

static bool noc__logical_pair(const char *source,
                              size_t count,
                              size_t position,
                              char first,
                              char second,
                              size_t *second_position)
{
    size_t next;
    if (position >= count || source[position] != first) return false;
    next = noc__skip_splices(source, count, position + 1);
    if (next >= count || source[next] != second) return false;
    if (second_position) *second_position = next;
    return true;
}

static bool noc__quoted_prefix(const char *source,
                               size_t count,
                               size_t start,
                               size_t *quote_position)
{
    size_t next;
    if (start >= count) return false;
    if (source[start] == '"' || source[start] == '\'') {
        *quote_position = start;
        return true;
    }
    next = noc__skip_splices(source, count, start + 1);
    if ((source[start] == 'L' || source[start] == 'u' || source[start] == 'U') &&
        next < count && (source[next] == '"' || source[next] == '\'')) {
        *quote_position = next;
        return true;
    }
    if (source[start] == 'u' && next < count && source[next] == '8') {
        next = noc__skip_splices(source, count, next + 1);
        if (next < count && source[next] == '"') {
            *quote_position = next;
            return true;
        }
    }
    return false;
}

static size_t noc__scan_line_comment(const Noc_Lexer *lexer, size_t second_slash)
{
    size_t i = second_slash + 1;
    while (i < lexer->source_count) {
        size_t splice = noc__splice_length(lexer->source, lexer->source_count, i);
        if (splice != 0) {
            i += splice;
            continue;
        }
        if (lexer->source[i] == '\n' || lexer->source[i] == '\r') break;
        i += 1;
    }
    return i;
}

static size_t noc__scan_block_comment(const Noc_Lexer *lexer,
                                      size_t content_start,
                                      bool *closed)
{
    size_t i = content_start;
    *closed = false;
    while (i < lexer->source_count) {
        size_t slash;
        size_t splice;
        if (noc__logical_pair(lexer->source,
                              lexer->source_count,
                              i,
                              '*',
                              '/',
                              &slash)) {
            *closed = true;
            return slash + 1;
        }
        splice = noc__splice_length(lexer->source, lexer->source_count, i);
        i += splice ? splice : 1;
    }
    return i;
}

static size_t noc__scan_preprocessor(Noc_Lexer *lexer, size_t start)
{
    size_t i = start;
    char quote = 0;
    bool block_comment = false;
    bool line_comment = false;
    while (i < lexer->source_count) {
        size_t second;
        size_t splice = noc__splice_length(lexer->source, lexer->source_count, i);
        char c;
        if (splice != 0) {
            i += splice;
            continue;
        }
        c = lexer->source[i];
        if (block_comment) {
            if (noc__logical_pair(lexer->source,
                                  lexer->source_count,
                                  i,
                                  '*',
                                  '/',
                                  &second)) {
                block_comment = false;
                i = second + 1;
            } else {
                i += 1;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            i += 1;
            if (c == '\r' && i < lexer->source_count && lexer->source[i] == '\n') i += 1;
            return i;
        }
        if (line_comment) {
            i += 1;
            continue;
        }
        if (quote != 0) {
            if (c == '\\' && i + 1 < lexer->source_count) {
                i += 2;
            } else {
                if (c == quote) quote = 0;
                i += 1;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            i += 1;
            continue;
        }
        if (noc__logical_pair(lexer->source,
                              lexer->source_count,
                              i,
                              '/',
                              '/',
                              &second)) {
            line_comment = true;
            i = second + 1;
            continue;
        }
        if (noc__logical_pair(lexer->source,
                              lexer->source_count,
                              i,
                              '/',
                              '*',
                              &second)) {
            block_comment = true;
            i = second + 1;
            continue;
        }
        i += 1;
    }
    if (block_comment) lexer->continuing_block_comment = true;
    return i;
}

static bool noc__match_logical_punctuator(const char *source,
                                          size_t count,
                                          size_t start,
                                          const char *punctuator,
                                          size_t *end)
{
    size_t position = start;
    size_t i;
    for (i = 0; punctuator[i] != '\0'; ++i) {
        if (i != 0) position = noc__skip_splices(source, count, position);
        if (position >= count || source[position] != punctuator[i]) return false;
        position += 1;
    }
    *end = position;
    return true;
}

static size_t noc__punctuator_end(const char *source, size_t count, size_t start)
{
    static const char *const punctuators4[] = {
        "%:%:", NULL
    };
    static const char *const punctuators3[] = {
        "<<=", ">>=", "...", NULL
    };
    static const char *const punctuators2[] = {
        "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
        "&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=",
        "|=", "##", "<:", ":>", "<%", "%>", "%:", NULL
    };
    size_t i;
    size_t end;
    for (i = 0; punctuators4[i]; ++i) {
        if (noc__match_logical_punctuator(source,
                                          count,
                                          start,
                                          punctuators4[i],
                                          &end)) return end;
    }
    for (i = 0; punctuators3[i]; ++i) {
        if (noc__match_logical_punctuator(source,
                                          count,
                                          start,
                                          punctuators3[i],
                                          &end)) return end;
    }
    for (i = 0; punctuators2[i]; ++i) {
        if (noc__match_logical_punctuator(source,
                                          count,
                                          start,
                                          punctuators2[i],
                                          &end)) return end;
    }
    return start + 1;
}

NOCDEF Noc_Token noc_lexer_next(Noc_Lexer *lexer)
{
    size_t start = lexer->cursor;
    size_t end = start;
    size_t quote = 0;
    bool closed = false;
    Noc_Token_Kind kind;
    Noc_Location location;
    Noc_Token token;

    location.path = lexer->path;
    location.offset = start;
    location.line = lexer->line;
    location.column = lexer->column;

    if (start >= lexer->source_count) {
        if (lexer->continuing_block_comment) {
            lexer->continuing_block_comment = false;
            token.kind = NOC_TOKEN_INVALID;
            token.text.data = lexer->source + lexer->source_count;
            token.text.count = 0;
            token.location = location;
            return token;
        }
        token.kind = NOC_TOKEN_EOF;
        token.text.data = lexer->source + lexer->source_count;
        token.text.count = 0;
        token.location = location;
        return token;
    }

    if (lexer->continuing_block_comment) {
        end = noc__scan_block_comment(lexer, start, &closed);
        lexer->continuing_block_comment = !closed;
        token = noc__make_token(lexer,
                                closed ? NOC_TOKEN_BLOCK_COMMENT : NOC_TOKEN_INVALID,
                                start,
                                end,
                                location);
        lexer->beginning_of_line = true;
        return token;
    }

    if (lexer->beginning_of_line &&
        (lexer->source[start] == '#' ||
         noc__logical_pair(lexer->source,
                           lexer->source_count,
                           start,
                           '%',
                           ':',
                           NULL))) {
        Noc_Slice directive_marker;
        end = noc__punctuator_end(lexer->source, lexer->source_count, start);
        directive_marker.data = lexer->source + start;
        directive_marker.count = end - start;
        if (noc__slice_logically_equal_cstr(directive_marker, "#") ||
            noc__slice_logically_equal_cstr(directive_marker, "%:")) {
            end = noc__scan_preprocessor(lexer, start);
            token = noc__make_token(lexer, NOC_TOKEN_PREPROCESSOR, start, end, location);
            lexer->beginning_of_line = noc__contains_newline(token.text.data,
                                                             token.text.count);
            return token;
        }
    }

    if (lexer->source[start] == '\r' || lexer->source[start] == '\n') {
        end = start + 1;
        if (lexer->source[start] == '\r' && end < lexer->source_count &&
            lexer->source[end] == '\n') {
            end += 1;
        }
        token = noc__make_token(lexer, NOC_TOKEN_NEWLINE, start, end, location);
        lexer->beginning_of_line = true;
        return token;
    }

    if (noc__is_horizontal_space(lexer->source[start]) ||
        (lexer->source[start] == '\\' && start + 1 < lexer->source_count &&
         (lexer->source[start + 1] == '\n' || lexer->source[start + 1] == '\r'))) {
        end = start;
        while (end < lexer->source_count) {
            if (noc__is_horizontal_space(lexer->source[end])) {
                end += 1;
            } else if (lexer->source[end] == '\\' && end + 1 < lexer->source_count &&
                       lexer->source[end + 1] == '\n') {
                end += 2;
            } else if (lexer->source[end] == '\\' && end + 1 < lexer->source_count &&
                       lexer->source[end + 1] == '\r') {
                end += 2;
                if (end < lexer->source_count && lexer->source[end] == '\n') end += 1;
            } else {
                break;
            }
        }
        return noc__make_token(lexer, NOC_TOKEN_WHITESPACE, start, end, location);
    }

    if (noc__logical_pair(lexer->source,
                          lexer->source_count,
                          start,
                          '/',
                          '/',
                          &end)) {
        end = noc__scan_line_comment(lexer, end);
        return noc__make_token(lexer, NOC_TOKEN_LINE_COMMENT, start, end, location);
    }

    if (noc__logical_pair(lexer->source,
                          lexer->source_count,
                          start,
                          '/',
                          '*',
                          &end)) {
        end = noc__scan_block_comment(lexer, end + 1, &closed);
        token = noc__make_token(lexer,
                                closed ? NOC_TOKEN_BLOCK_COMMENT : NOC_TOKEN_INVALID,
                                start,
                                end,
                                location);
        return token;
    }

    if (noc__quoted_prefix(lexer->source, lexer->source_count, start, &quote)) {
        char quote_character = lexer->source[quote];
        end = quote + 1;
        while (end < lexer->source_count) {
            if (lexer->source[end] == '\\') {
                end += 1;
                if (end < lexer->source_count) {
                    if (lexer->source[end] == '\r' && end + 1 < lexer->source_count &&
                        lexer->source[end + 1] == '\n') {
                        end += 2;
                    } else {
                        end += 1;
                    }
                }
            } else if (lexer->source[end] == quote_character) {
                end += 1;
                closed = true;
                break;
            } else if (lexer->source[end] == '\n' || lexer->source[end] == '\r') {
                break;
            } else {
                end += 1;
            }
        }
        kind = quote_character == '"' ? NOC_TOKEN_STRING : NOC_TOKEN_CHARACTER;
        token = noc__make_token(lexer, closed ? kind : NOC_TOKEN_INVALID, start, end, location);
        lexer->beginning_of_line = false;
        return token;
    }

    end = noc__universal_character_name_end(lexer->source,
                                            lexer->source_count,
                                            start);
    if (noc__is_identifier_start((unsigned char)lexer->source[start]) ||
        end != start) {
        if (end == start) end = start + 1;
        while (end < lexer->source_count) {
            size_t next = noc__skip_splices(lexer->source, lexer->source_count, end);
            size_t ucn_end;
            if (next >= lexer->source_count) break;
            ucn_end = noc__universal_character_name_end(lexer->source,
                                                        lexer->source_count,
                                                        next);
            if (ucn_end != next) {
                end = ucn_end;
            } else if (noc__is_identifier_continue(
                           (unsigned char)lexer->source[next])) {
                end = next + 1;
            } else {
                break;
            }
        }
        lexer->beginning_of_line = false;
        return noc__make_token(lexer, NOC_TOKEN_IDENTIFIER, start, end, location);
    }

    if (isdigit((unsigned char)lexer->source[start]) ||
        (lexer->source[start] == '.' &&
         (end = noc__skip_splices(lexer->source,
                                  lexer->source_count,
                                  start + 1)) < lexer->source_count &&
         isdigit((unsigned char)lexer->source[end]))) {
        unsigned char previous = (unsigned char)lexer->source[start];
        end = start + 1;
        while (end < lexer->source_count) {
            size_t next = noc__skip_splices(lexer->source,
                                            lexer->source_count,
                                            end);
            size_t ucn_end;
            unsigned char c;
            if (next >= lexer->source_count) break;
            c = (unsigned char)lexer->source[next];
            ucn_end = noc__universal_character_name_end(lexer->source,
                                                        lexer->source_count,
                                                        next);
            if (isalnum(c) || c == '_' || c == '.') {
                previous = c;
                end = next + 1;
            } else if (ucn_end != next) {
                previous = 0;
                end = ucn_end;
            } else if ((c == '+' || c == '-') &&
                       (previous == 'e' || previous == 'E' ||
                        previous == 'p' || previous == 'P')) {
                previous = c;
                end = next + 1;
            } else {
                break;
            }
        }
        lexer->beginning_of_line = false;
        return noc__make_token(lexer, NOC_TOKEN_NUMBER, start, end, location);
    }

    end = noc__punctuator_end(lexer->source, lexer->source_count, start);
    lexer->beginning_of_line = false;
    return noc__make_token(lexer, NOC_TOKEN_PUNCTUATOR, start, end, location);
}

NOCDEF void noc_buffer_free(Noc_Buffer *buffer)
{
    free(buffer->items);
    buffer->items = NULL;
    buffer->count = 0;
    buffer->capacity = 0;
}

NOCDEF bool noc_buffer_reserve(Noc_Buffer *buffer, size_t additional_count)
{
    size_t needed;
    size_t capacity;
    char *items;
    if (additional_count > SIZE_MAX - buffer->count) return false;
    needed = buffer->count + additional_count;
    if (needed <= buffer->capacity) return true;
    capacity = buffer->capacity ? buffer->capacity : 256;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    items = (char *)realloc(buffer->items, capacity);
    if (!items) return false;
    buffer->items = items;
    buffer->capacity = capacity;
    return true;
}

NOCDEF bool noc_buffer_append(Noc_Buffer *buffer, const void *data, size_t count)
{
    uintptr_t source_address;
    uintptr_t buffer_address;
    size_t source_offset = 0;
    bool source_is_internal = false;
    if (count == 0) return true;
    source_address = (uintptr_t)data;
    buffer_address = (uintptr_t)buffer->items;
    if (buffer->items && source_address >= buffer_address &&
        source_address <= buffer_address + buffer->count) {
        source_offset = (size_t)(source_address - buffer_address);
        if (count > buffer->count - source_offset) return false;
        source_is_internal = true;
    }
    if (!noc_buffer_reserve(buffer, count)) return false;
    if (source_is_internal) data = buffer->items + source_offset;
    memmove(buffer->items + buffer->count, data, count);
    buffer->count += count;
    return true;
}

NOCDEF bool noc_buffer_append_slice(Noc_Buffer *buffer, Noc_Slice slice)
{
    return noc_buffer_append(buffer, slice.data, slice.count);
}

NOCDEF bool noc_buffer_append_cstr(Noc_Buffer *buffer, const char *text)
{
    return noc_buffer_append(buffer, text, strlen(text));
}

static bool noc__buffer_appendfv(Noc_Buffer *buffer, const char *format, va_list arguments)
{
    va_list copy;
    int required;
    va_copy(copy, arguments);
    required = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (required < 0) return false;
    if (!noc_buffer_reserve(buffer, (size_t)required + 1)) return false;
    va_copy(copy, arguments);
    (void)vsnprintf(buffer->items + buffer->count, (size_t)required + 1, format, copy);
    va_end(copy);
    buffer->count += (size_t)required;
    return true;
}

NOCDEF bool noc_buffer_appendf(Noc_Buffer *buffer, const char *format, ...)
{
    bool result;
    va_list arguments;
    va_start(arguments, format);
    result = noc__buffer_appendfv(buffer, format, arguments);
    va_end(arguments);
    return result;
}

NOCDEF bool noc_buffer_terminate(Noc_Buffer *buffer)
{
    if (!noc_buffer_reserve(buffer, 1)) return false;
    buffer->items[buffer->count] = '\0';
    return true;
}

NOCDEF bool noc_token_logical_text(Noc_Token token, Noc_Buffer *output)
{
    Noc_Buffer generated = {0};
    size_t position = 0;
    size_t run_start = 0;
    if (!output || (!token.text.data && token.text.count != 0)) return false;
    while (position < token.text.count) {
        size_t splice = noc__splice_length(token.text.data, token.text.count, position);
        if (splice == 0) {
            position += 1;
            continue;
        }
        if (!noc_buffer_append(&generated,
                               token.text.data + run_start,
                               position - run_start)) {
            goto failed;
        }
        position += splice;
        run_start = position;
    }
    if (!noc_buffer_append(&generated,
                           token.text.data ? token.text.data + run_start : NULL,
                           position - run_start) ||
        !noc_buffer_terminate(&generated)) {
        goto failed;
    }
    noc_buffer_free(output);
    *output = generated;
    return true;

failed:
    noc_buffer_free(&generated);
    return false;
}

static bool noc__buffer_append_c_string(Noc_Buffer *buffer,
                                        const void *data,
                                        size_t count)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    if (!noc_buffer_append_cstr(buffer, "\"")) return false;
    for (i = 0; i < count; ++i) {
        unsigned char c = bytes[i];
        switch (c) {
        case '\\': if (!noc_buffer_append_cstr(buffer, "\\\\")) return false; break;
        case '"': if (!noc_buffer_append_cstr(buffer, "\\\"")) return false; break;
        case '?': if (!noc_buffer_append_cstr(buffer, "\\?")) return false; break;
        case '\n': if (!noc_buffer_append_cstr(buffer, "\\n")) return false; break;
        case '\r': if (!noc_buffer_append_cstr(buffer, "\\r")) return false; break;
        case '\t': if (!noc_buffer_append_cstr(buffer, "\\t")) return false; break;
        default:
            if (c >= 32 && c <= 126) {
                if (!noc_buffer_append(buffer, &c, 1)) return false;
            } else if (!noc_buffer_appendf(buffer, "\\%03o", (unsigned int)c)) {
                return false;
            }
            break;
        }
    }
    return noc_buffer_append_cstr(buffer, "\"");
}

static const char *noc__diagnostic_name(Noc_Diagnostic_Severity severity)
{
    switch (severity) {
    case NOC_DIAGNOSTIC_NOTE: return "note";
    case NOC_DIAGNOSTIC_WARNING: return "warning";
    case NOC_DIAGNOSTIC_ERROR: return "error";
    }
    return "diagnostic";
}

static void noc__default_diagnostic(void *user_data, const Noc_Diagnostic *diagnostic)
{
    FILE *stream = user_data ? (FILE *)user_data : stderr;
    if (diagnostic->location.path) {
        fprintf(stream,
                "%s:%zu:%zu: %s: %s\n",
                diagnostic->location.path,
                diagnostic->location.line,
                diagnostic->location.column,
                noc__diagnostic_name(diagnostic->severity),
                diagnostic->message);
    } else {
        fprintf(stream,
                "noc: %s: %s\n",
                noc__diagnostic_name(diagnostic->severity),
                diagnostic->message);
    }
}

static void noc__reportv(Noc_Context *context,
                         Noc_Diagnostic_Severity severity,
                         Noc_Location location,
                         const char *format,
                         va_list arguments)
{
    Noc_Buffer message = {0};
    Noc_Diagnostic diagnostic;
    if (!noc__buffer_appendfv(&message, format, arguments) ||
        !noc_buffer_terminate(&message)) {
        static const char allocation_failure[] = "out of memory while formatting diagnostic";
        diagnostic.severity = NOC_DIAGNOSTIC_ERROR;
        diagnostic.location = location;
        diagnostic.message = allocation_failure;
    } else {
        diagnostic.severity = severity;
        diagnostic.location = location;
        diagnostic.message = message.items;
    }
    if (severity == NOC_DIAGNOSTIC_ERROR) context->error_count += 1;
    context->diagnostic(context->diagnostic_user_data, &diagnostic);
    noc_buffer_free(&message);
}

static void noc__report(Noc_Context *context,
                        Noc_Diagnostic_Severity severity,
                        Noc_Location location,
                        const char *format,
                        ...)
{
    va_list arguments;
    va_start(arguments, format);
    noc__reportv(context, severity, location, format, arguments);
    va_end(arguments);
}

NOCDEF void noc_context_init(Noc_Context *context)
{
    memset(context, 0, sizeof(*context));
    context->diagnostic = noc__default_diagnostic;
    context->options.emit_line_directives = true;
    context->options.unknown_rule_is_error = true;
    context->options.disabled_rule_is_error = true;
}

NOCDEF void noc_context_deinit(Noc_Context *context)
{
    free(context->rules);
    free(context->rule_patterns);
    free(context->rule_enabled);
    memset(context, 0, sizeof(*context));
}

NOCDEF void noc_context_set_diagnostic(Noc_Context *context,
                                       Noc_Diagnostic_Fn diagnostic,
                                       void *user_data)
{
    context->diagnostic = diagnostic ? diagnostic : noc__default_diagnostic;
    context->diagnostic_user_data = user_data;
}

NOCDEF const Noc_Rule *noc_find_rule(const Noc_Context *context, Noc_Slice name)
{
    size_t i;
    for (i = 0; i < context->rules_count; ++i) {
        if (noc_slice_equal_cstr(name, context->rules[i].name)) return &context->rules[i];
    }
    return NULL;
}

static size_t noc__find_rule_token(const Noc_Context *context, Noc_Token token)
{
    size_t i;
    for (i = 0; i < context->rules_count; ++i) {
        if (!context->rule_patterns[i] &&
            noc_token_is_identifier(token, context->rules[i].name)) {
            return i;
        }
    }
    return NOC_TOKEN_INDEX_NONE;
}

static bool noc__pattern_has_trigraph(const char *pattern)
{
    while (*pattern) {
        if (pattern[0] == '?' && pattern[1] == '?' && pattern[2] != '\0' &&
            strchr("=/'()!<>-", pattern[2]) != NULL) {
            return true;
        }
        ++pattern;
    }
    return false;
}

static bool noc__patterns_equal(const char *left, const char *right)
{
    Noc_Lexer left_lexer;
    Noc_Lexer right_lexer;
    Noc_Token left_token;
    Noc_Token right_token;
    noc_lexer_init(&left_lexer, "<pattern>", left, strlen(left));
    noc_lexer_init(&right_lexer, "<pattern>", right, strlen(right));
    for (;;) {
        do {
            left_token = noc_lexer_next(&left_lexer);
        } while (noc_token_is_trivia(left_token));
        do {
            right_token = noc_lexer_next(&right_lexer);
        } while (noc_token_is_trivia(right_token));
        if (left_token.kind != right_token.kind) break;
        if (left_token.kind == NOC_TOKEN_EOF) return true;
        if (!noc__slices_logically_equal(left_token.text, right_token.text)) break;
    }
    return false;
}

static bool noc__register_rule(Noc_Context *context, const char *pattern, Noc_Rule rule)
{
    Noc_Rule *rules = NULL;
    const char **patterns = NULL;
    bool *enabled = NULL;
    size_t capacity;
    size_t i;
    Noc_Location no_location = {0};
    Noc_Lexer lexer;
    Noc_Token token;
    bool saw_token = false;
    if (context->active_transforms) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "cannot register a rule during an active transform");
        return false;
    }
    if (!rule.name || !rule.name[0]) {
        noc__report(context, NOC_DIAGNOSTIC_ERROR, no_location, "rule name cannot be empty");
        return false;
    }
    if (!rule.expand) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "rule '%s' has no expansion callback",
                    rule.name);
        return false;
    }
    if (noc_find_rule(context, noc_slice_from_cstr(rule.name))) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "rule '%s' is already registered",
                    rule.name);
        return false;
    }
    if (pattern) {
        if (!pattern[0] || noc__pattern_has_trigraph(pattern)) goto invalid_pattern;
        noc_lexer_init(&lexer, "<rule-pattern>", pattern, strlen(pattern));
        for (;;) {
            token = noc_lexer_next(&lexer);
            if (token.kind == NOC_TOKEN_EOF) break;
            if (noc_token_is_trivia(token)) continue;
            if (!saw_token && noc_token_is_punct(token, "@")) goto invalid_pattern;
            saw_token = true;
            if (token.kind == NOC_TOKEN_INVALID ||
                token.kind == NOC_TOKEN_PREPROCESSOR) {
                goto invalid_pattern;
            }
        }
        if (!saw_token) goto invalid_pattern;
        for (i = 0; i < context->rules_count; ++i) {
            if (context->rule_patterns[i] &&
                noc__patterns_equal(pattern, context->rule_patterns[i])) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            no_location,
                            "rule pattern '%s' is already registered",
                            pattern);
                return false;
            }
        }
    }
    if (context->rules_count == context->rules_capacity) {
        if (context->rules_capacity > SIZE_MAX / 2) goto allocation_failed;
        capacity = context->rules_capacity ? context->rules_capacity * 2 : 8;
        if (capacity > SIZE_MAX / sizeof(*rules) ||
            capacity > SIZE_MAX / sizeof(*patterns) ||
            capacity > SIZE_MAX / sizeof(*enabled)) {
            goto allocation_failed;
        }
        rules = (Noc_Rule *)malloc(capacity * sizeof(*rules));
        patterns = (const char **)malloc(capacity * sizeof(*patterns));
        enabled = (bool *)malloc(capacity * sizeof(*enabled));
        if (!rules || !patterns || !enabled) {
allocation_failed:
            free(rules);
            free(patterns);
            free(enabled);
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while registering rule '%s'",
                        rule.name);
            return false;
        }
        if (context->rules_count) {
            memcpy(rules, context->rules, context->rules_count * sizeof(*rules));
            memcpy(patterns,
                   context->rule_patterns,
                   context->rules_count * sizeof(*patterns));
            memcpy(enabled,
                   context->rule_enabled,
                   context->rules_count * sizeof(*enabled));
        }
        free(context->rules);
        free(context->rule_patterns);
        free(context->rule_enabled);
        context->rules = rules;
        context->rule_patterns = patterns;
        context->rule_enabled = enabled;
        context->rules_capacity = capacity;
    }
    context->rules[context->rules_count] = rule;
    context->rule_patterns[context->rules_count] = pattern;
    context->rule_enabled[context->rules_count] = true;
    context->rules_count += 1;
    return true;

invalid_pattern:
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "invalid rule pattern for rule '%s'",
                rule.name ? rule.name : "");
    return false;
}

NOCDEF bool noc_register_rule(Noc_Context *context, Noc_Rule rule)
{
    return noc__register_rule(context, NULL, rule);
}

NOCDEF bool noc_register_rule_pattern(Noc_Context *context,
                                      const char *pattern,
                                      Noc_Rule rule)
{
    if (!pattern) {
        Noc_Location none = {0};
        noc__report(context, NOC_DIAGNOSTIC_ERROR, none, "rule pattern cannot be NULL");
        return false;
    }
    return noc__register_rule(context, pattern, rule);
}

NOCDEF bool noc_rule_is_enabled(const Noc_Context *context, Noc_Slice name)
{
    size_t i;
    if (!name.data && name.count != 0) return false;
    for (i = 0; i < context->rules_count; ++i) {
        if (noc_slice_equal_cstr(name, context->rules[i].name)) {
            return context->rule_enabled[i];
        }
    }
    return false;
}

NOCDEF bool noc_set_rule_enabled(Noc_Context *context, Noc_Slice name, bool value)
{
    size_t i;
    Noc_Location none = {0};
    if (context->active_transforms) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    none,
                    "cannot change rule enable state during an active transform");
        return false;
    }
    if (!name.data && name.count != 0) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    none,
                    "rule name slice is invalid");
        return false;
    }
    for (i = 0; i < context->rules_count; ++i) {
        if (noc_slice_equal_cstr(name, context->rules[i].name)) {
            context->rule_enabled[i] = value;
            return true;
        }
    }
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                none,
                "unknown rule '%.*s%s'",
                (int)(name.count < 80 ? name.count : 80),
                name.data ? name.data : "",
                name.count > 80 ? "..." : "");
    return false;
}

NOCDEF const char *noc_rule_scope_name(Noc_Rule_Scope scope)
{
    switch (scope) {
    case NOC_RULE_TOKEN: return "token";
    case NOC_RULE_EXPRESSION: return "expression";
    case NOC_RULE_STATEMENT: return "statement";
    case NOC_RULE_DECLARATION: return "declaration";
    case NOC_RULE_ATTRIBUTE: return "attribute";
    case NOC_RULE_DIRECTIVE: return "directive";
    }
    return "unknown";
}

NOCDEF void noc_describe(const Noc_Context *context, FILE *stream)
{
    size_t i;
    fprintf(stream, "Project dialect (%zu rule%s):\n",
            context->rules_count,
            context->rules_count == 1 ? "" : "s");
    for (i = 0; i < context->rules_count; ++i) {
        const Noc_Rule *rule = &context->rules[i];
        fprintf(stream, "\n  %s%s [%s]%s\n",
                context->rule_patterns[i] ? "" : "@",
                context->rule_patterns[i] ? context->rule_patterns[i] : rule->name,
                noc_rule_scope_name(rule->scope),
                context->rule_enabled[i] ? "" : " (disabled)");
        if (rule->syntax) fprintf(stream, "    Syntax: %s\n", rule->syntax);
        if (rule->description) fprintf(stream, "    %s\n", rule->description);
    }
}

static bool noc__is_c_identifier(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    if (!cursor || !(cursor[0] == '_' ||
                     (cursor[0] >= 'A' && cursor[0] <= 'Z') ||
                     (cursor[0] >= 'a' && cursor[0] <= 'z'))) {
        return false;
    }
    cursor += 1;
    while (*cursor) {
        if (!(*cursor == '_' || (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= '0' && *cursor <= '9'))) {
            return false;
        }
        cursor += 1;
    }
    return true;
}

NOCDEF bool noc_generate_ide_metadata_header(
    Noc_Context *context,
    const Noc_Ide_Metadata_Options *options,
    Noc_Buffer *output)
{
    const char *guard = options && options->include_guard
                            ? options->include_guard
                            : "NOC_IDE_METADATA_H_INCLUDED";
    const char *prefix = options && options->macro_prefix
                             ? options->macro_prefix
                             : "NOC_IDE";
    const char *dialect = options && options->dialect_name
                              ? options->dialect_name
                              : "project";
    bool omit_descriptions = options && options->omit_descriptions;
    Noc_Buffer generated = {0};
    Noc_Location no_location = {0};
    size_t i;
    if (!context || !output) return false;
    if (!noc__is_c_identifier(guard) || !noc__is_c_identifier(prefix)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "IDE metadata guard and macro prefix must be C identifiers");
        return false;
    }
    if (!noc_buffer_appendf(&generated,
                            "/* Generated by noc.h %s. Do not edit. */\n"
                            "#ifndef %s\n"
                            "#define %s\n\n"
                            "#define %s_SCHEMA_VERSION 2\n"
                            "#define %s_NOC_VERSION ",
                            NOC_VERSION,
                            guard,
                            guard,
                            prefix,
                            prefix) ||
        !noc__buffer_append_c_string(&generated, NOC_VERSION, strlen(NOC_VERSION)) ||
        !noc_buffer_appendf(&generated, "\n#define %s_DIALECT_NAME ", prefix) ||
        !noc__buffer_append_c_string(&generated, dialect, strlen(dialect)) ||
        !noc_buffer_appendf(&generated,
                            "\n#define %s_RULE_COUNT %zu\n",
                            prefix,
                            context->rules_count)) {
        goto failed;
    }
    for (i = 0; i < context->rules_count; ++i) {
        const Noc_Rule *rule = &context->rules[i];
        const char *syntax = rule->syntax ? rule->syntax : "";
        const char *description = rule->description ? rule->description : "";
        const char *scope = noc_rule_scope_name(rule->scope);
        const char *pattern = context->rule_patterns[i];
        Noc_Buffer trigger = {0};
        if (pattern) {
            if (!noc_buffer_append_cstr(&trigger, pattern)) {
                noc_buffer_free(&trigger);
                goto failed;
            }
        } else if (!noc_buffer_appendf(&trigger, "@%s", rule->name)) {
            noc_buffer_free(&trigger);
            goto failed;
        }
        if (!noc_buffer_appendf(&generated, "\n#define %s_RULE_%zu_NAME ", prefix, i) ||
            !noc__buffer_append_c_string(&generated, rule->name, strlen(rule->name)) ||
            !noc_buffer_appendf(&generated,
                                "\n#define %s_RULE_%zu_TRIGGER_KIND %d"
                                "\n#define %s_RULE_%zu_TRIGGER_KIND_NAME \"%s\""
                                "\n#define %s_RULE_%zu_TRIGGER ",
                                prefix, i,
                                pattern ? NOC_RULE_TRIGGER_PATTERN
                                        : NOC_RULE_TRIGGER_AT_NAME,
                                prefix, i, pattern ? "pattern" : "at-name",
                                prefix, i) ||
            !noc__buffer_append_c_string(&generated, trigger.items, trigger.count) ||
            !noc_buffer_appendf(&generated, "\n#define %s_RULE_%zu_ENABLED %d",
                                prefix, i, context->rule_enabled[i] ? 1 : 0) ||
            !noc_buffer_appendf(&generated,
                                "\n#define %s_RULE_%zu_SCOPE %d"
                                "\n#define %s_RULE_%zu_SCOPE_NAME ",
                                prefix,
                                i,
                                (int)rule->scope,
                                prefix,
                                i) ||
            !noc__buffer_append_c_string(&generated, scope, strlen(scope)) ||
            !noc_buffer_appendf(&generated,
                                "\n#define %s_RULE_%zu_SYNTAX ",
                                prefix,
                                i) ||
            !noc__buffer_append_c_string(&generated, syntax, strlen(syntax))) {
            noc_buffer_free(&trigger);
            goto failed;
        }
        if (!omit_descriptions &&
            (!noc_buffer_appendf(&generated,
                                 "\n#define %s_RULE_%zu_DESCRIPTION ",
                                 prefix,
                                 i) ||
             !noc__buffer_append_c_string(&generated,
                                          description,
                                          strlen(description)))) {
            noc_buffer_free(&trigger);
            goto failed;
        }
        noc_buffer_free(&trigger);
        if (!noc_buffer_append_cstr(&generated, "\n")) goto failed;
    }
    if (!noc_buffer_appendf(&generated, "\n#endif /* %s */\n", guard) ||
        !noc_buffer_terminate(&generated)) {
        goto failed;
    }
    noc_buffer_free(output);
    *output = generated;
    return true;

failed:
    noc_buffer_free(&generated);
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "out of memory while generating IDE metadata header");
    return false;
}

static bool noc__depfile_path_is_valid(const char *path)
{
    const unsigned char *cursor = (const unsigned char *)path;
    if (!cursor || !cursor[0]) return false;
    while (*cursor) {
        if (*cursor == '\n' || *cursor == '\r') return false;
        cursor += 1;
    }
    return true;
}

static bool noc__depfile_append_path(Noc_Buffer *output, const char *path)
{
    const unsigned char *cursor = (const unsigned char *)path;
    while (*cursor) {
        if (*cursor == '$') {
            if (!noc_buffer_append_cstr(output, "$$")) return false;
        } else {
            if ((*cursor == ' ' || *cursor == '\t' || *cursor == '#' ||
                 *cursor == ':' || *cursor == '\\') &&
                !noc_buffer_append(output, "\\", 1)) {
                return false;
            }
            if (!noc_buffer_append(output, cursor, 1)) return false;
        }
        cursor += 1;
    }
    return true;
}

NOCDEF bool noc_generate_depfile(Noc_Context *context,
                                 const char *target_path,
                                 const char *source_path,
                                 const Noc_Transform_Result *result,
                                 Noc_Buffer *output)
{
    Noc_Buffer generated = {0};
    Noc_Location no_location = {0};
    size_t i;
    if (!context || !result || !output) return false;
    if (!noc__depfile_path_is_valid(target_path) ||
        !noc__depfile_path_is_valid(source_path) ||
        (result->dependency_count > 0 && !result->dependencies)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "depfile paths must be non-empty and cannot contain newlines");
        return false;
    }
    for (i = 0; i < result->dependency_count; ++i) {
        if (!noc__depfile_path_is_valid(result->dependencies[i])) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "depfile paths must be non-empty and cannot contain newlines");
            return false;
        }
    }
    if (!noc__depfile_append_path(&generated, target_path) ||
        !noc_buffer_append_cstr(&generated, ": ") ||
        !noc__depfile_append_path(&generated, source_path)) {
        goto failed;
    }
    for (i = 0; i < result->dependency_count; ++i) {
        if (strcmp(result->dependencies[i], source_path) == 0) continue;
        if (!noc_buffer_append_cstr(&generated, " ") ||
            !noc__depfile_append_path(&generated, result->dependencies[i])) {
            goto failed;
        }
    }
    if (!noc_buffer_append_cstr(&generated, "\n") ||
        !noc_buffer_terminate(&generated)) {
        goto failed;
    }
    noc_buffer_free(output);
    *output = generated;
    return true;

failed:
    noc_buffer_free(&generated);
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "out of memory while generating depfile");
    return false;
}

NOCDEF bool noc_generate_command_signature(Noc_Context *context,
                                           const char *const *arguments,
                                           size_t argument_count,
                                           Noc_Buffer *output)
{
    Noc_Buffer generated = {0};
    Noc_Location no_location = {0};
    size_t i;
    if (!context || !output || (argument_count > 0 && !arguments)) return false;
    for (i = 0; i < argument_count; ++i) {
        if (!arguments[i]) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "command signature arguments cannot be NULL");
            return false;
        }
    }
    if (!noc_buffer_appendf(&generated,
                            "noc-command-signature 1\n"
                            "noc-version %zu:%s\n"
                            "arguments %zu\n",
                            strlen(NOC_VERSION),
                            NOC_VERSION,
                            argument_count)) {
        goto failed;
    }
    for (i = 0; i < argument_count; ++i) {
        size_t count = strlen(arguments[i]);
        if (!noc_buffer_appendf(&generated, "argument %zu:", count) ||
            !noc_buffer_append(&generated, arguments[i], count) ||
            !noc_buffer_append_cstr(&generated, "\n")) {
            goto failed;
        }
    }
    if (!noc_buffer_terminate(&generated)) goto failed;
    noc_buffer_free(output);
    *output = generated;
    return true;

failed:
    noc_buffer_free(&generated);
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                no_location,
                "out of memory while generating command signature");
    return false;
}

static bool noc__emit_line_directive_at(Noc_Buffer *output,
                                        const char *path,
                                        size_t line);

NOCDEF const Noc_Token *noc_rw_peek_raw(const Noc_Rewriter *rewriter, size_t lookahead)
{
    size_t index = rewriter->cursor + lookahead;
    if (index >= rewriter->tokens_count) return NULL;
    return &rewriter->tokens[index];
}

NOCDEF const Noc_Token *noc_rw_peek(const Noc_Rewriter *rewriter, size_t lookahead)
{
    size_t index = rewriter->cursor;
    size_t found = 0;
    while (index < rewriter->tokens_count) {
        if (!noc_token_is_trivia(rewriter->tokens[index])) {
            if (found == lookahead) return &rewriter->tokens[index];
            found += 1;
        }
        index += 1;
    }
    return NULL;
}

NOCDEF bool noc_rw_take_raw(Noc_Rewriter *rewriter, Noc_Token *token)
{
    if (rewriter->cursor >= rewriter->tokens_count) return false;
    if (token) *token = rewriter->tokens[rewriter->cursor];
    rewriter->cursor += 1;
    return true;
}

NOCDEF void noc_rw_skip_trivia(Noc_Rewriter *rewriter)
{
    while (rewriter->cursor < rewriter->tokens_count &&
           noc_token_is_trivia(rewriter->tokens[rewriter->cursor])) {
        rewriter->cursor += 1;
    }
}

NOCDEF bool noc_rw_match_punct(Noc_Rewriter *rewriter, const char *punctuator)
{
    size_t cursor = rewriter->cursor;
    while (cursor < rewriter->tokens_count &&
           noc_token_is_trivia(rewriter->tokens[cursor])) {
        cursor += 1;
    }
    if (cursor < rewriter->tokens_count &&
        noc_token_is_punct(rewriter->tokens[cursor], punctuator)) {
        rewriter->cursor = cursor + 1;
        return true;
    }
    return false;
}

NOCDEF bool noc_rw_match_identifier(Noc_Rewriter *rewriter, const char *identifier)
{
    size_t cursor = rewriter->cursor;
    while (cursor < rewriter->tokens_count &&
           noc_token_is_trivia(rewriter->tokens[cursor])) {
        cursor += 1;
    }
    if (cursor < rewriter->tokens_count &&
        noc_token_is_identifier(rewriter->tokens[cursor], identifier)) {
        rewriter->cursor = cursor + 1;
        return true;
    }
    return false;
}

NOCDEF bool noc_rw_expect_punct(Noc_Rewriter *rewriter, const char *punctuator)
{
    const Noc_Token *token = noc_rw_peek(rewriter, 0);
    if (noc_rw_match_punct(rewriter, punctuator)) return true;
    if (token) {
        noc_rw_error_at(rewriter,
                        token->location,
                        "expected '%s' after trigger for rule '%s', got %s '%.*s%s'",
                        punctuator,
                        rewriter->rule->name,
                        noc_token_kind_name(token->kind),
                        (int)(token->text.count < 80 ? token->text.count : 80),
                        token->text.data,
                        token->text.count > 80 ? "..." : "");
    } else {
        noc_rw_error(rewriter, "expected '%s' after trigger for rule '%s'", punctuator, rewriter->rule->name);
    }
    return false;
}

NOCDEF bool noc_rw_expect_identifier(Noc_Rewriter *rewriter,
                                     const char *identifier,
                                     Noc_Token *token)
{
    const Noc_Token *next;
    noc_rw_skip_trivia(rewriter);
    next = noc_rw_peek_raw(rewriter, 0);
    if (next && next->kind == NOC_TOKEN_IDENTIFIER &&
        (!identifier || noc_token_is_identifier(*next, identifier))) {
        if (token) *token = *next;
        rewriter->cursor += 1;
        return true;
    }
    if (next) {
        noc_rw_error_at(rewriter,
                        next->location,
                        "expected %sidentifier after trigger for rule '%s', got %s '%.*s%s'",
                        identifier ? identifier : "",
                        rewriter->rule->name,
                        noc_token_kind_name(next->kind),
                        (int)(next->text.count < 80 ? next->text.count : 80),
                        next->text.data,
                        next->text.count > 80 ? "..." : "");
    } else {
        noc_rw_error(rewriter, "expected identifier after trigger for rule '%s'", rewriter->rule->name);
    }
    return false;
}

NOCDEF bool noc_rw_capture_balanced(Noc_Rewriter *rewriter,
                                    const char *open,
                                    const char *close,
                                    Noc_Slice *inside)
{
    const Noc_Token *token;
    size_t start;
    size_t depth = 1;
    if (!noc_rw_expect_punct(rewriter, open)) return false;
    start = rewriter->cursor < rewriter->tokens_count
                ? rewriter->tokens[rewriter->cursor].location.offset
                : rewriter->source_count;
    while (rewriter->cursor < rewriter->tokens_count) {
        token = &rewriter->tokens[rewriter->cursor++];
        if (token->kind == NOC_TOKEN_EOF) break;
        if (noc_token_is_punct(*token, open)) {
            depth += 1;
        } else if (noc_token_is_punct(*token, close)) {
            depth -= 1;
            if (depth == 0) {
                inside->data = rewriter->source + start;
                inside->count = token->location.offset - start;
                return true;
            }
        }
    }
    noc_rw_error(rewriter,
                 "unterminated '%s ... %s' after trigger for rule '%s'",
                 open,
                 close,
                 rewriter->rule->name);
    return false;
}

NOCDEF const char *noc_rw_source_path(const Noc_Rewriter *rewriter)
{
    return rewriter->path;
}

NOCDEF Noc_Location noc_rw_trigger_location(const Noc_Rewriter *rewriter)
{
    return rewriter->trigger_location;
}

NOCDEF Noc_Token_Range noc_rw_trigger_range(const Noc_Rewriter *rewriter)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    return rewriter ? rewriter->trigger_range : invalid;
}

NOCDEF const Noc_Token_Stream *noc_rw_token_stream(const Noc_Rewriter *rewriter)
{
    return rewriter && noc_token_stream_is_valid(rewriter->stream)
               ? rewriter->stream
               : NULL;
}

NOCDEF Noc_Token_Range noc_rw_remaining_range(const Noc_Rewriter *rewriter)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    const Noc_Token_Stream *stream = noc_rw_token_stream(rewriter);
    if (!stream || rewriter->cursor >= stream->count) return invalid;
    invalid.begin = rewriter->cursor;
    invalid.end = stream->count - 1;
    return invalid;
}

NOCDEF bool noc_rw_consume_range(Noc_Rewriter *rewriter, Noc_Token_Range range)
{
    Noc_Token_Range remaining = noc_rw_remaining_range(rewriter);
    if (remaining.begin == NOC_TOKEN_INDEX_NONE || range.begin != remaining.begin ||
        range.end < range.begin || range.end > remaining.end) {
        return false;
    }
    rewriter->cursor = range.end;
    return true;
}

NOCDEF const Noc_Syntax_Tree *noc_rw_syntax_tree(Noc_Rewriter *rewriter)
{
    if (!rewriter || !noc_rw_token_stream(rewriter)) return NULL;
    if (!rewriter->syntax_tree_attempted) {
        rewriter->syntax_tree_attempted = true;
        if (!noc_syntax_tree_build(rewriter->context,
                                   rewriter->stream,
                                   &rewriter->syntax_tree)) {
            rewriter->failed = true;
            return NULL;
        }
    }
    return noc_syntax_tree_is_valid(&rewriter->syntax_tree)
               ? &rewriter->syntax_tree
               : NULL;
}

NOCDEF bool noc_rw_take_syntax(Noc_Rewriter *rewriter,
                               Noc_Syntax_Kind kind,
                               size_t *node)
{
    const Noc_Syntax_Tree *tree;
    size_t cursor;
    size_t i;
    if (!rewriter || kind == NOC_SYNTAX_ROOT) return false;
    cursor = rewriter->cursor;
    while (cursor < rewriter->tokens_count &&
           noc_token_is_trivia(rewriter->tokens[cursor])) {
        cursor += 1;
    }
    tree = noc_rw_syntax_tree(rewriter);
    if (!tree) return false;
    for (i = 1; i < tree->count; ++i) {
        const Noc_Syntax_Node *syntax = &tree->items[i];
        if (syntax->range.begin == cursor) {
            if (syntax->kind != kind) return false;
            rewriter->cursor = syntax->range.end;
            if (node) *node = i;
            return true;
        }
    }
    return false;
}

static void noc__string_list_free(Noc__String_List *list)
{
    size_t i;
    for (i = 0; i < list->count; ++i) free(list->items[i]);
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool noc__string_list_append_unique(Noc__String_List *list, const char *text)
{
    char **items;
    char *copy;
    size_t text_count;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    size_t i;
    for (i = 0; i < list->count; ++i) {
        if (strcmp(list->items[i], text) == 0) return true;
    }
    text_count = strlen(text);
    if (text_count == SIZE_MAX) return false;
    copy = (char *)malloc(text_count + 1);
    if (!copy) return false;
    memcpy(copy, text, text_count + 1);
    if (list->count == list->capacity) {
        if (list->capacity >= maximum) goto failed;
        if (list->capacity == 0) {
            capacity = maximum < 4 ? maximum : 4;
        } else if (list->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = list->capacity * 2;
        }
        if (capacity <= list->capacity) goto failed;
        items = (char **)realloc(list->items, capacity * sizeof(*items));
        if (!items) goto failed;
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = copy;
    return true;

failed:
    free(copy);
    return false;
}

NOCDEF bool noc_rw_add_dependency(Noc_Rewriter *rewriter, const char *path)
{
    if (!rewriter || !rewriter->dependencies || !path || !path[0] ||
        !noc__string_list_append_unique(rewriter->dependencies, path)) {
        if (rewriter) {
            noc_rw_error(rewriter,
                         "could not record dependency while expanding rule '%s'",
                         rewriter->rule->name);
        }
        return false;
    }
    return true;
}

NOCDEF bool noc_rw_emit(Noc_Rewriter *rewriter, const void *data, size_t count)
{
    if (!noc_buffer_append(rewriter->output, data, count)) {
        noc_rw_error(rewriter, "out of memory while expanding rule '%s'", rewriter->rule->name);
        return false;
    }
    return true;
}

NOCDEF bool noc_rw_emit_slice(Noc_Rewriter *rewriter, Noc_Slice slice)
{
    return noc_rw_emit(rewriter, slice.data, slice.count);
}

NOCDEF bool noc_rw_emit_cstr(Noc_Rewriter *rewriter, const char *text)
{
    return noc_rw_emit(rewriter, text, strlen(text));
}

NOCDEF bool noc_rw_preserve_newlines(Noc_Rewriter *rewriter, Noc_Slice source)
{
    size_t i = 0;
    if (!rewriter || (source.count > 0 && !source.data)) {
        if (rewriter) noc_rw_error(rewriter, "invalid source while preserving newlines");
        return false;
    }
    while (i < source.count) {
        if (source.data[i] == '\r') {
            size_t count = i + 1 < source.count && source.data[i + 1] == '\n' ? 2 : 1;
            if (!noc_rw_emit(rewriter, source.data + i, count)) return false;
            i += count;
        } else if (source.data[i] == '\n') {
            if (!noc_rw_emit(rewriter, source.data + i, 1)) return false;
            i += 1;
        } else {
            i += 1;
        }
    }
    return true;
}

NOCDEF bool noc_rw_emit_line_directive(Noc_Rewriter *rewriter,
                                       Noc_Location location)
{
    const char *path;
    if (!rewriter || location.line == 0) {
        if (rewriter) noc_rw_error(rewriter, "cannot emit a #line directive for line 0");
        return false;
    }
    path = location.path ? location.path : rewriter->path;
    if (!path) {
        noc_rw_error(rewriter, "cannot emit a #line directive without a source path");
        return false;
    }
    if (rewriter->output->count > 0 &&
        rewriter->output->items[rewriter->output->count - 1] != '\n' &&
        rewriter->output->items[rewriter->output->count - 1] != '\r' &&
        !noc_rw_emit_cstr(rewriter, "\n")) {
        return false;
    }
    if (!noc__emit_line_directive_at(rewriter->output, path, location.line)) {
        noc_rw_error(rewriter, "out of memory while emitting a #line directive");
        return false;
    }
    return true;
}

NOCDEF bool noc_rw_emit_transformed(Noc_Rewriter *rewriter, Noc_Slice source)
{
    Noc_Transform_Result nested = {0};
    size_t i;
    bool ok = false;
    if ((source.count > 0 && !source.data) || rewriter->expansion_depth >= 64) {
        noc_rw_error(rewriter,
                     rewriter->expansion_depth >= 64
                         ? "nested expansion limit reached while expanding rule '%s'"
                         : "invalid nested source while expanding rule '%s'",
                     rewriter->rule->name);
        return false;
    }
    if (!noc__transform_source(rewriter->context,
                               rewriter->path,
                               source.data ? source.data : "",
                               source.count,
                               &nested,
                               rewriter->expansion_depth + 1,
                               false,
                               false)) {
        rewriter->failed = true;
        goto done;
    }
    for (i = 0; i < nested.dependency_count; ++i) {
        if (!noc__string_list_append_unique(rewriter->dependencies,
                                            nested.dependencies[i])) {
            noc_rw_error(rewriter,
                         "could not merge nested dependencies while expanding rule '%s'",
                         rewriter->rule->name);
            goto done;
        }
    }
    if (!noc_rw_emit(rewriter, nested.output, nested.output_count)) goto done;
    ok = true;

done:
    noc_transform_result_free(&nested);
    return ok;
}

NOCDEF bool noc_rw_emitf(Noc_Rewriter *rewriter, const char *format, ...)
{
    bool result;
    va_list arguments;
    va_start(arguments, format);
    result = noc__buffer_appendfv(rewriter->output, format, arguments);
    va_end(arguments);
    if (!result) noc_rw_error(rewriter, "out of memory while expanding rule '%s'", rewriter->rule->name);
    return result;
}

NOCDEF bool noc_rw_emit_c_string(Noc_Rewriter *rewriter,
                                 const void *data,
                                 size_t count)
{
    if (!rewriter || (count > 0 && !data) ||
        !noc__buffer_append_c_string(rewriter->output, data, count)) {
        if (rewriter) {
            noc_rw_error(rewriter,
                         "could not emit a C string while expanding rule '%s'",
                         rewriter->rule->name);
        }
        return false;
    }
    return true;
}

NOCDEF void noc_rw_error_at(Noc_Rewriter *rewriter,
                            Noc_Location location,
                            const char *format,
                            ...)
{
    va_list arguments;
    rewriter->failed = true;
    va_start(arguments, format);
    noc__reportv(rewriter->context, NOC_DIAGNOSTIC_ERROR, location, format, arguments);
    va_end(arguments);
}

NOCDEF void noc_rw_error(Noc_Rewriter *rewriter, const char *format, ...)
{
    va_list arguments;
    rewriter->failed = true;
    va_start(arguments, format);
    noc__reportv(rewriter->context,
                 NOC_DIAGNOSTIC_ERROR,
                 rewriter->trigger_location,
                 format,
                 arguments);
    va_end(arguments);
}

static int noc__hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

NOCDEF bool noc_decode_string_token(Noc_Token token, Noc_Buffer *decoded)
{
    size_t i;
    if (token.kind != NOC_TOKEN_STRING || token.text.count < 2 ||
        token.text.data[0] != '"' || token.text.data[token.text.count - 1] != '"') {
        return false;
    }
    i = 1;
    while (i + 1 < token.text.count) {
        unsigned char value;
        char c = token.text.data[i++];
        if (c != '\\') {
            if (!noc_buffer_append(decoded, &c, 1)) return false;
            continue;
        }
        if (i + 1 > token.text.count) return false;
        c = token.text.data[i++];
        switch (c) {
        case '\'': value = '\''; break;
        case '"': value = '"'; break;
        case '?': value = '?'; break;
        case '\\': value = '\\'; break;
        case 'a': value = '\a'; break;
        case 'b': value = '\b'; break;
        case 'f': value = '\f'; break;
        case 'n': value = '\n'; break;
        case 'r': value = '\r'; break;
        case 't': value = '\t'; break;
        case 'v': value = '\v'; break;
        case 'x': {
            int digit;
            unsigned int number = 0;
            size_t digits = 0;
            while (i + 1 < token.text.count &&
                   (digit = noc__hex_digit(token.text.data[i])) >= 0) {
                number = number * 16u + (unsigned int)digit;
                i += 1;
                digits += 1;
            }
            if (digits == 0 || number > 255u) return false;
            value = (unsigned char)number;
            break;
        }
        case '\n': continue;
        case '\r':
            if (i + 1 < token.text.count && token.text.data[i] == '\n') i += 1;
            continue;
        default:
            if (c >= '0' && c <= '7') {
                unsigned int number = (unsigned int)(c - '0');
                size_t digits = 1;
                while (digits < 3 && i + 1 < token.text.count &&
                       token.text.data[i] >= '0' && token.text.data[i] <= '7') {
                    number = number * 8u + (unsigned int)(token.text.data[i] - '0');
                    i += 1;
                    digits += 1;
                }
                if (number > 255u) return false;
                value = (unsigned char)number;
            } else {
                return false;
            }
            break;
        }
        if (!noc_buffer_append(decoded, &value, 1)) return false;
    }
    return true;
}

static bool noc__tokens_append(Noc__Tokens *tokens, Noc_Token token)
{
    Noc_Token *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (tokens->count > tokens->capacity) return false;
    if (tokens->count == tokens->capacity) {
        if (tokens->capacity >= maximum) return false;
        if (tokens->capacity == 0) {
            capacity = maximum < 256 ? maximum : 256;
        } else if (tokens->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = tokens->capacity * 2;
        }
        if (capacity <= tokens->capacity) return false;
        items = (Noc_Token *)realloc(tokens->items, capacity * sizeof(*items));
        if (!items) return false;
        tokens->items = items;
        tokens->capacity = capacity;
    }
    tokens->items[tokens->count++] = token;
    return true;
}

static bool noc__emit_line_directive_at(Noc_Buffer *output,
                                        const char *path,
                                        size_t line)
{
    const unsigned char *cursor = (const unsigned char *)path;
    if (!noc_buffer_appendf(output, "#line %zu \"", line)) return false;
    while (*cursor) {
        if (*cursor == '\\' || *cursor == '"' || *cursor == '?') {
            if (!noc_buffer_append(output, "\\", 1)) return false;
        }
        if (*cursor == '\n') {
            if (!noc_buffer_append_cstr(output, "\\n")) return false;
        } else if (*cursor == '\r') {
            if (!noc_buffer_append_cstr(output, "\\r")) return false;
        } else if (*cursor == '\t') {
            if (!noc_buffer_append_cstr(output, "\\t")) return false;
        } else if (*cursor >= 32 && *cursor <= 126) {
            if (!noc_buffer_append(output, cursor, 1)) return false;
        } else if (!noc_buffer_appendf(output, "\\%03o", (unsigned int)*cursor)) {
            return false;
        }
        cursor += 1;
    }
    return noc_buffer_append_cstr(output, "\"\n");
}

static bool noc__reject_trigraphs(Noc_Context *context,
                                  const char *path,
                                  const char *source,
                                  size_t source_count)
{
    size_t i;
    size_t line = 1;
    size_t column = 1;
    for (i = 0; i < source_count; ++i) {
        if (i + 2 < source_count && source[i] == '?' && source[i + 1] == '?' &&
            strchr("=/'()!<>-", source[i + 2]) != NULL) {
            Noc_Location location;
            location.path = path;
            location.offset = i;
            location.line = line;
            location.column = column;
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "C trigraphs are not supported by noc.h");
            return false;
        }
        if (source[i] == '\r') {
            if (i + 1 < source_count && source[i + 1] == '\n') i += 1;
            line += 1;
            column = 1;
        } else if (source[i] == '\n') {
            line += 1;
            column = 1;
        } else {
            column += 1;
        }
    }
    return true;
}

NOCDEF void noc_token_stream_free(Noc_Token_Stream *stream)
{
    size_t generation = stream->generation;
    free(stream->items);
    free(stream->source);
    free(stream->path);
    memset(stream, 0, sizeof(*stream));
    stream->generation = generation;
}

NOCDEF bool noc_token_stream_is_valid(const Noc_Token_Stream *stream)
{
    return stream && stream->items && stream->source && stream->path &&
           stream->generation != 0 &&
           stream->count > 0 && stream->count <= stream->capacity &&
           stream->items[stream->count - 1].kind == NOC_TOKEN_EOF;
}

static bool noc__tokenize(Noc_Context *context,
                          const char *path,
                          const char *source,
                          size_t source_count,
                          bool recover_incomplete_directive,
                          Noc_Token_Stream *stream)
{
    const char *display_path = path ? path : "<memory>";
    size_t path_count;
    Noc_Location no_location = {0};
    Noc_Token_Stream parsed = {0};
    Noc_Lexer lexer;
    Noc_Token token;
    bool ok = true;

    if (stream->generation == SIZE_MAX) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "token stream generation is exhausted");
        return false;
    }
    if (source_count == SIZE_MAX) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "source is too large to tokenize");
        return false;
    }
    if (!noc__reject_trigraphs(context, display_path, source, source_count)) return false;
    path_count = strlen(display_path);
    parsed.source = (char *)malloc(source_count + 1);
    parsed.path = (char *)malloc(path_count + 1);
    if (!parsed.source || !parsed.path) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while copying source for '%s'",
                    display_path);
        noc_token_stream_free(&parsed);
        return false;
    }
    if (source_count > 0) memcpy(parsed.source, source, source_count);
    parsed.source[source_count] = '\0';
    parsed.source_count = source_count;
    memcpy(parsed.path, display_path, path_count + 1);

    noc_lexer_init(&lexer, parsed.path, parsed.source, parsed.source_count);
    do {
        token = noc_lexer_next(&lexer);
        if (recover_incomplete_directive && token.kind == NOC_TOKEN_INVALID &&
            token.text.count == 0 && lexer.cursor == lexer.source_count &&
            parsed.count > 0 &&
            parsed.items[parsed.count - 1].kind == NOC_TOKEN_PREPROCESSOR) {
            continue;
        }
        if (!noc__tokens_append(&parsed, token)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while tokenizing '%s'",
                        display_path);
            ok = false;
            break;
        }
        if (token.kind == NOC_TOKEN_INVALID) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "unterminated or invalid token '%.*s%s'",
                        (int)(token.text.count < 80 ? token.text.count : 80),
                        token.text.data,
                        token.text.count > 80 ? "..." : "");
            ok = false;
        }
    } while (token.kind != NOC_TOKEN_EOF);
    if (!ok) {
        noc_token_stream_free(&parsed);
        return false;
    }
    parsed.generation = stream->generation + 1;
    noc_token_stream_free(stream);
    *stream = parsed;
    return true;
}

NOCDEF bool noc_tokenize(Noc_Context *context,
                         const char *path,
                         const char *source,
                         size_t source_count,
                         Noc_Token_Stream *stream)
{
    return noc__tokenize(context,
                         path,
                         source,
                         source_count,
                         false,
                         stream);
}

NOCDEF Noc_Slice noc_token_stream_source(const Noc_Token_Stream *stream)
{
    Noc_Slice source = {0};
    if (!noc_token_stream_is_valid(stream)) return source;
    source.data = stream->source;
    source.count = stream->source_count;
    return source;
}

typedef enum {
    NOC__PREPROCESSOR_OTHER = 0,
    NOC__PREPROCESSOR_IF,
    NOC__PREPROCESSOR_IFDEF,
    NOC__PREPROCESSOR_IFNDEF,
    NOC__PREPROCESSOR_ELIF,
    NOC__PREPROCESSOR_ELSE,
    NOC__PREPROCESSOR_ENDIF,
} Noc__Preprocessor_Directive_Kind;

typedef struct {
    Noc__Preprocessor_Directive_Kind kind;
    Noc_Preprocessor_Activity condition;
} Noc__Preprocessor_Directive;

typedef struct {
    Noc_Preprocessor_Activity parent;
    Noc_Preprocessor_Activity prior_taken;
    Noc_Location opening_location;
    bool saw_else;
} Noc__Preprocessor_Frame;

static Noc_Token noc__preprocessor_next_significant(Noc_Lexer *lexer)
{
    Noc_Token token;
    do {
        token = noc_lexer_next(lexer);
    } while (noc_token_is_trivia(token));
    return token;
}

static bool noc__preprocessor_parse_directive(Noc_Token token,
                                              Noc__Preprocessor_Directive *directive)
{
    Noc_Buffer logical = {0};
    Noc_Lexer lexer;
    Noc_Token marker;
    Noc_Token keyword;
    bool ok = false;
    directive->kind = NOC__PREPROCESSOR_OTHER;
    directive->condition = NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
    if (token.kind != NOC_TOKEN_PREPROCESSOR) return true;
    if (!noc_token_logical_text(token, &logical)) return false;
    noc_lexer_init(&lexer, token.location.path, logical.items, logical.count);
    lexer.beginning_of_line = false;
    marker = noc__preprocessor_next_significant(&lexer);
    if (!noc_token_is_punct(marker, "#") && !noc_token_is_punct(marker, "%:")) {
        ok = true;
        goto done;
    }
    keyword = noc__preprocessor_next_significant(&lexer);
    if (noc_token_is_identifier(keyword, "if")) {
        directive->kind = NOC__PREPROCESSOR_IF;
    } else if (noc_token_is_identifier(keyword, "ifdef")) {
        directive->kind = NOC__PREPROCESSOR_IFDEF;
    } else if (noc_token_is_identifier(keyword, "ifndef")) {
        directive->kind = NOC__PREPROCESSOR_IFNDEF;
    } else if (noc_token_is_identifier(keyword, "elif")) {
        directive->kind = NOC__PREPROCESSOR_ELIF;
    } else if (noc_token_is_identifier(keyword, "elifdef") ||
               noc_token_is_identifier(keyword, "elifndef")) {
        directive->kind = NOC__PREPROCESSOR_ELIF;
    } else if (noc_token_is_identifier(keyword, "else")) {
        directive->kind = NOC__PREPROCESSOR_ELSE;
    } else if (noc_token_is_identifier(keyword, "endif")) {
        directive->kind = NOC__PREPROCESSOR_ENDIF;
    }
    if (directive->kind == NOC__PREPROCESSOR_IF ||
        directive->kind == NOC__PREPROCESSOR_ELIF) {
        Noc_Token expression = noc__preprocessor_next_significant(&lexer);
        Noc_Token trailing = noc__preprocessor_next_significant(&lexer);
        bool trailing_multiline_comment =
            trailing.kind == NOC_TOKEN_INVALID &&
            noc__logical_pair(trailing.text.data,
                              trailing.text.count,
                              0,
                              '/',
                              '*',
                              NULL) &&
            noc__contains_newline(trailing.text.data, trailing.text.count);
        if (expression.kind == NOC_TOKEN_NUMBER &&
            (trailing.kind == NOC_TOKEN_EOF || trailing_multiline_comment)) {
            if (noc_slice_equal_cstr(expression.text, "0")) {
                directive->condition = NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
            } else if (noc_slice_equal_cstr(expression.text, "1")) {
                directive->condition = NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
            }
        }
    }
    ok = true;

done:
    noc_buffer_free(&logical);
    return ok;
}

static Noc_Preprocessor_Activity noc__preprocessor_and(Noc_Preprocessor_Activity left,
                                                       Noc_Preprocessor_Activity right)
{
    if (left == NOC_PREPROCESSOR_ACTIVITY_INACTIVE ||
        right == NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    }
    if (left == NOC_PREPROCESSOR_ACTIVITY_ACTIVE &&
        right == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

static Noc_Preprocessor_Activity noc__preprocessor_or(Noc_Preprocessor_Activity left,
                                                      Noc_Preprocessor_Activity right)
{
    if (left == NOC_PREPROCESSOR_ACTIVITY_ACTIVE ||
        right == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    }
    if (left == NOC_PREPROCESSOR_ACTIVITY_INACTIVE &&
        right == NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

static Noc_Preprocessor_Activity noc__preprocessor_not(Noc_Preprocessor_Activity activity)
{
    if (activity == NOC_PREPROCESSOR_ACTIVITY_ACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_INACTIVE;
    }
    if (activity == NOC_PREPROCESSOR_ACTIVITY_INACTIVE) {
        return NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    }
    return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
}

NOCDEF void noc_preprocessor_map_free(Noc_Preprocessor_Map *map)
{
    free(map->items);
    memset(map, 0, sizeof(*map));
}

NOCDEF bool noc_preprocessor_map_is_valid(const Noc_Preprocessor_Map *map)
{
    return map && noc_token_stream_is_valid(map->stream) &&
           map->stream_generation == map->stream->generation && map->items &&
           map->count == map->stream->count;
}

NOCDEF Noc_Preprocessor_Activity noc_preprocessor_activity_at(
    const Noc_Preprocessor_Map *map,
    size_t token_index)
{
    if (!noc_preprocessor_map_is_valid(map) || token_index >= map->count) {
        return NOC_PREPROCESSOR_ACTIVITY_UNKNOWN;
    }
    return map->items[token_index];
}

NOCDEF bool noc_preprocessor_map_build(Noc_Context *context,
                                       const Noc_Token_Stream *stream,
                                       Noc_Preprocessor_Map *map)
{
    Noc_Preprocessor_Map parsed = {0};
    Noc__Preprocessor_Frame *frames = NULL;
    Noc_Preprocessor_Activity current = NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
    Noc_Location no_location = {0};
    size_t frame_count = 0;
    size_t frame_capacity = 0;
    size_t i;
    if (!map || !noc_token_stream_is_valid(stream)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "cannot analyze preprocessor activity from an invalid token stream");
        return false;
    }
    if (stream->count > SIZE_MAX / sizeof(*parsed.items)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "token stream is too large for preprocessor activity analysis");
        return false;
    }
    parsed.items = (Noc_Preprocessor_Activity *)malloc(stream->count *
                                                       sizeof(*parsed.items));
    if (!parsed.items) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while starting preprocessor activity analysis");
        goto failed;
    }
    parsed.stream = stream;
    parsed.stream_generation = stream->generation;
    parsed.count = stream->count;
    for (i = 0; i < stream->count; ++i) {
        Noc_Token token = stream->items[i];
        Noc__Preprocessor_Directive directive;
        parsed.items[i] = current;
        if (token.kind != NOC_TOKEN_PREPROCESSOR) continue;
        if (!noc__preprocessor_parse_directive(token, &directive)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "out of memory while parsing preprocessor directive");
            goto failed;
        }
        if (directive.kind == NOC__PREPROCESSOR_IF ||
            directive.kind == NOC__PREPROCESSOR_IFDEF ||
            directive.kind == NOC__PREPROCESSOR_IFNDEF) {
            Noc__Preprocessor_Frame *frame;
            if (frame_count == frame_capacity) {
                Noc__Preprocessor_Frame *grown;
                size_t capacity = frame_capacity ? frame_capacity * 2 : 8;
                if (capacity < frame_capacity || capacity > SIZE_MAX / sizeof(*frames)) {
                    noc__report(context,
                                NOC_DIAGNOSTIC_ERROR,
                                token.location,
                                "preprocessor conditional nesting is too deep");
                    goto failed;
                }
                grown = (Noc__Preprocessor_Frame *)realloc(frames,
                                                           capacity * sizeof(*frames));
                if (!grown) {
                    noc__report(context,
                                NOC_DIAGNOSTIC_ERROR,
                                token.location,
                                "out of memory while nesting preprocessor conditionals");
                    goto failed;
                }
                frames = grown;
                frame_capacity = capacity;
            }
            frame = &frames[frame_count++];
            frame->parent = current;
            frame->prior_taken = directive.condition;
            frame->opening_location = token.location;
            frame->saw_else = false;
            current = noc__preprocessor_and(frame->parent, directive.condition);
        } else if (directive.kind == NOC__PREPROCESSOR_ELIF) {
            Noc__Preprocessor_Frame *frame;
            Noc_Preprocessor_Activity available;
            if (frame_count == 0) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "#elif has no matching conditional directive");
                goto failed;
            }
            frame = &frames[frame_count - 1];
            if (frame->saw_else) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "#elif cannot follow #else in the same conditional group");
                goto failed;
            }
            available = noc__preprocessor_not(frame->prior_taken);
            current = noc__preprocessor_and(
                frame->parent,
                noc__preprocessor_and(available, directive.condition));
            frame->prior_taken = noc__preprocessor_or(frame->prior_taken,
                                                      directive.condition);
        } else if (directive.kind == NOC__PREPROCESSOR_ELSE) {
            Noc__Preprocessor_Frame *frame;
            if (frame_count == 0) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "#else has no matching conditional directive");
                goto failed;
            }
            frame = &frames[frame_count - 1];
            if (frame->saw_else) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "conditional group contains more than one #else");
                goto failed;
            }
            current = noc__preprocessor_and(
                frame->parent,
                noc__preprocessor_not(frame->prior_taken));
            frame->prior_taken = NOC_PREPROCESSOR_ACTIVITY_ACTIVE;
            frame->saw_else = true;
        } else if (directive.kind == NOC__PREPROCESSOR_ENDIF) {
            if (frame_count == 0) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            token.location,
                            "#endif has no matching conditional directive");
                goto failed;
            }
            current = frames[frame_count - 1].parent;
            frame_count -= 1;
        }
    }
    if (frame_count != 0) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    frames[frame_count - 1].opening_location,
                    "conditional directive has no matching #endif");
        goto failed;
    }
    free(frames);
    noc_preprocessor_map_free(map);
    *map = parsed;
    return true;

failed:
    free(frames);
    noc_preprocessor_map_free(&parsed);
    return false;
}

NOCDEF bool noc_token_range_is_valid(const Noc_Token_Stream *stream,
                                     Noc_Token_Range range)
{
    return noc_token_stream_is_valid(stream) &&
           range.begin <= range.end && range.end <= stream->count;
}

NOCDEF Noc_Token_Range noc_token_range_trim_trivia(const Noc_Token_Stream *stream,
                                                    Noc_Token_Range range)
{
    Noc_Token_Range invalid = {NOC_TOKEN_INDEX_NONE, NOC_TOKEN_INDEX_NONE};
    if (!noc_token_range_is_valid(stream, range)) return invalid;
    while (range.begin < range.end && noc_token_is_trivia(stream->items[range.begin])) {
        range.begin += 1;
    }
    while (range.end > range.begin && noc_token_is_trivia(stream->items[range.end - 1])) {
        range.end -= 1;
    }
    return range;
}

NOCDEF Noc_Slice noc_token_range_source(const Noc_Token_Stream *stream,
                                        Noc_Token_Range range)
{
    Noc_Slice result = {0};
    size_t begin_offset;
    size_t end_offset;
    const Noc_Token *last;
    if (!noc_token_range_is_valid(stream, range)) return result;
    begin_offset = range.begin < stream->count
                       ? stream->items[range.begin].location.offset
                       : stream->source_count;
    if (range.begin == range.end) {
        result.data = stream->source + begin_offset;
        return result;
    }
    last = &stream->items[range.end - 1];
    end_offset = last->location.offset + last->text.count;
    if (begin_offset > end_offset || end_offset > stream->source_count) return result;
    result.data = stream->source + begin_offset;
    result.count = end_offset - begin_offset;
    return result;
}

NOCDEF Noc_Location noc_token_range_location(const Noc_Token_Stream *stream,
                                             Noc_Token_Range range)
{
    Noc_Location location = {0};
    if (!noc_token_range_is_valid(stream, range)) return location;
    if (range.begin < stream->count) return stream->items[range.begin].location;
    if (stream->count > 0) return stream->items[stream->count - 1].location;
    location.path = stream->path;
    location.offset = stream->source_count;
    return location;
}

NOCDEF void noc_token_cursor_init(Noc_Token_Cursor *cursor,
                                  const Noc_Token_Stream *stream)
{
    if (!noc_token_stream_is_valid(stream)) {
        memset(cursor, 0, sizeof(*cursor));
        return;
    }
    cursor->stream = stream;
    cursor->begin = 0;
    cursor->index = 0;
    cursor->end = stream ? stream->count : 0;
}

NOCDEF bool noc_token_cursor_init_range(Noc_Token_Cursor *cursor,
                                        const Noc_Token_Stream *stream,
                                        Noc_Token_Range range)
{
    if (!noc_token_range_is_valid(stream, range)) {
        memset(cursor, 0, sizeof(*cursor));
        return false;
    }
    cursor->stream = stream;
    cursor->begin = range.begin;
    cursor->index = range.begin;
    cursor->end = range.end;
    return true;
}

NOCDEF size_t noc_token_cursor_mark(const Noc_Token_Cursor *cursor)
{
    return cursor->index;
}

NOCDEF bool noc_token_cursor_rewind(Noc_Token_Cursor *cursor, size_t mark)
{
    if (!cursor->stream || mark < cursor->begin || mark > cursor->end) return false;
    cursor->index = mark;
    return true;
}

NOCDEF const Noc_Token *noc_token_cursor_peek_raw(const Noc_Token_Cursor *cursor,
                                                  size_t lookahead)
{
    if (!cursor->stream || cursor->index > cursor->end ||
        lookahead >= cursor->end - cursor->index) {
        return NULL;
    }
    return &cursor->stream->items[cursor->index + lookahead];
}

NOCDEF const Noc_Token *noc_token_cursor_peek(const Noc_Token_Cursor *cursor,
                                              size_t lookahead)
{
    size_t index;
    size_t found = 0;
    if (!cursor->stream) return NULL;
    index = cursor->index;
    while (index < cursor->end) {
        if (!noc_token_is_trivia(cursor->stream->items[index])) {
            if (found == lookahead) return &cursor->stream->items[index];
            found += 1;
        }
        index += 1;
    }
    return NULL;
}

NOCDEF bool noc_token_cursor_at_end(const Noc_Token_Cursor *cursor)
{
    const Noc_Token *token = noc_token_cursor_peek(cursor, 0);
    return !token || token->kind == NOC_TOKEN_EOF;
}

NOCDEF bool noc_token_cursor_take_raw(Noc_Token_Cursor *cursor, Noc_Token *token)
{
    const Noc_Token *next = noc_token_cursor_peek_raw(cursor, 0);
    if (!next) return false;
    if (token) *token = *next;
    cursor->index += 1;
    return true;
}

NOCDEF void noc_token_cursor_skip_trivia(Noc_Token_Cursor *cursor)
{
    if (!cursor->stream) return;
    while (cursor->index < cursor->end &&
           noc_token_is_trivia(cursor->stream->items[cursor->index])) {
        cursor->index += 1;
    }
}

NOCDEF bool noc_token_cursor_take(Noc_Token_Cursor *cursor, Noc_Token *token)
{
    noc_token_cursor_skip_trivia(cursor);
    return noc_token_cursor_take_raw(cursor, token);
}

NOCDEF bool noc_token_cursor_match_kind(Noc_Token_Cursor *cursor,
                                        Noc_Token_Kind kind,
                                        Noc_Token *token)
{
    Noc_Token_Cursor candidate = *cursor;
    Noc_Token matched;
    if (!noc_token_cursor_take(&candidate, &matched) || matched.kind != kind) return false;
    *cursor = candidate;
    if (token) *token = matched;
    return true;
}

NOCDEF bool noc_token_cursor_match_punct(Noc_Token_Cursor *cursor,
                                         const char *punctuator,
                                         Noc_Token *token)
{
    Noc_Token_Cursor candidate = *cursor;
    Noc_Token matched;
    if (!noc_token_cursor_take(&candidate, &matched) ||
        !noc_token_is_punct(matched, punctuator)) {
        return false;
    }
    *cursor = candidate;
    if (token) *token = matched;
    return true;
}

NOCDEF bool noc_token_cursor_match_identifier(Noc_Token_Cursor *cursor,
                                              const char *identifier,
                                              Noc_Token *token)
{
    Noc_Token_Cursor candidate = *cursor;
    Noc_Token matched;
    if (!noc_token_cursor_take(&candidate, &matched) ||
        matched.kind != NOC_TOKEN_IDENTIFIER ||
        (identifier && !noc_slice_equal_cstr(matched.text, identifier))) {
        return false;
    }
    *cursor = candidate;
    if (token) *token = matched;
    return true;
}

NOCDEF bool noc_token_cursor_take_balanced(Noc_Token_Cursor *cursor,
                                           const char *open,
                                           const char *close,
                                           Noc_Token_Range *whole,
                                           Noc_Token_Range *inside)
{
    Noc_Token_Cursor candidate = *cursor;
    Noc_Buffer delimiter_stack = {0};
    size_t open_index;
    Noc_Token token;
    char initial_close;
    bool ok = false;
    if (strcmp(open, "(") == 0 && strcmp(close, ")") == 0) initial_close = ')';
    else if (strcmp(open, "[") == 0 && strcmp(close, "]") == 0) initial_close = ']';
    else if (strcmp(open, "{") == 0 && strcmp(close, "}") == 0) initial_close = '}';
    else return false;
    noc_token_cursor_skip_trivia(&candidate);
    open_index = candidate.index;
    if (!noc_token_cursor_match_punct(&candidate, open, NULL)) return false;
    if (!noc_buffer_append(&delimiter_stack, &initial_close, 1)) return false;
    while (noc_token_cursor_take_raw(&candidate, &token)) {
        size_t token_index = candidate.index - 1;
        char expected = 0;
        bool closing = false;
        if (noc_token_is_punct(token, "(")) expected = ')';
        else if (noc_token_is_punct(token, "[")) expected = ']';
        else if (noc_token_is_punct(token, "{")) expected = '}';
        else if (noc_token_is_punct(token, ")") || noc_token_is_punct(token, "]") ||
                 noc_token_is_punct(token, "}")) {
            closing = true;
        }
        if (expected != 0) {
            if (!noc_buffer_append(&delimiter_stack, &expected, 1)) goto done;
        } else if (closing) {
            if (delimiter_stack.count == 0 ||
                delimiter_stack.items[delimiter_stack.count - 1] != token.text.data[0]) {
                goto done;
            }
            delimiter_stack.count -= 1;
            if (delimiter_stack.count == 0) {
                if (whole) {
                    whole->begin = open_index;
                    whole->end = candidate.index;
                }
                if (inside) {
                    inside->begin = open_index + 1;
                    inside->end = token_index;
                }
                *cursor = candidate;
                ok = true;
                goto done;
            }
        }
    }

done:
    noc_buffer_free(&delimiter_stack);
    return ok;
}

static bool noc__argument_list_append(Noc_Argument_List *arguments, Noc_Token_Range range)
{
    Noc_Token_Range *items;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    if (arguments->count > arguments->capacity) return false;
    if (arguments->count == arguments->capacity) {
        if (arguments->capacity >= maximum) return false;
        if (arguments->capacity == 0) {
            capacity = maximum < 4 ? maximum : 4;
        } else if (arguments->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = arguments->capacity * 2;
        }
        if (capacity <= arguments->capacity) return false;
        items = (Noc_Token_Range *)realloc(arguments->items, capacity * sizeof(*items));
        if (!items) return false;
        arguments->items = items;
        arguments->capacity = capacity;
    }
    arguments->items[arguments->count++] = range;
    return true;
}

NOCDEF void noc_argument_list_free(Noc_Argument_List *arguments)
{
    free(arguments->items);
    memset(arguments, 0, sizeof(*arguments));
}

NOCDEF bool noc_parse_arguments(const Noc_Token_Stream *stream,
                                Noc_Token_Range range,
                                Noc_Argument_List *arguments)
{
    Noc_Argument_List parsed = {0};
    Noc_Buffer delimiter_stack = {0};
    Noc_Token_Range trimmed;
    size_t argument_begin;
    size_t i;
    bool saw_comma = false;
    bool ok = false;
    if (!noc_token_range_is_valid(stream, range)) return false;
    if (range.end > range.begin && stream->items[range.end - 1].kind == NOC_TOKEN_EOF) {
        range.end -= 1;
    }
    trimmed = noc_token_range_trim_trivia(stream, range);
    if (trimmed.begin == trimmed.end) {
        noc_argument_list_free(arguments);
        return true;
    }
    argument_begin = range.begin;
    for (i = range.begin; i < range.end; ++i) {
        Noc_Token token = stream->items[i];
        char expected = 0;
        if (token.kind != NOC_TOKEN_PUNCTUATOR) continue;
        if (noc_token_is_punct(token, "(")) expected = ')';
        else if (noc_token_is_punct(token, "[")) expected = ']';
        else if (noc_token_is_punct(token, "{")) expected = '}';
        if (expected != 0) {
            if (!noc_buffer_append(&delimiter_stack, &expected, 1)) goto done;
            continue;
        }
        if (noc_token_is_punct(token, ")") || noc_token_is_punct(token, "]") ||
            noc_token_is_punct(token, "}")) {
            if (delimiter_stack.count == 0 ||
                delimiter_stack.items[delimiter_stack.count - 1] != token.text.data[0]) {
                goto done;
            }
            delimiter_stack.count -= 1;
            continue;
        }
        if (noc_token_is_punct(token, ",") && delimiter_stack.count == 0) {
            Noc_Token_Range argument = {argument_begin, i};
            argument = noc_token_range_trim_trivia(stream, argument);
            if (!noc__argument_list_append(&parsed, argument)) goto done;
            argument_begin = i + 1;
            saw_comma = true;
        }
    }
    if (delimiter_stack.count != 0) goto done;
    {
        Noc_Token_Range argument = {argument_begin, range.end};
        argument = noc_token_range_trim_trivia(stream, argument);
        if (argument.begin != argument.end || saw_comma) {
            if (!noc__argument_list_append(&parsed, argument)) goto done;
        }
    }
    noc_argument_list_free(arguments);
    *arguments = parsed;
    memset(&parsed, 0, sizeof(parsed));
    ok = true;

done:
    noc_buffer_free(&delimiter_stack);
    noc_argument_list_free(&parsed);
    return ok;
}

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

static bool noc__edit_range_is_valid(const Noc_Token_Stream *stream,
                                     Noc_Token_Range range)
{
    return noc_token_range_is_valid(stream, range) && stream->count > 0 &&
           range.end <= stream->count - 1;
}

static bool noc__edit_ranges_conflict(Noc_Token_Range left,
                                      Noc_Token_Range right)
{
    bool left_empty = left.begin == left.end;
    bool right_empty = right.begin == right.end;
    if (left_empty && right_empty) return left.begin == right.begin;
    if (left_empty) return left.begin > right.begin && left.begin < right.end;
    if (right_empty) return right.begin > left.begin && right.begin < left.end;
    return left.begin < right.end && right.begin < left.end;
}

NOCDEF bool noc_edit_set_is_valid(const Noc_Edit_Set *edits,
                                  const Noc_Token_Stream *stream)
{
    size_t i;
    if (!edits || !noc_token_stream_is_valid(stream) || edits->count > edits->capacity ||
        (edits->count > 0 && !edits->items)) {
        return false;
    }
    if (edits->count == 0) return true;
    if (edits->stream != stream || edits->stream_generation != stream->generation) {
        return false;
    }
    for (i = 0; i < edits->count; ++i) {
        const Noc_Edit *edit = &edits->items[i];
        if (!noc__edit_range_is_valid(stream, edit->range) ||
            (edit->replacement_count > 0 && !edit->replacement)) {
            return false;
        }
        if (i > 0) {
            const Noc_Edit *previous = &edits->items[i - 1];
            if (previous->range.begin > edit->range.begin ||
                (previous->range.begin == edit->range.begin &&
                 previous->range.begin != previous->range.end &&
                 edit->range.begin == edit->range.end) ||
                noc__edit_ranges_conflict(previous->range, edit->range)) {
                return false;
            }
        }
    }
    return true;
}

NOCDEF bool noc_edit_set_add(Noc_Edit_Set *edits,
                             const Noc_Token_Stream *stream,
                             Noc_Token_Range range,
                             Noc_Slice replacement)
{
    Noc_Edit *items;
    char *replacement_copy = NULL;
    size_t position = 0;
    size_t capacity;
    size_t maximum = SIZE_MAX / sizeof(*items);
    size_t i;
    if (!edits || !noc__edit_range_is_valid(stream, range) ||
        (replacement.count > 0 && !replacement.data) || edits->count > edits->capacity ||
        (edits->count > 0 && !noc_edit_set_is_valid(edits, stream))) {
        return false;
    }
    for (i = 0; i < edits->count; ++i) {
        if (noc__edit_ranges_conflict(edits->items[i].range, range)) return false;
        if (edits->items[i].range.begin < range.begin ||
            (edits->items[i].range.begin == range.begin &&
             edits->items[i].range.begin == edits->items[i].range.end &&
             range.begin != range.end)) {
            position = i + 1;
        }
    }
    if (replacement.count > 0) {
        replacement_copy = (char *)malloc(replacement.count);
        if (!replacement_copy) return false;
        memcpy(replacement_copy, replacement.data, replacement.count);
    }
    if (edits->count == edits->capacity) {
        if (edits->capacity >= maximum) goto failed;
        if (edits->capacity == 0) {
            capacity = maximum < 8 ? maximum : 8;
        } else if (edits->capacity > maximum / 2) {
            capacity = maximum;
        } else {
            capacity = edits->capacity * 2;
        }
        if (capacity <= edits->capacity) goto failed;
        items = (Noc_Edit *)realloc(edits->items, capacity * sizeof(*items));
        if (!items) goto failed;
        edits->items = items;
        edits->capacity = capacity;
    }
    if (position < edits->count) {
        memmove(&edits->items[position + 1],
                &edits->items[position],
                (edits->count - position) * sizeof(*edits->items));
    }
    edits->items[position].range = range;
    edits->items[position].replacement = replacement_copy;
    edits->items[position].replacement_count = replacement.count;
    edits->count += 1;
    edits->stream = stream;
    edits->stream_generation = stream->generation;
    return true;

failed:
    free(replacement_copy);
    return false;
}

NOCDEF bool noc_edit_set_add_cstr(Noc_Edit_Set *edits,
                                  const Noc_Token_Stream *stream,
                                  Noc_Token_Range range,
                                  const char *replacement)
{
    Noc_Slice slice;
    if (!replacement) return false;
    slice.data = replacement;
    slice.count = strlen(replacement);
    return noc_edit_set_add(edits, stream, range, slice);
}

NOCDEF bool noc_edit_set_add_syntax(Noc_Edit_Set *edits,
                                    const Noc_Syntax_Tree *tree,
                                    size_t node,
                                    Noc_Slice replacement)
{
    const Noc_Syntax_Node *syntax = noc_syntax_node(tree, node);
    return syntax && noc_edit_set_add(edits, tree->stream, syntax->range, replacement);
}

NOCDEF bool noc_edit_set_apply(const Noc_Edit_Set *edits,
                               const Noc_Token_Stream *stream,
                               Noc_Buffer *output)
{
    Noc_Buffer parsed = {0};
    size_t source_offset = 0;
    size_t i;
    if (!output || !noc_edit_set_is_valid(edits, stream)) return false;
    for (i = 0; i < edits->count; ++i) {
        const Noc_Edit *edit = &edits->items[i];
        Noc_Slice source = noc_token_range_source(stream, edit->range);
        size_t begin_offset;
        size_t end_offset;
        if (!source.data) goto failed;
        begin_offset = (size_t)(source.data - stream->source);
        end_offset = begin_offset + source.count;
        if (begin_offset < source_offset || end_offset < begin_offset ||
            end_offset > stream->source_count ||
            !noc_buffer_append(&parsed,
                               stream->source + source_offset,
                               begin_offset - source_offset) ||
            !noc_buffer_append(&parsed,
                               edit->replacement,
                               edit->replacement_count)) {
            goto failed;
        }
        source_offset = end_offset;
    }
    if (!noc_buffer_append(&parsed,
                           stream->source + source_offset,
                           stream->source_count - source_offset) ||
        !noc_buffer_terminate(&parsed)) {
        goto failed;
    }
    noc_buffer_free(output);
    *output = parsed;
    return true;

failed:
    noc_buffer_free(&parsed);
    return false;
}

NOCDEF void noc_edit_set_free(Noc_Edit_Set *edits)
{
    size_t i;
    for (i = 0; i < edits->count; ++i) free(edits->items[i].replacement);
    free(edits->items);
    memset(edits, 0, sizeof(*edits));
}

static size_t noc__pattern_match(const char *pattern,
                                 const Noc_Token *tokens,
                                 size_t count,
                                 size_t start,
                                 size_t *significant_count)
{
    Noc_Lexer lexer;
    Noc_Token expected;
    size_t cursor = start;
    size_t matched = 0;
    noc_lexer_init(&lexer, "<rule-pattern>", pattern, strlen(pattern));
    for (;;) {
        do {
            expected = noc_lexer_next(&lexer);
        } while (noc_token_is_trivia(expected));
        if (expected.kind == NOC_TOKEN_EOF) break;
        if (matched) {
            while (cursor < count && noc_token_is_trivia(tokens[cursor])) cursor += 1;
        }
        if (cursor >= count || tokens[cursor].kind != expected.kind ||
            !noc__slices_logically_equal(expected.text, tokens[cursor].text)) {
            return NOC_TOKEN_INDEX_NONE;
        }
        ++matched;
        ++cursor;
    }
    *significant_count = matched;
    return cursor;
}

static bool noc__transform_source(Noc_Context *context,
                                  const char *path,
                                  const char *source,
                                  size_t source_count,
                                  Noc_Transform_Result *result,
                                  size_t expansion_depth,
                                  bool emit_line_directives,
                                  bool analyze_preprocessor_activity)
{
    Noc_Lexer lexer;
    Noc__Tokens tokens = {0};
    Noc_Preprocessor_Map preprocessor = {0};
    Noc_Buffer output = {0};
    Noc__String_List dependencies = {0};
    Noc_Token token;
    size_t index = 0;
    size_t errors_before = context->error_count;
    bool ok = true;
    Noc_Location no_location = {0};

    memset(result, 0, sizeof(*result));
    context->active_transforms += 1;
    if (!noc__reject_trigraphs(context, path, source, source_count)) {
        ok = false;
        goto done;
    }
    noc_lexer_init(&lexer, path, source, source_count);
    do {
        token = noc_lexer_next(&lexer);
        if (!noc__tokens_append(&tokens, token)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while tokenizing '%s'",
                        path);
            ok = false;
            goto done;
        }
        if (token.kind == NOC_TOKEN_INVALID) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "unterminated or invalid token '%.*s%s'",
                        (int)(token.text.count < 80 ? token.text.count : 80),
                        token.text.data,
                        token.text.count > 80 ? "..." : "");
            ok = false;
        }
    } while (token.kind != NOC_TOKEN_EOF);
    if (!ok) goto done;
    tokens.source = (char *)source;
    tokens.source_count = source_count;
    tokens.path = (char *)(path ? path : "<memory>");
    tokens.generation = 1;

    if (analyze_preprocessor_activity &&
        !noc_preprocessor_map_build(context, &tokens, &preprocessor)) {
        ok = false;
        goto done;
    }

    if (emit_line_directives && !noc__emit_line_directive_at(&output, path, 1)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while starting output for '%s'",
                    path);
        ok = false;
        goto done;
    }

    while (index < tokens.count && tokens.items[index].kind != NOC_TOKEN_EOF) {
        token = tokens.items[index];
        if (!noc_token_is_trivia(token) && token.kind != NOC_TOKEN_PREPROCESSOR &&
            (!analyze_preprocessor_activity ||
             noc_preprocessor_activity_at(&preprocessor, index) !=
                 NOC_PREPROCESSOR_ACTIVITY_INACTIVE)) {
            size_t selected = NOC_TOKEN_INDEX_NONE;
            size_t trigger_end = 0;
            size_t best_count = 0;
            size_t r;
            size_t legacy_name = index + 1;
            if (noc_token_is_punct(token, "@")) {
                while (legacy_name < tokens.count &&
                       noc_token_is_trivia(tokens.items[legacy_name])) {
                    legacy_name += 1;
                }
                if (legacy_name < tokens.count &&
                    tokens.items[legacy_name].kind == NOC_TOKEN_IDENTIFIER) {
                    r = noc__find_rule_token(context, tokens.items[legacy_name]);
                    if (r != NOC_TOKEN_INDEX_NONE) {
                        selected = r;
                        trigger_end = legacy_name + 1;
                        best_count = 2;
                    }
                }
            }
            for (r = 0; r < context->rules_count; ++r) {
                if (context->rule_patterns[r]) {
                    size_t significant_count = 0;
                    size_t end = noc__pattern_match(context->rule_patterns[r],
                                                    tokens.items,
                                                    tokens.count,
                                                    index,
                                                    &significant_count);
                    if (end != NOC_TOKEN_INDEX_NONE &&
                        significant_count > best_count) {
                        selected = r;
                        trigger_end = end;
                        best_count = significant_count;
                    }
                }
            }
            if (selected != NOC_TOKEN_INDEX_NONE) {
                const Noc_Rule *rule = &context->rules[selected];
                if (!context->rule_enabled[selected]) {
                    if (context->options.disabled_rule_is_error) {
                        noc__report(context,
                                    NOC_DIAGNOSTIC_ERROR,
                                    token.location,
                                    "rule '%s' is disabled",
                                    rule->name);
                        ok = false;
                        goto done;
                    }
                    while (index < trigger_end) {
                        if (!noc_buffer_append_slice(&output, tokens.items[index++].text)) {
                            noc__report(context,
                                        NOC_DIAGNOSTIC_ERROR,
                                        token.location,
                                        "out of memory while preserving disabled rule '%s'",
                                        rule->name);
                            ok = false;
                            goto done;
                        }
                    }
                    continue;
                } else {
                    Noc_Rewriter rewriter;
                    size_t expansion_errors = context->error_count;
                    bool expanded;
                    memset(&rewriter, 0, sizeof(rewriter));
                    rewriter.context = context;
                    rewriter.rule = rule;
                    rewriter.path = path;
                    rewriter.source = source;
                    rewriter.source_count = source_count;
                    rewriter.tokens = tokens.items;
                    rewriter.tokens_count = tokens.count;
                    rewriter.cursor = trigger_end;
                    rewriter.stream = &tokens;
                    rewriter.trigger_location = token.location;
                    rewriter.trigger_range.begin = index;
                    rewriter.trigger_range.end = trigger_end;
                    rewriter.output = &output;
                    rewriter.dependencies = &dependencies;
                    rewriter.expansion_depth = expansion_depth;
                    expanded = rule->expand(&rewriter, rule, rule->user_data);
                    noc_syntax_tree_free(&rewriter.syntax_tree);
                    if (!expanded && context->error_count == expansion_errors) {
                        noc_rw_error(&rewriter,
                                     "expansion callback for rule '%s' failed without reporting an error",
                                     rule->name);
                    }
                    if (!expanded || context->error_count != expansion_errors) {
                        rewriter.failed = true;
                    }
                    if (rewriter.failed) {
                        ok = false;
                        goto done;
                    }
                    index = rewriter.cursor;
                    continue;
                }
            } else if (noc_token_is_punct(token, "@") && legacy_name < tokens.count &&
                       tokens.items[legacy_name].kind == NOC_TOKEN_IDENTIFIER) {
                if (context->options.unknown_rule_is_error) {
                    noc__report(context,
                                NOC_DIAGNOSTIC_ERROR,
                                token.location,
                                "unknown dialect rule '@%.*s%s'",
                                (int)(tokens.items[legacy_name].text.count < 80
                                          ? tokens.items[legacy_name].text.count
                                          : 80),
                                tokens.items[legacy_name].text.data,
                                tokens.items[legacy_name].text.count > 80 ? "..." : "");
                    ok = false;
                    goto done;
                }
                while (index <= legacy_name) {
                    if (!noc_buffer_append_slice(&output, tokens.items[index++].text)) {
                        noc__report(context,
                                    NOC_DIAGNOSTIC_ERROR,
                                    token.location,
                                    "out of memory while preserving an unknown dialect rule");
                        ok = false;
                        goto done;
                    }
                }
                continue;
            }
        }
        if (!noc_buffer_append_slice(&output, token.text)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        token.location,
                        "out of memory while transforming '%s'",
                        path);
            ok = false;
            goto done;
        }
        index += 1;
    }

    if (!noc_buffer_terminate(&output)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while finishing output for '%s'",
                    path);
        ok = false;
        goto done;
    }
    if (context->error_count != errors_before) {
        ok = false;
        goto done;
    }
    result->output = output.items;
    result->output_count = output.count;
    result->dependencies = dependencies.items;
    result->dependency_count = dependencies.count;
    output.items = NULL;
    output.count = 0;
    output.capacity = 0;
    dependencies.items = NULL;
    dependencies.count = 0;
    dependencies.capacity = 0;

done:
    result->error_count = context->error_count - errors_before;
    noc_preprocessor_map_free(&preprocessor);
    free(tokens.items);
    noc_buffer_free(&output);
    noc__string_list_free(&dependencies);
    context->active_transforms -= 1;
    return ok && result->error_count == 0;
}

NOCDEF bool noc_transform_source(Noc_Context *context,
                                 const char *path,
                                 const char *source,
                                 size_t source_count,
                                 Noc_Transform_Result *result)
{
    return noc__transform_source(context,
                                 path,
                                 source,
                                 source_count,
                                 result,
                                 0,
                                 context->options.emit_line_directives,
                                 context->options.skip_inactive_preprocessor_branches);
}

NOCDEF void noc_transform_result_free(Noc_Transform_Result *result)
{
    size_t i;
    free(result->output);
    for (i = 0; i < result->dependency_count; ++i) free(result->dependencies[i]);
    free(result->dependencies);
    memset(result, 0, sizeof(*result));
}

static bool noc__read_file(Noc_Context *context,
                           const char *path,
                           Noc_Buffer *contents,
                           Noc_Location location)
{
    FILE *file = fopen(path, "rb");
    char chunk[8192];
    size_t count;
    if (!file) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not open '%s': %s",
                    path,
                    strerror(errno));
        return false;
    }
    while ((count = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (!noc_buffer_append(contents, chunk, count)) {
            fclose(file);
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "out of memory while reading '%s'",
                        path);
            return false;
        }
    }
    if (ferror(file)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not read '%s': %s",
                    path,
                    strerror(errno));
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool noc__paths_refer_to_same_file(const char *left, const char *right)
{
    if (strcmp(left, right) == 0) return true;
#ifdef _WIN32
    {
        HANDLE left_handle = CreateFileA(left,
                                         0,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                         NULL,
                                         OPEN_EXISTING,
                                         FILE_FLAG_BACKUP_SEMANTICS,
                                         NULL);
        HANDLE right_handle = CreateFileA(right,
                                          0,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                          NULL,
                                          OPEN_EXISTING,
                                          FILE_FLAG_BACKUP_SEMANTICS,
                                          NULL);
        BY_HANDLE_FILE_INFORMATION left_info;
        BY_HANDLE_FILE_INFORMATION right_info;
        bool same = false;
        if (left_handle != INVALID_HANDLE_VALUE && right_handle != INVALID_HANDLE_VALUE &&
            GetFileInformationByHandle(left_handle, &left_info) &&
            GetFileInformationByHandle(right_handle, &right_info)) {
            same = left_info.dwVolumeSerialNumber == right_info.dwVolumeSerialNumber &&
                   left_info.nFileIndexHigh == right_info.nFileIndexHigh &&
                   left_info.nFileIndexLow == right_info.nFileIndexLow;
        }
        if (left_handle != INVALID_HANDLE_VALUE) CloseHandle(left_handle);
        if (right_handle != INVALID_HANDLE_VALUE) CloseHandle(right_handle);
        return same;
    }
#else
    {
        struct stat left_stat;
        struct stat right_stat;
        return stat(left, &left_stat) == 0 && stat(right, &right_stat) == 0 &&
               left_stat.st_dev == right_stat.st_dev &&
               left_stat.st_ino == right_stat.st_ino;
    }
#endif
}

static FILE *noc__open_unique_output(Noc_Context *context,
                                     const char *output_path,
                                     Noc_Buffer *temporary_path,
                                     Noc_Location location)
{
    unsigned long long salt = (unsigned long long)time(NULL) ^
                              (unsigned long long)clock() ^
                              (unsigned long long)(uintptr_t)temporary_path;
    size_t attempt;
    for (attempt = 0; attempt < 128; ++attempt) {
        FILE *file;
        temporary_path->count = 0;
        if (!noc_buffer_appendf(temporary_path,
                                "%s.noc-tmp-%llx-%zu",
                                output_path,
                                salt,
                                attempt) ||
            !noc_buffer_terminate(temporary_path)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "out of memory while creating temporary output path");
            return NULL;
        }
        errno = 0;
        file = fopen(temporary_path->items, "wbx");
        if (file) return file;
        if (errno != EEXIST) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "could not create '%s': %s",
                        temporary_path->items,
                        strerror(errno));
            return NULL;
        }
    }
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                location,
                "could not create a unique temporary file for '%s'",
                output_path);
    return NULL;
}

static bool noc__replace_output(const char *temporary_path, const char *output_path)
{
#ifdef _WIN32
    return MoveFileExA(temporary_path,
                       output_path,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary_path, output_path) == 0;
#endif
}

static bool noc__write_output_atomic(Noc_Context *context,
                                     const char *output_path,
                                     const void *data,
                                     size_t count,
                                     Noc_Location location)
{
    Noc_Buffer temporary_path = {0};
    FILE *file = NULL;
    bool ok = false;
    bool temporary_created = false;
    file = noc__open_unique_output(context, output_path, &temporary_path, location);
    if (!file) goto done;
    temporary_created = true;
    if (count > 0 && fwrite(data, 1, count, file) != count) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not write '%s': %s",
                    temporary_path.items,
                    strerror(errno));
        goto done;
    }
    if (fclose(file) != 0) {
        file = NULL;
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not close '%s': %s",
                    temporary_path.items,
                    strerror(errno));
        goto done;
    }
    file = NULL;
    if (!noc__replace_output(temporary_path.items, output_path)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "could not rename '%s' to '%s': %s",
                    temporary_path.items,
                    output_path,
                    strerror(errno));
        goto done;
    }
    temporary_created = false;
    ok = true;

done:
    if (file) fclose(file);
    if (temporary_created) (void)remove(temporary_path.items);
    noc_buffer_free(&temporary_path);
    return ok;
}

NOCDEF bool noc_transform_file_with_result(Noc_Context *context,
                                           const char *input_path,
                                           const char *output_path,
                                           Noc_Transform_Result *result)
{
    Noc_Buffer source = {0};
    Noc_Location no_location = {0};
    bool ok = false;
    if (!context || !input_path || !output_path || !result) return false;
    memset(result, 0, sizeof(*result));
    if (noc__paths_refer_to_same_file(input_path, output_path)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "input and output paths must differ: '%s'",
                    input_path);
        goto done;
    }
    if (!noc__read_file(context, input_path, &source, no_location)) goto done;
    if (!noc_transform_source(context,
                              input_path,
                              source.items ? source.items : "",
                              source.count,
                              result)) {
        goto done;
    }
    ok = noc__write_output_atomic(context,
                                  output_path,
                                  result->output,
                                  result->output_count,
                                  no_location);

done:
    if (!ok) noc_transform_result_free(result);
    noc_buffer_free(&source);
    return ok;
}

NOCDEF bool noc_transform_file(Noc_Context *context,
                               const char *input_path,
                               const char *output_path)
{
    Noc_Transform_Result result = {0};
    bool ok = noc_transform_file_with_result(context,
                                             input_path,
                                             output_path,
                                             &result);
    noc_transform_result_free(&result);
    return ok;
}

static bool noc__path_is_absolute(const char *path)
{
    if (!path[0]) return false;
#ifdef _WIN32
    if (path[0] == '/' || path[0] == '\\') return true;
    return isalpha((unsigned char)path[0]) && path[1] == ':' &&
           (path[2] == '/' || path[2] == '\\');
#else
    return path[0] == '/';
#endif
}

static bool noc__path_is_separator(char c)
{
    return c == '/' || c == '\\';
}

static bool noc__path_character_equal(char left, char right)
{
    if (noc__path_is_separator(left) && noc__path_is_separator(right)) return true;
#ifdef _WIN32
    return tolower((unsigned char)left) == tolower((unsigned char)right);
#else
    return left == right;
#endif
}

static bool noc__paths_equal_for_output(const char *left, const char *right)
{
    while (*left && *right && noc__path_character_equal(*left, *right)) {
        left += 1;
        right += 1;
    }
    return *left == '\0' && *right == '\0';
}

static bool noc__batch_platform_path_is_supported(const char *path)
{
#ifdef _WIN32
    const unsigned char *cursor = (const unsigned char *)path;
    if (noc__path_is_separator(path[0]) && noc__path_is_separator(path[1])) {
        return false;
    }
    if (isalpha((unsigned char)path[0]) && path[1] == ':' &&
        !noc__path_is_separator(path[2])) {
        return false;
    }
    while (*cursor) {
        if (*cursor >= 128) return false;
        cursor += 1;
    }
#else
    (void)path;
#endif
    return true;
}

static bool noc__batch_map_output(Noc_Context *context,
                                  const Noc_Batch_Options *options,
                                  const char *input_path,
                                  Noc_Buffer *output)
{
    const char *root = options->input_root;
    const char *suffix;
    const char *cursor;
    size_t root_count;
    size_t relative_components = 0;
    Noc_Buffer relative = {0};
    Noc_Location location = {0};
    bool ok = false;
    location.path = input_path;
    if (!noc__depfile_path_is_valid(root) ||
        !noc__depfile_path_is_valid(options->output_root) ||
        !noc__depfile_path_is_valid(input_path)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "batch paths must be non-empty and cannot contain newlines");
        goto done;
    }
    if (!noc__batch_platform_path_is_supported(root) ||
        !noc__batch_platform_path_is_supported(options->output_root) ||
        !noc__batch_platform_path_is_supported(input_path)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "batch path is not supported by the Windows ANSI path backend");
        goto done;
    }
    root_count = strlen(root);
    while (root_count > 1 && noc__path_is_separator(root[root_count - 1])) {
#ifdef _WIN32
        if (root_count == 3 && root[1] == ':') break;
#endif
        root_count -= 1;
    }
    if (root_count == 1 && root[0] == '.') {
        if (noc__path_is_absolute(input_path)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "absolute batch input is outside relative root '.'");
            goto done;
        }
        suffix = input_path;
        while (suffix[0] == '.' && noc__path_is_separator(suffix[1])) suffix += 2;
    } else {
        size_t i;
        for (i = 0; i < root_count; ++i) {
            if (!input_path[i] || !noc__path_character_equal(root[i], input_path[i])) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            location,
                            "batch input is outside input root '%s'",
                            root);
                goto done;
            }
        }
        suffix = input_path + root_count;
        if (root_count == 0 || !noc__path_is_separator(root[root_count - 1])) {
            if (!noc__path_is_separator(*suffix)) {
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            location,
                            "batch input is outside input root '%s'",
                            root);
                goto done;
            }
        }
        while (noc__path_is_separator(*suffix)) suffix += 1;
    }
    cursor = suffix;
    while (*cursor) {
        const char *component;
        size_t component_count;
        while (noc__path_is_separator(*cursor)) cursor += 1;
        if (!*cursor) break;
        component = cursor;
        while (*cursor && !noc__path_is_separator(*cursor)) cursor += 1;
        component_count = (size_t)(cursor - component);
        if ((component_count == 1 && component[0] == '.') ||
            (component_count == 2 && component[0] == '.' && component[1] == '.') ||
            memchr(component, ':', component_count) != NULL) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "batch input contains a non-portable or escaping path component");
            goto done;
        }
        if ((relative_components > 0 && !noc_buffer_append_cstr(&relative, "/")) ||
            !noc_buffer_append(&relative, component, component_count)) {
            goto memory_failed;
        }
        relative_components += 1;
    }
    if (relative.count < 2 || relative.items[relative.count - 2] != '.' ||
        (relative.items[relative.count - 1] != 'c' &&
         relative.items[relative.count - 1] != 'h')) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "batch inputs must use ordinary .c or .h suffixes");
        goto done;
    }
    {
        size_t output_root_count = strlen(options->output_root);
        size_t i;
        while (output_root_count > 1 &&
               noc__path_is_separator(options->output_root[output_root_count - 1])) {
#ifdef _WIN32
            if (output_root_count == 3 && options->output_root[1] == ':') break;
#endif
            output_root_count -= 1;
        }
        for (i = 0; i < output_root_count; ++i) {
            char c = noc__path_is_separator(options->output_root[i])
                         ? '/'
                         : options->output_root[i];
            if (!noc_buffer_append(output, &c, 1)) goto memory_failed;
        }
        if ((output_root_count > 0 && output->items[output->count - 1] != '/' &&
             !noc_buffer_append_cstr(output, "/")) ||
            !noc_buffer_append(output, relative.items, relative.count) ||
            !noc_buffer_terminate(output)) {
            goto memory_failed;
        }
    }
    ok = true;
    goto done;

memory_failed:
    noc__report(context,
                NOC_DIAGNOSTIC_ERROR,
                location,
                "out of memory while mapping batch output path");
done:
    noc_buffer_free(&relative);
    return ok;
}

static bool noc__directory_exists(const char *path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

static bool noc__make_directory(const char *path)
{
    if (noc__directory_exists(path)) return true;
#ifdef _WIN32
    return _mkdir(path) == 0 || (errno == EEXIST && noc__directory_exists(path));
#else
    return mkdir(path, 0777) == 0 || (errno == EEXIST && noc__directory_exists(path));
#endif
}

static bool noc__ensure_parent_directories(Noc_Context *context, const char *path)
{
    Noc_Buffer copy = {0};
    Noc_Location location = {0};
    size_t i;
    bool ok = false;
    location.path = path;
    if (!noc_buffer_append_cstr(&copy, path) || !noc_buffer_terminate(&copy)) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    location,
                    "out of memory while creating output directories");
        goto done;
    }
    for (i = 0; i < copy.count; ++i) {
        bool separator = copy.items[i] == '/';
#ifdef _WIN32
        separator = separator || copy.items[i] == '\\';
#endif
        if (!separator || i == 0) continue;
#ifdef _WIN32
        if (i == 2 && copy.items[1] == ':') continue;
#endif
        copy.items[i] = '\0';
        if (!noc__make_directory(copy.items)) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        location,
                        "could not create output directory '%s': %s",
                        copy.items,
                        strerror(errno));
            copy.items[i] = '/';
            goto done;
        }
        copy.items[i] = '/';
    }
    ok = true;

done:
    noc_buffer_free(&copy);
    return ok;
}

NOCDEF bool noc_transform_files(Noc_Context *context,
                                const Noc_Batch_Options *options,
                                const char *const *input_paths,
                                size_t input_count)
{
    Noc_Buffer *outputs = NULL;
    Noc_Location no_location = {0};
    size_t i;
    size_t j;
    bool ok = false;
    if (!context || !options || !input_paths || input_count == 0 ||
        input_count > SIZE_MAX / sizeof(*outputs)) {
        if (context) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "batch transformation requires at least one input");
        }
        return false;
    }
    outputs = (Noc_Buffer *)calloc(input_count, sizeof(*outputs));
    if (!outputs) {
        noc__report(context,
                    NOC_DIAGNOSTIC_ERROR,
                    no_location,
                    "out of memory while preparing batch transformation");
        return false;
    }
    for (i = 0; i < input_count; ++i) {
        if (!input_paths[i]) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "batch input path cannot be NULL");
            goto done;
        }
        if (!noc__batch_map_output(context, options, input_paths[i], &outputs[i])) {
            goto done;
        }
        for (j = 0; j < i; ++j) {
            if (noc__paths_equal_for_output(outputs[j].items, outputs[i].items)) {
                Noc_Location location = {0};
                location.path = input_paths[i];
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            location,
                            "batch inputs map to the same output '%s'",
                            outputs[i].items);
                goto done;
            }
        }
    }
    for (i = 0; i < input_count; ++i) {
        for (j = 0; j < input_count; ++j) {
            if (noc__paths_equal_for_output(outputs[i].items, input_paths[j]) ||
                noc__paths_refer_to_same_file(outputs[i].items, input_paths[j])) {
                Noc_Location location = {0};
                location.path = input_paths[j];
                noc__report(context,
                            NOC_DIAGNOSTIC_ERROR,
                            location,
                            "batch output '%s' aliases input '%s'",
                            outputs[i].items,
                            input_paths[j]);
                goto done;
            }
        }
    }
    for (i = 0; i < input_count; ++i) {
        if (!noc__ensure_parent_directories(context, outputs[i].items)) goto done;
        if (options->emit_depfiles) {
            Noc_Transform_Result result = {0};
            Noc_Buffer depfile = {0};
            Noc_Buffer depfile_path = {0};
            size_t errors_before = context->error_count;
            bool transformed = noc_transform_file_with_result(context,
                                                               input_paths[i],
                                                               outputs[i].items,
                                                               &result);
            if (transformed) {
                transformed = noc_buffer_append_cstr(&depfile_path, outputs[i].items) &&
                              noc_buffer_append_cstr(&depfile_path, ".d") &&
                              noc_buffer_terminate(&depfile_path) &&
                              noc_generate_depfile(context,
                                                   outputs[i].items,
                                                   input_paths[i],
                                                   &result,
                                                   &depfile) &&
                              noc__write_output_atomic(context,
                                                       depfile_path.items,
                                                       depfile.items,
                                                       depfile.count,
                                                       no_location);
                if (!transformed && context->error_count == errors_before) {
                    noc__report(context,
                                NOC_DIAGNOSTIC_ERROR,
                                no_location,
                                "could not generate batch depfile");
                }
            }
            noc_buffer_free(&depfile_path);
            noc_buffer_free(&depfile);
            noc_transform_result_free(&result);
            if (!transformed) goto done;
        } else if (!noc_transform_file(context, input_paths[i], outputs[i].items)) {
            goto done;
        }
    }
    ok = true;

done:
    for (i = 0; i < input_count; ++i) noc_buffer_free(&outputs[i]);
    free(outputs);
    return ok;
}

static bool noc__resolve_path(const char *source_path,
                              const char *referenced_path,
                              Noc_Buffer *resolved)
{
    const char *slash;
#ifdef _WIN32
    const char *backslash;
#endif
    const char *separator;
    if (noc__path_is_absolute(referenced_path)) {
        return noc_buffer_append_cstr(resolved, referenced_path) &&
               noc_buffer_terminate(resolved);
    }
    slash = strrchr(source_path, '/');
#ifdef _WIN32
    backslash = strrchr(source_path, '\\');
    separator = slash;
    if (!separator || (backslash && backslash > separator)) separator = backslash;
#else
    separator = slash;
#endif
    if (separator &&
        !noc_buffer_append(resolved, source_path, (size_t)(separator - source_path + 1))) {
        return false;
    }
    return noc_buffer_append_cstr(resolved, referenced_path) &&
           noc_buffer_terminate(resolved);
}

static bool noc__expand_embed(Noc_Rewriter *rewriter,
                              const Noc_Rule *rule,
                              void *user_data)
{
    const Noc_Token *path_token;
    Noc_Buffer decoded_path = {0};
    Noc_Buffer resolved_path = {0};
    Noc_Buffer contents = {0};
    Noc_Token consumed;
    bool ok = false;
    (void)rule;
    (void)user_data;
    if (!noc_rw_expect_punct(rewriter, "(")) goto done;
    noc_rw_skip_trivia(rewriter);
    path_token = noc_rw_peek_raw(rewriter, 0);
    if (!path_token || path_token->kind != NOC_TOKEN_STRING) {
        if (path_token) {
            noc_rw_error_at(rewriter,
                            path_token->location,
                            "@%s expects one ordinary string literal path",
                            rewriter->rule->name);
        } else {
            noc_rw_error(rewriter,
                         "@%s expects one ordinary string literal path",
                         rewriter->rule->name);
        }
        goto done;
    }
    if (!noc_decode_string_token(*path_token, &decoded_path) ||
        !noc_buffer_terminate(&decoded_path) ||
        memchr(decoded_path.items, '\0', decoded_path.count) != NULL) {
        noc_rw_error_at(rewriter,
                        path_token->location,
                        "@%s path is not a supported C string literal",
                        rewriter->rule->name);
        goto done;
    }
    (void)noc_rw_take_raw(rewriter, &consumed);
    if (!noc_rw_expect_punct(rewriter, ")")) goto done;
    if (!noc__resolve_path(rewriter->path, decoded_path.items, &resolved_path)) {
        noc_rw_error(rewriter, "out of memory while resolving embedded file path");
        goto done;
    }
    if (!noc__read_file(rewriter->context,
                        resolved_path.items,
                        &contents,
                        path_token->location)) {
        rewriter->failed = true;
        goto done;
    }
    if (!noc_rw_add_dependency(rewriter, resolved_path.items)) goto done;
    if (!noc_rw_emit_c_string(rewriter, contents.items, contents.count)) goto done;
    ok = true;

done:
    noc_buffer_free(&contents);
    noc_buffer_free(&resolved_path);
    noc_buffer_free(&decoded_path);
    return ok;
}

NOCDEF bool noc_register_embed_rule(Noc_Context *context, const char *name)
{
    Noc_Rule rule;
    rule.name = name;
    rule.scope = NOC_RULE_EXPRESSION;
    rule.syntax = "@<registered-name>(\"path\")";
    rule.description = "Embed a file as a C string literal; paths are relative to the source file.";
    rule.expand = noc__expand_embed;
    rule.user_data = NULL;
    return noc_register_rule(context, rule);
}

static void noc__print_usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "Usage:\n"
            "  %s [options] INPUT.[ch] -o OUTPUT.[ch]\n"
            "  %s --batch INPUT_ROOT OUTPUT_ROOT INPUT.[ch]...\n"
            "  %s --batch-depfiles INPUT_ROOT OUTPUT_ROOT INPUT.[ch]...\n"
            "  %s --describe\n"
            "  %s --ide-metadata OUTPUT.h\n"
            "  %s --command-signature OUTPUT -- COMMAND [ARG...]\n\n"
            "Options:\n"
            "  -o PATH               Write transformed C source/header to PATH\n"
            "  --depfile PATH        Write Make/Ninja dependencies to PATH\n"
            "  --dep-target PATH     Override the depfile target (default: -o PATH)\n"
            "  --describe            Describe all registered dialect rules\n"
            "  --ide-metadata PATH    Write a default IDE metadata header\n"
            "  --no-line-directives  Do not prepend a #line directive\n"
            "  -h, --help            Show this help\n",
            program,
            program,
            program,
            program,
            program,
            program);
}

NOCDEF int noc_run_cli(Noc_Context *context, int argc, char **argv)
{
    const char *input = NULL;
    const char *output = NULL;
    const char *depfile = NULL;
    const char *dep_target = NULL;
    int i;
    if (argc > 1 && strcmp(argv[1], "--command-signature") == 0) {
        const char **command_arguments;
        size_t command_argument_count;
        Noc_Buffer signature = {0};
        Noc_Location no_location = {0};
        bool ok;
        if (argc < 5 || strcmp(argv[3], "--") != 0) {
            noc__print_usage(stderr, argv[0]);
            return 2;
        }
        command_argument_count = (size_t)(argc - 4);
        command_arguments = (const char **)malloc(command_argument_count *
                                                   sizeof(*command_arguments));
        if (!command_arguments) {
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while preparing command signature arguments");
            return 1;
        }
        for (i = 0; i < (int)command_argument_count; ++i) {
            command_arguments[i] = argv[i + 4];
        }
        ok = noc_generate_command_signature(context,
                                            command_arguments,
                                            command_argument_count,
                                            &signature) &&
             noc__write_output_atomic(context,
                                      argv[2],
                                      signature.items,
                                      signature.count,
                                      no_location);
        free(command_arguments);
        noc_buffer_free(&signature);
        return ok ? 0 : 1;
    }
    if (argc > 1 &&
        (strcmp(argv[1], "--batch") == 0 ||
         strcmp(argv[1], "--batch-depfiles") == 0)) {
        Noc_Batch_Options options;
        const char **batch_inputs;
        size_t batch_input_count;
        bool ok;
        if (argc < 5) {
            noc__print_usage(stderr, argv[0]);
            return 2;
        }
        options.input_root = argv[2];
        options.output_root = argv[3];
        options.emit_depfiles = strcmp(argv[1], "--batch-depfiles") == 0;
        batch_input_count = (size_t)(argc - 4);
        if (batch_input_count > SIZE_MAX / sizeof(*batch_inputs)) return 1;
        batch_inputs = (const char **)malloc(batch_input_count * sizeof(*batch_inputs));
        if (!batch_inputs) {
            Noc_Location no_location = {0};
            noc__report(context,
                        NOC_DIAGNOSTIC_ERROR,
                        no_location,
                        "out of memory while preparing batch CLI inputs");
            return 1;
        }
        for (i = 0; i < (int)batch_input_count; ++i) batch_inputs[i] = argv[i + 4];
        ok = noc_transform_files(context, &options, batch_inputs, batch_input_count);
        free(batch_inputs);
        return ok ? 0 : 1;
    }
    for (i = 1; i < argc; ++i) {
        const char *argument = argv[i];
        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            noc__print_usage(stdout, argv[0]);
            return 0;
        }
        if (strcmp(argument, "--describe") == 0) {
            noc_describe(context, stdout);
            return 0;
        }
        if (strcmp(argument, "--ide-metadata") == 0) {
            Noc_Buffer metadata = {0};
            Noc_Location no_location = {0};
            bool ok;
            if (i + 1 >= argc) {
                fprintf(stderr, "noc: error: --ide-metadata requires a path\n");
                return 2;
            }
            if (i != 1 || argc != 3) {
                fprintf(stderr, "noc: error: --ide-metadata must be used alone\n");
                return 2;
            }
            ok = noc_generate_ide_metadata_header(context, NULL, &metadata) &&
                 noc__write_output_atomic(context,
                                          argv[i + 1],
                                          metadata.items,
                                          metadata.count,
                                          no_location);
            noc_buffer_free(&metadata);
            return ok ? 0 : 1;
        }
        if (strcmp(argument, "--no-line-directives") == 0) {
            context->options.emit_line_directives = false;
            continue;
        }
        if (strcmp(argument, "--depfile") == 0 ||
            strcmp(argument, "--dep-target") == 0) {
            const char **destination = strcmp(argument, "--depfile") == 0
                                           ? &depfile
                                           : &dep_target;
            if (i + 1 >= argc) {
                fprintf(stderr, "noc: error: %s requires a path\n", argument);
                return 2;
            }
            if (*destination) {
                fprintf(stderr, "noc: error: %s may be specified only once\n", argument);
                return 2;
            }
            *destination = argv[++i];
            continue;
        }
        if (strcmp(argument, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "noc: error: -o requires a path\n");
                return 2;
            }
            output = argv[++i];
            continue;
        }
        if (argument[0] == '-') {
            fprintf(stderr, "noc: error: unknown option '%s'\n", argument);
            return 2;
        }
        if (input) {
            fprintf(stderr, "noc: error: only one input is currently supported\n");
            return 2;
        }
        input = argument;
    }
    if (!input || !output) {
        noc__print_usage(stderr, argv[0]);
        return 2;
    }
    if (dep_target && !depfile) {
        fprintf(stderr, "noc: error: --dep-target requires --depfile\n");
        return 2;
    }
    if (depfile &&
        (noc__paths_refer_to_same_file(depfile, input) ||
         noc__paths_refer_to_same_file(depfile, output))) {
        fprintf(stderr, "noc: error: depfile path must differ from input and output\n");
        return 2;
    }
    if (depfile) {
        Noc_Transform_Result result = {0};
        Noc_Buffer generated = {0};
        Noc_Location no_location = {0};
        bool ok = noc_transform_file_with_result(context, input, output, &result);
        if (ok && noc__paths_refer_to_same_file(depfile, output)) {
            fprintf(stderr, "noc: error: depfile path resolves to the output path\n");
            ok = false;
        }
        if (ok) {
            ok = noc_generate_depfile(context,
                                      dep_target ? dep_target : output,
                                      input,
                                      &result,
                                      &generated) &&
                 noc__write_output_atomic(context,
                                          depfile,
                                          generated.items,
                                          generated.count,
                                          no_location);
        }
        noc_buffer_free(&generated);
        noc_transform_result_free(&result);
        return ok ? 0 : 1;
    }
    return noc_transform_file(context, input, output) ? 0 : 1;
}

#endif /* NOC_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION */

/*
   This is free and unencumbered software released into the public domain.

   Anyone is free to copy, modify, publish, use, compile, sell, or distribute
   this software, either in source code form or as a compiled binary, for any
   purpose, commercial or non-commercial, and by any means.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
*/
