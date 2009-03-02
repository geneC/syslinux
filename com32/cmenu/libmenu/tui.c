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

com32sys_t inreg,outreg; // Global register sets for use

char bkspstr[] = " \b$";
char eolstr[] = "\n$";
#define GETSTRATTR 0x07

// Reads a line of input from stdin. Replace CR with NUL byte
// password <> 0 implies not echoed on screen
// showoldvalue <> 0 implies currentvalue displayed first
// If showoldvalue <> 0 then caller responsibility to ensure that
// str is NULL terminated.
void getuserinput(char *stra, unsigned int size, unsigned int password,
  unsigned int showoldvalue)
{
    unsigned char c,scan;
    char *p,*q; // p = current char of string, q = tmp
    char *last; // The current last char of string
    char *str; // pointer to string which is going to be allocated
    char page;
    char row,col;
    char start,end; // Cursor shape
    char fudge; // How many chars should be removed from output
    char insmode; // Are we in insert or overwrite

    page = getdisppage();
    getpos(&row,&col,page); // Get current position
    getcursorshape(&start,&end);
    insmode = 1;

    str = (char *)malloc(size+1); // Allocate memory to store user input
    memset(str,0,size+1); // Zero it out
    if (password != 0) showoldvalue = 0; // Password's never displayed

    if (showoldvalue != 0) strcpy(str,stra); // If show old value copy current value

    last = str;
    while (*last) {last++;} // Find the terminating null byte
    p = str+ strlen(str);

    if (insmode == 0)
       setcursorshape(1,7); // Block cursor
    else setcursorshape(6,7); // Normal cursor

    // Invariants: p is the current char
    // col is the corresponding column on the screen
    if (password == 0) // Not a password, print initial value
    {
       gotoxy(row,col,page);
       csprint(str,GETSTRATTR);
    }
    while (1) { // Do forever
      c = inputc(&scan);
      if (c == '\r') break; // User hit Enter getout of loop
      if (scan == ESCAPE) // User hit escape getout and nullify string
      { *str = 0;
        break;
      }
      fudge = 0;
        // if scan code is regognized do something
        // else if char code is recognized do something
        // else ignore
      switch(scan) {
        case HOMEKEY:
             p = str;
             break;
        case ENDKEY:
             p = last;
             break;
        case LTARROW:
             if (p > str) p--;
             break;
        case CTRLLT:
             if (p==str) break;
             if (*p == ' ')
                while ((p > str) && (*p == ' ')) p--;
             else {
                if (*(p-1) == ' ') {
                   p--;
                   while ((p > str) && (*p == ' ')) p--;
                }
             }
             while ((p > str) && ((*p == ' ') || (*(p-1) != ' '))) p--;
             break;
        case RTARROW:
             if (p < last) p++;
             break;
        case CTRLRT:
             if (*p==0) break; // At end of string
             if (*p != ' ')
                while ((*p!=0) && (*p != ' ')) p++;
             while ((*p!=0) && ((*p == ' ') && (*(p+1) != ' '))) p++;
             if (*p==' ') p++;
             break;
        case DELETE:
             q = p;
             while (*(q+1)) {*q = *(q+1); q++; }
             if (last > str) last--;
             fudge = 1;
             break;
        case INSERT:
           insmode = 1-insmode; // Switch mode
           if (insmode == 0)
              setcursorshape(1,7); // Block cursor
           else setcursorshape(6,7); // Normal cursor
           break;

        default: // Unrecognized scan code, look at the ascii value
	  switch (c) {
	  case '\b': // Move over by one
              q=p;
	      while ( q <= last ) { *(q-1)=*q; q++;}
              if (last > str) last--;
              if (p > str) p--;
              fudge = 1;
	      break;
	  case '\x15':		/* Ctrl-U: kill input */
              fudge = last-str;
	      while ( p > str ) *p--=0;
              p = str; *p=0; last = str;
	      break;
	  default: // Handle insert and overwrite mode
	      if ((c >= ' ') && (c < 128) &&
                  ((unsigned int)(p-str) < size-1) ) {
                if (insmode == 0) { // Overwrite mode
                  if (p==last) last++;
                  *last = 0;
	          *p++ = c;
                } else {  // Insert mode
                  if (p==last) { // last char
                     last++;
                     *last=0;
                     *p++=c;
                  } else { // Non-last char
                     q=last++;
                     while (q >= p) { *q=*(q-1); q--;}
                     *p++=c;
                  }
                }
              }
              else beep();
	  }
	  break;
	}
        // Now the string has been modified, print it
        if (password == 0) {
          gotoxy(row,col,page);
          csprint(str,GETSTRATTR);
          if (fudge > 0) cprint(' ',GETSTRATTR,fudge,page);
          gotoxy(row,col+(p-str),page);
        }
      }
    *p = '\0';
    if (password == 0) csprint("\r\n",GETSTRATTR);
    setcursorshape(start,end); // Block cursor
    // If user hit ESCAPE so return without any changes
    if (scan != ESCAPE) strcpy(stra,str);
    free(str);
}

