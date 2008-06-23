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

#include "menu.h"
#include "com32io.h"
#include <stdlib.h>

// Local Variables
static pt_menusystem ms; // Pointer to the menusystem
char TITLESTR[] = "COMBOOT Menu System for SYSLINUX developed by Murali Krishnan Ganapathy";
char TITLELONG[] = " TITLE too long ";
char ITEMLONG[] = " ITEM too long ";
char ACTIONLONG[] = " ACTION too long ";
char STATUSLONG[] = " STATUS too long ";
char EMPTYSTR[] = "";

/* Forward declarations */
int calc_visible(pt_menu menu,int first);
int next_visible(pt_menu menu,int index);
int prev_visible(pt_menu menu,int index);
int next_visible_sep(pt_menu menu,int index);
int prev_visible_sep(pt_menu menu,int index);
int calc_first_early(pt_menu menu,int curr);
int calc_first_late(pt_menu menu,int curr);
int isvisible(pt_menu menu,int first, int curr);


/* Basic Menu routines */

// This is same as inputc except it honors the ontimeout handler
// and calls it when needed. For the callee, there is no difference
// as this will not return unless a key has been pressed.
char getch(char *scan)
{
  unsigned long i;
  TIMEOUTCODE c;
  t_timeout_handler th;

  // Wait until keypress if no handler specified
  if ((ms->ontimeout==NULL) && (ms->ontotaltimeout==NULL)) return inputc(scan);

  th = ms->ontimeout;
  while (1) // Forever do
    {
      for (i=0; i < ms->tm_numsteps; i++)
	{
	  if (checkkbdbuf()) return inputc(scan);
	  sleep(ms->tm_stepsize);
          if ( (ms->tm_total_timeout == 0) || (ms->ontotaltimeout==NULL))
              continue; // Dont bother with calculations if no handler
          ms->tm_sofar_timeout += ms->tm_stepsize;
          if (ms->tm_sofar_timeout >= ms->tm_total_timeout) {
             th = ms->ontotaltimeout;
             ms->tm_sofar_timeout = 0;
             break; // Get out of the for loop
          }
	}
      if (!th) continue; // no handler dont call
      c = th();
      switch(c)
	{
	case CODE_ENTER: // Pretend user hit enter
	  *scan = ENTERA;
	  return '\015'; // \015 octal = 13
	case CODE_ESCAPE: // Pretend user hit escape
	  *scan = ESCAPE;
	  return '\033'; // \033 octal = 27
	default:
	  break;
	}
    }
  return 0;
}

/* Print a menu item */
/* attr[0] is non-hilite attr, attr[1] is highlight attr */
void printmenuitem(const char *str,uchar* attr)
{
    uchar page = getdisppage();
    uchar row,col;
    int hlite=NOHLITE; // Initially no highlighting

    getpos(&row,&col,page);
    while ( *str ) {
      switch (*str)
	{
	case '\b':
	  --col;
	  break;
	case '\n':
	  ++row;
	  break;
	case '\r':
	  col=0;
	  break;
	case BELL: // No Bell Char
	  break;
	case ENABLEHLITE: // Switch on highlighting
	  hlite = HLITE;
	  break;
	case DISABLEHLITE: // Turn off highlighting
	  hlite = NOHLITE;
	  break;
	default:
	  putch(*str, attr[hlite], page);
	  ++col;
	}
      if (col > getnumcols())
	{
	  ++row;
	  col=0;
	}
      if (row > getnumrows())
	{
	  scrollup();
	  row= getnumrows();
	}
      gotoxy(row,col,page);
      str++;
    }
}

int find_shortcut(pt_menu menu,uchar shortcut, int index)
// Find the next index with specified shortcut key
{
  int ans;
  pt_menuitem mi;

  // Garbage in garbage out
  if ((index <0) || (index >= menu->numitems)) return index;
  ans = index+1;
  // Go till end of menu
  while (ans < menu->numitems)
    {
      mi = menu->items[ans];
      if ((mi->action == OPT_INVISIBLE) || (mi->action == OPT_SEP)
	  || (mi->shortcut != shortcut))
	ans ++;
      else return ans;
    }
  // Start at the beginning and try again
  ans = 0;
  while (ans < index)
    {
      mi = menu->items[ans];
      if ((mi->action == OPT_INVISIBLE) || (mi->action == OPT_SEP)
	  || (mi->shortcut != shortcut))
	ans ++;
      else return ans;
    }
  return index; // Sorry not found
}

