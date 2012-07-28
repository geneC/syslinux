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
#include "xfs_ag.h"
#include "misc.h"
#include "xfs.h"
#include "xfs_dinode.h"
#include "xfs_dir2.h"

static int fill_dirent(struct fs_info *fs, struct dirent *dirent,
		       uint32_t offset, xfs_ino_t ino, char *name,
		       size_t namelen)
{
    xfs_dinode_t *core;

    dirent->d_ino = ino;
    dirent->d_off = offset;
    dirent->d_reclen = offsetof(struct dirent, d_name) + namelen + 1;

    core = xfs_dinode_get_core(fs, ino);
    if (!core) {
        xfs_error("Failed to get dinode from disk (ino 0x%llx)", ino);
        return -1;
    }

    if (be16_to_cpu(core->di_mode) & S_IFDIR)
	dirent->d_type = DT_DIR;
    else if (be16_to_cpu(core->di_mode) & S_IFREG)
	dirent->d_type = DT_REG;

    memcpy(dirent->d_name, name, namelen + 1);

    return 0;
}

static int xfs_fmt_local_readdir(struct file *file, struct dirent *dirent,
				 xfs_dinode_t *core)
{
    xfs_dir2_sf_t *sf = (xfs_dir2_sf_t *)&core->di_literal_area[0];
    xfs_dir2_sf_entry_t *sf_entry;
    uint8_t count = sf->hdr.i8count ? sf->hdr.i8count : sf->hdr.count;
    uint32_t offset = file->offset;
    uint8_t *start_name;
    uint8_t *end_name;
    char *name;
    xfs_ino_t ino;
    struct fs_info *fs = file->fs;
    int retval = 0;

    xfs_debug("count %hhu i8count %hhu", sf->hdr.count, sf->hdr.i8count);

    if (file->offset + 1 > count)
	return -1;

    file->offset++;

    sf_entry = (xfs_dir2_sf_entry_t *)((uint8_t *)&sf->list[0] -
				       (!sf->hdr.i8count ? 4 : 0));

    if (file->offset - 1) {
	offset = file->offset;
	while (--offset) {
	    sf_entry = (xfs_dir2_sf_entry_t *)(
				(uint8_t *)sf_entry +
				offsetof(struct xfs_dir2_sf_entry,
					 name[0]) +
				sf_entry->namelen +
				(sf->hdr.i8count ? 8 : 4));
	}
    }

    start_name = &sf_entry->name[0];
    end_name = start_name + sf_entry->namelen;

    name = xfs_dir2_get_entry_name(start_name, end_name);

    ino = xfs_dir2_sf_get_inumber(sf, (xfs_dir2_inou_t *)(
				      (uint8_t *)sf_entry +
				      offsetof(struct xfs_dir2_sf_entry,
					       name[0]) +
				      sf_entry->namelen));

    retval = fill_dirent(fs, dirent, file->offset, ino, (char *)name,
			 end_name - start_name);
    if (retval)
	xfs_error("Failed to fill in dirent structure");

    free(name);

    return retval;
}

static int xfs_dir2_block_readdir(struct file *file, struct dirent *dirent,
				  xfs_dinode_t *core)
{
    xfs_bmbt_irec_t r;
    block_t dir_blk;
    struct fs_info *fs = file->fs;
    uint8_t *dirblk_buf;
    uint8_t *p;
    uint32_t offset;
    xfs_dir2_data_hdr_t *hdr;
    xfs_dir2_block_tail_t *btp;
    xfs_dir2_data_unused_t *dup;
    xfs_dir2_data_entry_t *dep;
    uint8_t *start_name;
    uint8_t *end_name;
    char *name;
    xfs_ino_t ino;
    int retval = 0;

    bmbt_irec_get(&r, (xfs_bmbt_rec_t *)&core->di_literal_area[0]);
    dir_blk = fsblock_to_bytes(fs, r.br_startblock) >> BLOCK_SHIFT(fs);

    dirblk_buf = xfs_dir2_get_dirblks(fs, dir_blk, r.br_blockcount);
    hdr = (xfs_dir2_data_hdr_t *)dirblk_buf;
    if (be32_to_cpu(hdr->magic) != XFS_DIR2_BLOCK_MAGIC) {
        xfs_error("Block directory header's magic number does not match!");
        xfs_debug("hdr->magic: 0x%lx", be32_to_cpu(hdr->magic));

	free(dirblk_buf);

	return -1;
    }

    btp = xfs_dir2_block_tail_p(XFS_INFO(fs), hdr);

    if (file->offset + 1 > be32_to_cpu(btp->count))
	return -1;

    file->offset++;

    p = (uint8_t *)(hdr + 1);

    if (file->offset - 1) {
	offset = file->offset;
	while (--offset) {
	    dep = (xfs_dir2_data_entry_t *)p;

	    dup = (xfs_dir2_data_unused_t *)p;
	    if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
		p += be16_to_cpu(dup->length);
		continue;
	    }

	    p += xfs_dir2_data_entsize(dep->namelen);
	}
    }

    dep = (xfs_dir2_data_entry_t *)p;

    start_name = &dep->name[0];
    end_name = start_name + dep->namelen;
    name = xfs_dir2_get_entry_name(start_name, end_name);

    ino = be64_to_cpu(dep->inumber);

    retval = fill_dirent(fs, dirent, file->offset, ino, name,
			 end_name - start_name);
    if (retval)
	xfs_error("Failed to fill in dirent structure");

    free(dirblk_buf);
    free(name);

    return retval;
}

