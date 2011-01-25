/* -*- c -*- ------------------------------------------------------------- *
 *
 *   Copyright 2004-2005 Murali Krishnan Ganapathy - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <string.h>
#include <com32.h>
#include "syslnx.h"

com32sys_t inreg, outreg;	// Global registers for this module

char issyslinux(void)
{
    REG_EAX(inreg) = 0x00003000;
    REG_EBX(inreg) = REG_ECX(inreg) = REG_EDX(inreg) = 0xFFFFFFFF;
    __intcall(0x21, &inreg, &outreg);
    return (REG_EAX(outreg) == 0x59530000) &&
	(REG_EBX(outreg) == 0x4c530000) &&
	(REG_ECX(outreg) == 0x4e490000) && (REG_EDX(outreg) == 0x58550000);
}

void runsyslinuxcmd(const char *cmd)
{
    strcpy(__com32.cs_bounce, cmd);
    REG_AX(inreg) = 0x0003;	// Run command
    REG_BX(inreg) = OFFS(__com32.cs_bounce);
    REG_ES(inreg) = SEG(__com32.cs_bounce);
    __intcall(0x22, &inreg, &outreg);
}

void gototxtmode(void)
{
    REG_AX(inreg) = 0x0005;
    __intcall(0x22, &inreg, &outreg);
}

void syslinux_idle(void)
{
    REG_AX(inreg) = 0x0013;
    __intcall(0x22, &inreg, &outreg);
}

unsigned int getversion(char *deriv, unsigned int *numfun)
{
    REG_AX(inreg) = 0x0001;
    __intcall(0x22, &inreg, &outreg);
    if (deriv)
	*deriv = REG_DL(outreg);
    if (numfun)
	*numfun = REG_AX(outreg);
    return REG_CX(outreg);
}

void runsyslinuximage(const char *cmd, long ipappend)
{
    unsigned int numfun = 0;
    char *ptr, *cmdline;

    (void)ipappend;		// XXX: Unused?!

    getversion(NULL, &numfun);
    // Function 16h not supported Fall back to runcommand
    if (numfun < 0x16)
	runsyslinuxcmd(cmd);
    // Try the Run Kernel Image function
    // Split command line into
    strcpy(__com32.cs_bounce, cmd);
    ptr = __com32.cs_bounce;
    // serach for first space or end of string
    while ((*ptr) && (*ptr != ' '))
	ptr++;
    if (!*ptr)
	cmdline = ptr;		// no command line
    else {
	*ptr++ = '\0';		// terminate kernal name
	cmdline = ptr + 1;
	while (*cmdline != ' ')
	    cmdline++;		// find first non-space
    }
    // Now call the interrupt
    REG_BX(inreg) = OFFS(cmdline);
    REG_ES(inreg) = SEG(cmdline);
    REG_SI(inreg) = OFFS(__com32.cs_bounce);
    REG_DS(inreg) = SEG(__com32.cs_bounce);
    REG_EDX(inreg) = 0;

    __intcall(0x22, &inreg, &outreg);	// If successful does not return
}
