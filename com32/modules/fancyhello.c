#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2004 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * fancyhello.c
 *
 * Hello, World! using libcom32 and ASI console
 */

#include <string.h>
#include <stdio.h>
#include <console.h>

int main(void)
{
  char buffer[1024];

  /* Write both to the ANSI console and the serial port, if configured */
  openconsole(&dev_stdcon_r, &dev_ansiserial_w);

  printf("(lifesign)\r\n(another)\r\n(another)\r\n");
  printf("\033[1;33;44m *** \033[37mHello, World!\033[33m *** \033[0m\r\n");

  for (;;) {
    printf("\033[1;36m>\033[0m ");
    fgets(buffer, sizeof buffer, stdin);
    /* fgets() prints an \n for us, but not \r */
    putchar('\r');
    if ( !strncmp(buffer, "exit", 4) )
      break;
    printf("\033[1m:\033[0m %s\r", buffer);
  }
  return 0;
}
