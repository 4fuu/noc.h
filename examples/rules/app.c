#include <stdio.h>

@counter(total);

@private int increment(int value)
{
    return value + 1;
}

int main(void)
{
    repeat(3) {
        total += @square(increment(2));
    }
    printf("rules example: %d\n", total);
    return total == 27 ? 0 : 1;
}
