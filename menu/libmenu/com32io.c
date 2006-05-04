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
#include "com32io.h"
#include "syslnx.h"

com32sys_t inreg,outreg; // Global register sets for use

/* Print character and attribute at cursor */
void cprint(char chr,char attr,unsigned int times,char disppage)
{
    REG_AH(inreg) = 0x09;
    REG_AL(inreg) = chr;
    REG_BH(inreg) = disppage;
    REG_BL(inreg) = attr;
    REG_CX(inreg) = times;
    __intcall(0x10,&inreg,&outreg);
}

void setdisppage(char num) // Set the display page to specified number
{
    REG_AH(inreg) = 0x05;
    REG_AL(inreg) = num;
    __intcall(0x10,&inreg,&outreg);
}

char getdisppage() // Get current display page
{
    REG_AH(inreg) = 0x0f;
    __intcall(0x10,&inreg,&outreg);
    return REG_BH(outreg);
}

void getpos(char * row, char * col, char page)
{
   REG_AH(inreg) = 0x03;
   REG_BH(inreg) = page;
   __intcall(0x10,&inreg,&outreg);
   *row = REG_DH(outreg);
   *col = REG_DL(outreg);
}

void gotoxy(char row,char col, char page)
{
   REG_AH(inreg) = 0x02;
   REG_BH(inreg) = page;
   REG_DX(inreg) = (row << 8)+col;
   __intcall(0x10,&inreg,&outreg);
}

unsigned char sleep(unsigned int msec)
{
   unsigned long micro = 1000*msec;

   REG_AH(inreg) = 0x86;
   REG_CX(inreg) = (micro >> 16);
   REG_DX(inreg) = (micro & 0xFFFF);
   __intcall(0x15,&inreg,&outreg);
   return REG_AH(outreg);
}

void beep()
{
   REG_AH(inreg) = 0x0E;
   REG_AL(inreg) = 0x07;
   REG_BH(inreg) = 0;
   __intcall(0x10,&inreg,&outreg);
}

void scrollupwindow(char top, char left, char bot, char right, char attr,char numlines)
{
  REG_AH(inreg) = 0x06;
  REG_AL(inreg) = numlines;
  REG_BH(inreg) = attr; // Attribute to write blanks lines
  REG_DX(inreg) = (bot << 8) + right; // BOT RIGHT corner of window
  REG_CX(inreg) = (top << 8) + left; // TOP LEFT of window
  __intcall(0x10,&inreg,&outreg);
}

char inputc(char * scancode)
{
  syslinux_idle(); /* So syslinux can perform periodic activity */
  REG_AH(inreg) = 0x10;
  __intcall(0x16,&inreg,&outreg);
  if (scancode) *scancode = REG_AH(outreg);
  return REG_AL(outreg);
}

void getcursorshape(char *start, char *end)
{
   char page = getdisppage();
   REG_AH(inreg) = 0x03;
   REG_BH(inreg) = page;
   __intcall(0x10,&inreg,&outreg);
   *start = REG_CH(outreg);
   *end = REG_CL(outreg);
}

void setcursorshape(char start, char end)
{
   REG_AH(inreg) = 0x01;
   REG_CH(inreg) = start;
   REG_CL(inreg) = end;
   __intcall(0x10,&inreg,&outreg);
}

char getchar(void)
{
   REG_AH(inreg) = 0x08;
   __intcall(0x21,&inreg,&outreg);
   return REG_AL(outreg);
}

void setvideomode(char mode)
{
   REG_AH(inreg) = 0x00;
   REG_AL(inreg) = mode;
   __intcall(0x10,&inreg,&outreg);
}

unsigned char checkkbdbuf()
{
   REG_AH(inreg) = 0x11;
   __intcall(0x16,&inreg,&outreg);
   return !(outreg.eflags.l & EFLAGS_ZF);
}

// Get char displayed at current position
unsigned char getcharat(char page)
{
   REG_AH(inreg) = 0x08;
   REG_BH(inreg) = page;
   __intcall(0x16,&inreg,&outreg);
   return REG_AL(outreg);
}