/* Print a C string (NUL-terminated) */
void cswprint(const char *str,char attr,char left)
{
    char page = getdisppage();
    char newattr=0,cha,chb;
    char row,col;
    char nr,nc;

    nr = getnumrows();
    nc = getnumcols();
    getpos(&row,&col,page);
    while ( *str ) {
      switch (*str)
	{
	case '\b':
	  --col;
	  break;
	case '\n':
	  ++row;
          col = left;
	  break;
	case '\r':
	  //col=left;
	  break;
	case BELL: // Bell Char
	  beep();
	  break;
	case CHRELATTR: // change attribute (relatively)
	case CHABSATTR: // change attribute (absolute)
	  cha = *(str+1);
	  chb = *(str+2);
	  if ((((cha >= '0') && (cha <= '9')) ||
	       ((cha >= 'A') && (cha <= 'F'))) &&
	      (((chb >= '0') && (chb <= '9')) ||
	       ((chb >= 'A') && (chb <= 'F')))) // Next two chars are legal
	    {
	      if ((cha >= 'A') && (cha <= 'F'))
		cha = cha - 'A'+10;
	      else cha = cha - '0';
	      if ((chb >= 'A') && (chb <= 'F'))
		chb = chb - 'A'+10;
	      else chb = chb - '0';
	      newattr = (cha << 4) + chb;
	      attr = (*str == CHABSATTR ? newattr : attr ^ newattr);
	      str += 2; // Will be incremented again later
	    }
	  break;
	default:
	  putch(*str, attr, page);
	  ++col;
	}
      if (col >= nc)
	{
	  ++row;
	  col=left;
	}
      if (row > nr)
	{
	  scrollup();
	  row= nr;
	}
      gotoxy(row,col,page);
      str++;
    }
}

void clearwindow(char top, char left, char bot, char right, char page, char fillchar, char fillattr)
{
    char x;
    for (x=top; x < bot+1; x++)
    {
        gotoxy(x,left,page);
        cprint(fillchar,fillattr,right-left+1,page);
    }
}

void cls(void)
{
  unsigned char dp = getdisppage();
  gotoxy(0,0,dp);
  cprint(' ',GETSTRATTR,(1+getnumrows())*getnumcols(),dp);
}

//////////////////////////////Box Stuff

// This order of numbers must match
// the values of BOX_TOPLEFT,... in the header file

unsigned char SINSIN_CHARS[] = {218,192,191,217, //Corners
                              196,179, // Horiz and Vertical
                              195,180,194,193,197}; // Connectors & Middle

unsigned char DBLDBL_CHARS[] = {201,200,187,188,  // Corners
                              205,186, // Horiz and Vertical
                              199,182,203,202,206}; // Connectors & Middle

unsigned char SINDBL_CHARS[] =  {214,211,183,189, // Corners
                                 196,186, // Horiz & Vert
                                 199,182,210,208,215}; // Connectors & Middle

unsigned char DBLSIN_CHARS[] = {213,212,184,190, // Corners
                                205,179, // Horiz & Vert
                                198,181,209,207,216}; // Connectors & Middle

unsigned char * getboxchars(boxtype bt)
{
   switch (bt)
   {
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
void drawbox(char top,char left,char bot, char right,
             char page, char attr,boxtype bt)
{
   unsigned char *box_chars; // pointer to array of box chars
   unsigned char x;

  box_chars = getboxchars(bt);
  // Top border
  gotoxy(top,left,page);
  cprint(box_chars[BOX_TOPLEFT],attr,1,page);
  gotoxy(top,left+1,page);
  cprint(box_chars[BOX_TOP],attr,right-left,page);
  gotoxy(top,right,page);
  cprint(box_chars[BOX_TOPRIGHT],attr,1,page);
  // Bottom border
  gotoxy(bot,left,page);
  cprint(box_chars[BOX_BOTLEFT],attr,1,page);
  gotoxy(bot,left+1,page);
  cprint(box_chars[BOX_BOT],attr,right-left,page);
  gotoxy(bot,right,page);
  cprint(box_chars[BOX_BOTRIGHT],attr,1,page);
  // Left & right borders
  for (x=top+1; x < bot; x++)
    {
      gotoxy(x,left,page);
      cprint(box_chars[BOX_LEFT],attr,1,page);
      gotoxy(x,right,page);
      cprint(box_chars[BOX_RIGHT],attr,1,page);
    }
}

void drawhorizline(char top, char left, char right, char page, char attr,
                   boxtype bt, char dumb)
{
  unsigned char start,end;
  unsigned char *box_chars = getboxchars(bt);
  if (dumb==0) {
    start = left+1;
    end = right-1;
  } else {
    start = left;
    end = right;
  }
  gotoxy(top,start,page);
  cprint(box_chars[BOX_HORIZ],attr,end-start+1,page);
  if (dumb == 0)
  {
    gotoxy(top,left,page);
    cprint(box_chars[BOX_LTRT],attr,1,page);
    gotoxy(top,right,page);
    cprint(box_chars[BOX_RTLT],attr,1,page);
  }
}
