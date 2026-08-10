#include <noc/noc.h>

int main(int argc, char **argv)
{
    Noc_Context noc;
    int result;
    noc_context_init(&noc);
    /* Structured syntax is contextual: this dialect explicitly opts into it. */
    if (!noc_enable_mvp_features(&noc)) {
        noc_context_deinit(&noc);
        return 1;
    }
    result = noc_run_cli(&noc, argc, argv);
    noc_context_deinit(&noc);
    return result;
}
