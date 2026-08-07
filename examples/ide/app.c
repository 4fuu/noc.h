#include "math.h"
#include "rules_metadata.h"

#if NOC_IDE_RULE_COUNT != 4
#error "unexpected IDE rule metadata"
#endif

int main(void)
{
    int value = @square(triple(1));
    return value == 9 ? 0 : 1;
}
