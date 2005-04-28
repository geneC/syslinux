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

com32sys_t inreg,outreg; // Global registers for this module

char issyslinux(void)
{
  REG_EAX(inreg) = 0x00003000;
  REG_EBX(inreg) = REG_ECX(inreg) = REG_EDX(inreg) = 0xFFFFFFFF;
  __intcall(0x21,&inreg,&outreg);
  return (REG_EAX(outreg) == 0x59530000) && 
         (REG_EBX(outreg) == 0x4c530000) &&
         (REG_ECX(outreg) == 0x4e490000) && 
         (REG_EDX(outreg) == 0x58550000);
}

void runsyslinuxcmd(const char *cmd)
{
  strcpy(__com32.cs_bounce, cmd);
  REG_AX(inreg) = 0x0003; // Run command
  REG_BX(inreg) = OFFS(__com32.cs_bounce);
  inreg.es = SEG(__com32.cs_bounce);
  __intcall(0x22, &inreg, &outreg);
}

void gototxtmode(void)
{
  REG_AX(inreg) = 0x0005;
  __intcall(0x22,&inreg,&outreg);
}

