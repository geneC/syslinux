/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2006 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * vesamenu.c
 *
 * Simple menu system which displays a list and allows the user to select
 * a command line and/or edit it.
 *
 * VESA graphics version.
 */

#include <stdio.h>
#include <console.h>
#include "menu.h"

void console_prepare(void)
{
  fputs("\033[0m\033[20h\033[25l", stdout);
}

void console_cleanup(void)
{
  /* For the serial console, be nice and clean up */
  fputs("\033[0m\033[20l", stdout);
}

int vesacon_load_background(const char *);

int main(int argc, char *argv[])
{
  openconsole(&dev_rawcon_r, &dev_vesaserial_w);

  draw_background = vesacon_load_background;

  return menu_main(argc, argv);
}
