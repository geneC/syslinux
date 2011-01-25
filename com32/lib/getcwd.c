/*
 * getcwd.c
 */

#include <com32.h>
#include <syslinux/pmapi.h>

char *getcwd(char *buf, size_t size)
{
    return __com32.cs_pm->getcwd(buf, size);
}
