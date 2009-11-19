/*
 * dprintf.c
 */

#include <stdio.h>
#include <stdarg.h>

int dprintf(const char *format, ...)
{
    va_list ap;
    int rv;

    va_start(ap, format);
    rv = vdprintf(stdout, format, ap);
    va_end(ap);
    return rv;
}
