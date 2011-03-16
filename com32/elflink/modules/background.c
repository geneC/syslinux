/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <consoles.h>
#include <string.h>
#include <core-elf.h>
#include <syslinux/vesacon.h>
#include <sys/module.h>
#include "menu.h"

static int background_init(void)
{
    return 0;			// Nothing to do; return success
}

const char *current_background = NULL;

int draw_background(const char *what)
{
#if 0
    if (!what)
	return vesacon_default_background();
    else if (what[0] == '#')
	return vesacon_set_background(parse_argb((char **)&what));
    else
	return vesacon_load_background(what);
#endif

    return 0;
}

void set_background(const char *new_background)
{
    if (!current_background || !new_background ||
	strcmp(current_background, new_background)) {
	draw_background(new_background);
	current_background = new_background;
    }
}

static void background_exit(void)
{
    // Nothing to do
}

// Define entry and exit points.
MODULE_INIT(background_init);
MODULE_EXIT(background_exit);
