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

com32sys_t inreg, outreg;	// Global register sets for use

static void cprint_vga2ansi(char chr, char attr)
{
	static const char ansi_char[8] = "04261537";
	static uint8_t last_attr = 0x07;
	char buf[16], *p;

	if (attr != last_attr) {
		p = buf;
		*p++ = '\033';
		*p++ = '[';

		if (last_attr & ~attr & 0x88) {
			*p++ = '0';
			*p++ = ';';
		}
		if (attr & 0x08) {
			*p++ = '1';
			*p++ = ';';
		}
		if (attr & 0x80) {
			*p++ = '4';
			*p++ = ';';
		}
		if ((attr ^ last_attr) & 0x07) {
			*p++ = '3';
			*p++ = ansi_char[attr & 7];
			*p++ = ';';
		}
		if ((attr ^ last_attr) & 0x70) {
			*p++ = '4';
			*p++ = ansi_char[(attr >> 4) & 7];
			*p++ = ';';
		}
		p[-1] = 'm';	/* We'll have generated at least one semicolon */
		p[0] = '\0';

		last_attr = attr;

		fputs(buf, stdout);
	}

	putchar(chr);
}

/* Print character and attribute at cursor */
// Note: attr is a vga attribute
void cprint(char chr, char attr, unsigned int times, char disppage)
{
	// XXX disppage

	while (times--)
		cprint_vga2ansi(chr, attr);
}

void setdisppage(char num)	// Set the display page to specified number
{
    REG_AH(inreg) = 0x05;
    REG_AL(inreg) = num;
    __intcall(0x10, &inreg, &outreg);
}

char getdisppage()		// Get current display page
{
    REG_AH(inreg) = 0x0f;
    __intcall(0x10, &inreg, &outreg);
    return REG_BH(outreg);
}

void getpos(char *row, char *col, char page)
{
    REG_AH(inreg) = 0x03;
    REG_BH(inreg) = page;
    __intcall(0x10, &inreg, &outreg);
    *row = REG_DH(outreg);
    *col = REG_DL(outreg);
}

unsigned char sleep(unsigned int msec)
{
    unsigned long micro = 1000 * msec;

    REG_AH(inreg) = 0x86;
    REG_CX(inreg) = (micro >> 16);
    REG_DX(inreg) = (micro & 0xFFFF);
    __intcall(0x15, &inreg, &outreg);
    return REG_AH(outreg);
}

void beep()
{
    REG_AH(inreg) = 0x0E;
    REG_AL(inreg) = 0x07;
    REG_BH(inreg) = 0;
    __intcall(0x10, &inreg, &outreg);
}

void scrollupwindow(char top, char left, char bot, char right, char attr,
		    char numlines)
{
    REG_AH(inreg) = 0x06;
    REG_AL(inreg) = numlines;
    REG_BH(inreg) = attr;	// Attribute to write blanks lines
    REG_DX(inreg) = (bot << 8) + right;	// BOT RIGHT corner of window
    REG_CX(inreg) = (top << 8) + left;	// TOP LEFT of window
    __intcall(0x10, &inreg, &outreg);
}

char inputc(char *scancode)
{
    syslinux_idle();		/* So syslinux can perform periodic activity */
    REG_AH(inreg) = 0x10;
    __intcall(0x16, &inreg, &outreg);
    if (scancode)
	*scancode = REG_AH(outreg);
    return REG_AL(outreg);
}

void getcursorshape(char *start, char *end)
{
    char page = getdisppage();
    REG_AH(inreg) = 0x03;
    REG_BH(inreg) = page;
    __intcall(0x10, &inreg, &outreg);
    *start = REG_CH(outreg);
    *end = REG_CL(outreg);
}

void setcursorshape(char start, char end)
{
    REG_AH(inreg) = 0x01;
    REG_CH(inreg) = start;
    REG_CL(inreg) = end;
    __intcall(0x10, &inreg, &outreg);
}

void setvideomode(char mode)
{
    REG_AH(inreg) = 0x00;
    REG_AL(inreg) = mode;
    __intcall(0x10, &inreg, &outreg);
}

unsigned char checkkbdbuf()
{
    REG_AH(inreg) = 0x11;
    __intcall(0x16, &inreg, &outreg);
    return !(outreg.eflags.l & EFLAGS_ZF);
}

// Get char displayed at current position
unsigned char getcharat(char page)
{
    REG_AH(inreg) = 0x08;
    REG_BH(inreg) = page;
    __intcall(0x16, &inreg, &outreg);
    return REG_AL(outreg);
}
