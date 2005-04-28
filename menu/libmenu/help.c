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

char helpbasedir[HELPDIRLEN]; // name of help directory limited to HELPDIRLEN

void showhelp(const char *filename)
{
   char nc,nr;
   FILE *f;
   char line[512]; // Max length of a line

   nc = getnumcols();
   nr = getnumrows();
   cls();
   drawbox(0,0,nr,nc-1,HELPPAGE,0x07,HELPBOX);

   drawhorizline(2,0,nc-1,HELPPAGE,0x07,HELPBOX,0); // dumb==0
   if (filename == NULL) { // print file contents
     gotoxy(HELP_BODY_ROW,HELP_LEFT_MARGIN,HELPPAGE);
     cswprint("Help system not initialized",0x07,HELP_LEFT_MARGIN);
     return;
   }
   
   f = fopen(filename,"r");
   if (!f) { // No such file
      sprintf(line, "File %s not found",filename);
      gotoxy(HELP_BODY_ROW,HELP_LEFT_MARGIN,HELPPAGE);
      cswprint(line,0x07,HELP_LEFT_MARGIN);
      return;
   }

   // Now we have a file just print it.
   fgets(line,sizeof line,f); // Get first line (TITLE)
   gotoxy(1,(nc-strlen(line))/2,HELPPAGE);
   csprint(line,0x07);

   gotoxy(HELP_BODY_ROW,HELP_LEFT_MARGIN,HELPPAGE);
   while ( fgets(line, sizeof line, f) ) cswprint(line,0x07,HELP_LEFT_MARGIN);
   fclose(f);
}

void runhelpsystem(unsigned int helpid)
{
   char dp;
   char scan;
   char filename[HELPDIRLEN+16];

   dp = getdisppage();
   if (dp != HELPPAGE) setdisppage(HELPPAGE);
   if (helpbasedir[0] != 0) {
      sprintf(filename,"%s/hlp%05d.txt",helpbasedir,helpid);
      showhelp(filename);
   }
   else showhelp (NULL);
   while (1) {
     inputc(&scan);
     if (scan == ESCAPE) break;
   }
   if (dp != HELPPAGE) setdisppage(dp);
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
