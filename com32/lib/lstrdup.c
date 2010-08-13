/*
 * lstrdup.c
 */

#include <string.h>
#include <stdlib.h>
#include <com32.h>

char *lstrdup(const char *s)
{
    int l = strlen(s) + 1;
    char *d = clmalloc(l);

    if (d)
	memcpy(d, s, l);

    return d;
}
