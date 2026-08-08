#include <noc/noc.h>

#include <string.h>

int main(void)
{
    Noc_Context context;
    Noc_Lexer lexer;
    Noc_Token token;
    Noc_Buffer buffer = {0};

    noc_context_init(&context);
    noc_lexer_init(&lexer, "modules.c", "name", 4);
    token = noc_lexer_next(&lexer);
    if (!noc_token_is_identifier(token, "name")) return 1;
    if (!noc_buffer_append_cstr(&buffer, NOC_VERSION) ||
        buffer.count != strlen(NOC_VERSION)) return 1;
    noc_buffer_free(&buffer);
    noc_context_deinit(&context);
    return 0;
}
