#include <noc/noc.h>

/* This example deliberately keeps ordinary C declarations, expressions, and
   control flow recognizable. Only concepts that C cannot express directly get
   new syntax:

       generic identity(T)
       T identity(T value) { return value; }
       specialize identity(int) as identity_int;
       owned(Resource *, release_resource) value = acquire_resource();
       inspect(borrow(value));
       return move(value);
       defer cleanup();

   The first syntax phase maps generic/specialize/owned to Noc's canonical
   structured forms. The built-in template, ownership, and defer passes then
   provide the semantics. This keeps the public spelling replaceable without
   duplicating those structured lowerers in a project dialect. */

static bool expand_generic(Noc_Rewriter *rewriter,
                           const Noc_Rule *rule,
                           void *user_data)
{
    Noc_Token name;
    Noc_Slice parameter;
    (void)rule;
    (void)user_data;
    return noc_rw_expect_identifier(rewriter, NULL, &name) &&
           noc_rw_capture_balanced(rewriter, "(", ")", &parameter) &&
           noc_rw_emit_cstr(rewriter, "template(") &&
           noc_rw_emit_slice(rewriter, parameter) &&
           noc_rw_emit_cstr(rewriter, ", ") &&
           noc_rw_emit_slice(rewriter, name.text) &&
           noc_rw_emit_cstr(rewriter, ")");
}

static bool expand_specialize(Noc_Rewriter *rewriter,
                              const Noc_Rule *rule,
                              void *user_data)
{
    Noc_Token generic_name;
    Noc_Token generated_name;
    Noc_Slice type;
    (void)rule;
    (void)user_data;
    return noc_rw_expect_identifier(rewriter, NULL, &generic_name) &&
           noc_rw_capture_balanced(rewriter, "(", ")", &type) &&
           noc_rw_expect_identifier(rewriter, "as", NULL) &&
           noc_rw_expect_identifier(rewriter, NULL, &generated_name) &&
           noc_rw_expect_punct(rewriter, ";") &&
           noc_rw_emit_cstr(rewriter, "instantiate(") &&
           noc_rw_emit_slice(rewriter, generic_name.text) &&
           noc_rw_emit_cstr(rewriter, ", ") &&
           noc_rw_emit_slice(rewriter, type) &&
           noc_rw_emit_cstr(rewriter, ", ") &&
           noc_rw_emit_slice(rewriter, generated_name.text) &&
           noc_rw_emit_cstr(rewriter, ");");
}

static bool expand_owned(Noc_Rewriter *rewriter,
                         const Noc_Rule *rule,
                         void *user_data)
{
    (void)rule;
    (void)user_data;
    return noc_rw_emit_cstr(rewriter, "own");
}

int main(int argc, char **argv)
{
    static const char *patterns[] = {"generic", "specialize", "owned"};
    Noc_Rule rules[] = {
        {
            "generic-function-syntax",
            NOC_RULE_DECLARATION,
            "generic name(type_parameter) C-function-definition",
            "Declare one explicitly monomorphized generic C function.",
            expand_generic,
            NULL,
        },
        {
            "generic-instance-syntax",
            NOC_RULE_DECLARATION,
            "specialize name(type) as generated_name;",
            "Generate one named C function from a generic declaration.",
            expand_specialize,
            NULL,
        },
        {
            "owned-declaration-syntax",
            NOC_RULE_DECLARATION,
            "owned(pointer_type, drop_function) name = initializer;",
            "Declare one movable lexical owner with deterministic cleanup.",
            expand_owned,
            NULL,
        },
    };
    Noc_Context noc;
    size_t i;
    int result;
    noc_context_init(&noc);
    if (!noc_enable_mvp_features(&noc)) {
        noc_context_deinit(&noc);
        return 1;
    }
    for (i = 0; i < sizeof(rules) / sizeof(rules[0]); ++i) {
        if (!noc_register_rule_pattern_in_phase(&noc,
                                                NOC_RULE_PHASE_SYNTAX,
                                                patterns[i],
                                                rules[i])) {
            noc_context_deinit(&noc);
            return 1;
        }
    }
    result = noc_run_cli(&noc, argc, argv);
    noc_context_deinit(&noc);
    return result;
}
