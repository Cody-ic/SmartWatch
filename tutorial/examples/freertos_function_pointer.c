/* Standalone C example: print 25 through a function pointer. */
#include <stdint.h>
#include <stdio.h>

static void show_number(void *argument)
{
    uint32_t *number = argument;
    printf("%lu\n", (unsigned long)*number);
}

int main(void)
{
    uint32_t value = 25;
    void (*function)(void *) = show_number;
    function(&value);
    return 0;
}
