#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 1998 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * syslinux.c - Linux installer program for SYSLINUX
 */

#include <stdio.h>
#include <mntent.h>

extern const unsigned char bootsect[];
extern const unsigned int  bootsect_len;

extern const unsigned char ldlinux[];
extern const unsigned int  ldlinux_len;

char *device;			/* Device to install to */

int main(int argc, char *argv[])
{
  if ( argc != 2 ) {
    fprintf(stderr, "Usage: %s device\n", argv[0]);
    exit(1);
  }

  return 0;
}
