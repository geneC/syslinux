#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2004-2005 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * menu.h
 *
 * Header file for the simple menu system
 */

#ifndef MENU_H
#define MENU_H

#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <inttypes.h>
#include <unistd.h>

#ifndef CLK_TCK
# define CLK_TCK sysconf(_SC_CLK_TCK)
#endif

struct menu_entry {
  char *displayname;
  char *label;
  char *cmdline;
  char *passwd;
  unsigned char hotkey;
};

/* 512 is the current definition inside syslinux */
#define MAX_CMDLINE_LEN	 512

#define MAX_ENTRIES	4096	/* Oughta be enough for anybody */
extern struct menu_entry menu_entries[];
extern struct menu_entry *menu_hotkeys[256];

struct menu_parameter {
  const char *name;
  int value;
};

extern struct menu_parameter mparm[];

extern int nentries;
extern int defentry;
extern int allowedit;
extern int timeout;
extern int shiftkey;
extern long long totaltimeout;

extern char *menu_title;
extern char *ontimeout;
extern char *menu_master_passwd;

void parse_config(const char *filename);

#endif /* MENU_H */

