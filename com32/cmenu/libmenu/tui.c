/* -*- c -*- ------------------------------------------------------------- *
 *
 *   Copyright 2004-2006 Murali Krishnan Ganapathy - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include "tui.h"
#include <string.h>
#include <com32.h>
#include <stdlib.h>
#include "com32io.h"

com32sys_t inreg, outreg;	// Global register sets for use

char bkspstr[] = " \b$";
char eolstr[] = "\n$";

// Reads a line of input from stdin. Replace CR with NUL byte
// password <> 0 implies not echoed on screen
// showoldvalue <> 0 implies currentvalue displayed first
// If showoldvalue <> 0 then caller responsibility to ensure that
// str is NULL terminated.
void getuserinput(char *stra, unsigned int size, unsigned int password,
		  unsigned int showoldvalue)
{
    unsigned char c, scan;
    char *p, *q;		// p = current char of string, q = tmp
    char *last;			// The current last char of string
    char *str;			// pointer to string which is going to be allocated
    char row, col;
    char start, end;		// Cursor shape
    char fudge;			// How many chars should be removed from output
    char insmode;		// Are we in insert or overwrite

    getpos(&row, &col, 0);	// Get current position
    getcursorshape(&start, &end);
    insmode = 1;

    str = (char *)malloc(size + 1);	// Allocate memory to store user input
    memset(str, 0, size + 1);	// Zero it out
    if (password != 0)
	showoldvalue = 0;	// Password's never displayed

    if (showoldvalue != 0)
	strcpy(str, stra);	// If show old value copy current value

    last = str;
    while (*last) {
	last++;
    }				// Find the terminating null byte
    p = str + strlen(str);

    if (insmode == 0)
	setcursorshape(1, 7);	// Block cursor
    else
	setcursorshape(6, 7);	// Normal cursor

    // Invariants: p is the current char
    // col is the corresponding column on the screen
    if (password == 0)		// Not a password, print initial value
    {
	gotoxy(row, col);
	csprint(str, GETSTRATTR);
    }
    while (1) {			// Do forever
	c = inputc(&scan);
	if (c == '\r')
	    break;		// User hit Enter getout of loop
	if (scan == ESCAPE)	// User hit escape getout and nullify string
	{
	    *str = 0;
	    break;
	}
	fudge = 0;
	// if scan code is regognized do something
	// else if char code is recognized do something
	// else ignore
	switch (scan) {
	case HOMEKEY:
	    p = str;
	    break;
	case ENDKEY:
	    p = last;
	    break;
	case LTARROW:
	    if (p > str)
		p--;
	    break;
	case CTRLLT:
	    if (p == str)
		break;
	    if (*p == ' ')
		while ((p > str) && (*p == ' '))
		    p--;
	    else {
		if (*(p - 1) == ' ') {
		    p--;
		    while ((p > str) && (*p == ' '))
			p--;
		}
	    }
	    while ((p > str) && ((*p == ' ') || (*(p - 1) != ' ')))
		p--;
	    break;
	case RTARROW:
	    if (p < last)
		p++;
	    break;
	case CTRLRT:
	    if (*p == 0)
		break;		// At end of string
	    if (*p != ' ')
		while ((*p != 0) && (*p != ' '))
		    p++;
	    while ((*p != 0) && ((*p == ' ') && (*(p + 1) != ' ')))
		p++;
	    if (*p == ' ')
		p++;
	    break;
	case DELETE:
	    q = p;
	    while (*(q + 1)) {
		*q = *(q + 1);
		q++;
	    }
	    if (last > str)
		last--;
	    fudge = 1;
	    break;
	case INSERT:
	    insmode = 1 - insmode;	// Switch mode
	    if (insmode == 0)
		setcursorshape(1, 7);	// Block cursor
	    else
		setcursorshape(6, 7);	// Normal cursor
	    break;

	default:		// Unrecognized scan code, look at the ascii value
	    switch (c) {
	    case '\b':		// Move over by one
		q = p;
		while (q <= last) {
		    *(q - 1) = *q;
		    q++;
		}
		if (last > str)
		    last--;
		if (p > str)
		    p--;
		fudge = 1;
		break;
	    case '\x15':	/* Ctrl-U: kill input */
		fudge = last - str;
		while (p > str)
		    *p-- = 0;
		p = str;
		*p = 0;
		last = str;
		break;
	    default:		// Handle insert and overwrite mode
		if ((c >= ' ') && (c < 128) &&
		    ((unsigned int)(p - str) < size - 1)) {
		    if (insmode == 0) {	// Overwrite mode
			if (p == last)
			    last++;
			*last = 0;
			*p++ = c;
		    } else {	// Insert mode
			if (p == last) {	// last char
			    last++;
			    *last = 0;
			    *p++ = c;
			} else {	// Non-last char
			    q = last++;
			    while (q >= p) {
				*q = *(q - 1);
				q--;
			    }
			    *p++ = c;
			}
		    }
		} else
		    beep();
	    }
	    break;
	}
	// Now the string has been modified, print it
	if (password == 0) {
	    gotoxy(row, col);
	    csprint(str, GETSTRATTR);
	    if (fudge > 0)
		cprint(' ', GETSTRATTR, fudge);
	    gotoxy(row, col + (p - str));
	}
    }
    *p = '\0';
    if (password == 0)
	csprint("\r\n", GETSTRATTR);
    setcursorshape(start, end);	// Block cursor
    // If user hit ESCAPE so return without any changes
    if (scan != ESCAPE)
	strcpy(stra, str);
    free(str);
}

