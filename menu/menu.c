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
 * wcl -3 -osx -mt getargs.c
 */

#include "biosio.h"
#include "string.h"
#include "menu.h"
#include "heap.h"

// Local Variables
static pt_menusystem ms; // Pointer to the menusystem
static char TITLESTR[] = "COMBOOT Menu System for SYSLINUX developed by Murali Krishnan Ganapathy";
static char TITLELONG[] = " TITLE too long ";
static char ITEMLONG[] = " ITEM too long ";
static char ACTIONLONG[] = " ACTION too long ";
static char STATUSLONG[] = " STATUS too long ";
static char EMPTYSTR[] = "";

/* Basic Menu routines */

void drawbox(char top, char left, char bot, char right,char attr, char page)
{
    char x;
    
    // Top border
    gotoxy(top,left,page);
    cprint(TOPLEFT,attr,1,page);
    gotoxy(top,left+1,page);
    cprint(TOP,attr,right-left,page);
    gotoxy(top,right,page);
    cprint(TOPRIGHT,attr,1,page);
    // Bottom border
    gotoxy(bot,left,page);
    cprint(BOTLEFT,attr,1,page);
    gotoxy(bot,left+1,page);
    cprint(BOT,attr,right-left,page);
    gotoxy(bot,right,page);
    cprint(BOTRIGHT,attr,1,page);
    // Left & right borders
    for (x=top+1; x < bot; x++)
    {
        gotoxy(x,left,page);
        cprint(LEFT,attr,1,page);
        gotoxy(x,right,page);
        cprint(RIGHT,attr,1,page);
    }
}

void printmenu(pt_menu menu, int curr, char top, char left)
{
    int x;
    int numitems,menuwidth;
    char fchar[5],lchar[5]; // The first and last char in for each entry
    const char *str;  // and inbetween the item or a seperator is printed
    char attr;  // all in the attribute attr
    char sep[MENULEN];// and inbetween the item or a seperator is printed
    pt_menuitem ci;

    numitems = menu->numitems;
    menuwidth = menu->menuwidth+3;
    clearwindow(top,left-2,top+numitems+1,left+menuwidth+1,ms->menupage,ms->fillchar,ms->shadowattr);
    drawbox(top-1,left-3,top+numitems,left+menuwidth,ms->normalattr,ms->menupage);
    memset(sep,HORIZ,menuwidth); // String containing the seperator string
    sep[menuwidth-1] = 0; 
    // Menu title
    x = (menuwidth - strlen(menu->title) - 1) >> 1;
    gotoxy(top-1,left+x,ms->menupage);
    csprint(menu->title);
    for (x=0; x < numitems; x++)
    {
        // Setup the defaults now
        lchar[0] = fchar[0] = ' '; 
        lchar[1] = fchar[1] = '\0'; // fchar and lchar are just spaces
        ci = menu->items[x];
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
            case OPT_CHECKBOX:
                 lchar[0] = (ci->itemdata.checked ? CHECKED : UNCHECKED);
                 lchar[1] = 0;
                 break;
            case OPT_SEP:
                 fchar[0] = '\b'; fchar[1] = LTRT; fchar[2] = HORIZ; fchar[3] = HORIZ; fchar[4] = 0;
                 lchar[0] = HORIZ; lchar[1] = RTLT; lchar[3] = 0;
                 str = sep;
                 break;
            case OPT_EXITMENU:
                 fchar[0] = EXITMENUCHAR; fchar[1] = 0;
            //default:
        }
        gotoxy(top+x,left-2,ms->menupage);
        cprint(ms->spacechar,attr,menuwidth+2,ms->menupage); // Wipe area with spaces
        gotoxy(top+x,left-2,ms->menupage);
        csprint(fchar); // Print first part
        gotoxy(top+x,left,ms->menupage);
        csprint(str); // Print main part
        gotoxy(top+x,left+menuwidth-1,ms->menupage); // Last char if any
        csprint(lchar); // Print last part
    }
    if (ms->handler) ms->handler(ms,menu->items[curr]);
}

void cleanupmenu(pt_menu menu, char top,char left)
{
    clearwindow(top,left-2,top+menu->numitems+1,left+menu->menuwidth+4,ms->menupage,ms->fillchar,ms->fillattr); // Clear the shadow
    clearwindow(top-1,left-3,top+menu->numitems,left+menu->menuwidth+3,ms->menupage,ms->fillchar,ms->fillattr); // clear the main window
}

