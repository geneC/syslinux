/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Gene Cumm
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * mk-lba-img.c
 *
 * Makes an image that contains the LBA in every *word of every sector
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define NUM_SECT (256*63+1)
#define BPS (512)
#define SECT_INT (512 / sizeof(int))

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

const char DEF_FN[] = "lba.img";

int main(int argc, char *argv[])
{
	int i, j, b[SECT_INT], rv = 0, one = 0;
	FILE *f;
	uint8_t tt = 0;
	const char *fn;

	if (argc >= 2) {
		if (argc >= 3) {
			if (strcasecmp("-1", argv[1]) == 0) {
				fn = argv[2];
				one = 1;
			} else {
				fn = argv[1];
			}
		} else {
			fn = argv[1];
		}
	} else {
		fn = DEF_FN;
	}

	f = fopen(fn, "w");

	if (f) {
		for (i = 0; i < NUM_SECT; i++) {
			if (one) {
				b[0] = i;
			} else {
				for (j = 0; j < (512 / sizeof(int)); j++) {
					b[j] = i;
				}
			}
			fwrite(b, 512, 1, f);
		}
		fclose(f);
	} else {
		puts("Unable to open for writing");
		rv = 1;
	}
	return rv;
}
