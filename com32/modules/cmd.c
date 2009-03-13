/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2008 Michael Brown - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * cmd.c
 *
 * Execute arbitrary commands
 */

#include <string.h>
#include <alloca.h>
#include <com32.h>

int main(int argc, const char *argv[])
{
  size_t len = 0;
  char *cmd;
  char *tmp;
  int i;

  for (i = 1; i < argc; i++)
    len += strlen(argv[i]) + 1;

  tmp = cmd = alloca(len);

  for (i = 1; i < argc; i++) {
    tmp = strpcpy(tmp, argv[i]);
    if (i != argc-1)
      *tmp++ = ' ';
  }
  *tmp = '\0';

  syslinux_run_command(cmd);
}
