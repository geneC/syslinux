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

#ifndef MULTIFS_H
#define MULTIFS_H

/*
 * MULTIFS SYNTAX:
 * (hd[disk number],[partition number])/path/to/file
 *
 * E.G.: (hd0,1)/dir/file means /dir/file at partition 1 of the disk 0.
 * Disk and Partition numbering starts from 0 and 1 respectivelly.
 */
#include "fs.h"

/*
 * root_fs means the file system where ldlinux.sys lives in.
 * this_fs means the file system being currently used.
 */
extern struct fs_info *this_fs, *root_fs;

/*
 * Set this_fs back to root_fs,
 * otherwise unexpected behavior may occurs.
 */
static inline void restore_fs(void)
{
    this_fs = root_fs;
}

/*
 * Basically restores the cwd of the underlying fs.
 */
static inline void restore_chdir_start(void)
{
    if (this_fs->fs_ops->chdir_start) {
	if (this_fs->fs_ops->chdir_start() < 0)
	    printf("Failed to chdir to start directory\n");
    }
}

typedef	struct fs_info *(*get_fs_info_t)(const char **);
extern int switch_fs(const char **);

#endif /* MULTIFS_H */
