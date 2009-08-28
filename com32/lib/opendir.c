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
    DIR *newdir = NULL;
    com32sys_t regs;
	
    strlcpy(__com32.cs_bounce, pathname, __com32.cs_bounce_size);

    regs.eax.w[0] = 0x0020;
    regs.esi.w[0] = OFFS(__com32.cs_bounce);
    regs.es = SEG(__com32.cs_bounce);

    __com32.cs_intcall(0x22, &regs, &regs);
	
    if (!(regs.eflags.l & EFLAGS_CF)) {
        /* Initialization: malloc() then zero */
        newdir = calloc(1, sizeof(DIR));
	newdir->dd_dir = (struct file *)regs.eax.l;
    }
	
    /* We're done */
    return newdir;
}
