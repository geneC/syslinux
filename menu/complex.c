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

#ifndef NULL
#define NULL ((void *) 0)
#endif

#include "menu.h"
#include "biosio.h"
#include "string.h"
#include "syslinux.h"

/* Global variables */
char infoline[160];

struct {
    unsigned int baseurl : 1; // Do we need to specify by url
    unsigned int mountcd : 1; // Should we mount the cd
    unsigned int network : 1; // want network?
    unsigned int dhcp    : 1; // want dhcp / static ip
    unsigned int winrep  : 1; // Want to repair windows?
    unsigned int linrep  : 1; // Want to repair linux?
} flags;

t_menuitem *baseurl,*mountcd,*network,*dhcp,*runprep,*winrep,*linrep;
// all the menus we are going to declare
char TESTING,RESCUE,MAIN,PREP;

/* End globals */

#define INFLINE 22

void msys_handler(t_menusystem *ms, t_menuitem *mi)
{
    char nc;
    nc = getnumcols(); // Get number of columns

    if (mi->parindex != PREP) // If we are not in the PREP MENU
    {
        gotoxy(INFLINE,0,ms->menupage);
        cprint(' ',0x07,nc,ms->menupage);
        return;
    }
    strcpy (infoline," ");
    if (flags.baseurl) strcat(infoline,"baseurl=http://192.168.11.12/gui ");
    if (flags.mountcd) strcat(infoline,"mountcd=yes ");
    if (!flags.network)
       strcat(infoline,"network=no ");
    else if (!flags.dhcp) strcat(infoline,"network=static ");
    if (flags.winrep) strcat(infoline,"repair=win ");
    if (flags.linrep) strcat(infoline,"repair=lin ");
    gotoxy(INFLINE,0,ms->menupage);
    cprint(' ',0x07,nc,ms->menupage);
    gotoxy(INFLINE+1,0,ms->menupage);
    cprint(' ',0x07,nc,ms->menupage);
    gotoxy(INFLINE,0,ms->menupage);
    csprint("Kernel Arguments:");
    gotoxy(INFLINE,17,ms->menupage);
    csprint(infoline);
}

void checkbox_handler(t_menusystem *ms, t_menuitem *mi)
{
    (void)ms; /* Unused */

    if (mi->action != OPT_CHECKBOX) return;
    
    if (strcmp(mi->data,"baseurl") == 0) flags.baseurl = (mi->itemdata.checked ? 1 : 0);
    if (strcmp(mi->data,"network") == 0) {
        if (mi->itemdata.checked)
        {
            flags.network = 1;
            dhcp->action = OPT_CHECKBOX;
        }
        else
        {
            flags.network = 0;
            dhcp->action = OPT_INACTIVE;
        }            
    }
    if (strcmp(mi->data,"winrepair") == 0) {
        if (mi->itemdata.checked)
        {
            flags.winrep = 1;
            linrep->action = OPT_INACTIVE;
        }
        else
        {
            flags.winrep = 0;
            linrep->action = OPT_CHECKBOX;
        }
    }
    if (strcmp(mi->data,"linrepair") == 0) {
        if (mi->itemdata.checked)
        {
            flags.linrep = 1;
            winrep->action = OPT_INACTIVE;
        }
        else
        {
            flags.winrep = 0;
            winrep->action = OPT_CHECKBOX;
        }
    }
    if (strcmp(mi->data,"mountcd") == 0) flags.mountcd = (mi->itemdata.checked ? 1 : 0);
    if (strcmp(mi->data,"dhcp") == 0) flags.dhcp    = (mi->itemdata.checked ? 1 : 0);
}

int menumain(char *cmdline)
{
  t_menuitem * curr;
  char cmd[160];
  char ip[30];

  (void)cmdline;		/* Not used */

  // Switch video mode here
  // setvideomode(0x18); // or whatever mode you want

  // Choose the default title and setup default values for all attributes....
  init_menusystem(NULL);
  set_window_size(1,1,20,78); // Leave some space around
  
  // Choose the default values for all attributes and char's
  // -1 means choose defaults (Actually the next 4 lines are not needed)
  //set_normal_attr (-1,-1,-1,-1); 
  //set_status_info (-1,-1); // Display status on the last line
  //set_title_info  (-1,-1); 
  //set_misc_info(-1,-1,-1,-1);

  reg_handler(&msys_handler);
  
  TESTING = add_menu(" Testing ");
  add_item("Memory Test","Perform extensive memory testing",OPT_RUN, "memtest",0);
  add_item("Exit this menu","Go one level up",OPT_EXITMENU,"exit",0);

  RESCUE = add_menu(" Rescue Options ");
  add_item("Linux Rescue","linresc",OPT_RUN,"linresc",0);
  add_item("Dos Rescue","dosresc",OPT_RUN,"dosresc",0);
  add_item("Windows Rescue","winresc",OPT_RUN,"winresc",0);
  add_item("Exit this menu","Go one level up",OPT_EXITMENU,"exit",0);

  PREP = add_menu(" Prep options ");
  baseurl = add_item("baseurl by IP?","Specify gui baseurl by IP address",OPT_CHECKBOX,"baseurl",0);
  mountcd = add_item("mountcd?","Mount the cdrom drive?",OPT_CHECKBOX,"mountcd",0);
  network = add_item("network?","Try to initialise network device?",OPT_CHECKBOX,"network",1);
  dhcp    = add_item("dhcp?","Use dhcp to get ipaddr?",OPT_CHECKBOX,"dhcp",1);
  winrep  = add_item("Reinstall windows","Re-install the windows side of a dual boot setup",OPT_CHECKBOX,"winrepair",0);
  linrep  = add_item("Reinstall linux","Re-install the linux side of a dual boot setup",OPT_CHECKBOX,"linrepair",0);
  runprep = add_item("Run prep now","Execute prep with the above options",OPT_RUN,"prep",0);
  baseurl->handler = &checkbox_handler;
  mountcd->handler = &checkbox_handler;
  network->handler = &checkbox_handler;
  dhcp->handler = &checkbox_handler;
  winrep->handler = &checkbox_handler;
  linrep->handler = &checkbox_handler;
  flags.baseurl = 0;
  flags.mountcd = 0;
  flags.network = 1;
  flags.dhcp = 1;
  flags.winrep = 0;
  flags.linrep = 0;

  MAIN = add_menu(" Main Menu ");  
  add_item("Prepare","prep",OPT_RUN,"prep",0);
  add_item("Prep options...","Options for prep image",OPT_SUBMENU,NULL,PREP);
  add_item("Rescue options...","Troubleshoot a system",OPT_SUBMENU,NULL,RESCUE);
  add_item("Testing...","Options to test hardware",OPT_SUBMENU,NULL,TESTING);
  add_item("Exit to prompt", "Exit the menu system", OPT_EXITMENU, "exit", 0);

  curr = showmenus(MAIN);
  if (curr)
  {
        if (curr->action == OPT_EXIT) return 0;
        if (curr->action == OPT_RUN)
        {
            strcpy(cmd,curr->data);
            if (curr == runprep)
            {
                strcat(cmd,infoline);
                if (flags.network && !flags.dhcp) // We want static
                {
                    csprint("Enter IP address (last two octets only): ");
                    getstring(ip, sizeof ip);
                    strcat(cmd,"ipaddr=192.168.");
                    strcat(cmd,ip);
                }
            }
            if (syslinux)
               runcommand(cmd);
            else csprint(cmd);
            return 1;
        }
  }
  return 0;
}

