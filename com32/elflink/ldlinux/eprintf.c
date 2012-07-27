#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <core.h>

#define BUFFER_SIZE	4096

static void veprintf(const char *format, va_list ap)
{
    int rv, _rv;
    char buffer[BUFFER_SIZE];
    char *p;

    _rv = rv = vsnprintf(buffer, BUFFER_SIZE, format, ap);

    if (rv < 0)
	return;

    if (rv > BUFFER_SIZE - 1)
	rv = BUFFER_SIZE - 1;

    p = buffer;
    while (rv--)
	    write_serial(*p++);

    _fwrite(buffer, _rv, stdout);
}

void eprintf(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    veprintf(format, ap);
    va_end(ap);
}
