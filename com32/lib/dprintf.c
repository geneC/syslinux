/*
 * dprintf.c
 */

#include <stdio.h>
#include <stdarg.h>

#ifdef DEBUG

#include <dprintf.h>

void dprintf(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vdprintf(format, ap);
    va_end(ap);
}

#endif
