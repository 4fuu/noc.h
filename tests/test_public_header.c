#include <noc/noc.h>

#include <string.h>

static Noc_Include_Resolve_Status public_header_resolver(
    void *user_data,
    const Noc_Include_Request *request,
    Noc_Document_Snapshot *output)
{
    (void)user_data;
    (void)request;
    (void)output;
    return NOC_INCLUDE_RESOLVE_NOT_FOUND;
}

int main(void)
{
    Noc_Slice version = {NOC_VERSION, sizeof(NOC_VERSION) - 1};
    Noc_Token_Range empty = {0, 0};
    Noc_Include_Resolver resolver;
    resolver.resolve = public_header_resolver;
    resolver.user_data = NULL;
    return version.count == strlen(NOC_VERSION) &&
                   empty.begin == empty.end &&
                   resolver.resolve != NULL &&
                   NOC_INCLUDE_FORM_QUOTED != NOC_INCLUDE_FORM_ANGLED &&
                   NOC_VERSION_MAJOR == 0 &&
                   NOC_VERSION_MINOR == 37 &&
                   NOC_VERSION_PATCH == 0
               ? 0
               : 1;
}
