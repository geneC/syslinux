/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2008 Gene Cumm - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * rosh.h
 *
 * Read-Only shell; Header
 */

/*
 * History
 * b021	Move much PreProcessing stuff to rosh.h
 * b018	Create rosh_debug() macro
 * b012	Version of rosh.c at time of creating this file.
 */

#ifndef ROSH_H
#define ROSH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>	/* macro: true false */
#include <string.h>	/* strcpy() strlen() memcpy() strchr() */
#include <sys/types.h>
#include <sys/stat.h>	/* fstat() */
#include <fcntl.h>	/* open(); open mode macros */
#include <dirent.h>	/* fdopendir() opendir() readdir() closedir() DIR */
#include <unistd.h>	/* getcwd() */
#include <errno.h>	/* errno; error macros */
#include <netinet/in.h>	/* For htonl/ntohl/htons/ntohs */

#include <getkey.h>
#include <consoles.h>

/* A GNUC extension to void out unused functions are used */
/*	Plus, there seem to be other references for SYSLINUX to __GNUC__ */
#ifndef __GNUC__
#error SYSLINUX (I believe) requires __GNUC__
#endif	/* __GNUC__ */

#ifdef DO_DEBUG
#define ROSH_DEBUG(f, ...)	printf (f, ## __VA_ARGS__)
#ifdef DO_DEBUG2
#define ROSH_DEBUG2(f, ...)	printf (f, ## __VA_ARGS__)
#else	/* DO_DEBUG2 */
#define ROSH_DEBUG2(f, ...)	((void)0)
#endif	/* DO_DEBUG2 */
#else	/* DO_DEBUG */
#define ROSH_DEBUG(f, ...)	((void)0)
#define ROSH_DEBUG2(f, ...)	((void)0)
#endif	/* DO_DEBUG */

#ifdef __COM32__
#define ROSH_IS_COM32	1
#include <console.h>		/* openconsole() */
#include <syslinux/config.h>	/* Has info on the SYSLINUX variant */
#include <syslinux/boot.h>	/* syslinux_run_command() */
#define ROSH_COM32(f, ...)	printf (f, ## __VA_ARGS__)
#else
#include <termios.h>
#define ROSH_IS_COM32	0
static inline char *syslinux_config_file()
{
	return "";
}
static inline int getscreensize(int fd, int *rows, int *cols)
{
	char *str;
	int rv;
	*rows = 0;
	*cols = 0;
	if (rows) {
		str = getenv("LINES");
		if (str) {
			*rows = atoi(str);
		}
	}
	if (cols) {
		str = getenv("COLUMNS");
		if (str) {
			*cols = atoi(str);
		}
	}
	if (!rows || !cols)
		rv = -1;
	else if (!*rows || !*cols)
		rv = -2;
	else
		rv = 0;
	return rv;
}
#define ROSH_COM32(f, ...)	((void)0)
#define syslinux_run_command(f)	((void)0)
#endif	/* __COM32__ */

#define SEP	'/'

/* Size of buffer string */
#define ROSH_BUF_SZ	16384
/* Size of screen output buffer (80*40) */
#define ROSH_SBUF_SZ	1200

/* Size of command buffer string */
#ifdef MAX_CMDLINE_LEN
#define ROSH_CMD_SZ	MAX_CMDLINE_LEN
#else
#ifdef COMMAND_LINE_SIZE
#define ROSH_CMD_SZ	COMMAND_LINE_SIZE
#else
#define ROSH_CMD_SZ	2048
#endif	/* COMMAND_LINE_SIZE */
#endif	/* MAX_CMDLINE_LEN */

/* Size of path buffer string */
#ifdef PATH_MAX
#define ROSH_PATH_SZ	PATH_MAX
#elif NAME_MAX
#define ROSH_PATH_SZ	NAME_MAX
#else
#define ROSH_PATH_SZ	255
#endif	/* NAME_MAX */

const char rosh_help_str1[] =
"Commands: ? cat cd cfg dir exit help less ls man more pwd run quit ver";

const char rosh_help_str2[] =
"Commands: (some 1-letter abreviations also allowed)\n\
  h     HELP\n     ALSO ? help man\n\
  cat   Concatenate file to console\n    cat <file>\n\
  cd    Change to directory <dir>\n    cd <dir>\n\
  less  Page a file with rewind\n\
  ls    List contents of current directory\n    ls <dir>\n\
    ALSO dir\n\
  more  Page a file\n\
  pwd   display Present Working Directory\n\
  run   Run a program/kernel with options\n\
  exit  Exit to previous environment\n    ALSO quit";

#endif	/* Not ROSH_H */