// print the menu starting from FIRST
// will print a maximum of menu->menuheight items
void printmenu(pt_menu menu, int curr, uchar top, uchar left, uchar first)
{
  int x,row; // x = index, row = position from top
  int numitems,menuwidth;
  char fchar[5],lchar[5]; // The first and last char in for each entry
  const char *str;  // and inbetween the item or a seperator is printed
  uchar *attr;  // attribute attr
  char sep[MENULEN];// and inbetween the item or a seperator is printed
  pt_menuitem ci;

  numitems = calc_visible(menu,first);
  if (numitems > menu->menuheight) numitems = menu->menuheight;

  menuwidth = menu->menuwidth+3;
  clearwindow(top,left-2, top+numitems+1, left+menuwidth+1,
	      ms->menupage, ms->fillchar, ms->shadowattr);
  drawbox(top-1,left-3,top+numitems,left+menuwidth,
          ms->menupage,ms->normalattr[NOHLITE],ms->menubt);
  memset(sep,ms->box_horiz,menuwidth); // String containing the seperator string
  sep[menuwidth-1] = 0;
  // Menu title
  x = (menuwidth - strlen(menu->title) - 1) >> 1;
  gotoxy(top-1,left+x,ms->menupage);
  printmenuitem(menu->title,ms->normalattr);
  row = -1; // 1 less than inital value of x
  for (x=first; x < menu->numitems; x++)
    {
      ci = menu->items[x];
      if (ci->action == OPT_INVISIBLE) continue;
      row++;
      if (row >= numitems) break; // Already have enough number of items
      // Setup the defaults now
      lchar[0] = fchar[0] = ' ';
      lchar[1] = fchar[1] = '\0'; // fchar and lchar are just spaces
      str = ci->item; // Pointer to item string
      attr = (x==curr ? ms->reverseattr : ms->normalattr); // Normal attributes
      switch (ci->action) // set up attr,str,fchar,lchar for everything
        {
	case OPT_INACTIVE:
	  attr = (x==curr? ms->revinactattr : ms->inactattr);
	  break;
	case OPT_SUBMENU:
	  lchar[0] = SUBMENUCHAR; lchar[1] = 0;
	  break;
	case OPT_RADIOMENU:
	  lchar[0] = RADIOMENUCHAR; lchar[1] = 0;
	  break;
	case OPT_CHECKBOX:
	  lchar[0] = (ci->itemdata.checked ? CHECKED : UNCHECKED);
	  lchar[1] = 0;
	  break;
	case OPT_SEP:
	  fchar[0] = '\b'; fchar[1] = ms->box_ltrt; fchar[2] = ms->box_horiz; fchar[3] = ms->box_horiz; fchar[4] = 0;
	  lchar[0] = ms->box_horiz; lchar[1] = ms->box_rtlt; lchar[2] = 0;
	  str = sep;
	  break;
	case OPT_EXITMENU:
	  fchar[0] = EXITMENUCHAR; fchar[1] = 0;
	  break;
	default: // Just to keep the compiler happy
	  break;
        }
      gotoxy(top+row,left-2,ms->menupage);
      cprint(ms->spacechar,attr[NOHLITE],menuwidth+2,ms->menupage); // Wipe area with spaces
      gotoxy(top+row,left-2,ms->menupage);
      csprint(fchar,attr[NOHLITE]); // Print first part
      gotoxy(top+row,left,ms->menupage);
      printmenuitem(str,attr); // Print main part
      gotoxy(top+row,left+menuwidth-1,ms->menupage); // Last char if any
      csprint(lchar,attr[NOHLITE]); // Print last part
    }
  // Check if we need to MOREABOVE and MOREBELOW to be added
  // reuse x
  row = 0;
  x = next_visible_sep(menu,0); // First item
  if (! isvisible(menu,first,x)) // There is more above
  {
     row = 1;
     gotoxy(top,left+menuwidth,ms->menupage);
     cprint(MOREABOVE,ms->normalattr[NOHLITE],1,ms->menupage);
  }
  x = prev_visible_sep(menu,menu->numitems); // last item
  if (! isvisible(menu,first,x)) // There is more above
  {
     row = 1;
     gotoxy(top+numitems-1,left+menuwidth,ms->menupage);
     cprint(MOREBELOW,ms->normalattr[NOHLITE],1,ms->menupage);
  }
  // Add a scroll box
  x = ((numitems-1)*curr)/(menu->numitems);
  if ((x>0) && (row==1)) {
  gotoxy(top+x,left+menuwidth,ms->menupage);
  cprint(SCROLLBOX,ms->normalattr[NOHLITE],1,ms->menupage);
  }
  if (ms->handler) ms->handler(ms,menu->items[curr]);
}

// Difference between this and regular menu, is that only
// OPT_INVISIBLE, OPT_SEP are honoured
void printradiomenu(pt_menu menu, int curr, uchar top, uchar left, int first)
{
  int x,row; // x = index, row = position from top
  int numitems,menuwidth;
  char fchar[5],lchar[5]; // The first and last char in for each entry
  const char *str;  // and inbetween the item or a seperator is printed
  uchar *attr;  // all in the attribute attr
  char sep[MENULEN];// and inbetween the item or a seperator is printed
  pt_menuitem ci;

  numitems = calc_visible(menu,first);
  if (numitems > menu->menuheight) numitems = menu->menuheight;

  menuwidth = menu->menuwidth+3;
  clearwindow(top,left-2, top+numitems+1, left+menuwidth+1,
	      ms->menupage, ms->fillchar, ms->shadowattr);
  drawbox(top-1,left-3,top+numitems,left+menuwidth,
          ms->menupage,ms->normalattr[NOHLITE],ms->menubt);
  memset(sep,ms->box_horiz,menuwidth); // String containing the seperator string
  sep[menuwidth-1] = 0;
  // Menu title
  x = (menuwidth - strlen(menu->title) - 1) >> 1;
  gotoxy(top-1,left+x,ms->menupage);
  printmenuitem(menu->title,ms->normalattr);
  row = -1; // 1 less than inital value of x
  for (x=first; x < menu->numitems; x++)
    {
      ci = menu->items[x];
      if (ci->action == OPT_INVISIBLE) continue;
      row++;
      if (row > numitems) break;
      // Setup the defaults now
      fchar[0] = RADIOUNSEL; fchar[1]='\0'; // Unselected ( )
      lchar[0] = '\0'; // Nothing special after
      str = ci->item; // Pointer to item string
      attr = ms->normalattr; // Always same attribute
      fchar[0] = (x==curr ? RADIOSEL : RADIOUNSEL);
      switch (ci->action) // set up attr,str,fchar,lchar for everything
        {
	case OPT_INACTIVE:
	  attr = ms->inactattr;
	  break;
	case OPT_SEP:
	  fchar[0] = '\b'; fchar[1] = ms->box_ltrt; fchar[2] = ms->box_horiz; fchar[3] = ms->box_horiz; fchar[4] = 0;
	  lchar[0] = ms->box_horiz; lchar[1] = ms->box_rtlt; lchar[3] = 0;
	  str = sep;
	  break;
	default: // To keep the compiler happy
	  break;
        }
      gotoxy(top+row,left-2,ms->menupage);
      cprint(ms->spacechar,attr[NOHLITE],menuwidth+2,ms->menupage); // Wipe area with spaces
      gotoxy(top+row,left-2,ms->menupage);
      csprint(fchar,attr[NOHLITE]); // Print first part
      gotoxy(top+row,left,ms->menupage);
      printmenuitem(str,attr); // Print main part
      gotoxy(top+row,left+menuwidth-1,ms->menupage); // Last char if any
      csprint(lchar,attr[NOHLITE]); // Print last part
    }
  // Check if we need to MOREABOVE and MOREBELOW to be added
  // reuse x
  row = 0;
  x = next_visible_sep(menu,0); // First item
  if (! isvisible(menu,first,x)) // There is more above
  {
     row = 1;
     gotoxy(top,left+menuwidth,ms->menupage);
     cprint(MOREABOVE,ms->normalattr[NOHLITE],1,ms->menupage);
  }
  x = prev_visible_sep(menu,menu->numitems); // last item
  if (! isvisible(menu,first,x)) // There is more above
  {
     row = 1;
     gotoxy(top+numitems-1,left+menuwidth,ms->menupage);
     cprint(MOREBELOW,ms->normalattr[NOHLITE],1,ms->menupage);
  }
  // Add a scroll box
  x = ((numitems-1)*curr)/(menu->numitems);
  if ((x > 0) && (row == 1))
  {
     gotoxy(top+x,left+menuwidth,ms->menupage);
     cprint(SCROLLBOX,ms->normalattr[NOHLITE],1,ms->menupage);
  }
  if (ms->handler) ms->handler(ms,menu->items[curr]);
}

