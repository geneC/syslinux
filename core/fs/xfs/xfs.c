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

static xfs_agi_t *xfs_get_agi(struct fs_info *fs, xfs_ino_t ino)
{
    xfs_agnumber_t agno;
    block_t blk;
    xfs_agi_t *agi;

    agno = XFS_INO_TO_AGNO(fs, ino);
    if (agno >= XFS_INFO(fs)->agcount) {
	xfs_error("Invalid AG number");
	goto out;
    }

    blk = agnumber_to_bytes(fs, agno) >> BLOCK_SHIFT(fs);
    agi = XFS_AGI_OFFS(fs, get_cache(fs->fs_dev, blk));
    if (!agi) {
	xfs_error("Error in reading filesystem block 0x%llX (%llu)", blk, blk);
	goto out;
    }

    if (be32_to_cpu(agi->agi_magicnum) !=
	be32_to_cpu(*(uint32_t *)XFS_AGI_MAGIC)) {
	xfs_error("AGI's magic number does not match!");
	goto out;
    }

    xfs_debug("agi_count %lu", be32_to_cpu(agi->agi_count));
    xfs_debug("agi_level %lu", be32_to_cpu(agi->agi_level));

    return agi;

out:
    return NULL;
}

static xfs_dinode_t *xfs_get_ino_core(struct fs_info *fs, xfs_ino_t ino)
{
    block_t blk;
    xfs_dinode_t *core;

    xfs_debug("ino %lu", ino);

    blk = ino_to_bytes(fs, ino) >> BLOCK_SHIFT(fs);

    xfs_debug("blk %llu block offset 0x%llx", blk, blk << BLOCK_SHIFT(fs));

    core = (xfs_dinode_t *)get_cache(fs->fs_dev, blk);
    if (!core) {
	xfs_error("Error in reading filesystem block 0x%llX (%llu)", blk, blk);
	goto out;
    }

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

/* Allocate a region for the chunk of 64 inodes found in the leaf entries of
 * inode B+Trees, and return that allocated region.
 */
static const void *xfs_get_ino_chunk(struct fs_info *fs, xfs_ino_t ino)
{
    const int len = 64 * XFS_INFO(fs)->inodesize;
    void *buf;
    block_t nblks;
    uint8_t *p;
    block_t start_blk = ino_to_bytes(fs, ino) >> BLOCK_SHIFT(fs);
    off_t offset = 0;

    buf = malloc(len);
    if (!buf)
	malloc_error("buffer memory");

    memset(buf, 0, len);

    nblks = len >> BLOCK_SHIFT(fs);
    while (nblks--) {
	p = (uint8_t *)get_cache(fs->fs_dev, start_blk++);

	memcpy(buf + offset, p, BLOCK_SIZE(fs));
	offset += BLOCK_SIZE(fs);
    }

    return buf;
}

static inline void xfs_ino_core_free(struct inode *inode, xfs_dinode_t *core)
{
    if (core && (inode && XFS_PVT(inode)->i_chunk_offset))
	free((void *)((uint8_t *)core - XFS_PVT(inode)->i_chunk_offset));
}

/* Find an inode from a chunk of 64 inodes by giving its inode # */
static xfs_dinode_t *xfs_find_chunk_ino(struct fs_info *fs,
					block_t start_ino, xfs_ino_t ino,
					uint64_t *chunk_offset)
{
    uint8_t *p;
    uint32_t mask = (uint32_t)((1ULL << XFS_INFO(fs)->inopb_shift) - 1);

    xfs_debug("start_ino %llu ino %llu", start_ino, ino);

    if (start_ino == ino) {
	*chunk_offset = 0;
	return xfs_get_ino_core(fs, ino);
    }

    p = (uint8_t *)xfs_get_ino_chunk(fs, start_ino);
    /* Get the inode number within the chunk from lower bits and calculate
     * the offset (lower bits * inode size).
     */
    *chunk_offset = (uint64_t)((int)ino & mask) << XFS_INFO(fs)->inode_shift;

    xfs_debug("chunk_offset %llu", *chunk_offset);

    return (xfs_dinode_t *)((uint8_t *)p + *chunk_offset);
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
    block_t parent_blk;
    block_t blk;
    xfs_agi_t *agi;
    xfs_btree_sblock_t *ibt_hdr;
    uint16_t i;
    xfs_inobt_rec_t *rec;
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

    XFS_PVT(inode)->i_chunk_offset = 0;

    ino = xfs_dir2_sf_get_inumber(sf, (xfs_dir2_inou_t *)(
				      (uint8_t *)sf_entry +
				      offsetof(struct xfs_dir2_sf_entry,
					       name[0]) +
				      sf_entry->namelen));

    xfs_debug("entry inode's number %lu", ino);

    /* Check if the inode's filesystem block is the as the parent inode */
    parent_blk = ino_to_bytes(fs, parent->ino) >> BLOCK_SHIFT(fs);
    blk = ino_to_bytes(fs, ino) >> BLOCK_SHIFT(fs);

    xfs_debug("parent_blk %llu blk %llu", parent_blk, blk);

    if (parent_blk != blk)
	goto no_agi_needed;

    agi = xfs_get_agi(fs, ino);
    if (!agi) {
	xfs_error("Failed to get AGI from inode %lu", ino);
	goto out;
    }

    blk = agnumber_to_bytes(fs, XFS_INO_TO_AGNO(fs, ino)) >> BLOCK_SHIFT(fs);
    XFS_PVT(inode)->i_agblock = blk;

    /* Get block number relative to the AG containing the root of the inode
     * B+tree.
     */
    blk += be32_to_cpu(agi->agi_root);;

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

    xfs_debug("bb_level %lu", be16_to_cpu(ibt_hdr->bb_level));
    xfs_debug("bb_numrecs %lu", be16_to_cpu(ibt_hdr->bb_numrecs));

    rec = (xfs_inobt_rec_t *)((uint8_t *)ibt_hdr + sizeof *ibt_hdr);
    for (i = be16_to_cpu(ibt_hdr->bb_numrecs); i; i--, rec++) {
	xfs_debug("freecount %lu free 0x%llx", be32_to_cpu(rec->ir_freecount),
		  be64_to_cpu(rec->ir_free));

	ncore = xfs_find_chunk_ino(fs, be32_to_cpu(rec->ir_startino),
				   ino, &XFS_PVT(inode)->i_chunk_offset);
	if (ncore) {
	    if (be16_to_cpu(ncore->di_magic) ==
		be16_to_cpu(*(uint16_t *)XFS_DINODE_MAGIC)) {
		goto core_found;
	    } else {
		xfs_error("Inode core's magic number does not match!");
		xfs_debug("magic number 0x%04x", be16_to_cpu(ncore->di_magic));
		goto out;
	    }
	}
    }

out:
    xfs_ino_core_free(inode, ncore);
    free(inode);

    return NULL;

no_agi_needed:
    ncore = xfs_get_ino_core(fs, ino);

core_found:
    inode->ino			= ino;
    XFS_PVT(inode)->i_ino_blk	= ino_to_bytes(fs, ino) >> BLOCK_SHIFT(fs);
    inode->size 		= be64_to_cpu(ncore->di_size);

    if (be16_to_cpu(ncore->di_mode) & S_IFDIR)
	inode->mode = DT_DIR;
    else if (be16_to_cpu(ncore->di_mode) & S_IFREG)
	inode->mode = DT_REG;

    xfs_debug("Found a %s inode", inode->mode == DT_DIR ? "directory" : "file");

    xfs_ino_core_free(inode, ncore);

    return inode;
}

static struct inode *xfs_iget(const char *dname, struct inode *parent)
{
    struct fs_info *fs = parent->fs;
    xfs_dinode_t *core = NULL;
    struct inode *inode = NULL;

    xfs_debug("dname %s parent %p parent ino %lu", dname, parent, parent->ino);

    /* Check if we need the region for the chunk of 64 inodes */
    if (XFS_PVT(parent)->i_chunk_offset) {
	core = (xfs_dinode_t *)((uint8_t *)xfs_get_ino_chunk(fs, parent->ino) +
				XFS_PVT(parent)->i_chunk_offset);

	xfs_debug("core's magic number 0x%04x", be16_to_cpu(core->di_magic));

	if (be16_to_cpu(core->di_magic) !=
	    be16_to_cpu(*(uint16_t *)XFS_DINODE_MAGIC)) {
	    xfs_error("Inode core's magic number does not match!");
	    goto out;
	}
    } else {
	core = xfs_get_ino_core(fs, parent->ino);
    }

    if (parent->mode == DT_DIR) { /* Is this inode a directory ? */
	xfs_debug("Parent inode is a directory");

	/* TODO: Handle both shortform directories and directory blocks */
	if (core->di_format == XFS_DINODE_FMT_LOCAL) {
	    inode = xfs_fmt_local_find_entry(dname, parent, core);
	} else {
	    xfs_debug("format %hhu", core->di_format);
	    xfs_debug("TODO: format \"local\" is the only supported ATM");
	    goto out;
	}
    }

    xfs_ino_core_free(inode, core);

    return inode;

out:
    xfs_ino_core_free(inode, core);

    return NULL;
}

static struct inode *xfs_iget_root(struct fs_info *fs)
{
    xfs_agi_t *agi;
    block_t blk;
    xfs_btree_sblock_t *ibt_hdr;
    uint16_t i;
    xfs_inobt_rec_t *rec;
    xfs_dinode_t *core = NULL;
    struct inode *inode = xfs_new_inode(fs);

    xfs_debug("Looking for the root inode...");

    agi = xfs_get_agi(fs, XFS_INFO(fs)->rootino);
    if (!agi) {
	xfs_error("Failed to get AGI from inode %lu", XFS_INFO(fs)->rootino);
	goto out;
    }

    blk = agnumber_to_bytes(fs, XFS_INO_TO_AGNO(fs, XFS_INFO(fs)->rootino)) >>
								BLOCK_SHIFT(fs);
    XFS_PVT(inode)->i_agblock = blk;

    /* Get block number relative to the AG containing the root of the inode
     * B+tree.
     */
    blk += be32_to_cpu(agi->agi_root);;

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

    xfs_debug("bb_level %lu", be16_to_cpu(ibt_hdr->bb_level));
    xfs_debug("bb_numrecs %lu", be16_to_cpu(ibt_hdr->bb_numrecs));

    XFS_PVT(inode)->i_chunk_offset = 0;

    rec = (xfs_inobt_rec_t *)((uint8_t *)ibt_hdr + sizeof *ibt_hdr);
    for (i = be16_to_cpu(ibt_hdr->bb_numrecs); i--; rec++) {
	xfs_debug("freecount %lu free 0x%llx", be32_to_cpu(rec->ir_freecount),
		  be64_to_cpu(rec->ir_free));

	core = xfs_find_chunk_ino(fs, be32_to_cpu(rec->ir_startino),
				  XFS_INFO(fs)->rootino,
				  &XFS_PVT(inode)->i_chunk_offset);
	if (core) {
	    if (be16_to_cpu(core->di_magic) ==
		be16_to_cpu(*(uint16_t *)XFS_DINODE_MAGIC)) {
		goto found;
	    } else {
		xfs_error("Inode core's magic number does not match!");
		xfs_debug("magic number 0x%04x", be16_to_cpu(core->di_magic));
		goto out;
	    }
	}
    }

    xfs_error("Root inode not found!");
    goto not_found;

found:
    xfs_debug("Root inode has been found!");

    if (!(be16_to_cpu(core->di_mode) & S_IFDIR)) {
	xfs_error("root inode is not a directory ?! No makes sense...");
	goto out;
    }

    XFS_PVT(inode)->i_ino_blk	= ino_to_bytes(fs, XFS_INFO(fs)->rootino) >>
								BLOCK_SHIFT(fs);
    inode->ino			= XFS_INFO(fs)->rootino;
    inode->mode 		= DT_DIR;
    inode->size 		= be64_to_cpu(core->di_size);

    xfs_ino_core_free(inode, core);

    return inode;

not_found:

out:
    xfs_ino_core_free(inode, core);
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
    .getfssec		= NULL,
    .load_config	= generic_load_config,
    .close_file         = generic_close_file,
    .mangle_name	= generic_mangle_name,
    .readdir		= NULL,
    .iget		= xfs_iget,
    .next_extent	= NULL,
};
