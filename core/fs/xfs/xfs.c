/*
 * Copyright (c) 2012 Paulo Alcantara <pcacjr@zytor.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <sys/dirent.h>
#include <cache.h>
#include <core.h>
#include <disk.h>
#include <fs.h>
#include <ilog2.h>
#include <klibc/compiler.h>
#include <ctype.h>

#include "codepage.h"
#include "xfs_types.h"
#include "xfs_sb.h"

static int xfs_fs_init(struct fs_info *fs)
{
    struct disk *disk = fs->fs_dev->disk;
    xfs_sb_t sb;
    int retval;

    /* Read XFS superblock (LBA 0) */
    retval = disk->rdwr_sectors(disk, &sb, 0, 1, 0);
    if (!retval)
	return -1;

    if (sb.sb_magicnum == *(uint32_t *)XFS_SB_MAGIC)
	printf("Cool! It's a XFS filesystem! :-)\n");

    /* Nothing to do for now... */

    return -1;
}

const struct fs_ops xfs_fs_ops = {
    .fs_name		= "ntfs",
    .fs_flags		= FS_USEMEM | FS_THISIND,
    .fs_init		= xfs_fs_init,
    .searchdir		= NULL,
    .getfssec		= NULL,
    .load_config	= NULL,
    .close_file         = NULL,
    .mangle_name	= NULL,
    .readdir		= NULL,
    .iget_root		= NULL,
    .iget		= NULL,
    .next_extent	= NULL,
};