void cleanupmenu(pt_menu menu, uchar top,uchar left,int numitems)
{
  if (numitems > menu->menuheight) numitems = menu->menuheight;
  clearwindow(top,left-2, top+numitems+1, left+menu->menuwidth+4,
	      ms->menupage, ms->fillchar, ms->fillattr); // Clear the shadow
  clearwindow(top-1, left-3, top+numitems, left+menu->menuwidth+3,
	      ms->menupage, ms->fillchar, ms->fillattr); // main window
}

/* Handle a radio menu */
pt_menuitem getradiooption(pt_menu menu, uchar top, uchar left, uchar startopt)
     // Return item chosen or NULL if ESC was hit.
{
  int curr,i,first,tmp;
  uchar asc,scan;
  uchar numitems;
  pt_menuitem ci; // Current item

  numitems = calc_visible(menu,0);
  // Setup status line
  gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
  cprint(ms->spacechar,ms->statusattr[NOHLITE],ms->numcols,ms->menupage);

  // Initialise current menu item
  curr = next_visible(menu,startopt);

  gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
  cprint(ms->spacechar,ms->statusattr[NOHLITE],ms->numcols,1);
  gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
  printmenuitem(menu->items[curr]->status,ms->statusattr);
  first = calc_first_early(menu,curr);
  while (1) // Forever
    {
      printradiomenu(menu,curr,top,left,first);
      ci = menu->items[curr];

      asc = getch(&scan);
      switch (scan)
        {
	case HOMEKEY:
	  curr = next_visible(menu,0);
          first = calc_first_early(menu,curr);
	  break;
	case ENDKEY:
	  curr = prev_visible(menu,numitems-1);
          first = calc_first_late(menu,curr);
	  break;
	case PAGEDN:
	  for (i=0; i < 5; i++) curr = next_visible(menu,curr+1);
          first = calc_first_late(menu,curr);
	  break;
	case PAGEUP:
	  for (i=0; i < 5; i++) curr = prev_visible(menu,curr-1);
          first = calc_first_early(menu,curr);
	  break;
	case UPARROW:
	  curr = prev_visible(menu,curr-1);
          if (curr < first) first = calc_first_early(menu,curr);
	  break;
	case DNARROW:
	  curr = next_visible(menu,curr+1);
          if (! isvisible(menu,first,curr))
               first = calc_first_late(menu,curr);
	  break;
	case LTARROW:
	case ESCAPE:
	  return NULL;
	  break;
	case RTARROW:
	case ENTERA:
	case ENTERB:
	  if (ci->action == OPT_INACTIVE) break;
	  if (ci->action == OPT_SEP) break;
	  return ci;
	  break;
	default:
	  // Check if this is a shortcut key
	  if (((asc >= 'A') && (asc <= 'Z')) ||
	      ((asc >= 'a') && (asc <= 'z')) ||
	      ((asc >= '0') && (asc <= '9')))
          {
	    tmp = find_shortcut(menu,asc,curr);
            if ((tmp > curr) && (! isvisible(menu,first,tmp)))
                  first = calc_first_late(menu,tmp);
            if (tmp < curr)
               first = calc_first_early(menu,tmp);
            curr = tmp;
          }
          else {
            if (ms->keys_handler) // Call extra keys handler
               ms->keys_handler(ms,menu->items[curr],(scan << 8) | asc);
          }
	  break;
        }
      // Update status line
      gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
      cprint(ms->spacechar,ms->statusattr[NOHLITE],ms->numcols,ms->menupage);
      printmenuitem(menu->items[curr]->status,ms->statusattr);
    }
  return NULL; // Should never come here
}

