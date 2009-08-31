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

#ifndef __COM32IO_H__
#define __COM32IO_H__

#include <com32.h>
#include <stdio.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#define CSI "\e["

static inline void beep(void)
{
	fputs("\007", stdout);
}

/* BIOS Assisted output routines */

// Print a C str (NUL-terminated) respecting the left edge of window
// i.e. \n in str will move cursor to column left
// Print a C str (NUL-terminated)

void csprint(const char *, const char);

//static inline void cswprint(const char *str, const char attr)
//{
//	csprint(str, attr);
//}

void cprint(const char, const char, unsigned int);

static inline void putch(const char x, char attr)
{
	cprint(x, attr, 1);
}

void clearwindow(const char, const char, const char, const char,
		 const char, const char);

/*
 * cls - clear and initialize the entire screen
 *
 * Note: when initializing xterm, one has to specify that
 * G1 points to the alternate character set (this is not true
 * by default). Without the initial printf "\033)0", line drawing
 * characters won't be displayed.
 */
static inline void cls(void)
{
	fputs("\033e\033%@\033)0\033(B\1#0\033[?25l\033[2J", stdout);
}

static inline void gotoxy(const char row, const char col)
{
	printf(CSI "%d;%dH", row + 1, col + 1);
}

void getpos(char *row, char *col, char page);

char inputc(char *scancode);	// Return ASCII char by val, and scancode by reference

void setcursorshape(char start, char end);	// Set cursor shape
void getcursorshape(char *start, char *end);	// Get shape for current page

// Get char displayed at current position in specified page
unsigned char getcharat(char page);

static inline void cursoroff(void)
{				/* Turns off cursor */
    setcursorshape(32, 33);
}

static inline void cursoron(void)
{				/* Turns on cursor */
    setcursorshape(6, 7);
}

static inline unsigned char readbiosb(unsigned int ofs)
{
    return *((unsigned char *)MK_PTR(0, ofs));
}

static inline char getnumrows()
{
    return readbiosb(0x484)+1; // Actually numrows - 1
}

static inline char getnumcols(void)
{
    return readbiosb(0x44a);	// Actually numcols
}

static inline char getshiftflags(void)
{
    return readbiosb(0x417);
}

void scrollupwindow(char top, char left, char bot, char right, char attr, char numlines);	//Scroll up given window

static inline void scrollup(void)	//Scroll up display screen by one line
{
	printf(CSI "S");
}

void setvideomode(char mode);	// Set the video mode.

static inline char getvideomode(void)	// Get the current video mode
{
    return readbiosb(0x449);
}

unsigned char sleep(unsigned int msec);	// Sleep for specified time

unsigned char checkkbdbuf();	// Check to see if there is kbd buffer is non-empty?

static inline void clearkbdbuf()	// Clear the kbd buffer (how many chars removed?)
{
    while (checkkbdbuf())
	inputc(NULL);
}

#endif
