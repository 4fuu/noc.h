#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_MACRO_ENVIRONMENT_IMPLEMENTATION_INCLUDED
#define NOC_MACRO_ENVIRONMENT_IMPLEMENTATION_INCLUDED

NOCDEF const char *noc_macro_environment_status_name(
    Noc_Macro_Environment_Status status)
{
    switch (status) {
    case NOC_MACRO_ENVIRONMENT_OK: return "ok";
    case NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_MACRO_ENVIRONMENT_INVALID_DIRECTIVE: return "invalid-directive";
    case NOC_MACRO_ENVIRONMENT_DISABLED: return "disabled";
    case NOC_MACRO_ENVIRONMENT_STALE: return "stale";
    case NOC_MACRO_ENVIRONMENT_GENERATION_EXHAUSTED:
        return "generation-exhausted";
    case NOC_MACRO_ENVIRONMENT_OUT_OF_MEMORY: return "out-of-memory";
    }
    return "unknown";
}

NOC__PRIVATE const Noc_Macro_Directive *noc__macro_environment_entry_directive(
    const Noc_Macro_Environment_Entry *entry)
{
    if (!entry || !noc_preprocessor_unit_is_valid(entry->unit) ||
        entry->unit_stream_generation != entry->unit->stream.generation ||
        entry->macro_directive_index >= entry->unit->macro_directive_count) {
        return NULL;
    }
    return &entry->unit->macro_directives[entry->macro_directive_index];
}

static Noc_Slice noc__macro_environment_directive_name(
    const Noc_Preprocessor_Unit *unit,
    const Noc_Macro_Directive *directive)
{
    Noc_Slice empty = {0};
    if (!unit || !directive ||
        directive->name_token_index >= unit->preprocessing_token_count) {
        return empty;
    }
    return unit->preprocessing_tokens[directive->name_token_index].token.text;
}

NOCDEF void noc_macro_environment_free(Noc_Macro_Environment *environment)
{
    size_t generation;
    if (!environment) return;
    generation = environment->generation;
    free(environment->items);
    memset(environment, 0, sizeof(*environment));
    environment->generation = generation;
}

NOCDEF bool noc_macro_environment_is_valid(
    const Noc_Macro_Environment *environment)
{
    size_t index;
    if (!environment || environment->count > environment->capacity ||
        ((environment->capacity == 0) != (environment->items == NULL))) {
        return false;
    }
    for (index = 0; index < environment->count; ++index) {
        const Noc_Macro_Environment_Entry *entry = &environment->items[index];
        const Noc_Macro_Directive *directive =
            noc__macro_environment_entry_directive(entry);
        if (!directive || directive->status != NOC_MACRO_DIRECTIVE_STATUS_VALID) {
            return false;
        }
        if (entry->previous_entry_index != NOC_TOKEN_INDEX_NONE &&
            entry->previous_entry_index >= index) {
            return false;
        }
    }
    return true;
}

static size_t noc__macro_environment_previous_entry(
    const Noc_Macro_Environment *environment,
    Noc_Slice name)
{
    size_t index = environment->count;
    while (index > 0) {
        const Noc_Macro_Environment_Entry *entry = &environment->items[--index];
        const Noc_Macro_Directive *directive =
            noc__macro_environment_entry_directive(entry);
        Noc_Slice entry_name = noc__macro_environment_directive_name(entry->unit,
                                                                     directive);
        if (noc__slices_logically_equal(entry_name, name)) return index;
    }
    return NOC_TOKEN_INDEX_NONE;
}