static int xfs_dir2_leaf_readdir(struct file *file, struct dirent *dirent,
				 xfs_dinode_t *core)
{
    xfs_bmbt_irec_t irec;
    struct fs_info *fs = file->fs;
    xfs_dir2_leaf_t *leaf;
    block_t leaf_blk, dir_blk;
    xfs_dir2_leaf_entry_t *lep;
    uint32_t newdb;
    uint32_t curdb = -1;
    xfs_dir2_data_entry_t *dep;
    xfs_dir2_data_hdr_t *data_hdr;
    uint8_t *start_name;
    uint8_t *end_name;
    char *name;
    xfs_intino_t ino;
    uint8_t *buf = NULL;
    int retval = 0;

    bmbt_irec_get(&irec, ((xfs_bmbt_rec_t *)&core->di_literal_area[0]) +
					be32_to_cpu(core->di_nextents) - 1);
    leaf_blk = fsblock_to_bytes(fs, irec.br_startblock) >>
					BLOCK_SHIFT(file->fs);

    leaf = (xfs_dir2_leaf_t *)xfs_dir2_get_dirblks(fs, leaf_blk,
						   irec.br_blockcount);
    if (be16_to_cpu(leaf->hdr.info.magic) != XFS_DIR2_LEAF1_MAGIC) {
        xfs_error("Single leaf block header's magic number does not match!");
        goto out;
    }

    if (!leaf->hdr.count)
        goto out;

    if (file->offset + 1 > be16_to_cpu(leaf->hdr.count))
	goto out;

    lep = &leaf->ents[file->offset++];

    /* Skip over stale leaf entries */
    for ( ; be32_to_cpu(lep->address) == XFS_DIR2_NULL_DATAPTR;
	  lep++, file->offset++);

    newdb = xfs_dir2_dataptr_to_db(fs, be32_to_cpu(lep->address));
    if (newdb != curdb) {
	if (buf)
	    free(buf);

	bmbt_irec_get(&irec,
		      ((xfs_bmbt_rec_t *)&core->di_literal_area[0]) + newdb);
	dir_blk = fsblock_to_bytes(fs, irec.br_startblock) >>
						BLOCK_SHIFT(fs);
	buf = xfs_dir2_get_dirblks(fs, dir_blk, irec.br_blockcount);
	data_hdr = (xfs_dir2_data_hdr_t *)buf;
	if (be32_to_cpu(data_hdr->magic) != XFS_DIR2_DATA_MAGIC) {
	    xfs_error("Leaf directory's data magic number does not match!");
	    goto out1;
	}

	curdb = newdb;
    }

    dep = (xfs_dir2_data_entry_t *)(
	(char *)buf + xfs_dir2_dataptr_to_off(fs,
					      be32_to_cpu(lep->address)));

    start_name = &dep->name[0];
    end_name = start_name + dep->namelen;
    name = xfs_dir2_get_entry_name(start_name, end_name);

    ino = be64_to_cpu(dep->inumber);

    retval = fill_dirent(fs, dirent, file->offset, ino, name,
			 end_name - start_name);
    if (retval)
	xfs_error("Failed to fill in dirent structure");

    free(name);
    free(buf);
    free(leaf);

    return retval;

out1:
    free(buf);

out:
    free(leaf);

    return -1;
}

static int xfs_dir2_node_readdir(struct file *file, struct dirent *dirent,
				 xfs_dinode_t *core)
{
    (void)file;
    (void)dirent;
    (void)core;

    return -1;
}

static int xfs_fmt_extents_readdir(struct file *file, struct dirent *dirent,
				   xfs_dinode_t *core)
{
    int retval;

    if (be32_to_cpu(core->di_nextents) <= 1) {
	/* Single-block Directories */
	retval = xfs_dir2_block_readdir(file, dirent, core);
    } else if (xfs_dir2_isleaf(file->fs, core)) {
	/* Leaf Directory */
	retval = xfs_dir2_leaf_readdir(file, dirent, core);
    } else {
	/* Node Directory */
	retval = xfs_dir2_node_readdir(file, dirent, core);
    }

    return retval;
}

