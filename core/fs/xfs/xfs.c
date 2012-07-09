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
#include "xfs.h"
#include "xfs_ag.h"
#include "misc.h"

static inline struct inode *xfs_new_inode(struct fs_info *fs)
{
    struct inode *inode;

    inode = alloc_inode(fs, 0, sizeof(struct xfs_inode));
    if (!inode)
	malloc_error("xfs_inode structure");

    return inode;
}

static xfs_dinode_t *xfs_get_dinode(struct fs_info *fs, xfs_ino_t ino)
{
    block_t blk;
    block_t blk_offset;
    xfs_dinode_t *dino;

    blk = ino << XFS_INFO(fs)->inode_shift >> BLOCK_SHIFT(fs);
    blk_offset = (blk << BLOCK_SHIFT(fs)) % BLOCK_SIZE(fs);

    dino = (xfs_dinode_t *)((uint8_t *)get_cache(fs->fs_dev, blk) +
			    blk_offset);
    if (!dino) {
	xfs_error("Error in reading filesystem block 0x%llX (%llu)", blk, blk);
	goto out;
    }

    if (be16_to_cpu(dino->di_magic) !=
	be16_to_cpu(*(uint16_t *)XFS_DINODE_MAGIC)) {
	xfs_error("Inode core's magic number does not match!");
	goto out;
    }

    return dino;

out:
    return NULL;
}

static struct inode *xfs_iget(const char *unused_0, struct inode *unused_1)
{
    (void)unused_0;
    (void)unused_1;

    xfs_debug("in");

    return NULL;
}

static struct inode *xfs_iget_root(struct fs_info *fs)
{
    xfs_agnumber_t agno;
    block_t blk;
    xfs_agi_t *agi;
    xfs_btree_sblock_t *ibt_hdr;
    uint32_t i;
    xfs_inobt_rec_t *rec;
    xfs_dinode_t *dino;
    struct inode *inode = xfs_new_inode(fs);

    xfs_debug("Looking for the root inode...");

    agno = XFS_INO_TO_AGNO(fs, XFS_INFO(fs)->rootino);
    if (agno >= XFS_INFO(fs)->agcount) {
	xfs_error("Invalid AG number");
	goto out;
    }

    blk = XFS_AGNO_TO_FSB(fs, agno);
    agi = XFS_AGI_OFFS(fs, get_cache(fs->fs_dev, blk));
    if (!agi) {
	xfs_error("Error in reading filesystem block 0x%llX (%llu)", blk, blk);
	goto out;
    }

    XFS_PVT(inode)->i_agblock = blk;

    if (be32_to_cpu(agi->agi_magicnum) !=
	be32_to_cpu(*(uint32_t *)XFS_AGI_MAGIC)) {
	xfs_error("AGI's magic number does not match!");
	goto out;
    }

    xfs_debug("agi_count %lu", be32_to_cpu(agi->agi_count));
    xfs_debug("agi_level %lu", be32_to_cpu(agi->agi_level));

    /* Get block number relative to the AG containing the root of the inode
     * B+tree.
     */
    blk = agno + be32_to_cpu(agi->agi_root);

    xfs_debug("inode B+tree's block %llu", blk);

    ibt_hdr = (xfs_btree_sblock_t *)get_cache(fs->fs_dev, blk);
    if (!ibt_hdr) {
	xfs_error("Error in reading filesystem block 0x%llX (%llu)", blk, blk);
	goto out;
    }

    if (be32_to_cpu(ibt_hdr->bb_magic) !=
	be32_to_cpu(*(uint32_t *)XFS_IBT_MAGIC)) {
	xfs_error("AGI inode B+tree header's magic number does not match!");
	goto out;
    }

    xfs_debug("bb_level %lu", ibt_hdr->bb_level);
    xfs_debug("bb_numrecs %lu", ibt_hdr->bb_numrecs);

    rec = (xfs_inobt_rec_t *)((uint8_t *)ibt_hdr + sizeof *ibt_hdr);
    i = ibt_hdr->bb_numrecs;
    for ( ; i--; rec++) {
	if (be32_to_cpu(rec->ir_startino) == XFS_INFO(fs)->rootino)
	    goto found;
    }