/* Handle one menu */
pt_menuitem getmenuoption( pt_menu menu, char top, char left, char startopt)
// Return item chosen or NULL if ESC was hit.
{
    int curr;
    char asc,scan;
    char numitems;
    pt_menuitem ci; // Current item
    
    numitems = menu->numitems;
    // Setup status line
    gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
    cprint(ms->spacechar,ms->reverseattr,ms->numcols,ms->menupage);

    // Initialise current menu item    
    curr = startopt;
    gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
    cprint(ms->spacechar,ms->statusattr,ms->numcols,1);
    gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
    csprint(menu->items[curr]->status);
    while (1) // Forever
    {
        printmenu(menu,curr,top,left);
        ci = menu->items[curr];
        asc = inputc(&scan);
        switch (scan)
        {
            case HOMEKEY:
                  curr = 0;
                  break;
            case ENDKEY:
                  curr = numitems -1;
                  break;
            case PAGEDN:
                  curr += 5;
                  break;
            case PAGEUP:
                  curr -= 5;
                  break;
            case UPARROW:
                  while((curr > 0) && (menu->items[--curr]->action == OPT_SEP)) ;
                  break;
            case DNARROW:
                  while((curr < numitems-1) && (menu->items[++curr]->action == OPT_SEP)) ;
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
                  return ci;
                  break;
            case SPACEKEY:
                  if (ci->action != OPT_CHECKBOX) break;
                  ci->itemdata.checked = !ci->itemdata.checked;
                  // Call handler to see it anything needs to be done
                  if (ci->handler != NULL) ci->handler(ms,ci); 
                  break;
        }
        // Adjust within range
        if (curr < 0) curr=0;
        if (curr >= numitems) curr = numitems -1;
        // Update status line
        gotoxy(ms->minrow+ms->statline,ms->mincol,ms->menupage);
        cprint(ms->spacechar,ms->statusattr,ms->numcols,ms->menupage);
        csprint(menu->items[curr]->status);
    }
    return NULL; // Should never come here
}

/* Handle the entire system of menu's. */
pt_menuitem runmenusystem(char top, char left, pt_menu cmenu)
/*
 * cmenu
 *    Which menu should be currently displayed
 * top,left
 *    What is the position of the top,left corner of the menu
 *
 * Return Value:
 *    Returns a pointer to the final item chosen, or NULL if nothing chosen.
 */
{
    pt_menuitem opt,choice;
    int numitems;
    char startopt;

    startopt = 0;
    if (cmenu == NULL) return NULL;
startover:
    numitems = cmenu->numitems;
    opt = getmenuoption(cmenu,top,left,startopt);
    if (opt == NULL)
    {
        // User hit Esc
        cleanupmenu(cmenu,top,left);
        return NULL;
    }
    if (opt->action != OPT_SUBMENU) // We are done with the menu system
    {
        cleanupmenu(cmenu,top,left);
        return opt; // parent cleanup other menus
    }
    if (opt->itemdata.submenunum >= ms->nummenus) // This is Bad....
    {
        gotoxy(12,12,ms->menupage); // Middle of screen
        csprint("Invalid submenu requested. Ask administrator to correct this.");
        cleanupmenu(cmenu,top,left);
        return NULL; // Pretend user hit esc
    }
    // Call recursively for submenu
    // Position the submenu below the current item,
    // covering half the current window (horizontally)
    choice = runmenusystem(top+opt->index+2, left+3+(cmenu->menuwidth >> 1), ms->menus[opt->itemdata.submenunum]);
    if (choice==NULL) // User hit Esc in submenu
    {
       // Startover
       startopt = opt->index;
       goto startover;
    }
    else
    {
        cleanupmenu(cmenu,top,left);
        return choice;
    }
}

/* User Callable functions */

pt_menuitem showmenus(char startmenu)
{
    pt_menuitem rv;
    char oldpage,tpos;

    // Setup screen for menusystem
    oldpage = getdisppage();
    setdisppage(ms->menupage);
    cls();
    clearwindow(ms->minrow,ms->mincol,ms->maxrow,ms->maxcol,ms->menupage,ms->fillchar,ms->fillattr);
    tpos = (ms->numcols - strlen(ms->title) - 1) >> 1; // To center it on line    
    gotoxy(ms->minrow,ms->mincol,ms->menupage);
    cprint(ms->tfillchar,ms->titleattr,ms->numcols,ms->menupage);
    gotoxy(ms->minrow,ms->mincol+tpos,ms->menupage);
    csprint(ms->title);
    
    cursoroff(); // Doesn't seem to work?

    // Go
    rv = runmenusystem(ms->minrow+MENUROW, ms->mincol+MENUCOL, ms->menus[startmenu]);

    // Hide the garbage we left on the screen
    cursoron();
    if (oldpage == ms->menupage) cls(); else setdisppage(oldpage);

    // Return user choice
    return rv;
}

void init_menusystem(const char *title)
{
    char i;
    
    ms = NULL;
    ms = (pt_menusystem) malloc(sizeof(t_menusystem));
    if (ms == NULL) return;
    ms->nummenus = 0;
    // Initialise all menu pointers
    for (i=0; i < MAXMENUS; i++) ms->menus[i] = NULL; 
    
    if (title == NULL)
        ms->title = TITLESTR; // Copy pointers
    else ms->title = title;

    ms->normalattr = NORMALATTR; 
    ms->reverseattr= REVERSEATTR;
    ms->inactattr = INACTATTR;
    ms->revinactattr = REVINACTATTR;

    ms->statusattr = STATUSATTR;
    ms->statline = STATLINE;
    ms->tfillchar= TFILLCHAR;
    ms->titleattr= TITLEATTR;
    
    ms->fillchar = FILLCHAR;
    ms->fillattr = FILLATTR;
    ms->spacechar= SPACECHAR;
    ms->shadowattr = SHADOWATTR;

    ms->menupage = MENUPAGE; // Usually no need to change this at all
    ms->handler = NULL; // No handler function

    // Figure out the size of the screen we are in now.
    // By default we use the whole screen for our menu
    ms->minrow = ms->mincol = 0;
    ms->numcols = getnumcols();
    ms->numrows = getnumrows();
    ms->maxcol = ms->numcols - 1;
    ms->maxrow = ms->numrows - 1;
}

