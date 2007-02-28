/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2007 H. Peter Anvin - All Rights Reserved
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

enum kernel_type {
  /* Meta-types for internal use */
  KT_NONE,
  KT_LOCALBOOT,

  /* The ones we can pass off to SYSLINUX, in order */
  KT_KERNEL,			/* Undefined type */
  KT_LINUX,			/* Linux kernel */
  KT_BOOT,			/* Bootstrap program */
  KT_BSS,			/* Boot sector with patch */
  KT_PXE,			/* PXE NBP */
  KT_FDIMAGE,			/* Floppy disk image */
  KT_COMBOOT,			/* COMBOOT image */
  KT_COM32,			/* COM32 image */
  KT_CONFIG,			/* Configuration file */
};

extern const char *kernel_types[];

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
extern char *onerror;
extern char *menu_master_passwd;
extern char *menu_tab_msg;
extern char *menu_autoboot_msg;
extern char *menu_passprompt_msg;

extern char *menu_background;

void parse_configs(char **argv);
extern int (*draw_background)(const char *filename);

static inline int my_isspace(char c)
{
  return (unsigned char)c <= ' ';
}

int menu_main(int argc, char *argv[]);
void console_prepare(void);
void console_cleanup(void);

#endif /* MENU_H */
