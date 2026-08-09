#include "noc.h"

#include <string.h>

int main(void)
{
    Noc_Slice version = {NOC_VERSION, sizeof(NOC_VERSION) - 1};
    return version.count == strlen(NOC_VERSION) &&
                   NOC_VERSION_MAJOR == 0 &&
                   NOC_VERSION_MINOR == 42 &&
                   NOC_VERSION_PATCH == 11
               ? 0
               : 1;
}
