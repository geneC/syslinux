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

static inline void fill_xfs_inode_pvt(struct inode *inode, struct fs_info *fs,
				      xfs_ino_t ino)
{
    XFS_PVT(inode)->i_agblock =
	agnumber_to_bytes(fs, XFS_INO_TO_AGNO(fs, ino)) >> BLOCK_SHIFT(fs);
    XFS_PVT(inode)->i_ino_blk = ino_to_bytes(fs, ino) >> BLOCK_SHIFT(fs);
    XFS_PVT(inode)->i_block_offset =
	XFS_INO_TO_OFFSET((struct xfs_fs_info *)(fs->fs_info), ino) <<
			((struct xfs_fs_info *)(fs->fs_info))->inode_shift;
}

static xfs_dinode_t *xfs_get_ino_core(struct fs_info *fs, xfs_ino_t ino)
{
    block_t blk;
    xfs_dinode_t *core;
    uint64_t offset;

    xfs_debug("ino %lu", ino);

    blk = ino_to_bytes(fs, ino) >> BLOCK_SHIFT(fs);
    offset = XFS_INO_TO_OFFSET((struct xfs_fs_info *)(fs->fs_info), ino) <<
	    ((struct xfs_fs_info *)(fs->fs_info))->inode_shift;
    if (offset > BLOCK_SIZE(fs)) {
        xfs_error("Invalid inode offset in block!");
        xfs_debug("offset: 0x%llx", offset);
        goto out;
    }

    xfs_debug("blk %llu block offset 0x%llx", blk, blk << BLOCK_SHIFT(fs));
    xfs_debug("inode offset in block (in bytes) is 0x%llx", offset);

    core = (xfs_dinode_t *)((uint8_t *)get_cache(fs->fs_dev, blk) + offset);
    if (be16_to_cpu(core->di_magic) !=
	be16_to_cpu(*(uint16_t *)XFS_DINODE_MAGIC)) {
	xfs_error("Inode core's magic number does not match!");
	xfs_debug("magic number 0x%04x", (be16_to_cpu(core->di_magic)));
	goto out;
    }

    return core;

out:
    return NULL;
}

static char *get_entry_name(uint8_t *start, uint8_t *end)
{
    char *s;
    char *p;

    s = malloc(end - start + 1);
    if (!s)
	malloc_error("string");

    p = s;
    while (start < end)
	*p++ = *start++;

    *p = '\0';

    return s;
}

struct inode *xfs_fmt_local_find_entry(const char *dname, struct inode *parent,
				       xfs_dinode_t *core)
{
    xfs_dir2_sf_t *sf = (xfs_dir2_sf_t *)&core->di_literal_area[0];
    xfs_dir2_sf_entry_t *sf_entry;
    uint8_t count = sf->hdr.i8count ? sf->hdr.i8count : sf->hdr.count;
    struct fs_info *fs = parent->fs;
    struct inode *inode;
    xfs_intino_t ino;
    xfs_dinode_t *ncore = NULL;

    xfs_debug("count %hhu i8count %hhu", sf->hdr.count, sf->hdr.i8count);

    sf_entry = (xfs_dir2_sf_entry_t *)((uint8_t *)&sf->list[0] -
				       (!sf->hdr.i8count ? 4 : 0));
    while (count--) {
	uint8_t *start_name = &sf_entry->name[0];
	uint8_t *end_name = start_name + sf_entry->namelen;
	char *name;

	name = get_entry_name(start_name, end_name);

	xfs_debug("entry name: %s", name);

	if (!strncmp(name, dname, strlen(dname))) {
	    free(name);
	    goto found;
	}

	free(name);

	sf_entry = (xfs_dir2_sf_entry_t *)((uint8_t *)sf_entry +
					   offsetof(struct xfs_dir2_sf_entry,
						    name[0]) +
					   sf_entry->namelen +
					   (sf->hdr.i8count ? 8 : 4));
    }

    return NULL;

found:
    inode = xfs_new_inode(fs);

    ino = xfs_dir2_sf_get_inumber(sf, (xfs_dir2_inou_t *)(
				      (uint8_t *)sf_entry +
				      offsetof(struct xfs_dir2_sf_entry,
					       name[0]) +
				      sf_entry->namelen));

    xfs_debug("entry inode's number %lu", ino);

    ncore = xfs_get_ino_core(fs, ino);
    fill_xfs_inode_pvt(inode, fs, ino);
    if (!ncore) {
        xfs_error("Failed to get dinode!");
        goto out;
    }

    inode->ino			= ino;
    XFS_PVT(inode)->i_ino_blk	= ino_to_bytes(fs, ino) >> BLOCK_SHIFT(fs);
    inode->size 		= be64_to_cpu(ncore->di_size);

    if (be16_to_cpu(ncore->di_mode) & S_IFDIR) {
	inode->mode = DT_DIR;
	xfs_debug("Found a directory inode!");
    } else if (be16_to_cpu(ncore->di_mode) & S_IFREG) {
	inode->mode = DT_REG;
	xfs_debug("Found a file inode!");
	xfs_debug("inode size %llu", inode->size);
    }

    return inode;

out:
    free(inode);

    return NULL;
}

static uint32_t xfs_getfssec(struct file *file, char *buf, int sectors,
			     bool *have_more)
{
    xfs_debug("in");

    return generic_getfssec(file, buf, sectors, have_more);
}

