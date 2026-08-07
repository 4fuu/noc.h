#define NOC_IMPLEMENTATION
#include "../../noc.h"

static bool expand_square(Noc_Rewriter *rewriter,
                          const Noc_Rule *rule,
                          void *user_data)
{
    Noc_Slice expression;
    (void)rule;
    (void)user_data;
    return noc_rw_capture_balanced(rewriter, "(", ")", &expression) &&
           noc_rw_emit_cstr(rewriter, "((") &&
           noc_rw_emit_transformed(rewriter, expression) &&
           noc_rw_emit_cstr(rewriter, ") * (") &&
           noc_rw_emit_transformed(rewriter, expression) &&
           noc_rw_emit_cstr(rewriter, "))");
}

static bool expand_repeat(Noc_Rewriter *rewriter,
                          const Noc_Rule *rule,
                          void *user_data)
{
    Noc_Location location = noc_rw_trigger_location(rewriter);
    Noc_Slice count;
    Noc_Slice body;
    (void)rule;
    (void)user_data;
    return noc_rw_capture_balanced(rewriter, "(", ")", &count) &&
           noc_rw_capture_balanced(rewriter, "{", "}", &body) &&
           noc_rw_emitf(rewriter,
                        "for (int noc_repeat_%zu_%zu = 0; noc_repeat_%zu_%zu < (",
                        location.line,
                        location.column,
                        location.line,
                        location.column) &&
           noc_rw_emit_transformed(rewriter, count) &&
           noc_rw_emitf(rewriter,
                        "); ++noc_repeat_%zu_%zu) {",
                        location.line,
                        location.column) &&
           noc_rw_emit_transformed(rewriter, body) &&
           noc_rw_emit_cstr(rewriter, "}");
}

static bool expand_counter(Noc_Rewriter *rewriter,
                           const Noc_Rule *rule,
                           void *user_data)
{
    Noc_Token name;
    (void)rule;
    (void)user_data;
    return noc_rw_expect_punct(rewriter, "(") &&
           noc_rw_expect_identifier(rewriter, NULL, &name) &&
           noc_rw_expect_punct(rewriter, ")") &&
           noc_rw_emit_cstr(rewriter, "static int ") &&
           noc_rw_emit_slice(rewriter, name.text) &&
           noc_rw_emit_cstr(rewriter, " = 0");
}

static bool expand_private(Noc_Rewriter *rewriter,
                           const Noc_Rule *rule,
                           void *user_data)
{
    (void)rule;
    (void)user_data;
    return noc_rw_emit_cstr(rewriter, "static");
}

int main(int argc, char **argv)
{
    Noc_Context noc;
    int result;
    Noc_Rule rules[] = {
        {
            "square",
            NOC_RULE_EXPRESSION,
            "@square(expression)",
            "Evaluate the product of an expression with itself.",
            expand_square,
            NULL,
        },
        {
            "repeat",
            NOC_RULE_STATEMENT,
            "@repeat(count) { statements }",
            "Repeat a statement block count times.",
            expand_repeat,
            NULL,
        },
        {
            "counter",
            NOC_RULE_DECLARATION,
            "@counter(name);",
            "Declare a file-local integer counter initialized to zero.",
            expand_counter,
            NULL,
        },
        {
            "private",
            NOC_RULE_ATTRIBUTE,
            "@private declaration",
            "Give a declaration internal linkage.",
            expand_private,
            NULL,
        },
    };
    size_t i;

    noc_context_init(&noc);
    for (i = 0; i < sizeof(rules) / sizeof(rules[0]); ++i) {
        if (!noc_register_rule(&noc, rules[i])) {
            noc_context_deinit(&noc);
            return 1;
        }
    }
    result = noc_run_cli(&noc, argc, argv);
    noc_context_deinit(&noc);
    return result;
}
