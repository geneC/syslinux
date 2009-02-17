/*
 * chdir.c
 */

#include <dirent.h>
#include <stdio.h>
#include <errno.h>

int chdir(const char *path)
{
	errno = ENOSYS;
	return -1;
}