/* Handle one menu */
pt_menuitem getmenuoption(pt_menu menu, uchar top, uchar left, uchar startopt)
     // Return item chosen or NULL if ESC was hit.
{
  int curr,i,first,tmp;
  uchar asc,scan;
  uchar numitems;
  pt_menuitem ci; // Current item
  t_handler_return hr; // Return value of handler

  numitems = calc_visible(menu,0);
  // Setup status line
  gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
  cprint(ms->spacechar,ms->statusattr[NOHLITE],ms->numcols,ms->menupage);

  // Initialise current menu item
  curr = next_visible(menu,startopt);

  gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
  cprint(ms->spacechar,ms->statusattr[NOHLITE],ms->numcols,1);
  gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
  printmenuitem(menu->items[curr]->status,ms->statusattr);
  first = calc_first_early(menu,curr);
  while (1) // Forever
    {
      printmenu(menu,curr,top,left,first);
      ci = menu->items[curr];
      asc = getch(&scan);
      switch (scan)
        {
	case HOMEKEY:
	  curr = next_visible(menu,0);
          first = calc_first_early(menu,curr);
	  break;
	case ENDKEY:
	  curr = prev_visible(menu,numitems-1);
          first = calc_first_late(menu,curr);
	  break;
	case PAGEDN:
	  for (i=0; i < 5; i++) curr = next_visible(menu,curr+1);
          first = calc_first_late(menu,curr);
	  break;
	case PAGEUP:
	  for (i=0; i < 5; i++) curr = prev_visible(menu,curr-1);
          first = calc_first_early(menu,curr);
	  break;
	case UPARROW:
	  curr = prev_visible(menu,curr-1);
          if (curr < first) first = calc_first_early(menu,curr);
	  break;
	case DNARROW:
	  curr = next_visible(menu,curr+1);
          if (! isvisible(menu,first,curr))
               first = calc_first_late(menu,curr);
	  break;
	case LTARROW:
	case ESCAPE:
	  return NULL;
	  break;
	case RTARROW:
	case ENTERA:
	case ENTERB:
	  if (ci->action == OPT_INACTIVE) break;
	  if (ci->action == OPT_CHECKBOX) break;
	  if (ci->action == OPT_SEP) break;
	  if (ci->action == OPT_EXITMENU) return NULL; // As if we hit Esc
          // If we are going into a radio menu, dont call handler, return ci
          if (ci->action == OPT_RADIOMENU) return ci;
          if (ci->handler != NULL) // Do we have a handler
          {
             hr = ci->handler(ms,ci);
             if (hr.refresh) // Do we need to refresh
             {
                // Cleanup menu using old number of items
                cleanupmenu(menu,top,left,numitems);
                // Recalculate the number of items
                numitems = calc_visible(menu,0);
                // Reprint the menu
                printmenu(menu,curr,top,left,first);
             }
             if (hr.valid) return ci;
          }
          else return ci;
	  break;
	case SPACEKEY:
          if (ci->action != OPT_CHECKBOX) break;
          ci->itemdata.checked = !ci->itemdata.checked;
          if (ci->handler != NULL) // Do we have a handler
          {
             hr = ci->handler(ms,ci);
             if (hr.refresh) // Do we need to refresh
             {
                // Cleanup menu using old number of items
                cleanupmenu(menu,top,left,numitems);
                // Recalculate the number of items
                numitems = calc_visible(menu,0);
                // Reprint the menu
                printmenu(menu,curr,top,left,first);
             }
          }
          break;
	default:
	  // Check if this is a shortcut key
	  if (((asc >= 'A') && (asc <= 'Z')) ||
	      ((asc >= 'a') && (asc <= 'z')) ||
	      ((asc >= '0') && (asc <= '9')))
          {
	    tmp = find_shortcut(menu,asc,curr);
            if ((tmp > curr) && (! isvisible(menu,first,tmp)))
                  first = calc_first_late(menu,tmp);
            if (tmp < curr)
               first = calc_first_early(menu,tmp);
            curr = tmp;
          }
          else {
            if (ms->keys_handler) // Call extra keys handler
               ms->keys_handler(ms,menu->items[curr],(scan << 8) | asc);
          }
	  break;
        }
      // Update status line
      gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
      cprint(ms->spacechar,ms->statusattr[NOHLITE],ms->numcols,ms->menupage);
      printmenuitem(menu->items[curr]->status,ms->statusattr);
    }
  return NULL; // Should never come here
}

