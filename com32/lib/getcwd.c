/*
 * getcwd.c
 */

#include <com32.h>
#include <syslinux/pmapi.h>

char *getcwd(char *buf, size_t size)
{
    return core_getcwd(buf, size);
}
