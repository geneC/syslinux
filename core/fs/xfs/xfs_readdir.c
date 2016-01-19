/*
 * Copyright (c) 2012-2015 Paulo Alcantara <pcacjr@zytor.com>
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

#include <cache.h>
#include <core.h>
#include <fs.h>

#include "xfs_types.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "misc.h"
#include "xfs.h"
#include "xfs_dinode.h"
#include "xfs_dir2.h"

#include "xfs_readdir.h"

static int fill_dirent(struct fs_info *fs, struct dirent *dirent,
		       uint32_t offset, xfs_ino_t ino, char *name,
		       size_t namelen)
{
    xfs_dinode_t *core;

    xfs_debug("fs %p, dirent %p offset %lu ino %llu name %s namelen %llu", fs,
	      dirent, offset, ino, name, namelen);

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
    else if (be16_to_cpu(core->di_mode) & S_IFLNK)
        dirent->d_type = DT_LNK;

    memcpy(dirent->d_name, name, namelen);
    dirent->d_name[namelen] = '\0';

    return 0;
}

int xfs_readdir_dir2_local(struct file *file, struct dirent *dirent,
			   xfs_dinode_t *core)
{
    xfs_dir2_sf_t *sf = XFS_DFORK_PTR(core, XFS_DATA_FORK);
    xfs_dir2_sf_entry_t *sf_entry;
    uint8_t ftypelen = core->di_version == 3 ? 1 : 0;
    uint8_t count = sf->hdr.i8count ? sf->hdr.i8count : sf->hdr.count;
    uint32_t offset = file->offset;
    xfs_dir2_inou_t *inou;
    uint8_t *start_name;
    uint8_t *end_name;
    xfs_ino_t ino;
    struct fs_info *fs = file->fs;
    int retval = 0;

    xfs_debug("file %p dirent %p core %p", file, dirent, core);
    xfs_debug("count %hhu i8count %hhu", sf->hdr.count, sf->hdr.i8count);

    if (file->offset + 1 > count)
	goto out;

    file->offset++;

    sf_entry = (xfs_dir2_sf_entry_t *)((uint8_t *)sf->list -
				       (!sf->hdr.i8count ? 4 : 0));

    if (file->offset - 1) {
	offset = file->offset;
	while (--offset) {
	    sf_entry = (xfs_dir2_sf_entry_t *)(
				(uint8_t *)sf_entry +
				offsetof(struct xfs_dir2_sf_entry,
					 name) +
				sf_entry->namelen +
				ftypelen +
				(sf->hdr.i8count ? 8 : 4));
	}
    }

    start_name = sf_entry->name;
    end_name = start_name + sf_entry->namelen;

    inou = (xfs_dir2_inou_t *)((uint8_t *)sf_entry +
			       offsetof(struct xfs_dir2_sf_entry,
					name) +
			       sf_entry->namelen +
			       ftypelen);
    ino = xfs_dir2_sf_get_inumber(sf, inou);

    retval = fill_dirent(fs, dirent, file->offset, ino, (char *)start_name,
			 end_name - start_name);
    if (retval)
	xfs_error("Failed to fill in dirent structure");

    return retval;

out:
    xfs_dir2_dirblks_flush_cache();

    return -1;
}

int xfs_readdir_dir2_block(struct file *file, struct dirent *dirent,
			   xfs_dinode_t *core)
{
    xfs_bmbt_irec_t r;
    block_t dir_blk;
    struct fs_info *fs = file->fs;
    const uint8_t *dirblk_buf;
    bool isdir3;
    uint8_t *p;
    uint32_t offset;
    xfs_dir2_data_hdr_t *hdr;
    xfs_dir2_block_tail_t *btp;
    xfs_dir2_data_unused_t *dup;
    xfs_dir2_data_entry_t *dep;
    uint8_t *start_name;
    uint8_t *end_name;
    xfs_ino_t ino;
    int retval = 0;

    xfs_debug("file %p dirent %p core %p", file, dirent, core);

    bmbt_irec_get(&r, XFS_DFORK_PTR(core, XFS_DATA_FORK));
    dir_blk = fsblock_to_bytes(fs, r.br_startblock) >> BLOCK_SHIFT(fs);

    dirblk_buf = xfs_dir2_dirblks_get_cached(fs, dir_blk, r.br_blockcount);
    hdr = (xfs_dir2_data_hdr_t *)dirblk_buf;
    if (be32_to_cpu(hdr->magic) == XFS_DIR2_BLOCK_MAGIC) {
	isdir3 = false;
    } else if (be32_to_cpu(hdr->magic) == XFS_DIR3_BLOCK_MAGIC) {
	isdir3 = true;
    } else {
        xfs_error("Block directory header's magic number does not match!");
        xfs_debug("hdr->magic: 0x%lx", be32_to_cpu(hdr->magic));
	goto out;
    }

    btp = xfs_dir2_block_tail_p(XFS_INFO(fs), hdr);

    if (file->offset + 1 > be32_to_cpu(btp->count))
	goto out;

    file->offset++;

    p = (uint8_t *)dirblk_buf + (isdir3 ? sizeof(struct xfs_dir3_data_hdr) :
				 sizeof(struct xfs_dir2_data_hdr));

    if (file->offset - 1) {
	offset = file->offset;
	while (--offset) {
	    dep = (xfs_dir2_data_entry_t *)p;

	    dup = (xfs_dir2_data_unused_t *)p;
	    if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
		p += be16_to_cpu(dup->length);
		continue;
	    }

	    p += (isdir3 ? xfs_dir3_data_entsize(dep->namelen) :
		  xfs_dir2_data_entsize(dep->namelen));
	}
    }

    dep = (xfs_dir2_data_entry_t *)p;

    start_name = &dep->name[0];
    end_name = start_name + dep->namelen;

    ino = be64_to_cpu(dep->inumber);

    retval = fill_dirent(fs, dirent, file->offset, ino, (char *)start_name,
			 end_name - start_name);
    if (retval)
	xfs_error("Failed to fill in dirent structure");

    return retval;

out:
    xfs_dir2_dirblks_flush_cache();

    return -1;
}

int xfs_readdir_dir2_leaf(struct file *file, struct dirent *dirent,
			  xfs_dinode_t *core)
{
    xfs_dir2_leaf_hdr_t *hdr;
    xfs_dir2_leaf_entry_t *ents;
    uint16_t count;
    xfs_bmbt_irec_t irec;
    struct fs_info *fs = file->fs;
    block_t leaf_blk, dir_blk;
    xfs_dir2_leaf_entry_t *lep;
    uint32_t db;
    unsigned int offset;
    xfs_dir2_data_entry_t *dep;
    xfs_dir2_data_hdr_t *data_hdr;
    uint8_t *start_name;
    uint8_t *end_name;
    xfs_intino_t ino;
    const uint8_t *buf = NULL;
    int retval = 0;

    xfs_debug("file %p dirent %p core %p", file, dirent, core);

    bmbt_irec_get(&irec, (xfs_bmbt_rec_t *)XFS_DFORK_PTR(core, XFS_DATA_FORK) +
					be32_to_cpu(core->di_nextents) - 1);
    leaf_blk = fsblock_to_bytes(fs, irec.br_startblock) >>
					BLOCK_SHIFT(file->fs);

    hdr = xfs_dir2_dirblks_get_cached(fs, leaf_blk, irec.br_blockcount);
    if (!hdr)
        return -1;

    if (be16_to_cpu(hdr->info.magic) == XFS_DIR2_LEAF1_MAGIC) {
	count = be16_to_cpu(hdr->count);
	ents = (xfs_dir2_leaf_entry_t *)((uint8_t *)hdr +
					 sizeof(struct xfs_dir2_leaf_hdr));
    } else if (be16_to_cpu(hdr->info.magic) == XFS_DIR3_LEAF1_MAGIC) {
	count = be16_to_cpu(((xfs_dir3_leaf_hdr_t *)hdr)->count);
	ents = (xfs_dir2_leaf_entry_t *)((uint8_t *)hdr +
					 sizeof(struct xfs_dir3_leaf_hdr));
    } else {
        xfs_error("Single leaf block header's magic number does not match!");
        goto out;
    }

    if (!count || file->offset + 1 > count)
	goto out;

    lep = &ents[file->offset++];

    /* Skip over stale leaf entries */
    for ( ; be32_to_cpu(lep->address) == XFS_DIR2_NULL_DATAPTR;
	  lep++, file->offset++)
	;

    db = xfs_dir2_dataptr_to_db(fs, be32_to_cpu(lep->address));

    bmbt_irec_get(&irec, (xfs_bmbt_rec_t *)XFS_DFORK_PTR(core,
							 XFS_DATA_FORK) + db);

    dir_blk = fsblock_to_bytes(fs, irec.br_startblock) >> BLOCK_SHIFT(fs);

    buf = xfs_dir2_dirblks_get_cached(fs, dir_blk, irec.br_blockcount);
    data_hdr = (xfs_dir2_data_hdr_t *)buf;
    if (be32_to_cpu(data_hdr->magic) != XFS_DIR2_DATA_MAGIC &&
	be32_to_cpu(data_hdr->magic) != XFS_DIR3_DATA_MAGIC) {
	xfs_error("Leaf directory's data magic number does not match!");
	goto out;
    }

    offset = xfs_dir2_dataptr_to_off(fs, be32_to_cpu(lep->address));

    dep = (xfs_dir2_data_entry_t *)((uint8_t *)buf + offset);

    start_name = &dep->name[0];
    end_name = start_name + dep->namelen;

    ino = be64_to_cpu(dep->inumber);

    retval = fill_dirent(fs, dirent, file->offset, ino, (char *)start_name,
			 end_name - start_name);
    if (retval)
	xfs_error("Failed to fill in dirent structure");

    return retval;

