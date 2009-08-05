/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#ifndef _MSDOS_H_
#define _MSDOS_H_

#include <disk/geom.h>

int parse_partition_table(struct driveinfo *, void *);

#endif /* _MSDOS_H_ */
