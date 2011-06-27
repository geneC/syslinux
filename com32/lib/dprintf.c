/*
 * dprintf.c
 */

#include <stdio.h>
#include <stdarg.h>

#undef DEBUG
#define DEBUG 1
#include <dprintf.h>

#ifndef dprintf
void dprintf(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vdprintf(format, ap);
    va_end(ap);
}
#endif
