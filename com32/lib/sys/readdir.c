/*
 * readdir.c
 */

#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#include <com32.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <syslinux/pmapi.h>

DIR *opendir(const char *pathname)
{
    return __com32.cs_pm->opendir(pathname);
}

struct dirent *readdir(DIR *dir)
{
    return __com32.cs_pm->readdir(dir);
}

int closedir(DIR *dir)
{
    return __com32.cs_pm->closedir(dir);
}