/* Handle the entire system of menu's. */
pt_menuitem runmenusystem(uchar top, uchar left, pt_menu cmenu, uchar startopt, uchar menutype)
     /*
      * cmenu
      *    Which menu should be currently displayed
      * top,left
      *    What is the position of the top,left corner of the menu
      * startopt
      *    which menu item do I start with
      * menutype
      *    NORMALMENU or RADIOMENU
      *
      * Return Value:
      *    Returns a pointer to the final item chosen, or NULL if nothing chosen.
      */
{
  pt_menuitem opt,choice;
  uchar startat,mt;
  uchar row,col;

  if (cmenu == NULL) return NULL;
 startover:
  // Set the menu height
  cmenu->menuheight = ms->maxrow - top-3;
  if (cmenu->menuheight > ms->maxmenuheight)
     cmenu->menuheight = ms->maxmenuheight;
  if (menutype == NORMALMENU)
    opt = getmenuoption(cmenu,top,left,startopt);
  else // menutype == RADIOMENU
    opt = getradiooption(cmenu,top,left,startopt);

  if (opt == NULL)
    {
      // User hit Esc
      cleanupmenu(cmenu,top,left,calc_visible(cmenu,0));
      return NULL;
    }
  // Are we done with the menu system?
  if ((opt->action != OPT_SUBMENU) && (opt->action != OPT_RADIOMENU))
    {
      cleanupmenu(cmenu,top,left,calc_visible(cmenu,0));
      return opt; // parent cleanup other menus
    }
  // Either radiomenu or submenu
  // Do we have a valid menu number? The next hack uses the fact that
  // itemdata.submenunum = itemdata.radiomenunum (since enum data type)
  if (opt->itemdata.submenunum >= ms->nummenus) // This is Bad....
    {
      gotoxy(12,12,ms->menupage); // Middle of screen
      csprint("ERROR: Invalid submenu requested.",0x07);
      cleanupmenu(cmenu,top,left,calc_visible(cmenu,0));
      return NULL; // Pretend user hit esc
    }
  // Call recursively for submenu
  // Position the submenu below the current item,
  // covering half the current window (horizontally)
  row = ms->menus[(unsigned int)opt->itemdata.submenunum]->row;
  col = ms->menus[(unsigned int)opt->itemdata.submenunum]->col;
  if (row == 0xFF) row = top+opt->index+2;
  if (col == 0xFF) col = left+3+(cmenu->menuwidth >> 1);
  mt = (opt->action == OPT_SUBMENU ? NORMALMENU : RADIOMENU );
  startat = 0;
  if ((opt->action == OPT_RADIOMENU) && (opt->data != NULL))
    startat = ((t_menuitem *)opt->data)->index;

  choice = runmenusystem(row, col,
			 ms->menus[(unsigned int)opt->itemdata.submenunum],
			 startat, mt );
  if (opt->action == OPT_RADIOMENU)
    {
      if (choice != NULL) opt->data = (void *)choice; // store choice in data field
      if (opt->handler != NULL) opt->handler(ms,opt);
      choice = NULL; // Pretend user hit esc
    }
  if (choice==NULL) // User hit Esc in submenu
    {
      // Startover
      startopt = opt->index;
      goto startover;
    }
  else
    {
      cleanupmenu(cmenu,top,left,calc_visible(cmenu,0));
      return choice;
    }
}

// Finds the indexof the menu with given name
uchar find_menu_num(const char *name)
{
  int i;
  pt_menu m;

  if (name == NULL) return (uchar)(-1);
  for (i=0; i < ms->nummenus; i++)
  {
    m = ms->menus[i];
    if ((m->name) && (strcmp(m->name,name)==0)) return i;
  }
  return (uchar)(-1);
}

// Run through all items and if they are submenus
// with a non-trivial "action" and trivial submenunum
// replace submenunum with the menu with name "action"
void fix_submenus()
{
  int i,j;
  pt_menu m;
  pt_menuitem mi;

  i = 0;
  for (i=0; i < ms->nummenus; i++)
  {
     m = ms->menus[i];
     for (j=0; j < m->numitems; j++)
     {
         mi = m->items[j];
         // if item is a submenu and has non-empty non-trivial data string
         if (mi->data && strlen(mi->data) > 0 &&
             ((mi->action == OPT_SUBMENU) || (mi->action == OPT_RADIOMENU)) ) {
            mi->itemdata.submenunum = find_menu_num (mi->data);
         }
     }
  }
}

/* User Callable functions */

pt_menuitem showmenus(uchar startmenu)
{
  pt_menuitem rv;
  uchar oldpage,tpos;

  fix_submenus(); // Fix submenu numbers incase nick names were used

  // Setup screen for menusystem
  oldpage = getdisppage();
  setdisppage(ms->menupage);
  cls();
  clearwindow(ms->minrow, ms->mincol, ms->maxrow, ms->maxcol,
	      ms->menupage, ms->fillchar, ms->fillattr);
  tpos = (ms->numcols - strlen(ms->title) - 1) >> 1; // center it on line
  gotoxy(ms->minrow,ms->mincol,ms->menupage);
  cprint(ms->tfillchar,ms->titleattr,ms->numcols,ms->menupage);
  gotoxy(ms->minrow,ms->mincol+tpos,ms->menupage);
  csprint(ms->title,ms->titleattr);

  cursoroff(); // Doesn't seem to work?


  // Go, main menu cannot be a radio menu
  rv = runmenusystem(ms->minrow+MENUROW, ms->mincol+MENUCOL,
		     ms->menus[(unsigned int)startmenu], 0, NORMALMENU);

  // Hide the garbage we left on the screen
  cursoron();
  if (oldpage == ms->menupage) cls(); else setdisppage(oldpage);

  // Return user choice
  return rv;
}