    xfs_error("Root inode not found!");
    goto not_found;

found:
    xfs_debug("Root inode has been found!");

    inode->ino = XFS_INFO(fs)->rootino;

    dino = xfs_get_dinode(fs, XFS_INFO(fs)->rootino);
    if (!dino) {
	xfs_error("Failed to get inode core from inode %lu",
		  XFS_INFO(fs)->rootino);
	goto out;
    }

    if (!(be16_to_cpu(dino->di_mode) & S_IFDIR)) {
	xfs_error("root inode is not a directory ?! No makes sense...");
	goto out;
    }

    inode->mode = DT_DIR;
    inode->size = dino->di_size;

    return inode;

not_found:

out:
    free(inode);

    return NULL;
}

static inline int xfs_read_superblock(struct fs_info *fs, xfs_sb_t *sb)
{
    struct disk *disk = fs->fs_dev->disk;

    if (!disk->rdwr_sectors(disk, sb, XFS_SB_DADDR, 1, false))
	return -1;

    return 0;
}

static struct xfs_fs_info *xfs_new_sb_info(xfs_sb_t *sb)
{
    struct xfs_fs_info *info;

    info = malloc(sizeof *info);
    if (!info)
	malloc_error("xfs_fs_info structure");

    info->blocksize = be32_to_cpu(sb->sb_blocksize);
    info->block_shift = sb->sb_blocklog;

    /* Calculate the bits used in the Relative inode number */
    info->ag_relative_ino_shift = sb->sb_inopblog + sb->sb_agblklog;
    /* The MSB is the AG number in Absolute inode numbers */
    info->ag_number_ino_shift = 8 * sizeof(xfs_ino_t) -
				info->ag_relative_ino_shift;

    info->rootino = be64_to_cpu(sb->sb_rootino);

    info->agblocks = be32_to_cpu(sb->sb_agblocks);
    info->agblocks_shift = sb->sb_agblklog;
    info->agcount = be32_to_cpu(sb->sb_agcount);

    info->inodesize = be16_to_cpu(sb->sb_inodesize);
    info->inode_shift = sb->sb_inodelog;

    return info;
}

static int xfs_fs_init(struct fs_info *fs)
{
    struct disk *disk = fs->fs_dev->disk;
    xfs_sb_t sb;
    struct xfs_fs_info *info;

    xfs_debug("fs %p", fs);

    SECTOR_SHIFT(fs) = disk->sector_shift;
    SECTOR_SIZE(fs) = 1 << SECTOR_SHIFT(fs);

    if (xfs_read_superblock(fs, &sb)) {
	xfs_error("Superblock read failed");
	goto out;
    }

    if (!xfs_is_valid_magicnum(&sb)) {
	xfs_error("Invalid superblock");
	goto out;
    }

    xfs_debug("magicnum 0x%lX", be32_to_cpu(sb.sb_magicnum));

    info = xfs_new_sb_info(&sb);
    if (!info) {
	xfs_error("Failed to fill in filesystem-specific info structure");
	goto out;
    }

    fs->fs_info = info;

    xfs_debug("block_shift %u blocksize 0x%lX (%lu)", info->block_shift,
	      info->blocksize, info->blocksize);

    xfs_debug("rootino 0x%llX (%llu)", info->rootino, info->rootino);

    BLOCK_SHIFT(fs) = info->block_shift;
    BLOCK_SIZE(fs) = info->blocksize;

    cache_init(fs->fs_dev, BLOCK_SHIFT(fs));

    return BLOCK_SHIFT(fs);

out:
    return -1;
}

const struct fs_ops xfs_fs_ops = {
    .fs_name		= "xfs",
    .fs_flags		= FS_USEMEM | FS_THISIND,
    .fs_init		= xfs_fs_init,
    .iget_root		= xfs_iget_root,
    .searchdir		= NULL,
    .getfssec		= NULL,
    .load_config	= generic_load_config,
    .close_file         = generic_close_file,
    .mangle_name	= generic_mangle_name,
    .readdir		= NULL,
    .iget		= xfs_iget,
    .next_extent	= NULL,
};
