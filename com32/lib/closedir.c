/*
 * closedir.c
 */

#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#include <com32.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int closedir(DIR *dir)
{
	int rv;
	com32sys_t regs;
	if (dir == NULL) {
		rv = 0;
	} else {
		memset(&regs, 0, sizeof regs);	/* ?Needed? */
		regs.eax.w[0] = 0x0022;
		regs.esi.w[0] = dir->dd_fd;
		__com32.cs_intcall(0x22, &regs, &regs);
		free(dir);	/* garbage collection? */
		rv = 0;
	}
	return rv;
}
