#include <stdio.h>
#include <unistd.h>




#define BUF_SIZE 1024

char buf[BUF_SIZE];


extern void myputs(const char *);

int printf(const char *format, ...)
{
    va_list ap;
    int rv;
    
    va_start(ap, format);
    rv = sprintf(buf, format, ap);
    va_end(ap);
    
    myputs(buf);

    return rv;

}
