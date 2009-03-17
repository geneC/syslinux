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

#include "help.h"
#include <stdio.h>
#include "string.h"
#include "com32io.h"
#include <syslinux/loadfile.h> // to read entire file into memory

char helpbasedir[HELPDIRLEN]; // name of help directory limited to HELPDIRLEN

// Find the occurence of the count'th \n in buffer (or NULL) if not found
char * findline(char*buffer,int count)
{
   int ctr;
   char *p= buffer-1;

   if (count < 1) return buffer;
   for (ctr=0; ctr < count; ctr++) {
       p = strchr(p+1,'\n');
       if (p==NULL) return NULL;
   }
   return p;
}

// return the number of lines in buffer
int countlines(char*buffer)
{
  int ans;
  const char *p;

  p = buffer-1;
  ans = 1;
  while(p) {p = strchr(p+1,'\n'); ans++; }
  return ans;
}


// Print numlines of text starting from buf
void printtext(char*buf, int from)
{
  char *p,*f;
  char right,bot,nlines;

  // clear window to print
  right = getnumcols() - HELP_RIGHT_MARGIN;
  bot = getnumrows() - HELP_BOTTOM_MARGIN;
  nlines = bot-HELP_BODY_ROW+1;
  scrollupwindow(HELP_BODY_ROW,HELP_LEFT_MARGIN,bot,right,0x07,nlines);

  f  = findline(buf,from);
  if (!f) return; // nothing to print
  if (*f=='\n') f++; // start of from+1st line
  p = findline(f,nlines);
  if (p && (*p=='\n')) *p = '\0'; // change to NUL
  gotoxy(HELP_BODY_ROW,HELP_LEFT_MARGIN,HELPPAGE);
  cswprint(f,0x07,HELP_LEFT_MARGIN);
  if (p) *p = '\n'; // set it back
}

void showhelp(const char *filename)
{
   char nc,nr,ph;
   char *title,*text;
   union { char *buffer; void *vbuf; } buf; // This is to avoild type-punning issues

   char line[512];
   size_t size;
   char scan;
   int rv,numlines,curr_line;

   nc = getnumcols();
   nr = getnumrows();
   ph = nr - HELP_BOTTOM_MARGIN - HELP_BODY_ROW - 1;
   cls();
   drawbox(0,0,nr,nc-1,HELPPAGE,0x07,HELPBOX);

   drawhorizline(2,0,nc-1,HELPPAGE,0x07,HELPBOX,0); // dumb==0
   if (filename == NULL) { // print file contents
     gotoxy(HELP_BODY_ROW,HELP_LEFT_MARGIN,HELPPAGE);
     cswprint("Filename not given",0x07,HELP_LEFT_MARGIN);
     while (1) {
       inputc(&scan);
       if (scan == ESCAPE) break;
     }
     cls();
     return;
   }

   rv = loadfile(filename,(void **)&buf.vbuf, &size); // load entire file into memory
   if (rv < 0) { // Error reading file or no such file
      sprintf(line, "Error reading file or file not found\n file=%s",filename);
      gotoxy(HELP_BODY_ROW,HELP_LEFT_MARGIN,HELPPAGE);
      cswprint(line,0x07,HELP_LEFT_MARGIN);
      while (1) {
        inputc(&scan);
        if (scan == ESCAPE) break;
      }
      cls();
      return;
   }

   title = buf.buffer;
   text = findline(title,1); // end of first line
   *text++='\0'; // end the title string and increment text

   // Now we have a file just print it.
   gotoxy(1,(nc-strlen(title))/2,HELPPAGE);
   csprint(title,0x07);
   numlines = countlines(text);
   curr_line = 0;
   scan = ESCAPE+1; // anything except ESCAPE

   while(scan != ESCAPE) {
       printtext(text,curr_line);
       gotoxy(HELP_BODY_ROW-1,nc-HELP_RIGHT_MARGIN,HELPPAGE);
       if (curr_line > 0)
          putch(HELP_MORE_ABOVE,0x07,HELPPAGE);
       else putch(' ',0x07,HELPPAGE);
       gotoxy(nr-HELP_BOTTOM_MARGIN+1,nc-HELP_RIGHT_MARGIN,HELPPAGE);
       if (curr_line < numlines - ph)
          putch(HELP_MORE_BELOW,0x07,HELPPAGE);
       else putch(' ',0x07,HELPPAGE);

       inputc(&scan); // wait for user keypress

       switch(scan) {
         case HOMEKEY:
             curr_line = 0;
             break;
         case ENDKEY:
             curr_line = numlines;
             break;
         case UPARROW:
             curr_line--;
             break;
         case DNARROW:
             curr_line++;
             break;
         case PAGEUP:
             curr_line -= ph;
             break;
         case PAGEDN:
             curr_line += ph;
             break;
         default:
           break;
       }
       if (curr_line > numlines - ph) curr_line = numlines-ph;
       if (curr_line < 0) curr_line = 0;
   }
   cls();
   return;
}

void runhelp(const char *filename)
{
   char dp;
   char fullname[HELPDIRLEN+16];

   dp = getdisppage();
   if (dp != HELPPAGE) setdisppage(HELPPAGE);
   cursoroff();
   if (helpbasedir[0] != 0) {
      strcpy(fullname,helpbasedir);
      strcat(fullname,"/");
      strcat(fullname,filename);
      showhelp(fullname);
   }
   else showhelp (filename); // Assume filename is absolute
   if (dp != HELPPAGE) setdisppage(dp);
}

void runhelpsystem(unsigned int helpid)
{
   char filename[15];

   sprintf(filename,"hlp%5d.txt",helpid);
   runhelp(filename);
}

void init_help(const char *helpdir)
{
   if (helpdir != NULL)
      strcpy(helpbasedir,helpdir);
   else helpbasedir[0] = 0;
}

void close_help(void)
{
}
