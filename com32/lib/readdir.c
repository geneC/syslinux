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

struct dirent *readdir(DIR * dir)
{
    struct dirent *newde;
    com32sys_t regs;
	    
	memset(&regs, 0, sizeof(regs));		
	regs.eax.w[0] = 0x0021;
	regs.esi.l = (uint32_t)dir;
	__com32.cs_intcall(0x22, &regs, &regs);
	newde = (struct dirent *)(regs.eax.l);
	
    return newde;
}