void set_normal_attr(char normal, char selected, char inactivenormal, char inactiveselected)
{
    if (normal != 0xFF)           ms->normalattr   = normal;
    if (selected != 0xFF)         ms->reverseattr  = selected;
    if (inactivenormal != 0xFF)   ms->inactattr    = inactivenormal;
    if (inactiveselected != 0xFF) ms->revinactattr = inactiveselected;
}

void set_status_info(char statusattr, char statline)
{
    if (statusattr != 0xFF) ms->statusattr = statusattr;
    // statline is relative to minrow
    if (statline >= ms->numrows) statline = ms->numrows - 1;
    ms->statline = statline; // relative to ms->minrow, 0 based
}

void set_title_info(char tfillchar, char titleattr)
{
    if (tfillchar  != 0xFF) ms->tfillchar  = tfillchar;
    if (titleattr  != 0xFF) ms->titleattr  = titleattr;
}

void set_misc_info(char fillchar, char fillattr,char spacechar, char shadowattr)
{
    if (fillchar  != 0xFF) ms->fillchar  = fillchar;
    if (fillattr  != 0xFF) ms->fillattr  = fillattr;
    if (spacechar != 0xFF) ms->spacechar = spacechar;
    if (shadowattr!= 0xFF) ms->shadowattr= shadowattr;
}

void set_window_size(char top, char left, char bot, char right) // Set the window which menusystem should use
{
    
    char nr,nc;
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

void reg_handler( t_menusystem_handler handler)
{
    ms->handler = handler;
}

void unreg_handler()
{
    ms->handler = NULL;
}

char add_menu(const char *title) // Create a new menu and return its position
{
   char num,i;
   pt_menu m;

   if (num >= MAXMENUS) return -1;
   num = ms->nummenus;
   m = NULL;
   m = (pt_menu) malloc(sizeof(t_menu));
   if (m == NULL) return -1;
   ms->menus[num] = m;
   m->numitems = 0;
   for (i=0; i < MAXMENUSIZE; i++) m->items[i] = NULL;
   
   if (title)
   {
       if (strlen(title) > MENULEN - 2)
          m->title = TITLELONG;
       else m->title = title; 
   }
   else m->title = EMPTYSTR; 
   m ->menuwidth = strlen(m->title);
   ms->nummenus ++;
   return ms->nummenus - 1;
}


pt_menuitem add_sep() // Add a separator to current menu
{
    pt_menuitem mi;
    pt_menu m;

    m = (ms->menus[ms->nummenus-1]);
    mi = NULL;
    mi = (pt_menuitem) malloc(sizeof(t_menuitem));
    if (mi == NULL) return NULL;
    m->items[m->numitems] = mi;
    mi->handler = NULL; // No handler
    mi->item = mi->status = mi->data = EMPTYSTR;
    mi->action = OPT_SEP;
    mi->index = m->numitems++;
    mi->parindex = ms->nummenus-1;
    return mi;
}

pt_menuitem add_item(const char *item, const char *status, t_action action, const char *data, char itemdata) // Add item to the "current" menu
{
    pt_menuitem mi;
    pt_menu m;

    m = (ms->menus[ms->nummenus-1]);
    mi = NULL;
    mi = (pt_menuitem) malloc(sizeof(t_menuitem));
    if (mi == NULL) return NULL;
    m->items[m->numitems] = mi;
    mi->handler = NULL; // No handler
    if (item) {
      if (strlen(item) > MENULEN - 2) {
        mi->item = ITEMLONG; 
      } else {
        mi->item = item; 
        if (strlen(item) > m->menuwidth) m->menuwidth = strlen(item);
      }
    } else mi->item = EMPTYSTR; 

    if (status) {
      if (strlen(status) > STATLEN - 2) {
          mi->status = STATUSLONG; 
      } else {
      mi->status = status; 
      }
    } else mi->status = EMPTYSTR; 
    
    mi->action = action;

    if (data) {
      if (strlen(data) > ACTIONLEN - 2) {
          mi->data = ACTIONLONG; 
      } else {
         mi->data = data; 
      }
    } else mi->data = EMPTYSTR; 

    switch (action)
    {
        case OPT_SUBMENU:
            mi->itemdata.submenunum = itemdata;
            break;
        case OPT_CHECKBOX:
            mi->itemdata.checked = itemdata;
            break;
        case OPT_RADIOBTN:
            mi->itemdata.choice = itemdata;
            break;
    }
    mi->index = m->numitems++;
    mi->parindex = ms->nummenus-1;
    return mi;
}


