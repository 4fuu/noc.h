#include <stdio.h>

static const char message[] = @embed("message.txt");

int main(void)
{
    fputs(message, stdout);
    return 0;
}
