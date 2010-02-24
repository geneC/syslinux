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
#include <stdlib.h>

DIR *opendir(const char *pathname)
{
    DIR *newdir = NULL;
    com32sys_t regs;
    char *lm_pathname;

    lm_pathname = lstrdup(pathname);
    if (!lm_pathname)
	return NULL;

    regs.eax.w[0] = 0x0020;
    regs.esi.w[0] = OFFS(lm_pathname);
    regs.es = SEG(lm_pathname);

    __com32.cs_intcall(0x22, &regs, &regs);
	
    if (!(regs.eflags.l & EFLAGS_CF)) {
        /* Initialization: malloc() then zero */
        newdir = zalloc(sizeof(DIR));
	newdir->dd_dir = (struct file *)regs.eax.l;
    }

    lfree(lm_pathname);

    /* We're done */
    return newdir;
}
