#include "alloc.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define ALIGNMENT 16 /* XXX: assuming that this alignment is enough */

char *alloc(unsigned int n)
{
    char *x;
    n = ALIGNMENT + n;
    x = malloc(n);
    return x;
}

void alloc_free(char *x)
{
    if (x != NULL)
    {
        free(x);
        x = NULL;
    }
}

int alloc_re(char **x, unsigned int old_size, unsigned int new_size)
{
    char *y = alloc(new_size);
    if (!y)
        return 0;

    memcpy(y, *x, old_size);
    alloc_free(*x);
    *x = y;
    return 1;
}