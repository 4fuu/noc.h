#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC_INTERNAL_H_INCLUDED

/* An individual implementation module has not seen the declaration section
   that precedes this file in the amalgamation. */
#ifndef NOC_H_INCLUDED
#include "../include/noc/noc.h"
#endif

#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)

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

#define NOC__PRIVATE extern

typedef Noc_Token_Stream Noc__Tokens;

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} Noc__String_List;

typedef Noc_Token (*Noc__Macro_Token_Query)(const void *, size_t);

typedef bool (*Noc__Line_Map_Cancel_Fn)(void *);

typedef enum {
    NOC__LINE_MAP_OK = 0,
    NOC__LINE_MAP_CANCELLED,
    NOC__LINE_MAP_LIMIT_EXCEEDED,
    NOC__LINE_MAP_OUT_OF_MEMORY,
} Noc__Line_Map_Status;

/* Sequence-relative invocation syntax shared by the physical-source query and
   expanded logical-token rescan. It owns only the argument-range array. */
typedef struct {
    size_t open_token_index;
    size_t close_token_index;
    Noc_Token_Range tokens;
    Noc_Macro_Argument *arguments;
    size_t argument_count;
    size_t argument_capacity;
    size_t problem_token_index;
    Noc_Macro_Invocation_Status status;
} Noc__Macro_Invocation_Collection;

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

NOC__PRIVATE bool noc__buffer_appendfv(Noc_Buffer *, const char *, va_list);
NOC__PRIVATE bool noc__buffer_append_c_string(Noc_Buffer *, const void *, size_t);
NOC__PRIVATE bool noc__slices_logically_equal(Noc_Slice, Noc_Slice);
NOC__PRIVATE size_t noc__splice_length(const char *, size_t, size_t);
NOC__PRIVATE bool noc__contains_newline(const char *, size_t);
NOC__PRIVATE bool noc__logical_pair(const char *, size_t, size_t, char, char, size_t *);
NOC__PRIVATE bool noc__source_class_is_valid(Noc_Source_Class);
NOC__PRIVATE Noc__Line_Map_Status noc__line_starts_build(
    const char *, size_t, Noc__Line_Map_Cancel_Fn, void *, size_t **, size_t *);
NOC__PRIVATE Noc_Token noc__make_token(Noc_Lexer *, Noc_Token_Kind, size_t, size_t,
                                      Noc_Location);
NOC__PRIVATE void noc__report(Noc_Context *, Noc_Diagnostic_Severity,
                             Noc_Location, const char *, ...)
    NOC_PRINTF_FORMAT(4, 5);
NOC__PRIVATE void noc__reportv(Noc_Context *, Noc_Diagnostic_Severity,
                              Noc_Location, const char *, va_list);
NOC__PRIVATE bool noc__tokens_append(Noc__Tokens *, Noc_Token);
NOC__PRIVATE bool noc__reject_trigraphs(Noc_Context *, const char *, const char *, size_t);
NOC__PRIVATE bool noc__emit_line_directive_at(Noc_Buffer *, const char *, size_t);
NOC__PRIVATE size_t noc__find_rule_token(const Noc_Context *, Noc_Token);
NOC__PRIVATE bool noc__depfile_path_is_valid(const char *);
NOC__PRIVATE bool noc__macro_parse_directive(Noc_Preprocessor_Unit *,
                                             Noc_Preprocessor_Directive *, size_t);
NOC__PRIVATE void noc__macro_invocation_collection_free(
    Noc__Macro_Invocation_Collection *);
NOC__PRIVATE Noc_Macro_Invocation_Build_Status noc__macro_invocation_collect(
    const void *, Noc__Macro_Token_Query, size_t, size_t,
    Noc__Macro_Invocation_Collection *);
NOC__PRIVATE const Noc_Macro_Directive *noc__macro_environment_entry_directive(
    const Noc_Macro_Environment_Entry *);
NOC__PRIVATE Noc_Macro_Environment_Status noc__macro_environment_clone_prefix(
    const Noc_Macro_Environment *, size_t, size_t, Noc_Macro_Environment *);
NOC__PRIVATE bool noc__macro_expansion_limits_are_valid(
    Noc_Macro_Expansion_Limits);
NOC__PRIVATE bool noc__macro_expansion_options_are_valid(
    Noc_Macro_Expansion_Options);
NOC__PRIVATE uint32_t noc__macro_builtin_mask_from_options(
    Noc_Macro_Expansion_Options);
NOC__PRIVATE bool noc__macro_builtin_mask_contains(
    uint32_t, Noc_Macro_Builtin_Kind);
NOC__PRIVATE Noc_Macro_Expansion_Status noc__macro_expansion_build(
    const Noc_Macro_Environment *, size_t, const Noc_Preprocessor_Unit *,
    Noc_Token_Range, Noc_Macro_Expansion_Options, bool, Noc_Macro_Expansion *);
NOC__PRIVATE int noc__include_decode_header(Noc_Token,
                                           Noc_Include_Form *,
                                           Noc_Slice *);
NOC__PRIVATE Noc_Include_Resolve_Status noc__include_resolve_request(
    Noc_Include_Resolver,
    const Noc_Include_Request *,
    Noc_Document_Snapshot *);
NOC__PRIVATE void noc__string_list_free(Noc__String_List *);
NOC__PRIVATE bool noc__string_list_append_unique(Noc__String_List *, const char *);
/* Stable C syntax category normalization shared by physical parser lookahead
   and normalized-AST MISSING-node details. */
NOC__PRIVATE Noc_C_Ast_Expected_Kind noc__c_grammar_expected_kind(
    Noc_Slice, bool);
NOC__PRIVATE bool noc__transform_source(Noc_Context *, const char *, const char *, size_t,
                                       Noc_Transform_Result *, size_t, bool, bool);

#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */

#endif /* NOC_INTERNAL_H_INCLUDED */
