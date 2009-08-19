/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
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

int main(int argc, char *argv[])
{
    int i;

    openconsole(&dev_stdcon_r, &dev_stdcon_w);

    printf("Hello, World!\n");

    for (i = 1; i < argc; i++)
	printf("%s%c", argv[i], (i == argc - 1) ? '\n' : ' ');

    return 0;
}
