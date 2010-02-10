/*
 * chdir.c
 */

#include <dirent.h>
#include <stdio.h>
#include <errno.h>

int chdir(const char *path)
{
    /* Actually implement something here... */

    (void)path;
    
    errno = ENOSYS;
    return -1;
}
