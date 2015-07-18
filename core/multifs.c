/*
 * Copyright (C) 2013 Raphael S. Carvalho <raphael.scarv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <klibc/compiler.h>
#include <stdio.h>
#include <assert.h>
#include "multifs.h"

static get_fs_info_t get_fs_info = NULL;

/*
 * Enable MultiFS support
 * This function is called from ldlinux.c32 to enable MultiFS support.
 *
 * @addr: address of the get_fs_info function from ldlinux.c32.
 */
__export void enable_multifs(get_fs_info_t addr)
{
    if (addr) {
	get_fs_info = addr;
	dprintf("MultiFS: set get_fs_info to %p\n", get_fs_info);
    }
}

/*
 * The request is considered MultiFS if the path starts
 * with an open parentheses.
 *
 * @ret: Upon success, set this_fs to the fs where the underlying file
 * resides and returns 0. Upon failure, returns -1;
 */
int switch_fs(const char **path)
{
    struct fs_info *fs;

    assert(path && *path);
    if ((*path)[0] != '(') {
	/* If so, don't need to restore chdir */
	if (this_fs == root_fs)
	    return 0;

	fs = root_fs;
	goto ret;
    }

    if (__unlikely(!get_fs_info)) {
	printf("MultiFS support is not enabled!\n");
	return -1;
    }

    fs = get_fs_info(path);
    if (!fs) {
	dprintf("MultiFS: It wasn't possible to get the proper fs!\n");
	return -1;
    }
ret:
    this_fs = fs;
    return 0;
}
