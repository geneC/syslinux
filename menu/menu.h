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

#ifndef __MENU_H__
#define __MENU_H__

#include "biosio.h"
#include "string.h"

// Scancodes of some keys
#define ESCAPE     1
#define ENTERA    28
#define ENTERB   224

#define HOMEKEY  71
#define UPARROW  72
#define PAGEUP   73
#define LTARROW  75
#define RTARROW  77
#define ENDKEY   79
#define DNARROW  80
#define PAGEDN   81
#define SPACEKEY 57 // Scan code for SPACE

// Attributes
#if 0
/* Original black/white with blink */

#define NORMALATTR   0x70
#define REVERSEATTR  0x87
#define INACTATTR    0x74
#define REVINACTATTR 0x87
#define STATUSATTR   0xF0
#define FILLCHAR     178
#define FILLATTR     0x07
#define SHADOWATTR   0x08
#define SPACECHAR    ' '

#define TFILLCHAR    ' '
#define TITLEATTR    0x70

#else
/* Alternate color scheme */

#define NORMALATTR   0x17
#define REVERSEATTR  0x70
#define INACTATTR    0x18
#define REVINACTATTR 0x78
#define STATUSATTR   0x74
#define FILLCHAR     177
#define FILLATTR     0x01
#define SHADOWATTR   0x00
#define SPACECHAR    ' '

#define TFILLCHAR    ' '
#define TITLEATTR    0x70

#endif

#define TITLESTR  "COMBOOT Menu System for SYSLINUX developed by Murali Krishnan Ganapathy"

// Single line Box drawing Chars

#define TOPLEFT  218
#define BOTLEFT  192
#define TOPRIGHT 191
#define BOTRIGHT 217
#define TOP      196
#define BOT      196
#define LEFT     179
#define RIGHT    179


// Double line Box Drawing Chars
/*
#define TOPLEFT  201
#define BOTLEFT  200
#define TOPRIGHT 187
#define BOTRIGHT 188
#define TOP      205
#define BOT      205
#define LEFT     186
#define RIGHT    186
*/

// Attributes of the menu system
#define MAXMENUS      10 // Maximum number of menu's allowed
#define MAXMENUSIZE   10 // Maximum number of entries in each menu
#define MENULEN       30 // Each menu entry is atmost MENULEN chars including the terminating $
#define STATLEN       70 // Maximum length of status string
#define ACTIONLEN     80 // Maximum length of an action string

// Layout of menu
#define MENUROW       3  // Row where menu is displayed
#define MENUCOL       4  // Col where menu is displayed
#define MENUPAGE      1  // show in display page 1
#define STATLINE      24 // Line number where status line starts

// Other Chars
#define SUBMENUCHAR  175 // This is >> symbol, << is 174
#define CHECKED      251 // Checkmark
#define UNCHECKED    250 // Light bullet

typedef enum {OPT_INACTIVE, OPT_SUBMENU, OPT_RUN, OPT_EXITMENU, OPT_CHECKBOX, OPT_RADIOBTN, OPT_EXIT} t_action;

typedef union {
    char submenunum;
    char checked; // For check boxes
    char choice; // For Radio buttons
} t_itemdata;

struct s_menuitem;
struct s_menu;
struct s_menusystem;

typedef void (*t_item_handler)(struct s_menusystem *, struct s_menuitem *);
typedef void (*t_menusystem_handler)(struct s_menusystem *, struct s_menuitem *);

typedef struct s_menuitem {
    char item[MENULEN+2];
    char status[STATLEN+2];
    char active; // Is this item active or not
    t_action action;
    char data[ACTIONLEN+2];
    t_itemdata itemdata; // Data depends on action value
    char index; // Index within the menu array
    char parindex; // Index of the menu in which this item appears. 
    void * extra_data; // Any other data user can point to
    t_item_handler handler; // Pointer to function of type menufn
} t_menuitem;

typedef struct s_menu {
    char numitems;
    char menuwidth;
    char title[MENULEN+2];
    t_menuitem items[MAXMENUSIZE];
} t_menu;

typedef struct s_menusystem {
    t_menu menus[MAXMENUS];    
    char title[80]; // Maximum title length
    t_menusystem_handler handler; // Handler function called every time a menu is re-printed.
    char nummenus;
    char normalattr; 
    char reverseattr;
    char inactattr;
    char revinactattr;
    char statusattr;
    char fillchar;
    char fillattr;
    char spacechar;
    char tfillchar;
    char titleattr;
    char shadowattr;
    char statline;
    char menupage;
} t_menusystem;

// User callable Functions

t_menuitem * showmenus(char startmenu);

void init_menusystem(const char *title);

void set_normal_attr(char normal, char selected, char inactivenormal, char inactiveselected);

void set_status_info(char statusattr, char statline);

void set_title_info(char tfillchar, char titleattr);

void set_misc_info(char fillchar, char fillattr,char spacechar, char shadowattr);

void reg_handler( t_menusystem_handler handler); // Register handler

void unreg_handler(); 

// Create a new menu and return its position
int add_menu(const char *title);

// Add item to the "current" menu
t_menuitem * add_item(const char *item, const char *status, t_action action, const char *data, char itemdata); 

// Main function for the user's config file
int menumain(void);

#endif
