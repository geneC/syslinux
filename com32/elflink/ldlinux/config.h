/*
 *   Copyright 2011 Intel Corporation - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

/*
 * These values correspond to the "default" and "ui" commands
 * respectively. "ui" takes precendence over "default".
 */
#define LEVEL_DEFAULT	1
#define LEVEL_UI	2

extern short uappendlen;	//bytes in append= command
extern short ontimeoutlen;	//bytes in ontimeout command
extern short onerrorlen;	//bytes in onerror command
extern short forceprompt;	//force prompt
extern short noescape;		//no escape
extern short nocomplete;	//no label completion on TAB key
extern short allowimplicit;	//allow implicit kernels
extern short allowoptions;	//user-specified options allowed
extern short includelevel;	//nesting level
extern short defaultlevel;	//the current level of default
extern short vkernel;		//have we seen any "label" statements?
extern short displaycon;	//conio.inc
extern short nohalt;		//idle.inc

extern char *default_cmd;	//"default" command line

#endif /* __CONFIG_H__ */
