/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#ifndef _READ_H_
#define _READ_H_

#include <disk/geom.h>

void *read_mbr(int, int*);
void *dev_read(int, unsigned int, int, int*);
void *read_sectors(struct driveinfo*, const unsigned int,
		   const int, int *);
#endif /* _READ_H */
