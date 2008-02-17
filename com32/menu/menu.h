/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
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
#include <colortbl.h>

#ifndef CLK_TCK
# define CLK_TCK sysconf(_SC_CLK_TCK)
#endif

struct menu;

struct menu_entry {
  char *displayname;
  char *label;
  char *passwd;
  char *helptext;
  char *cmdline;
  struct menu *submenu;
  unsigned char hotkey;
  unsigned char disabled;
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

extern const char * const kernel_types[];

/* Configurable messages */
enum message_number {
  MSG_TITLE,
  MSG_AUTOBOOT,
  MSG_TAB,
  MSG_NOTAB,
  MSG_PASSPROMPT,

  MSG_COUNT
};

struct messages {
  const char *name;		/* Message configuration name */
  const char *defmsg;		/* Default message text */
};

extern const struct messages messages[MSG_COUNT];

struct menu_parameter {
  const char *name;
  int value;
};

extern const struct menu_parameter mparm[NPARAMS];

struct fkey_help {
  const char *textname;
  const char *background;
};

struct menu {
  struct menu *parent;

  struct menu_entry *menu_entries;
  struct menu_entry *menu_hotkeys[256];

  const char *messages[MSG_COUNT];
  int mparm[NPARAMS];

  int nentries;
  int nentries_space;
  int defentry;
  int allowedit;
  int timeout;
  int shiftkey;
  bool hiddenmenu;
  long long totaltimeout;

  char *ontimeout;
  char *onerror;
  char *menu_master_passwd;
  char *menu_background;

  struct color_table *color_table;

  struct fkey_help fkeyhelp[12];
};

extern struct menu *root_menu;

/* 2048 is the current definition inside syslinux */
#define MAX_CMDLINE_LEN	 2048

void parse_configs(char **argv);
int draw_background(const char *filename);

static inline int my_isspace(char c)
{
  return (unsigned char)c <= ' ';
}

int my_isxdigit(char c);
unsigned int hexval(char c);
unsigned int hexval2(const char *p);
uint32_t parse_argb(char **p);

int menu_main(int argc, char *argv[]);
void console_prepare(void);
void console_cleanup(void);

extern const int message_base_color;
int mygetkey(clock_t timeout);
int show_message_file(const char *filename, const char *background);

#define MSG_COLORS_DEF_FG	0x90ffffff
#define MSG_COLORS_DEF_BG	0x80ffffff
#define MSG_COLORS_DEF_SHADOW	SHADOW_NORMAL
void set_msg_colors_global(unsigned int fg, unsigned int bg,
			   enum color_table_shadow shadow);

/* passwd.c */
int passwd_compare(const char *passwd, const char *entry);

#endif /* MENU_H */
