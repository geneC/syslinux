#include <stdio.h>
#include <unistd.h>

#include "core.h"

int printf(const char *format, ...)
{
    char buf[1024];
    va_list ap;
    int rv;
    
    va_start(ap, format);
    rv = vsnprintf(buf, sizeof buf, format, ap);
    va_end(ap);
    
    myputs(buf);

    return rv;

}
