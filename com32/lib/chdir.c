/*
 * chdir.c
 */

#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#include <com32.h>
#include <syslinux/pmapi.h>

int chdir(const char *path)
{
    return __com32.cs_pm->chdir(path);
}