pt_menusystem init_menusystem(const char *title)
{
  int i;

  ms = NULL;
  ms = (pt_menusystem) malloc(sizeof(t_menusystem));
  if (ms == NULL) return NULL;
  ms->nummenus = 0;
  // Initialise all menu pointers
  for (i=0; i < MAXMENUS; i++) ms->menus[i] = NULL;

  ms->title = (char *)malloc(TITLELEN+1);
  if (title == NULL)
    strcpy(ms->title,TITLESTR); // Copy string
  else strcpy(ms->title,title);

  // Timeout settings
  ms->tm_stepsize = TIMEOUTSTEPSIZE;
  ms->tm_numsteps = TIMEOUTNUMSTEPS;

  ms->normalattr[NOHLITE] = NORMALATTR;
  ms->normalattr[HLITE] = NORMALHLITE;

  ms->reverseattr[NOHLITE] = REVERSEATTR;
  ms->reverseattr[HLITE] = REVERSEHLITE;

  ms->inactattr[NOHLITE] = INACTATTR;
  ms->inactattr[HLITE] = INACTHLITE;

  ms->revinactattr[NOHLITE] = REVINACTATTR;
  ms->revinactattr[HLITE] = REVINACTHLITE;

  ms->statusattr[NOHLITE] = STATUSATTR;
  ms->statusattr[HLITE] = STATUSHLITE;

  ms->statline = STATLINE;
  ms->tfillchar= TFILLCHAR;
  ms->titleattr= TITLEATTR;

  ms->fillchar = FILLCHAR;
  ms->fillattr = FILLATTR;
  ms->spacechar= SPACECHAR;
  ms->shadowattr = SHADOWATTR;

  ms->menupage = MENUPAGE; // Usually no need to change this at all

  // Initialise all handlers
  ms->handler = NULL;
  ms->keys_handler = NULL;
  ms->ontimeout=NULL; // No timeout handler
  ms->tm_total_timeout = 0;
  ms->tm_sofar_timeout = 0;
  ms->ontotaltimeout = NULL;

  // Setup ACTION_{,IN}VALID
  ACTION_VALID.valid=1;
  ACTION_VALID.refresh=0;
  ACTION_INVALID.valid = 0;
  ACTION_INVALID.refresh = 0;

  // Figure out the size of the screen we are in now.
  // By default we use the whole screen for our menu
  ms->minrow = ms->mincol = 0;
  ms->numcols = getnumcols();
  ms->numrows = getnumrows();
  ms->maxcol = ms->numcols - 1;
  ms->maxrow = ms->numrows - 1;

  // How many entries per menu can we display at a time
  ms->maxmenuheight = ms->maxrow - ms->minrow - 3;
  if (ms->maxmenuheight > MAXMENUHEIGHT)
      ms->maxmenuheight= MAXMENUHEIGHT;

  // Set up the look of the box
  set_box_type(MENUBOXTYPE);
  return ms;
}

void set_normal_attr(uchar normal, uchar selected, uchar inactivenormal, uchar inactiveselected)
{
  if (normal != 0xFF)           ms->normalattr[0]   = normal;
  if (selected != 0xFF)         ms->reverseattr[0]  = selected;
  if (inactivenormal != 0xFF)   ms->inactattr[0]    = inactivenormal;
  if (inactiveselected != 0xFF) ms->revinactattr[0] = inactiveselected;
}

void set_normal_hlite(uchar normal, uchar selected, uchar inactivenormal, uchar inactiveselected)
{
  if (normal != 0xFF)           ms->normalattr[1]   = normal;
  if (selected != 0xFF)         ms->reverseattr[1]  = selected;
  if (inactivenormal != 0xFF)   ms->inactattr[1]    = inactivenormal;
  if (inactiveselected != 0xFF) ms->revinactattr[1] = inactiveselected;
}

void set_status_info(uchar statusattr, uchar statushlite, uchar statline)
{
  if (statusattr != 0xFF) ms->statusattr[NOHLITE] = statusattr;
  if (statushlite!= 0xFF) ms->statusattr[HLITE] = statushlite;
  // statline is relative to minrow
  if (statline >= ms->numrows) statline = ms->numrows - 1;
  ms->statline = statline; // relative to ms->minrow, 0 based
}

void set_title_info(uchar tfillchar, uchar titleattr)
{
  if (tfillchar  != 0xFF) ms->tfillchar  = tfillchar;
  if (titleattr  != 0xFF) ms->titleattr  = titleattr;
}

void set_misc_info(uchar fillchar, uchar fillattr,uchar spacechar, uchar shadowattr)
{
  if (fillchar  != 0xFF) ms->fillchar  = fillchar;
  if (fillattr  != 0xFF) ms->fillattr  = fillattr;
  if (spacechar != 0xFF) ms->spacechar = spacechar;
  if (shadowattr!= 0xFF) ms->shadowattr= shadowattr;
}

void set_box_type(boxtype bt)
{
  uchar *bxc;
  ms->menubt = bt;
  bxc = getboxchars(bt);
  ms->box_horiz = bxc[BOX_HORIZ]; // The char used to draw top line
  ms->box_ltrt = bxc[BOX_LTRT];
  ms->box_rtlt = bxc[BOX_RTLT];
}

void set_menu_options(uchar maxmenuheight)
{
  if (maxmenuheight != 0xFF) ms->maxmenuheight = maxmenuheight;
}

// Set the window which menusystem should use
void set_window_size(uchar top, uchar left, uchar bot, uchar right)
{

  uchar nr,nc;
  if ((top > bot) || (left > right)) return; // Sorry no change will happen here
  nr = getnumrows();
  nc = getnumcols();
  if (bot >= nr) bot = nr-1;
  if (right >= nc) right = nc-1;
  ms->minrow = top;
  ms->mincol = left;
  ms->maxrow = bot;
  ms->maxcol = right;
  ms->numcols = right - left + 1;
  ms->numrows = bot - top + 1;
  if (ms->statline >= ms->numrows) ms->statline = ms->numrows - 1; // Clip statline if need be
}

void reg_handler( t_handler htype, void * handler)
{
  // If bad value set to default screen handler
  switch(htype) {
    case HDLR_KEYS:
         ms->keys_handler = (t_keys_handler) handler;
         break;
    default:
         ms->handler = (t_menusystem_handler) handler;
         break;
  }
}

void unreg_handler(t_handler htype)
{
  switch(htype) {
    case HDLR_KEYS:
         ms->keys_handler = NULL;
         break;
    default:
         ms->handler = NULL;
         break;
  }
}

