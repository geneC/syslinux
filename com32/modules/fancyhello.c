
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
 * Hello, World! using libcom32 and ANSI console; also possible to compile
 * as a Linux application for testing.
 */

#include <string.h>
#include <stdio.h>

#ifdef __COM32__

#include <console.h>

static void console_init(void)
{
  /* Write both to the ANSI console and the serial port, if configured */
  openconsole(&dev_stdcon_r, &dev_ansiserial_w);
}  

#else

#include <termios.h>
#include <unistd.h>

static void console_init(void)
{
  struct termios tio;

  /* Set the termios flag so we behave the same as libcom32 */
  tcgetattr(0, &tio);
  tio.c_iflag &= ~ICRNL;
  tio.c_iflag |= IGNCR;
  tcsetattr(0, TCSANOW, &tio);
}  

#endif

int main(void)
{
  char buffer[1024];

  console_init();
  printf("\033[20h");		/* Automatically convert \r\n -> \n */

  printf("\033[1;33;44m *** \033[37mHello, World!\033[33m *** \033[0m\n");

  for (;;) {
    printf("\033[1;36m>\033[0m ");
    fgets(buffer, sizeof buffer, stdin);
    if ( !strncmp(buffer, "exit", 4) )
      break;
    printf("\033[1m:\033[0m %s", buffer);
  }
  return 0;
}
