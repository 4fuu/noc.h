#include <noc/noc.h>

int main(int argc, char **argv)
{
    Noc_Context noc;
    int result;
    noc_context_init(&noc);
    if (!noc_register_embed_rule(&noc, "embed")) {
        noc_context_deinit(&noc);
        return 1;
    }
    result = noc_run_cli(&noc, argc, argv);
    noc_context_deinit(&noc);
    return result;
}
