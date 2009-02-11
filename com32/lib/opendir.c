/*
 * opendir.c
 */

#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#include <com32.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


DIR *opendir(const char *pathname)
{
	DIR *newdir;
	com32sys_t regs;

	newdir = NULL;

	strlcpy(__com32.cs_bounce, pathname, __com32.cs_bounce_size);

	regs.eax.w[0] = 0x0020;
	regs.esi.w[0] = OFFS(__com32.cs_bounce);
	regs.es = SEG(__com32.cs_bounce);

	__com32.cs_intcall(0x22, &regs, &regs);

	if (!(regs.eflags.l & EFLAGS_CF)) {
		/* Initialization: malloc() then zero */
		newdir = calloc(1, sizeof(DIR));
		strcpy(newdir->dd_name, pathname);
		newdir->dd_fd = regs.esi.w[0];
		newdir->dd_sect = regs.eax.l;
		newdir->dd_stat = 0;
	}

	/* We're done */
	return newdir;
}
