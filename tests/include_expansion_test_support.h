#ifndef NOC_INCLUDE_EXPANSION_TEST_SUPPORT_H_INCLUDED
#define NOC_INCLUDE_EXPANSION_TEST_SUPPORT_H_INCLUDED

#include "macro_expansion_test_support.h"

static inline Noc_Macro_Expansion_Status include_expansion_build_at(
    Macro_Expansion_Fixture *fixture,
    size_t directive_index,
    Noc_Include_Expansion *output)
{
    Noc_Include_Operand operand = {0};
    Noc_Macro_Expansion_Status status;
    CHECK(noc_include_operand_build(&fixture->input,
                                    directive_index,
                                    &operand) ==
          NOC_INCLUDE_OPERAND_BUILD_OK);
    CHECK(operand.status == NOC_INCLUDE_OPERAND_EXPANSION_REQUIRED);
    status = noc_include_expansion_build(
        &fixture->environment,
        fixture->environment.count,
        &operand,
        noc_macro_expansion_default_limits(),
        output);
    noc_include_operand_free(&operand);
    return status;
}

#endif /* NOC_INCLUDE_EXPANSION_TEST_SUPPORT_H_INCLUDED */
