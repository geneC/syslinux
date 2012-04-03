/*
 * -----------------------------------------------------------------------
 *
 *   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * -----------------------------------------------------------------------
 *
 *
 * font.c
 *
 * VGA font handling code
 *
 */

#include <sys/io.h>
#include <stdio.h>
#include <fs.h>
#include "bios.h"
#include "core.h"

char fontbuf[8192];
char serial[serial_buf_size];

extern uint16_t VGAFontSize;
extern uint8_t UserFont;

uint16_t GXPixCols = 1;		/* Graphics mode pixel columns */
uint16_t GXPixRows = 1;		/* Graphics mode pixel rows */

/*
 * loadfont:	Load a .psf font file and install it onto the VGA console
 *		(if we're not on a VGA screen then ignore.)
 *
 * The .psf font file must alredy be open and getc_file must be set.
 */
void loadfont(char *filename)
{
	uint16_t height, magic;
	uint32_t *di, *si;
	FILE *f;
	char *p;
	int i;

	f = fopen(filename, "r");
	if (!f)
		return;

	p = trackbuf;
	/* Read header */
	for (i = 0; i < 4; i++) {
		char ch = getc(f);
		if (ch == EOF)
			return;
		*p++ = ch;
	}

	/* Magic number */
	magic = *(uint16_t *)trackbuf;
	if (magic != 0x0436)
		return;

	/* File mode: font modes 0-5 supported */
	if (*(trackbuf) > 5)
		return;

	height = *(trackbuf + 3); /* Height of font */

	/* VGA minimum/maximum */
	if (height < 2 || height > 32)
		return;

	/* Load the actual font. Bytes = font height * 256 */
	p = trackbuf;
	for (i = 0; i < (height << 8); i++) {
		char ch = getc(f);

		if (ch == EOF)
			return;
		*p++ = ch;
	}

	/* Copy to font buffer */
	VGAFontSize = height;
	di = (uint32_t *)fontbuf;
	si = (uint32_t *)trackbuf;
	for (i = 0; i < (height << 6); i++)
		*di++ = *si++;

	UserFont = 1;		/* Set font flag */
	use_font();
}

/*
 * use_font:
 *	This routine activates whatever font happens to be in the
 *	vgafontbuf, and updates the adjust_screen data.
 *      Must be called with CS = DS
 */
void use_font(void)
{
	com32sys_t ireg, oreg;
	uint8_t bytes = VGAFontSize;


	/* Nonstandard mode? */
	if (UsingVGA & ~0x3)
		vgaclearmode();

	memset(&ireg, 0, sizeof(ireg));

	ireg.es = SEG(fontbuf);
	ireg.ebp.w[0] = OFFS(fontbuf); /* ES:BP -> font */

	/* Are we using a user-specified font? */
	if (UserFont & 0x1) {
		/* Are we in graphics mode? */
		if (UsingVGA & 0x1) {
			uint8_t rows;

			rows = GXPixRows / bytes;
			VidRows = rows - 1;

			/* Set user character table */
			ireg.eax.w[0] = 0x1121;
			ireg.ebx.b[0] = 0;
			ireg.ecx.b[0] = bytes; /* bytes/character */
			ireg.edx.b[0] = rows;

			__intcall(0x10, &ireg, &oreg);

			/* 8 pixels/character */
			VidCols = ((GXPixCols >> 3) - 1);

			/* No need to call adjust_screen */
			return;
		} else {
			ireg.eax.w[0] = 0x1110;	/* Load into VGA RAM */
			ireg.ebx.b[0] = 0;
			ireg.ebx.b[1] = bytes; /* bytes/character */
			ireg.ecx.w[0] = 256;
			ireg.edx.w[0] = 0;

			__intcall(0x10, &ireg, &oreg);

			ireg.ebx.b[0] = 0;
			ireg.eax.w[0] = 0x1103; /* Select page 0 */
			__intcall(0x10, &ireg, NULL);
		}
	}

	adjust_screen();
}

/*
 * adjust_screen: Set the internal variables associated with the screen size.
 *		This is a subroutine in case we're loading a custom font.
 */
void adjust_screen(void)
{
	com32sys_t ireg, oreg;
	volatile uint8_t *vidrows = (volatile uint8_t *)BIOS_vidrows;
	uint8_t rows, cols;

	rows = *vidrows;
	if (!rows) {
		/*
		 * No vidrows in BIOS, assume 25.
		 * (Remember: vidrows == rows-1)
		 */
		rows = 24;
	}

	VidRows = rows;

	ireg.eax.b[1] = 0x0f;	/* Read video state */
	__intcall(0x10, &ireg, &oreg);
	cols = oreg.eax.b[1];

	VidCols = --cols;	/* Store count-1 (same as rows) */
}

void pm_adjust_screen(com32sys_t *regs)
{
	adjust_screen();
}

void pm_userfont(com32sys_t *regs)
{
	regs->es = SEG(fontbuf);
	regs->ebx.w[0] = OFFS(fontbuf);
}