//////////////////////////////Box Stuff

// This order of numbers must match
// the values of BOX_TOPLEFT,... in the header file

unsigned char SINSIN_CHARS[] = { 218, 192, 191, 217,	//Corners
    196, 179,			// Horiz and Vertical
    195, 180, 194, 193, 197
};				// Connectors & Middle

unsigned char DBLDBL_CHARS[] = { 201, 200, 187, 188,	// Corners
    205, 186,			// Horiz and Vertical
    199, 182, 203, 202, 206
};				// Connectors & Middle

unsigned char SINDBL_CHARS[] = { 214, 211, 183, 189,	// Corners
    196, 186,			// Horiz & Vert
    199, 182, 210, 208, 215
};				// Connectors & Middle

unsigned char DBLSIN_CHARS[] = { 213, 212, 184, 190,	// Corners
    205, 179,			// Horiz & Vert
    198, 181, 209, 207, 216
};				// Connectors & Middle

unsigned char *getboxchars(boxtype bt)
{
    switch (bt) {
    case BOX_SINSIN:
	return SINSIN_CHARS;
	break;
    case BOX_DBLDBL:
	return DBLDBL_CHARS;
	break;
    case BOX_SINDBL:
	return SINDBL_CHARS;
	break;
    case BOX_DBLSIN:
	return DBLSIN_CHARS;
	break;
    default:
	return SINSIN_CHARS;
	break;
    }
    return SINSIN_CHARS;
}

// Draw box and lines
void drawbox(const char top, const char left, const char bot,
	     const char right, const char attr)
{
    unsigned char x;
	putchar(SO);
    // Top border
    gotoxy(top, left);
    putch(TOP_LEFT_CORNER_BORDER, attr);
    cprint(TOP_BORDER, attr, right - left - 1);
    putch(TOP_RIGHT_CORNER_BORDER, attr);
    // Bottom border
    gotoxy(bot, left);
    putch(BOTTOM_LEFT_CORNER_BORDER, attr);
    cprint(BOTTOM_BORDER, attr, right - left - 1);
    putch(BOTTOM_RIGHT_CORNER_BORDER, attr);
    // Left & right borders
    for (x = top + 1; x < bot; x++) {
	gotoxy(x, left);
	putch(LEFT_BORDER, attr);
	gotoxy(x, right);
	putch(RIGHT_BORDER, attr);
    }
	putchar(SI);
}

void drawhorizline(const char top, const char left, const char right,
		   const char attr, char dumb)
{
    unsigned char start, end;
    if (dumb == 0) {
	start = left + 1;
	end = right - 1;
    } else {
	start = left;
	end = right;
    }
    gotoxy(top, start);
	putchar(SO);
    cprint(MIDDLE_BORDER, attr, end - start + 1);
    if (dumb == 0) {
	gotoxy(top, left);
	putch(MIDDLE_BORDER, attr);
	gotoxy(top, right);
	putch(MIDDLE_BORDER, attr);
    }
	putchar(SI);
}