static int xfs_next_extent(struct inode *inode, uint32_t lstart)
{
    struct fs_info *fs = inode->fs;
    xfs_dinode_t *core = NULL;
    xfs_bmbt_rec_t *rec;
    uint64_t startoff;
    uint64_t startblock;
    uint64_t blockcount;
    block_t blk;

    (void)lstart;

    xfs_debug("in");

    core = xfs_get_ino_core(fs, inode->ino);

    if (core->di_format == XFS_DINODE_FMT_EXTENTS) {
	/* The data fork contains the file's data extents */
	if (XFS_PVT(inode)->i_cur_extent == be32_to_cpu(core->di_nextents))
	    goto out;

	rec = (xfs_bmbt_rec_t *)&core->di_literal_area[0] +
				XFS_PVT(inode)->i_cur_extent++;

	xfs_debug("l0 0x%llx l1 0x%llx", rec->l0, rec->l1);

	/* l0:9-62 are startoff */
	startoff = (be64_to_cpu(rec->l0) & ((1ULL << 63) -1)) >> 9;
	/* l0:0-8 and l1:21-63 are startblock */
	startblock = (be64_to_cpu(rec->l0) & ((1ULL << 9) - 1)) |
			(be64_to_cpu(rec->l1) >> 21);
	/* l1:0-20 are blockcount */
	blockcount = be64_to_cpu(rec->l1) & ((1ULL << 21) - 1);

	xfs_debug("startoff 0x%llx startblock 0x%llx blockcount 0x%llx",
		  startoff, startblock, blockcount);

	blk = fsblock_to_bytes(fs, startblock) >> BLOCK_SHIFT(fs);

	xfs_debug("blk %llu", blk);

	XFS_PVT(inode)->i_offset = startoff;

	inode->next_extent.pstart = blk << BLOCK_SHIFT(fs) >> SECTOR_SHIFT(fs);
	inode->next_extent.len = ((blockcount << BLOCK_SHIFT(fs)) +
				  SECTOR_SIZE(fs) - 1) >> SECTOR_SHIFT(fs);
    }

    return 0;

out:
    return -1;
}

static struct inode *xfs_iget(const char *dname, struct inode *parent)
{
    struct fs_info *fs = parent->fs;
    xfs_dinode_t *core = NULL;
    struct inode *inode = NULL;

    xfs_debug("dname %s parent %p parent ino %lu", dname, parent, parent->ino);

    core = xfs_get_ino_core(fs, parent->ino);
    if (!core) {
        xfs_debug("Cannot get dinode from disk. ino: 0x%llx", parent->ino);
        goto out;
    }

    /* TODO: Handle both shortform and block directories */
    if (core->di_format == XFS_DINODE_FMT_LOCAL) {
	inode = xfs_fmt_local_find_entry(dname, parent, core);
	if (!inode) {
	    xfs_error("Entry not found!");
	    goto out;
	}
    } else {
	xfs_debug("format %hhu", core->di_format);
	xfs_debug("TODO: format \"local\" is the only supported ATM");
	goto out;
    }

    if (inode->mode == DT_REG) {
	XFS_PVT(inode)->i_offset = 0;
	XFS_PVT(inode)->i_cur_extent = 0;
    }

    return inode;

out:

    return NULL;
}

static struct inode *xfs_iget_root(struct fs_info *fs)
{
    xfs_dinode_t *core = NULL;
    struct inode *inode = xfs_new_inode(fs);

    xfs_debug("Looking for the root inode...");

    core = xfs_get_ino_core(fs, XFS_INFO(fs)->rootino);
    fill_xfs_inode_pvt(inode, fs, XFS_INFO(fs)->rootino);
    if (!core) {
	xfs_error("Inode core's magic number does not match!");
	xfs_debug("magic number 0x%04x", be16_to_cpu(core->di_magic));
	goto out;
    }

    xfs_debug("Root inode has been found!");

    if (!(be16_to_cpu(core->di_mode) & S_IFDIR)) {
	xfs_error("root inode is not a directory ?! No makes sense...");
	goto out;
    }

    inode->ino			= XFS_INFO(fs)->rootino;
    inode->mode 		= DT_DIR;
    inode->size 		= be64_to_cpu(core->di_size);

    return inode;

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

    info->blocksize		= be32_to_cpu(sb->sb_blocksize);
    info->block_shift		= sb->sb_blocklog;
    info->inopb_shift 		= sb->sb_inopblog;
    info->agblk_shift 		= sb->sb_agblklog;
    info->rootino 		= be64_to_cpu(sb->sb_rootino);
    info->agblocks 		= be32_to_cpu(sb->sb_agblocks);
    info->agblocks_shift 	= sb->sb_agblklog;
    info->agcount 		= be32_to_cpu(sb->sb_agcount);
    info->inodesize 		= be16_to_cpu(sb->sb_inodesize);
    info->inode_shift 		= sb->sb_inodelog;

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
    .getfssec		= xfs_getfssec,
    .load_config	= generic_load_config,
    .close_file         = generic_close_file,
    .mangle_name	= generic_mangle_name,
    .readdir		= NULL,
    .iget		= xfs_iget,
    .next_extent	= xfs_next_extent,
};
