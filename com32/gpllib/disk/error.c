/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>

#include <disk/errno_disk.h>

/**
 * get_error - decode a disk error status
 * @buffer_ptr:	Preallocated buffer
 *
 * Fill @buffer_ptr with the last errno_disk
 **/
void get_error(void* buffer_ptr)
{
	snprintf(buffer_ptr, MAX_DISK_ERRNO, "Disklib: error %d\n", errno_disk);
}