static int xfs_readdir(struct file *file, struct dirent *dirent)
{
    struct fs_info *fs = file->fs;
    xfs_dinode_t *core;
    struct inode *inode = file->inode;
    int retval = -1;

    core = xfs_dinode_get_core(fs, inode->ino);
    if (!core) {
	xfs_error("Failed to get dinode from disk (ino %llx)", inode->ino);
	return -1;
    }

    if (core->di_format == XFS_DINODE_FMT_LOCAL)
	retval = xfs_fmt_local_readdir(file, dirent, core);
    else if (core->di_format == XFS_DINODE_FMT_EXTENTS)
	retval = xfs_fmt_extents_readdir(file, dirent, core);

    return retval;
}

static uint32_t xfs_getfssec(struct file *file, char *buf, int sectors,
			     bool *have_more)
{
    return generic_getfssec(file, buf, sectors, have_more);
}

static int xfs_next_extent(struct inode *inode, uint32_t lstart)
{
    struct fs_info *fs = inode->fs;
    xfs_dinode_t *core = NULL;
    xfs_bmbt_irec_t rec;
    block_t blk;

    (void)lstart;

    xfs_debug("in");

    core = xfs_dinode_get_core(fs, inode->ino);
    if (!core) {
	xfs_error("Failed to get dinode from disk (ino %llx)", inode->ino);
	goto out;
    }

    if (core->di_format == XFS_DINODE_FMT_EXTENTS) {
	/* The data fork contains the file's data extents */
	if (XFS_PVT(inode)->i_cur_extent == be32_to_cpu(core->di_nextents))
	    goto out;

	bmbt_irec_get(&rec, (xfs_bmbt_rec_t *)&core->di_literal_area[0] +
						XFS_PVT(inode)->i_cur_extent++);

	blk = fsblock_to_bytes(fs, rec.br_startblock) >> BLOCK_SHIFT(fs);

	XFS_PVT(inode)->i_offset = rec.br_startoff;

	inode->next_extent.pstart = blk << BLOCK_SHIFT(fs) >> SECTOR_SHIFT(fs);
	inode->next_extent.len = ((rec.br_blockcount << BLOCK_SHIFT(fs)) +
				  SECTOR_SIZE(fs) - 1) >> SECTOR_SHIFT(fs);
    }

    return 0;

out:
    return -1;
}

static struct inode *xfs_fmt_extents_find_entry(const char *dname,
						struct inode *parent,
						xfs_dinode_t *core)
{
    struct inode *inode;

    xfs_debug("parent ino %llu", parent->ino);

    if (be32_to_cpu(core->di_nextents) <= 1) {
        /* Single-block Directories */
        inode = xfs_dir2_block_find_entry(dname, parent, core);
    } else if (xfs_dir2_isleaf(parent->fs, core)) {
        /* Leaf Directory */
	inode = xfs_dir2_leaf_find_entry(dname, parent, core);
    } else {
        /* Node Directory */
        inode = xfs_dir2_node_find_entry(dname, parent, core);
    }

    return inode;
}

static struct inode *xfs_iget(const char *dname, struct inode *parent)
{
    struct fs_info *fs = parent->fs;
    xfs_dinode_t *core = NULL;
    struct inode *inode = NULL;

    xfs_debug("dname %s parent %p parent ino %lu", dname, parent, parent->ino);

    core = xfs_dinode_get_core(fs, parent->ino);
    if (!core) {
        xfs_error("Failed to get dinode from disk (ino 0x%llx)", parent->ino);
        goto out;
    }

    if (core->di_format == XFS_DINODE_FMT_LOCAL) {
	inode = xfs_dir2_local_find_entry(dname, parent, core);
    } else if (core->di_format == XFS_DINODE_FMT_EXTENTS) {
        inode = xfs_fmt_extents_find_entry(dname, parent, core);
    } else {
	xfs_debug("format %hhu", core->di_format);
	xfs_debug("TODO: format \"local\" and \"extents\" are the only "
		  "supported ATM");
	goto out;
    }

    if (!inode) {
	xfs_debug("Entry not found!");
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

    core = xfs_dinode_get_core(fs, XFS_INFO(fs)->rootino);
    if (!core) {
	xfs_error("Inode core's magic number does not match!");
	xfs_debug("magic number 0x%04x", be16_to_cpu(core->di_magic));
	goto out;
    }

    fill_xfs_inode_pvt(fs, inode, XFS_INFO(fs)->rootino);

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
    info->dirblksize		= 1 << (sb->sb_blocklog + sb->sb_dirblklog);
    info->dirblklog		= sb->sb_dirblklog;
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

    XFS_INFO(fs)->dirleafblk = xfs_dir2_db_to_da(fs, XFS_DIR2_LEAF_FIRSTDB(fs));

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
    .readdir		= xfs_readdir,
    .iget		= xfs_iget,
    .next_extent	= xfs_next_extent,
};