NOCDEF Noc_Macro_Environment_Status noc_macro_environment_apply(
    Noc_Macro_Environment *environment,
    const Noc_Preprocessor_Unit *unit,
    size_t macro_directive_index)
{
    const Noc_Macro_Directive *directive;
    const Noc_Preprocessor_Directive *inventory;
    Noc_Macro_Environment_Entry entry;
    Noc_Macro_Environment_Entry *items;
    Noc_Slice name;
    size_t capacity;
    if (!environment || !unit) return NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT;
    if (!noc_macro_environment_is_valid(environment)) {
        return NOC_MACRO_ENVIRONMENT_STALE;
    }
    if (!noc_preprocessor_unit_is_valid(unit) ||
        macro_directive_index >= unit->macro_directive_count) {
        return NOC_MACRO_ENVIRONMENT_INVALID_ARGUMENT;
    }
    directive = &unit->macro_directives[macro_directive_index];
    if (directive->status != NOC_MACRO_DIRECTIVE_STATUS_VALID ||
        directive->directive_index >= unit->count ||
        directive->name_token_index >= unit->preprocessing_token_count) {
        return NOC_MACRO_ENVIRONMENT_INVALID_DIRECTIVE;
    }
    inventory = &unit->items[directive->directive_index];
    if (!inventory->macro_definition_allowed) {
        return NOC_MACRO_ENVIRONMENT_DISABLED;
    }
    if (environment->generation == SIZE_MAX) {
        return NOC_MACRO_ENVIRONMENT_GENERATION_EXHAUSTED;
    }
    name = noc__macro_environment_directive_name(unit, directive);
    entry.unit = unit;
    entry.unit_stream_generation = unit->stream.generation;
    entry.macro_directive_index = macro_directive_index;
    entry.previous_entry_index = noc__macro_environment_previous_entry(environment,
                                                                        name);
    if (environment->count == environment->capacity) {
        if (environment->capacity == 0) {
            capacity = 16;
        } else {
            if (environment->capacity > SIZE_MAX / 2) {
                return NOC_MACRO_ENVIRONMENT_OUT_OF_MEMORY;
            }
            capacity = environment->capacity * 2;
        }
        if (capacity > SIZE_MAX / sizeof(*items)) {
            return NOC_MACRO_ENVIRONMENT_OUT_OF_MEMORY;
        }
        items = (Noc_Macro_Environment_Entry *)realloc(
            environment->items,
            capacity * sizeof(*items));
        if (!items) return NOC_MACRO_ENVIRONMENT_OUT_OF_MEMORY;
        environment->items = items;
        environment->capacity = capacity;
    }
    environment->items[environment->count++] = entry;
    environment->generation += 1;
    return NOC_MACRO_ENVIRONMENT_OK;
}

NOCDEF const Noc_Macro_Environment_Entry *noc_macro_environment_entry_at(
    const Noc_Macro_Environment *environment,
    size_t index)
{
    if (!noc_macro_environment_is_valid(environment) ||
        index >= environment->count) {
        return NULL;
    }
    return &environment->items[index];
}

NOCDEF const Noc_Macro_Directive *noc_macro_environment_entry_directive(
    const Noc_Macro_Environment *environment,
    size_t index)
{
    const Noc_Macro_Environment_Entry *entry =
        noc_macro_environment_entry_at(environment, index);
    return noc__macro_environment_entry_directive(entry);
}

NOCDEF const Noc_Macro_Environment_Entry *noc_macro_environment_lookup_before(
    const Noc_Macro_Environment *environment,
    Noc_Slice logical_name,
    size_t entry_limit)
{
    size_t index;
    if (!noc_macro_environment_is_valid(environment) ||
        (!logical_name.data && logical_name.count != 0) ||
        logical_name.count == 0 || entry_limit > environment->count) {
        return NULL;
    }
    index = entry_limit;
    while (index > 0) {
        const Noc_Macro_Environment_Entry *entry = &environment->items[--index];
        const Noc_Macro_Directive *directive =
            noc__macro_environment_entry_directive(entry);
        Noc_Slice name = noc__macro_environment_directive_name(entry->unit,
                                                               directive);
        if (!noc__slices_logically_equal(name, logical_name)) continue;
        return directive->kind == NOC_MACRO_DIRECTIVE_UNDEF ? NULL : entry;
    }
    return NULL;
}

NOCDEF const Noc_Macro_Environment_Entry *noc_macro_environment_lookup(
    const Noc_Macro_Environment *environment,
    Noc_Slice logical_name)
{
    if (!environment) return NULL;
    return noc_macro_environment_lookup_before(environment,
                                               logical_name,
                                               environment->count);
}

#endif /* NOC_MACRO_ENVIRONMENT_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
