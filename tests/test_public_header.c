#include <noc/noc.h>

#include <string.h>

int main(void)
{
    Noc_Slice version = {NOC_VERSION, sizeof(NOC_VERSION) - 1};
    Noc_Token_Range empty = {0, 0};
    return version.count == strlen(NOC_VERSION) &&
                   empty.begin == empty.end &&
                   NOC_VERSION_MAJOR == 0 &&
                   NOC_VERSION_MINOR == 31 &&
                   NOC_VERSION_PATCH == 0
               ? 0
               : 1;
}
