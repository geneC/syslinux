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

// Single line Box drawing Chars

#define TOPLEFT  218
#define BOTLEFT  192
#define TOPRIGHT 191
#define BOTRIGHT 217
#define TOP      196
#define BOT      196
#define LEFT     179
#define RIGHT    179
#define HORIZ    196
#define LTRT     195 // The |- char
#define RTLT     180 // The -| char

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
#define HORIZ    205
#define LTRT     199 // The ||- char
#define RTLT     182 // The -|| char
*/

// Attributes of the menu system
#define MAXMENUS      8 // Maximum number of menu's allowed
#define MAXMENUSIZE   12 // Maximum number of entries in each menu

// Upper bounds on lengths
// Now that the onus of allocating space is with the user, these numbers
// are only for sanity checks. You may increase these values without
// affecting the memory footprint of this program
#define MENULEN       30 // Each menu entry is atmost MENULEN chars
#define STATLEN       80 // Maximum length of status string
#define ACTIONLEN     80 // Maximum length of an action string

// Layout of menu
#define MENUROW       3  // Row where menu is displayed (relative to window)
#define MENUCOL       4  // Col where menu is displayed (relative to window)
#define MENUPAGE      1  // show in display page 1
#define STATLINE      23 // Line number where status line starts (relative to window)

// Other Chars
#define SUBMENUCHAR  175 // This is >> symbol
#define EXITMENUCHAR 174 // This is << symbol
#define CHECKED      251 // Check mark
#define UNCHECKED    250 // Light bullet

typedef enum {OPT_INACTIVE, OPT_SUBMENU, OPT_RUN, OPT_EXITMENU, OPT_CHECKBOX,
    OPT_RADIOBTN, OPT_EXIT, OPT_SEP} t_action;

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
    const char *item;
    const char *status;
    const char *data; 
    void * extra_data; // Any other data user can point to
    t_item_handler handler; // Pointer to function of type menufn
    char active; // Is this item active or not
    t_action action;
    t_itemdata itemdata; // Data depends on action value
    char index; // Index within the menu array
    char parindex; // Index of the menu in which this item appears. 
} t_menuitem;

typedef t_menuitem *pt_menuitem; // Pointer to type menuitem

typedef struct s_menu {
    pt_menuitem items[MAXMENUSIZE];
    const char *title;
    char numitems;
    char menuwidth;
    char row,col; // Position where this menu should be displayed
} t_menu;

typedef t_menu *pt_menu; // Pointer to type menu

typedef struct s_menusystem {
    pt_menu menus[MAXMENUS];
    const char *title; 
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
    char maxrow,minrow,numrows; // Number of rows in the window 
    char maxcol,mincol,numcols; // Number of columns in the window
} t_menusystem;

typedef t_menusystem *pt_menusystem; // Pointer to type menusystem

/************************************************************************
 * IMPORTANT INFORMATION
 *
 * All functions which take a string as argument store the pointer
 * for later use. So if you have alloc'ed a space for the string
 * and are passing it to any of these functions, DO NOT deallocate it.
 *
 * If they are constant strings, you may receive warning from the compiler
 * about "converting from char const * to char *". Ignore these errors.
 *
 * This hack/trick of storing these pointers will help in reducing the size
 * of the internal structures by a lot.
 *
 ***************************************************************************
 */

pt_menuitem showmenus(char startmenu);

void init_menusystem(const char *title); // This pointer value is stored internally

void set_normal_attr(char normal, char selected, char inactivenormal, char inactiveselected);

void set_status_info(char statusattr, char statline);

void set_title_info(char tfillchar, char titleattr);

void set_misc_info(char fillchar, char fillattr,char spacechar, char shadowattr);

void set_window_size(char top, char left, char bot, char right); // Set the window which menusystem should use

void reg_handler( t_menusystem_handler handler); // Register handler

void unreg_handler(); 

// Create a new menu and return its position
char add_menu(const char *title); // This pointer value is stored internally

// Add item to the "current" menu // pointer values are stored internally
pt_menuitem add_item(const char *item, const char *status, t_action action, const char *data, char itemdata);

void set_menu_pos(char row,char col); // Set the position of this menu.

// Add a separator to the "current" menu
pt_menuitem add_sep();

// Main function for the user's config file
int menumain(char *cmdline);

#endif
