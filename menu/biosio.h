/* -*- c -*- ------------------------------------------------------------- *
 *   
 *   Copyright 2004 Murali Krishnan Ganapathy - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/* This program can be compiled for DOS with the OpenWatcom compiler
 * (http://www.openwatcom.org/):
 *
 * wcl -3 -osx -mt <filename>.c
 */

#ifndef __BIOSIO_H__
#define __BIOSIO_H__

#ifndef NULL
#define NULL ((void *)0)
#endif

/* BIOS Assisted output routines */

void csprint(char *str); // Print a C str (NULL terminated)

void sprint(const char *str); // Print a $ terminated string

void cprint(char chr,char attr,int times,char disppage); // Print a char 

void setdisppage(char num); // Set the display page to specified number

char getdisppage(); // Get current display page 

void clearwindow(char top,char left,char bot,char right, char page,char fillchar, char fillattr);

void cls();

void gotoxy(char row,char col, char page);

void getpos(char * row, char * col, char page);

char inputc(char * scancode); // Return ASCII char by val, and scancode by reference

void cursoroff(void);		/* Turns on cursor */

void cursoron(void);		/* Turns off cursor */

void getstring(char *str, unsigned int size);

#endif
