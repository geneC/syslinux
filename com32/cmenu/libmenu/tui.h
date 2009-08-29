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

#ifndef __TUI_H__
#define __TUI_H__

#include <com32.h>
#include "syslnx.h"
#include "com32io.h"
#include "scancodes.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

#define SO '\016'
#define SI '\017'

#define TOP_LEFT_CORNER_BORDER '\154'
#define TOP_BORDER '\161'
#define TOP_RIGHT_CORNER_BORDER '\153'
#define BOTTOM_LEFT_CORNER_BORDER '\155'
#define BOTTOM_BORDER '\161'
#define BOTTOM_RIGHT_CORNER_BORDER '\152'
#define LEFT_BORDER '\170'
#define RIGHT_BORDER '\170'

#define BELL 0x07
// CHRELATTR = ^N, CHABSATTR = ^O
#define CHABSATTR 15
#define CHRELATTR 14

void clearwindow(char top, char left, char bot, char right,
		 char page, char fillchar, char fillattr);

/*
 * Clears the entire screen
 *
 * Note: when initializing xterm, one has to specify that
 * G1 points to the alternate character set (this is not true
 * by default). Without the initial printf "\033)0", line drawing
 * characters won't be displayed.
 */
static inline void cls(void)
{
	return fputs("\033e\033%@\033)0\033(B\1#0\033[?25l\033[2J", stdout);
}


// Generic user input,
// password = 0 iff chars echoed on screen
// showoldvalue <> 0 iff current displayed for editing
void getuserinput(char *str, unsigned int size,
		  unsigned int password, unsigned int showoldvalue);

static inline void getstring(char *str, unsigned int size)
{
    getuserinput(str, size, 0, 0);
}

static inline void editstring(char *str, unsigned int size)
{
    getuserinput(str, size, 0, 1);
}

static inline void getpwd(char *str, unsigned int size)
{
    getuserinput(str, size, 1, 0);
}

// Box drawing Chars offsets into array
#define BOX_TOPLEFT  0x0
#define BOX_BOTLEFT  0x1
#define BOX_TOPRIGHT 0x2
#define BOX_BOTRIGHT 0x3
#define BOX_TOP      0x4	// TOP = BOT = HORIZ
#define BOX_BOT      0x4
#define BOX_HORIZ    0x4
#define BOX_LEFT     0x5
#define BOX_RIGHT    0x5
#define BOX_VERT     0x5	// LEFT=RIGHT=VERT
#define BOX_LTRT     0x6
#define BOX_RTLT     0x7
#define BOX_TOPBOT   0x8
#define BOX_BOTTOP   0x9
#define BOX_MIDDLE   0xA

typedef enum { BOX_SINSIN, BOX_DBLDBL, BOX_SINDBL, BOX_DBLSIN } boxtype;

unsigned char *getboxchars(boxtype bt);

void drawbox(char top, char left, char bot, char right,
	     char page, char attr, boxtype bt);

// Draw a horizontal line
// dumb == 1, means just draw the line
// dumb == 0 means check the first and last positions and depending on what is
//    currently on the screen make it a LTRT and/or RTLT appropriately.
void drawhorizline(char top, char left, char right, char page, char attr,
		   boxtype bt, char dumb);

#endif
