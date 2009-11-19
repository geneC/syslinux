/*
 * dprintf.c
 */

#include <stdio.h>
#include <stdarg.h>

#undef DEBUG
#define DEBUG 1
#include <dprintf.h>

int dprintf(const char *format, ...)
{
    va_list ap;
    int rv;

    va_start(ap, format);
    rv = vdprintf(format, ap);
    va_end(ap);
    return rv;
}
