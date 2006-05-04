#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * hello.c
 *
 * Hello, World! using libcom32
 */

#include <string.h>
#include <stdio.h>
#include <console.h>

int main(void)
{
  char buffer[1024];

  openconsole(&dev_stdcon_r, &dev_stdcon_w);

  printf("Hello, World!\n");

  for (;;) {
    printf("> ");
    fgets(buffer, sizeof buffer, stdin);
    if ( !strncmp(buffer, "exit", 4) )
      break;
    printf(": %s", buffer);
  }

  return 0;
}