void reg_ontimeout(t_timeout_handler handler, unsigned int numsteps, unsigned int stepsize)
{
  ms->ontimeout = handler;
  if (numsteps != 0) ms->tm_numsteps = numsteps;
  if (stepsize != 0) ms->tm_stepsize = stepsize;
}

void unreg_ontimeout()
{
  ms->ontimeout = NULL;
}

void reg_ontotaltimeout (t_timeout_handler handler, unsigned long numcentiseconds)
{
  if (numcentiseconds != 0) {
     ms->ontotaltimeout = handler;
     ms->tm_total_timeout = numcentiseconds*10; // to convert to milliseconds
     ms->tm_sofar_timeout = 0;
  }
}

void unreg_ontotaltimeout()
{
  ms->ontotaltimeout = NULL;
}


int next_visible(pt_menu menu, int index)
{
  int ans;
  if (index < 0) ans = 0 ;
  else if (index >= menu->numitems) ans = menu->numitems-1;
  else ans = index;
  while ((ans < menu->numitems-1) &&
	 ((menu->items[ans]->action == OPT_INVISIBLE) ||
	  (menu->items[ans]->action == OPT_SEP)))
    ans++;
  return ans;
}

int prev_visible(pt_menu menu, int index) // Return index of prev visible
{
  int ans;
  if (index < 0) ans = 0;
  else if (index >= menu->numitems) ans = menu->numitems-1;
  else ans = index;
  while ((ans > 0) &&
	 ((menu->items[ans]->action == OPT_INVISIBLE) ||
	  (menu->items[ans]->action == OPT_SEP)))
    ans--;
  return ans;
}

int next_visible_sep(pt_menu menu, int index)
{
  int ans;
  if (index < 0) ans = 0 ;
  else if (index >= menu->numitems) ans = menu->numitems-1;
  else ans = index;
  while ((ans < menu->numitems-1) &&
	 (menu->items[ans]->action == OPT_INVISIBLE))
    ans++;
  return ans;
}

int prev_visible_sep(pt_menu menu, int index) // Return index of prev visible
{
  int ans;
  if (index < 0) ans = 0;
  else if (index >= menu->numitems) ans = menu->numitems-1;
  else ans = index;
  while ((ans > 0) &&
	 (menu->items[ans]->action == OPT_INVISIBLE))
    ans--;
  return ans;
}

int calc_visible(pt_menu menu,int first)
{
  int ans,i;

  if (menu == NULL) return 0;
  ans = 0;
  for (i=first; i < menu->numitems; i++)
    if (menu->items[i]->action != OPT_INVISIBLE) ans++;
  return ans;
}

// is curr visible if first entry is first?
int isvisible(pt_menu menu,int first, int curr)
{
  if (curr < first) return 0;
  return (calc_visible(menu,first)-calc_visible(menu,curr) < menu->menuheight);
}

// Calculate the first entry to be displayed
// so that curr is visible and make curr as late as possible
int calc_first_late(pt_menu menu,int curr)
{
  int ans,i,nv;

  nv = calc_visible(menu,0);
  if (nv <= menu->menuheight) return 0;
  // Start with curr and go back menu->menuheight times
  ans = curr+1;
  for (i=0; i < menu->menuheight; i++)
    ans = prev_visible_sep(menu,ans-1);
  return ans;
}

// Calculate the first entry to be displayed
// so that curr is visible and make curr as early as possible
int calc_first_early(pt_menu menu,int curr)
{
  int ans,i,nv;

  nv = calc_visible(menu,0);
  if (nv <= menu->menuheight) return 0;
  // Start with curr and go back till >= menu->menuheight
  // items are visible
  nv = calc_visible(menu,curr); // Already nv of them are visible
  ans = curr;
  for (i=0; i < menu->menuheight - nv; i++)
    ans = prev_visible_sep(menu,ans-1);
  return ans;
}

// Create a new menu and return its position
uchar add_menu(const char *title, int maxmenusize)
{
  int num,i;
  pt_menu m;

  num = ms->nummenus;
  if (num >= MAXMENUS) return -1;
  m = NULL;
  m = (pt_menu) malloc(sizeof(t_menu));
  if (m == NULL) return -1;
  ms->menus[num] = m;
  m->numitems = 0;
  m->name = NULL;
  m->row = 0xFF;
  m->col = 0xFF;
  if (maxmenusize < 1)
     m->maxmenusize = MAXMENUSIZE;
  else m->maxmenusize = maxmenusize;
  m->items = (pt_menuitem *) malloc(sizeof(pt_menuitem)*(m->maxmenusize));
  for (i=0; i < m->maxmenusize; i++) m->items[i] = NULL;

  m->title = (char *)malloc(MENULEN+1);
  if (title)
    {
      if (strlen(title) > MENULEN - 2)
	strcpy(m->title,TITLELONG);
      else strcpy(m->title,title);
    }
  else strcpy(m->title,EMPTYSTR);
  m ->menuwidth = strlen(m->title);
  ms->nummenus ++;
  return ms->nummenus - 1;
}

void set_menu_name(const char *name) // Set the "name" of this menu
{
  pt_menu m;

  m = ms->menus[ms->nummenus-1];
  if (m->name)  // Free up previous name
  {
     free(m->name);
     m -> name = NULL;
  }

  if (name)
    {
      m->name = (char *)malloc(strlen(name)+1);
      strcpy(m->name,name);
    }
}

// Create a new named menu and return its position
uchar add_named_menu(const char * name, const char *title, int maxmenusize)
{
   add_menu(title,maxmenusize);
   set_menu_name(name);
   return ms->nummenus - 1;
}