out:
    xfs_dir2_dirblks_flush_cache();

    return -1;
}

int xfs_readdir_dir2_node(struct file *file, struct dirent *dirent,
			  xfs_dinode_t *core)
{
    struct fs_info *fs = file->fs;
    block_t fsblkno;
    xfs_da_node_hdr_t *nhdr;
    xfs_da_node_entry_t *btree;
    uint16_t btcount;
    uint16_t lfcount;
    xfs_dir2_leaf_hdr_t *lhdr;
    xfs_dir2_leaf_entry_t *ents;
    struct inode *inode = file->inode;
    int error;
    xfs_dir2_data_hdr_t *data_hdr;
    xfs_dir2_leaf_entry_t *lep;
    unsigned int offset;
    xfs_dir2_data_entry_t *dep;
    uint8_t *start_name;
    uint8_t *end_name;
    uint32_t db;
    const uint8_t *buf = NULL;
    int retval = 0;

    xfs_debug("file %p dirent %p core %p", file, dirent, core);

    db = xfs_dir2_byte_to_db(fs, XFS_DIR2_LEAF_OFFSET);
    fsblkno = xfs_dir2_get_right_blk(fs, core, db, &error);
    if (error) {
	xfs_error("Cannot find fs block");
	return -1;
    }

    nhdr = xfs_dir2_dirblks_get_cached(fs, fsblkno, 1);
    if (be16_to_cpu(nhdr->info.magic) == XFS_DA_NODE_MAGIC) {
	btcount = be16_to_cpu(nhdr->count);
	btree = (xfs_da_node_entry_t *)((uint8_t *)nhdr +
					sizeof(struct xfs_da_node_hdr));
    } else if (be16_to_cpu(nhdr->info.magic) == XFS_DA3_NODE_MAGIC) {
	btcount = be16_to_cpu(((xfs_da3_node_hdr_t *)nhdr)->count);
	btree = (xfs_da_node_entry_t *)((uint8_t *)nhdr +
					sizeof(struct xfs_da3_node_hdr));
    } else {
        xfs_error("Node's magic number (0x%04x) does not match!",
		  be16_to_cpu(nhdr->info.magic));
        goto out;
    }

try_next_btree:
    if (!btcount ||
	XFS_PVT(inode)->i_btree_offset >= btcount)
	goto out;

    fsblkno = be32_to_cpu(btree[XFS_PVT(inode)->i_btree_offset].before);
    fsblkno = xfs_dir2_get_right_blk(fs, core, fsblkno, &error);
    if (error) {
        xfs_error("Cannot find leaf rec!");
        goto out;
    }

    lhdr = xfs_dir2_dirblks_get_cached(fs, fsblkno, 1);
    if (be16_to_cpu(lhdr->info.magic) == XFS_DIR2_LEAFN_MAGIC) {
	lfcount = be16_to_cpu(lhdr->count);
	ents = (xfs_dir2_leaf_entry_t *)((uint8_t *)lhdr +
					 sizeof(struct xfs_dir2_leaf_hdr));
    } else if (be16_to_cpu(lhdr->info.magic) == XFS_DIR3_LEAFN_MAGIC) {
	lfcount = be16_to_cpu(((xfs_dir3_leaf_hdr_t *)lhdr)->count);
	ents = (xfs_dir2_leaf_entry_t *)((uint8_t *)lhdr +
					 sizeof(struct xfs_dir3_leaf_hdr));
    } else {
        xfs_error("Leaf's magic number does not match (0x%04x)!",
		  be16_to_cpu(lhdr->info.magic));
        goto out;
    }

    if (!lfcount ||
	XFS_PVT(inode)->i_leaf_ent_offset >= lfcount) {
	XFS_PVT(inode)->i_btree_offset++;
	XFS_PVT(inode)->i_leaf_ent_offset = 0;
	goto try_next_btree;
    }

    lep = &ents[XFS_PVT(inode)->i_leaf_ent_offset];

    /* Skip over stale leaf entries */
    for ( ; XFS_PVT(inode)->i_leaf_ent_offset < lfcount &&
			be32_to_cpu(lep->address) == XFS_DIR2_NULL_DATAPTR;
	  lep++, XFS_PVT(inode)->i_leaf_ent_offset++)
	;

    if (XFS_PVT(inode)->i_leaf_ent_offset == lfcount) {
	XFS_PVT(inode)->i_btree_offset++;
	XFS_PVT(inode)->i_leaf_ent_offset = 0;
	goto try_next_btree;
    } else {
	XFS_PVT(inode)->i_leaf_ent_offset++;
    }

    db = xfs_dir2_dataptr_to_db(fs, be32_to_cpu(lep->address));

    fsblkno = xfs_dir2_get_right_blk(fs, core, db, &error);
    if (error) {
	xfs_error("Cannot find data block!");
	goto out;
    }

    buf = xfs_dir2_dirblks_get_cached(fs, fsblkno, 1);
    data_hdr = (xfs_dir2_data_hdr_t *)buf;
    if (be32_to_cpu(data_hdr->magic) != XFS_DIR2_DATA_MAGIC &&
	be32_to_cpu(data_hdr->magic) != XFS_DIR3_DATA_MAGIC) {
	xfs_error("Leaf directory's data magic No. does not match!");
	goto out;
    }

    offset = xfs_dir2_dataptr_to_off(fs, be32_to_cpu(lep->address));

    dep = (xfs_dir2_data_entry_t *)((uint8_t *)buf + offset);

    start_name = &dep->name[0];
    end_name = start_name + dep->namelen;

    retval = fill_dirent(fs, dirent, 0, be64_to_cpu(dep->inumber),
			 (char *)start_name, end_name - start_name);
    if (retval)
	xfs_error("Failed to fill in dirent structure");

    return retval;

out:
    xfs_dir2_dirblks_flush_cache();

    XFS_PVT(inode)->i_btree_offset = 0;
    XFS_PVT(inode)->i_leaf_ent_offset = 0;

    return -1;
}
