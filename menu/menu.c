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

 #include "biosio.h"
 #include "string.h"
 #include "menu.h"

 // Structures

 // Declare a menusystem here
 static t_menusystem menusystem;

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

 void printmenu(t_menu * menu, int curr, char top, char left)
 {
     int x;
     int numitems,menuwidth;
     t_menusystem *ms;
     char attr;

     ms = & menusystem;
     numitems = menu->numitems;
     menuwidth = menu->menuwidth+2;
     clearwindow(top,left-1,top+numitems+1,left+menuwidth+1,ms->menupage,ms->fillchar,ms->shadowattr);
     drawbox(top-1,left-2,top+numitems,left+menuwidth,ms->normalattr,ms->menupage);
     // Menu title
     x = (menuwidth - strlen(menu->title) - 1) >> 1;
     gotoxy(top-1,left+x,ms->menupage);
     csprint(menu->title);
     for (x=0; x < numitems; x++)
     {
	 gotoxy(top+x,left-1,ms->menupage);
	 if (menu->items[x].action == OPT_INACTIVE)
	 {
	     attr = (x==curr? ms->revinactattr : ms->inactattr);
	 } else {
	     attr = (x==curr ? ms->reverseattr : ms->normalattr);
	 }
	 cprint(ms->spacechar,attr,menuwidth+1,ms->menupage);        
	 gotoxy(top+x,left,ms->menupage);
	 csprint(menu->items[x].item);
	 gotoxy(top+x,left+menuwidth-1,ms->menupage); // Last char if any
	 switch (menu->items[x].action)
	 {
	    case OPT_SUBMENU:
		 cprint(SUBMENUCHAR,attr,1,ms->menupage);
		 break;
	    case OPT_CHECKBOX:
		 cprint( (menu->items[x].itemdata.checked ? CHECKED : UNCHECKED),attr,1,ms->menupage);
		 break;
	 }
     }
     if (menusystem.handler) menusystem.handler(&menusystem,menu->items+curr);
 }

 void cleanupmenu(t_menu *menu, char top,char left)
 {
     t_menusystem *ms = &menusystem;
     clearwindow(top,left-1,top+menu->numitems+1,left+menu->menuwidth+3,ms->menupage,ms->fillchar,ms->fillattr); // Clear the shadow
     clearwindow(top-1,left-2,top+menu->numitems,left+menu->menuwidth+2,ms->menupage,ms->fillchar,ms->fillattr); // clear the main window
 }

 /* Handle one menu */
 t_menuitem * getmenuoption( t_menu *menu, char top, char left, char startopt)
 // Return item chosen or NULL if ESC was hit.
 {
     int curr;
     char asc,scan;
     char numitems;
     t_menusystem *ms;
     t_menuitem *ci; // Current item

     ms = & menusystem;
     numitems = menu->numitems;
     // Setup status line
     gotoxy(ms->statline,0,ms->menupage);
     cprint(ms->spacechar,ms->reverseattr,80,ms->menupage);

     // Initialise current menu item    
     curr = startopt;
     gotoxy(ms->statline,0,ms->menupage);
     cprint(ms->spacechar,ms->statusattr,80,1);
     gotoxy(ms->statline,0,ms->menupage);
     csprint(menu->items[curr].status);
     while (1) // Forever
     {
	 printmenu(menu,curr,top,left);
	 ci = &(menu->items[curr]);
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
		   curr --;
		   break;
	     case DNARROW:
		   curr++;
		   break;
	     case LTARROW:
	     case ESCAPE:
		   return NULL;
		   break;
	     case ENTERA:
	     case RTARROW:
	     case ENTERB:
		   if (ci->action == OPT_INACTIVE) break;
		   if (ci->action == OPT_CHECKBOX) break;
		   if (ci->action == OPT_EXITMENU) return NULL; // As if we hit Esc
		   return ci;
		   break;
	     case SPACEKEY:
		   if (ci->action != OPT_CHECKBOX) break;
		   ci->itemdata.checked = !ci->itemdata.checked;
		   // Call handler to see it anything needs to be done
		   if (ci->handler != NULL) ci->handler(&menusystem,ci); 
		   break;
	 }
	 // Adjust within range
	 if (curr < 0) curr=0;
	 if (curr >= numitems) curr = numitems -1;
	 // Update status line
	 gotoxy(ms->statline,0,ms->menupage);
	 cprint(ms->spacechar,ms->statusattr,80,ms->menupage);
	 csprint(menu->items[curr].status);
     }
     return NULL; // Should never come here
 }

 /* Handle the entire system of menu's. */
 t_menuitem * runmenusystem(char top, char left, int currmenu)
 /*
  * currmenu
  *    Which menu should be currently displayed
  * top,left
  *    What is the position of the top,left corner of the menu
  *
  * Return Value:
  *    Returns a pointer to the final item chosen, or NULL if nothing chosen.
  */
 {
     t_menu *cmenu;
     t_menusystem *ms = &menusystem;
     t_menuitem *opt,*choice;
     int numitems;
     char startopt;

     startopt = 0;
 startover:
     cmenu = (menusystem.menus+currmenu);
     numitems = menusystem.menus[currmenu].numitems;
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
     if (opt->itemdata.submenunum >= menusystem.nummenus) // This is Bad....
     {
	 gotoxy(12,12,ms->menupage); // Middle of screen
	 csprint("Invalid submenu requested. Ask administrator to correct this.");
	 cleanupmenu(cmenu,top,left);
	 return NULL; // Pretend user hit esc
     }
     // Call recursively for submenu
     // Position the submenu below the current item,
     // covering half the current window (horizontally)
     choice = runmenusystem(top+opt->index+2, left+3+(cmenu->menuwidth >> 1), opt->itemdata.submenunum);
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

 t_menuitem * showmenus(char startmenu)
 {
     t_menuitem *rv;
     t_menusystem *ms;
     char oldpage, tpos;
     char oldrow, oldcol;

     ms = & menusystem;
     // Setup screen for menusystem
     oldpage = getdisppage();
     getpos(&oldrow, &oldcol, oldpage);
     setdisppage(ms->menupage);
     clearwindow(0,0,24,79,ms->menupage,ms->fillchar,ms->fillattr);
     tpos = (80 - strlen(menusystem.title) - 1) >> 1; // To center it on line    
     gotoxy(0,0,ms->menupage);
     cprint(ms->tfillchar,ms->titleattr,80,ms->menupage);
     gotoxy(0,tpos,ms->menupage);
     csprint(menusystem.title);

     cursoroff(); // Doesn't seem to work?

     // Go
     rv = runmenusystem(MENUROW, MENUCOL, startmenu);

     // Hide the garbage we left on the screen
     cursoron();
     if (oldpage == ms->menupage) {
	 cls();
     } else {
	 setdisppage(oldpage);
	 gotoxy(oldrow, oldcol, oldpage);
    }

    // Return user choice
    return rv;
}

void init_menusystem(const char *title)
{
    menusystem.nummenus = 0;
    if (title == NULL)
        strcpy(menusystem.title,TITLESTR);
    else strcpy(menusystem.title,title);

    menusystem.normalattr = NORMALATTR; 
    menusystem.reverseattr= REVERSEATTR;
    menusystem.inactattr = INACTATTR;
    menusystem.revinactattr = REVINACTATTR;

    menusystem.statusattr = STATUSATTR;
    menusystem.statline = STATLINE;
    menusystem.tfillchar= TFILLCHAR;
    menusystem.titleattr= TITLEATTR;
    
    menusystem.fillchar = FILLCHAR;
    menusystem.fillattr = FILLATTR;
    menusystem.spacechar= SPACECHAR;
    menusystem.shadowattr = SHADOWATTR;

    menusystem.menupage = MENUPAGE; // Usually no need to change this at all
    menusystem.handler = NULL; // No handler function
}

void set_normal_attr(char normal, char selected, char inactivenormal, char inactiveselected)
{
    if (normal != 0xFF)           menusystem.normalattr   = normal;
    if (selected != 0xFF)         menusystem.reverseattr  = selected;
    if (inactivenormal != 0xFF)   menusystem.inactattr    = inactivenormal;
    if (inactiveselected != 0xFF) menusystem.revinactattr = inactiveselected;
}

void set_status_info(char statusattr, char statline)
{
    if (statusattr != 0xFF) menusystem.statusattr = statusattr;
    if (statline   != 0xFF) menusystem.statline   = statline;
}

void set_title_info(char tfillchar, char titleattr)
{
    if (tfillchar  != 0xFF) menusystem.tfillchar  = tfillchar;
    if (titleattr  != 0xFF) menusystem.titleattr  = titleattr;
}

void set_misc_info(char fillchar, char fillattr,char spacechar, char shadowattr)
{
    if (fillchar  != 0xFF) menusystem.fillchar  = fillchar;
    if (fillattr  != 0xFF) menusystem.fillattr  = fillattr;
    if (spacechar != 0xFF) menusystem.spacechar = spacechar;
    if (shadowattr!= 0xFF) menusystem.shadowattr= shadowattr;
}

void reg_handler( t_menusystem_handler handler)
{
    menusystem.handler = handler;
}

void unreg_handler()
{
    menusystem.handler = NULL;
}

int add_menu(const char *title) // Create a new menu and return its position
{
    t_menu *m;

    if (menusystem.nummenus >= MAXMENUS)
	return -1;

    m = &menusystem.menus[(unsigned int)menusystem.nummenus];

    m->numitems = 0;
    if (title) {
        if (strlen(title) > MENULEN - 2) {
	    strcpy(m->title," TITLE TOO LONG ");          
        } else {
	    strcpy(m->title,title);
        }
    } else {
        strcpy(m->title,"");
    }

    m->menuwidth = strlen(m->title);

    return menusystem.nummenus++;
}

t_menuitem * add_item(const char *item, const char *status, t_action action, const char *data, char itemdata) // Add item to the "current" menu
{
    t_menuitem *mi;
    t_menu *m;

    m = &(menusystem.menus[menusystem.nummenus-1]);
    mi = &(m->items[m->numitems]);
    mi->handler = NULL; // No handler
    if (item) {
      if (strlen(item) > MENULEN - 2) {
        strcpy(mi->item,"ITEM TOO LONG");
      } else {
        strcpy(mi->item,item);
        if (strlen(item) > m->menuwidth) m->menuwidth = strlen(item);
      }
    } else strcpy(mi->item,"");

    if (status) {
      if (strlen(status) > STATLEN - 2) {
          strcpy(mi->status,"STATUS STRING TOO LONG");
      } else {
      strcpy(mi->status,status);
      }
    } else strcpy(mi->status,"");
    
    mi->action = action;

    if (data) {
      if (strlen(data) > ACTIONLEN - 2) {
          strcpy(mi->data,"ACTION STRING LONG");
      } else {
         strcpy(mi->data,data); // This is only null terminated
      }
    } else strcpy(mi->data,"");

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
    mi->parindex = menusystem.nummenus-1;
    return mi;
}