void set_menu_pos(uchar row,uchar col) // Set the position of this menu.
{
  pt_menu m;

  m = ms->menus[ms->nummenus-1];
  m->row = row;
  m->col = col;
}

pt_menuitem add_sep() // Add a separator to current menu
{
  pt_menuitem mi;
  pt_menu m;

  m = (ms->menus[ms->nummenus-1]);
  mi = NULL;
  mi = (pt_menuitem) malloc(sizeof(t_menuitem));
  if (mi == NULL) return NULL;
  m->items[(unsigned int)m->numitems] = mi;
  mi->handler = NULL; // No handler
  mi->item = mi->status = mi->data = NULL;
  mi->action = OPT_SEP;
  mi->index = m->numitems++;
  mi->parindex = ms->nummenus-1;
  mi->shortcut = 0;
  mi->helpid=0;
  return mi;
}

// Add item to the "current" menu
pt_menuitem add_item(const char *item, const char *status, t_action action,
		     const char *data, uchar itemdata)
{
  pt_menuitem mi;
  pt_menu m;
  const char *str;
  uchar inhlite=0; // Are we inside hlite area

  m = (ms->menus[ms->nummenus-1]);
  mi = NULL;
  mi = (pt_menuitem) malloc(sizeof(t_menuitem));
  if (mi == NULL) return NULL;
  m->items[(unsigned int) m->numitems] = mi;
  mi->handler = NULL; // No handler

  // Allocate space to store stuff
  mi->item = (char *)malloc(MENULEN+1);
  mi->status = (char *)malloc(STATLEN+1);
  mi->data = (char *)malloc(ACTIONLEN+1);

  if (item) {
    if (strlen(item) > MENULEN) {
      strcpy(mi->item,ITEMLONG);
    } else {
      strcpy(mi->item,item);
    }
    if (strlen(mi->item) > m->menuwidth) m->menuwidth = strlen(mi->item);
  } else strcpy(mi->item,EMPTYSTR);

  if (status) {
    if (strlen(status) > STATLEN) {
      strcpy(mi->status,STATUSLONG);
    } else {
      strcpy(mi->status,status);
    }
  } else strcpy(mi->status,EMPTYSTR);

  mi->action=action;
  str = mi->item;
  mi->shortcut = 0;
  mi->helpid = 0xFFFF;
  inhlite = 0; // We have not yet seen an ENABLEHLITE char
  // Find the first char in [A-Za-z0-9] after ENABLEHLITE and not arg to control char
  while (*str)
    {
      if (*str == ENABLEHLITE)
	{
	  inhlite=1;
	}
      if (*str == DISABLEHLITE)
	{
	  inhlite = 0;
	}
      if ( (inhlite == 1) &&
	   (((*str >= 'A') && (*str <= 'Z')) ||
	    ((*str >= 'a') && (*str <= 'z')) ||
	    ((*str >= '0') && (*str <= '9'))))
	{
	  mi->shortcut=*str;
	  break;
	}
      ++str;
    }
  if ((mi->shortcut >= 'A') && (mi->shortcut <= 'Z')) // Make lower case
    mi->shortcut = mi->shortcut -'A'+'a';

  if (data) {
    if (strlen(data) > ACTIONLEN) {
      strcpy(mi->data,ACTIONLONG);
    } else {
      strcpy(mi->data,data);
    }
  } else strcpy(mi->data,EMPTYSTR);

  switch (action)
    {
    case OPT_SUBMENU:
      mi->itemdata.submenunum = itemdata;
      break;
    case OPT_CHECKBOX:
      mi->itemdata.checked = itemdata;
      break;
    case OPT_RADIOMENU:
      mi->itemdata.radiomenunum = itemdata;
      if (mi->data) free(mi->data);
      mi->data = NULL; // No selection made
      break;
    default: // to keep the compiler happy
      break;
    }
  mi->index = m->numitems++;
  mi->parindex = ms->nummenus-1;
  return mi;
}

// Set the shortcut key for the current item
void set_item_options(uchar shortcut,int helpid)
{
  pt_menuitem mi;
  pt_menu m;

  m = (ms->menus[ms->nummenus-1]);
  if (m->numitems <= 0) return;
  mi = m->items[(unsigned int) m->numitems-1];

  if (shortcut != 0xFF) mi->shortcut = shortcut;
  if (helpid != 0xFFFF) mi->helpid = helpid;
}

// Free internal datasutructures
void close_menusystem(void)
{
}

// append_line_helper(pt_menu menu,char *line)
void append_line_helper(int menunum, char *line)
{
  pt_menu menu;
  pt_menuitem mi,ri;
  char *app;
  int ctr;
  char dp;


  dp = getdisppage();
  menu = ms->menus[menunum];
  for (ctr = 0; ctr < (int) menu->numitems; ctr++)
  {
      mi = menu->items[ctr];
      app = NULL; //What to append
      switch (mi->action) {
        case OPT_CHECKBOX:
             if (mi->itemdata.checked) app = mi->data;
             break;
        case OPT_RADIOMENU:
             if (mi->data) { // Some selection has been made
                ri = (pt_menuitem) (mi->data);
                app = ri->data;
             }
             break;
        case OPT_SUBMENU:
             append_line_helper(mi->itemdata.submenunum,line);
             break;
        default:
             break;
      }
      if (app) {
         strcat(line," ");
         strcat(line,app);
      }
  }
}


// Generate string based on state of checkboxes and radioitem in given menu
// Assume line points to large enough buffer
void gen_append_line(const char *menu_name,char *line)
{
  int menunum;

  menunum = find_menu_num(menu_name);
  if (menunum < 0) return; // No such menu
  append_line_helper(menunum,line);
}
