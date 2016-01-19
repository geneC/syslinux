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

#define XFS_DIR2_DIRBLKS_CACHE_SIZE 128

struct xfs_dir2_dirblks_cache {
    block_t        dc_startblock;
    xfs_filblks_t  dc_blkscount;
    void          *dc_area;
};

static struct xfs_dir2_dirblks_cache dirblks_cache[XFS_DIR2_DIRBLKS_CACHE_SIZE];
static unsigned char                 dirblks_cached_count = 0;

uint32_t xfs_dir2_da_hashname(const uint8_t *name, int namelen)
{
    uint32_t hash;

    /*
     * Do four characters at a time as long as we can.
     */
    for (hash = 0; namelen >= 4; namelen -=4, name += 4)
        hash = (name[0] << 21) ^ (name[1] << 14) ^ (name[2] << 7) ^
               (name[3] << 0) ^ rol32(hash, 7 * 4);

    /*
     * Now do the rest of the characters.
     */
    switch (namelen) {
    case 3:
        return (name[0] << 14) ^ (name[1] << 7) ^ (name[2] << 0) ^
               rol32(hash, 7 * 3);
    case 2:
        return (name[0] << 7) ^ (name[1] << 0) ^ rol32(hash, 7 * 2);
    case 1:
        return (name[0] << 0) ^ rol32(hash, 7 * 1);
    default: /* case 0: */
        return hash;
    }
}

static void *get_dirblks(struct fs_info *fs, block_t startblock,
                         xfs_filblks_t c)
{
    const size_t len = c * XFS_INFO(fs)->dirblksize;
    uint64_t offs = startblock << BLOCK_SHIFT(fs);
    void *buf;
    size_t ret;

    buf = malloc(len);
    if (!buf)
        malloc_error("buffer memory");
    memset(buf, 0, len);

    ret = cache_read(fs, buf, offs, len);
    if (ret != len) {
        xfs_error("failed to read contiguous directory blocks\n");
        free(buf);
        return NULL;
    }
    return buf;
}

void *xfs_dir2_dirblks_get_cached(struct fs_info *fs, block_t startblock,
				  xfs_filblks_t c)
{
    unsigned char i;
    void *buf;

    xfs_debug("fs %p startblock %llu (0x%llx) blkscount %lu", fs, startblock,
	      startblock, c);

    if (!dirblks_cached_count) {
	buf = get_dirblks(fs, startblock, c);
	if (!buf)
	    return NULL;

	dirblks_cache[dirblks_cached_count].dc_startblock = startblock;
	dirblks_cache[dirblks_cached_count].dc_blkscount = c;
	dirblks_cache[dirblks_cached_count].dc_area = buf;

	return dirblks_cache[dirblks_cached_count++].dc_area;
    } else if (dirblks_cached_count == XFS_DIR2_DIRBLKS_CACHE_SIZE) {
	for (i = 0; i < XFS_DIR2_DIRBLKS_CACHE_SIZE / 2; i++) {
	    unsigned char k = XFS_DIR2_DIRBLKS_CACHE_SIZE - (i + 1);

	    free(dirblks_cache[i].dc_area);
	    dirblks_cache[i] = dirblks_cache[k];
	    memset(&dirblks_cache[k], 0, sizeof(dirblks_cache[k]));
	}

	buf = get_dirblks(fs, startblock, c);
	if (!buf)
	    return NULL;

	dirblks_cache[XFS_DIR2_DIRBLKS_CACHE_SIZE / 2].dc_startblock =
	    startblock;
	dirblks_cache[XFS_DIR2_DIRBLKS_CACHE_SIZE / 2].dc_blkscount = c;
	dirblks_cache[XFS_DIR2_DIRBLKS_CACHE_SIZE / 2].dc_area = buf;

	dirblks_cached_count = XFS_DIR2_DIRBLKS_CACHE_SIZE / 2;

	return dirblks_cache[dirblks_cached_count++].dc_area;
    } else {
	block_t block;
	xfs_filblks_t count;

	block = dirblks_cache[dirblks_cached_count - 1].dc_startblock;
	count = dirblks_cache[dirblks_cached_count - 1].dc_blkscount;

	if (block == startblock && count == c) {
	    return dirblks_cache[dirblks_cached_count - 1].dc_area;
	} else {
	    for (i = 0; i < dirblks_cached_count; i++) {
		block = dirblks_cache[i].dc_startblock;
		count = dirblks_cache[i].dc_blkscount;

		if (block == startblock && count == c)
		    return dirblks_cache[i].dc_area;
	    }

	    buf = get_dirblks(fs, startblock, c);
	    if (!buf)
		return NULL;

	    dirblks_cache[dirblks_cached_count].dc_startblock = startblock;
	    dirblks_cache[dirblks_cached_count].dc_blkscount = c;
	    dirblks_cache[dirblks_cached_count].dc_area = buf;

	    return dirblks_cache[dirblks_cached_count++].dc_area;
	}
    }
    return NULL;
}

void xfs_dir2_dirblks_flush_cache(void)
{
    unsigned char i;

    for (i = 0; i < dirblks_cached_count; i++) {
	free(dirblks_cache[i].dc_area);
	memset(&dirblks_cache[i], 0, sizeof(dirblks_cache[i]));
    }

    dirblks_cached_count = 0;
}

struct inode *xfs_dir2_local_find_entry(const char *dname, struct inode *parent,
					xfs_dinode_t *core)
{
    xfs_dir2_sf_t *sf = XFS_DFORK_PTR(core, XFS_DATA_FORK);
    xfs_dir2_sf_entry_t *sf_entry;
    uint8_t ftypelen = core->di_version == 3 ? 1 : 0;
    uint8_t count = sf->hdr.i8count ? sf->hdr.i8count : sf->hdr.count;
    struct fs_info *fs = parent->fs;
    struct inode *inode;
    xfs_dir2_inou_t *inou;
    xfs_intino_t ino;
    xfs_dinode_t *ncore = NULL;

    xfs_debug("dname %s parent %p core %p", dname, parent, core);
    xfs_debug("count %hhu i8count %hhu", sf->hdr.count, sf->hdr.i8count);

    sf_entry = (xfs_dir2_sf_entry_t *)((uint8_t *)sf->list -
				       (!sf->hdr.i8count ? 4 : 0));
    while (count--) {
	uint8_t *start_name = sf_entry->name;
	uint8_t *end_name = start_name + sf_entry->namelen;

	xfs_debug("namelen %u", sf_entry->namelen);

	if (!xfs_dir2_entry_name_cmp(start_name, end_name, dname)) {
	    xfs_debug("Found entry %s", dname);
	    goto found;
	}

	sf_entry = (xfs_dir2_sf_entry_t *)((uint8_t *)sf_entry +
					   offsetof(struct xfs_dir2_sf_entry,
						    name) +
					   sf_entry->namelen +
					   ftypelen +
					   (sf->hdr.i8count ? 8 : 4));
    }

    return NULL;

found:
    inode = xfs_new_inode(fs);

    inou = (xfs_dir2_inou_t *)((uint8_t *)sf_entry +
			       offsetof(struct xfs_dir2_sf_entry,
					name) +
			       sf_entry->namelen +
			       ftypelen);
    ino = xfs_dir2_sf_get_inumber(sf, inou);

    xfs_debug("entry inode's number %lu", ino);

    ncore = xfs_dinode_get_core(fs, ino);
    if (!ncore) {
        xfs_error("Failed to get dinode!");
        goto out;
    }

    fill_xfs_inode_pvt(fs, inode, ino);

    inode->ino			= ino;
    inode->size 		= be64_to_cpu(ncore->di_size);

    if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFDIR) {
	inode->mode = DT_DIR;
	xfs_debug("Found a directory inode!");
    } else if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFREG) {
	inode->mode = DT_REG;
	xfs_debug("Found a file inode!");
	xfs_debug("inode size %llu", inode->size);
    } else if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFLNK) {
	inode->mode = DT_LNK;
	xfs_debug("Found a symbolic link inode!");
    }

    return inode;

out:
    free(inode);

    return NULL;
}

struct inode *xfs_dir2_block_find_entry(const char *dname, struct inode *parent,
					xfs_dinode_t *core)
{
    xfs_bmbt_irec_t r;
    block_t dir_blk;
    struct fs_info *fs = parent->fs;
    const uint8_t *dirblk_buf;
    bool isdir3;
    uint8_t *p, *endp;
    xfs_dir2_data_hdr_t *hdr;
    struct inode *inode = NULL;
    xfs_dir2_block_tail_t *btp;
    xfs_dir2_data_unused_t *dup;
    xfs_dir2_data_entry_t *dep;
    xfs_intino_t ino;
    xfs_dinode_t *ncore;

    xfs_debug("dname %s parent %p core %p", dname, parent, core);

    bmbt_irec_get(&r, XFS_DFORK_PTR(core, XFS_DATA_FORK));
    dir_blk = fsblock_to_bytes(fs, r.br_startblock) >> BLOCK_SHIFT(fs);

    dirblk_buf = xfs_dir2_dirblks_get_cached(fs, dir_blk, r.br_blockcount);
    if (!dirblk_buf)
        return NULL;

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

    p = (uint8_t *)dirblk_buf + (isdir3 ? sizeof(struct xfs_dir3_data_hdr) :
				 sizeof(struct xfs_dir2_data_hdr));

    btp = xfs_dir2_block_tail_p(XFS_INFO(fs), hdr);
    endp = (uint8_t *)((xfs_dir2_leaf_entry_t *)btp - be32_to_cpu(btp->count));

    while (p < endp) {
        uint8_t *start_name;
        uint8_t *end_name;

        dup = (xfs_dir2_data_unused_t *)p;
        if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
            p += be16_to_cpu(dup->length);
            continue;
        }

        dep = (xfs_dir2_data_entry_t *)p;

        start_name = dep->name;
        end_name = start_name + dep->namelen;

	if (!xfs_dir2_entry_name_cmp(start_name, end_name, dname)) {
	    xfs_debug("Found entry %s", dname);
            goto found;
        }

	p += (isdir3 ? xfs_dir3_data_entsize(dep->namelen) :
	      xfs_dir2_data_entsize(dep->namelen));
    }

out:
    return NULL;

found:
    inode = xfs_new_inode(fs);

    ino = be64_to_cpu(dep->inumber);

    xfs_debug("entry inode's number %lu", ino);

    ncore = xfs_dinode_get_core(fs, ino);
    if (!ncore) {
        xfs_error("Failed to get dinode!");
        goto failed;
    }

    fill_xfs_inode_pvt(fs, inode, ino);

    inode->ino = ino;
    XFS_PVT(inode)->i_ino_blk = ino_to_bytes(fs, ino) >> BLOCK_SHIFT(fs);
    inode->size = be64_to_cpu(ncore->di_size);

    if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFDIR) {
        inode->mode = DT_DIR;
        xfs_debug("Found a directory inode!");
    } else if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFREG) {
        inode->mode = DT_REG;
        xfs_debug("Found a file inode!");
        xfs_debug("inode size %llu", inode->size);
    } else if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFLNK) {
        inode->mode = DT_LNK;
        xfs_debug("Found a symbolic link inode!");
    }

    xfs_debug("entry inode's number %lu", ino);

    return inode;

failed:
    free(inode);

    return NULL;
}

struct inode *xfs_dir2_leaf_find_entry(const char *dname, struct inode *parent,
				       xfs_dinode_t *core)
{
    xfs_dir2_leaf_hdr_t *hdr;
    xfs_dir2_leaf_entry_t *ents;
    uint16_t count;
    xfs_bmbt_irec_t irec;
    block_t leaf_blk, dir_blk;
    xfs_dir2_leaf_entry_t *lep;
    int low;
    int high;
    int mid = 0;
    uint32_t hash = 0;
    uint32_t hashwant;
    uint32_t newdb, curdb = -1;
    xfs_dir2_data_entry_t *dep = NULL;
    struct inode *ip;
    xfs_dir2_data_hdr_t *data_hdr;
    uint8_t *start_name;
    uint8_t *end_name;
    xfs_intino_t ino;
    xfs_dinode_t *ncore;
    uint8_t *buf = NULL;

    xfs_debug("dname %s parent %p core %p", dname, parent, core);

    bmbt_irec_get(&irec, (xfs_bmbt_rec_t *)XFS_DFORK_PTR(core, XFS_DATA_FORK) +
					be32_to_cpu(core->di_nextents) - 1);
    leaf_blk = fsblock_to_bytes(parent->fs, irec.br_startblock) >>
	    BLOCK_SHIFT(parent->fs);

    hdr = xfs_dir2_dirblks_get_cached(parent->fs, leaf_blk,
				      irec.br_blockcount);
    if (!hdr)
        return NULL;

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

    if (!count)
	goto out;

    hashwant = xfs_dir2_da_hashname((uint8_t *)dname, strlen(dname));

    /* Binary search */
    for (lep = ents, low = 0, high = count - 1; low <= high; ) {
        mid = (low + high) >> 1;
        if ((hash = be32_to_cpu(lep[mid].hashval)) == hashwant)
            break;
        if (hash < hashwant)
            low = mid + 1;
        else
            high = mid - 1;
    }

    /* If hash is not the one we want, then the directory does not contain the
     * entry we're looking for and there is nothing to do anymore.
     */
    if (hash != hashwant)
	goto out;

    while (mid > 0 && be32_to_cpu(lep[mid - 1].hashval) == hashwant)
	mid--;

    for (lep = &ents[mid];
	 mid < count && be32_to_cpu(lep->hashval) == hashwant; lep++, mid++) {
        /* Skip over stale leaf entries. */
        if (be32_to_cpu(lep->address) == XFS_DIR2_NULL_DATAPTR)
            continue;

        newdb = xfs_dir2_dataptr_to_db(parent->fs, be32_to_cpu(lep->address));
        if (newdb != curdb) {
            bmbt_irec_get(&irec,
			  (xfs_bmbt_rec_t *)XFS_DFORK_PTR(core,
							  XFS_DATA_FORK) +
			  newdb);
            dir_blk = fsblock_to_bytes(parent->fs, irec.br_startblock) >>

		      BLOCK_SHIFT(parent->fs);
            buf = xfs_dir2_dirblks_get_cached(parent->fs, dir_blk,
					      irec.br_blockcount);
            data_hdr = (xfs_dir2_data_hdr_t *)buf;
            if (be32_to_cpu(data_hdr->magic) != XFS_DIR2_DATA_MAGIC &&
		be32_to_cpu(data_hdr->magic) != XFS_DIR3_DATA_MAGIC) {
                xfs_error("Leaf directory's data magic No. does not match!");
                goto out;
            }

            curdb = newdb;
        }

        dep = (xfs_dir2_data_entry_t *)(
            buf + xfs_dir2_dataptr_to_off(parent->fs,
                                          be32_to_cpu(lep->address)));
        start_name = dep->name;
        end_name = start_name + dep->namelen;

	if (!xfs_dir2_entry_name_cmp(start_name, end_name, dname)) {
	    xfs_debug("Found entry %s", dname);
            goto found;
        }
    }

out:
    return NULL;

found:
    ip = xfs_new_inode(parent->fs);

    ino = be64_to_cpu(dep->inumber);

    xfs_debug("entry inode's number %lu", ino);

    ncore = xfs_dinode_get_core(parent->fs, ino);
    if (!ncore) {
        xfs_error("Failed to get dinode!");
        goto failed;
    }

    fill_xfs_inode_pvt(parent->fs, ip, ino);

    ip->ino = ino;
    XFS_PVT(ip)->i_ino_blk = ino_to_bytes(parent->fs, ino) >>
	                        BLOCK_SHIFT(parent->fs);
    ip->size = be64_to_cpu(ncore->di_size);

    if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFDIR) {
        ip->mode = DT_DIR;
        xfs_debug("Found a directory inode!");
    } else if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFREG) {
        ip->mode = DT_REG;
        xfs_debug("Found a file inode!");
        xfs_debug("inode size %llu", ip->size);
    } else if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFLNK) {
        ip->mode = DT_LNK;
        xfs_debug("Found a symbolic link inode!");
    }

    xfs_debug("entry inode's number %lu", ino);

    return ip;

failed:
    free(ip);

    return ip;
}

static xfs_fsblock_t
select_child(xfs_dfiloff_t off,
             xfs_bmbt_key_t *kp,
             xfs_bmbt_ptr_t *pp,
             int nrecs)
{
    int i;

    for (i = 0; i < nrecs; i++) {
        if (be64_to_cpu(kp[i].br_startoff) == off)
            return be64_to_cpu(pp[i]);
        if (be64_to_cpu(kp[i].br_startoff) > off) {
            if (i == 0)
                return be64_to_cpu(pp[i]);
            else
                return be64_to_cpu(pp[i-1]);
        }
    }

    return be64_to_cpu(pp[nrecs - 1]);
}

block_t xfs_dir2_get_right_blk(struct fs_info *fs, xfs_dinode_t *core,
			       block_t fsblkno, int *error)
{
    uint32_t idx;
    xfs_bmbt_irec_t irec;
    block_t bno;
    block_t nextbno;
    xfs_bmdr_block_t *rblock;
    int fsize;
    int nextents;
    xfs_bmbt_ptr_t *pp;
    xfs_bmbt_key_t *kp;
    const xfs_btree_block_t *blk;
    xfs_bmbt_rec_t *xp;

    *error = 0;
    if (core->di_format == XFS_DINODE_FMT_EXTENTS) {
        xfs_debug("XFS_DINODE_FMT_EXTENTS");
        for (idx = 0; idx < be32_to_cpu(core->di_nextents); idx++) {
            bmbt_irec_get(&irec,
                          (xfs_bmbt_rec_t *)
			  XFS_DFORK_PTR(core, XFS_DATA_FORK) + idx);
            if (fsblkno >= irec.br_startoff &&
                fsblkno < irec.br_startoff + irec.br_blockcount)
                break;
        }
    } else if (core->di_format == XFS_DINODE_FMT_BTREE) {
        xfs_debug("XFS_DINODE_FMT_BTREE");
        bno = NULLFSBLOCK;
        rblock = XFS_DFORK_PTR(core, XFS_DATA_FORK);
        fsize = XFS_DFORK_SIZE(core, fs, XFS_DATA_FORK);
        pp = XFS_BMDR_PTR_ADDR(rblock, 1, xfs_bmdr_maxrecs(fsize, 0));
        kp = XFS_BMDR_KEY_ADDR(rblock, 1);
        bno = fsblock_to_bytes(fs,
                  select_child(fsblkno, kp, pp,
                      be16_to_cpu(rblock->bb_numrecs))) >> BLOCK_SHIFT(fs);

        /* Find the leaf */
        for (;;) {
            blk = get_cache(fs->fs_dev, bno);
            if (be16_to_cpu(blk->bb_level) == 0)
                break;
            pp = XFS_BMBT_PTR_ADDR(fs, blk, 1,
                     xfs_bmdr_maxrecs(XFS_INFO(fs)->blocksize, 0));
            kp = XFS_BMBT_KEY_ADDR(fs, blk, 1);
            bno = fsblock_to_bytes(fs,
                      select_child(fsblkno, kp, pp,
                          be16_to_cpu(blk->bb_numrecs))) >> BLOCK_SHIFT(fs);
        }

        /* Find the records among leaves */
        for (;;) {
            nextbno = be64_to_cpu(blk->bb_u.l.bb_rightsib);
            nextents = be16_to_cpu(blk->bb_numrecs);
            xp = (xfs_bmbt_rec_t *)XFS_BMBT_REC_ADDR(fs, blk, 1);
            for (idx = 0; idx < nextents; idx++) {
                bmbt_irec_get(&irec, xp + idx);
                if (fsblkno >= irec.br_startoff &&
                    fsblkno < irec.br_startoff + irec.br_blockcount) {
                    nextbno = NULLFSBLOCK;
                    break;
                }
            }
            if (nextbno == NULLFSBLOCK)
                break;
            bno = fsblock_to_bytes(fs, nextbno) >> BLOCK_SHIFT(fs);
            blk = get_cache(fs->fs_dev, bno);
        }
    }

    if (fsblkno < irec.br_startoff ||
        fsblkno >= irec.br_startoff + irec.br_blockcount)
        *error = 1;

    return fsblock_to_bytes(fs,
                fsblkno - irec.br_startoff + irec.br_startblock) >>
                BLOCK_SHIFT(fs);
}

struct inode *xfs_dir2_node_find_entry(const char *dname, struct inode *parent,
				       xfs_dinode_t *core)
{
    block_t fsblkno;
    xfs_da_node_hdr_t *nhdr;
    uint32_t hashwant;
    uint32_t hash = 0;
    uint16_t i;
    uint16_t count;
    xfs_da_node_entry_t *btree;
    int error;
    xfs_dir2_leaf_hdr_t *lhdr;
    xfs_dir2_data_hdr_t *data_hdr;
    xfs_dir2_leaf_entry_t *lep;
    xfs_dir2_data_entry_t *dep;
    xfs_dir2_leaf_entry_t *ents;
    struct inode *ip;
    uint8_t *start_name;
    uint8_t *end_name;
    int low;
    int high;
    int mid = 0;
    uint32_t newdb;
    uint32_t curdb;
    xfs_intino_t ino;
    xfs_dinode_t *ncore;
    uint8_t *buf = NULL;

    xfs_debug("dname %s parent %p core %p", dname, parent, core);

    curdb = xfs_dir2_byte_to_db(parent->fs, XFS_DIR2_LEAF_OFFSET);
    fsblkno = xfs_dir2_get_right_blk(parent->fs, core, curdb, &error);
    if (error) {
        xfs_error("Cannot find right rec!");
        return NULL;
    }

    nhdr = xfs_dir2_dirblks_get_cached(parent->fs, fsblkno, 1);
    if (be16_to_cpu(nhdr->info.magic) == XFS_DA_NODE_MAGIC) {
	count = be16_to_cpu(nhdr->count);
	btree = (xfs_da_node_entry_t *)((uint8_t *)nhdr +
					sizeof(struct xfs_da_node_hdr));
    } else if (be16_to_cpu(nhdr->info.magic) == XFS_DA3_NODE_MAGIC) {
	count = be16_to_cpu(((xfs_da3_node_hdr_t *)nhdr)->count);
	btree = (xfs_da_node_entry_t *)((uint8_t *)nhdr +
					sizeof(struct xfs_da3_node_hdr));
    } else {
        xfs_error("Node's magic number (0x%04x) does not match!");
        goto out;
    }

    hashwant = xfs_dir2_da_hashname((uint8_t *)dname, strlen(dname));

    /* Given a hash to lookup, you read the node's btree array and first
     * "hashval" in the array that exceeds the given hash and it can then
     * be found in the block pointed by the "before" value.
     */
    fsblkno = 0;
    for (i = 0; i < count; i++) {
	if (hashwant < be32_to_cpu(btree[i].hashval)) {
	    fsblkno = be32_to_cpu(btree[i].before);
	    break;
	}
    }

    if (!fsblkno)
	goto out;

    fsblkno = xfs_dir2_get_right_blk(parent->fs, core, fsblkno, &error);
    if (error) {
	xfs_error("Cannot find leaf record");
	goto out;
    }

    lhdr = xfs_dir2_dirblks_get_cached(parent->fs, fsblkno, 1);
    if (be16_to_cpu(lhdr->info.magic) == XFS_DIR2_LEAFN_MAGIC) {
	count = be16_to_cpu(lhdr->count);
	ents = (xfs_dir2_leaf_entry_t *)((uint8_t *)lhdr +
					 sizeof(struct xfs_dir2_leaf_hdr));
    } else if (be16_to_cpu(lhdr->info.magic) == XFS_DIR3_LEAFN_MAGIC) {
	count = be16_to_cpu(((xfs_dir3_leaf_hdr_t *)lhdr)->count);
	ents = (xfs_dir2_leaf_entry_t *)((uint8_t *)lhdr +
					 sizeof(struct xfs_dir3_leaf_hdr));
    } else {
        xfs_error("Leaf's magic number does not match (0x%04x)!",
		  be16_to_cpu(lhdr->info.magic));
        goto out;
    }

    if (!count)
	goto out;

    lep = ents;
    low = 0;
    high = count - 1;
    while (low <= high) {
        mid = (low + high) >> 1;
        if ((hash = be32_to_cpu(lep[mid].hashval)) == hashwant)
            break;
        if (hash < hashwant)
            low = mid + 1;
        else
            high = mid - 1;
    }

    /* If hash is not the one we want, then the directory does not contain the
     * entry we're looking for and there is nothing to do anymore.
     */
    if (hash != hashwant)
        goto out;

    while (mid > 0 && be32_to_cpu(lep[mid - 1].hashval) == hashwant)
        mid--;

    curdb = -1;
    for (lep = &ents[mid]; mid < count && be32_to_cpu(lep->hashval) == hashwant;
         lep++, mid++) {
        /* Skip over stale leaf entries. */
        if (be32_to_cpu(lep->address) == XFS_DIR2_NULL_DATAPTR)
            continue;

        newdb = xfs_dir2_dataptr_to_db(parent->fs, be32_to_cpu(lep->address));
        if (newdb != curdb) {
            fsblkno = xfs_dir2_get_right_blk(parent->fs, core, newdb, &error);
            if (error) {
                xfs_error("Cannot find data block!");
                goto out;
            }

            buf = xfs_dir2_dirblks_get_cached(parent->fs, fsblkno, 1);
            data_hdr = (xfs_dir2_data_hdr_t *)buf;
            if (be32_to_cpu(data_hdr->magic) != XFS_DIR2_DATA_MAGIC &&
		be32_to_cpu(data_hdr->magic) != XFS_DIR3_DATA_MAGIC) {
                xfs_error("Leaf directory's data magic No. does not match!");
                goto out;
            }

            curdb = newdb;
        }

        dep = (xfs_dir2_data_entry_t *)(
            buf + xfs_dir2_dataptr_to_off(parent->fs,
                                          be32_to_cpu(lep->address)));
        start_name = dep->name;
        end_name = start_name + dep->namelen;

	if (!xfs_dir2_entry_name_cmp(start_name, end_name, dname)) {
	    xfs_debug("Found entry %s", dname);
	    goto found;
        }
    }

out:
    return NULL;

found:
    ip = xfs_new_inode(parent->fs);
    ino = be64_to_cpu(dep->inumber);
    ncore = xfs_dinode_get_core(parent->fs, ino);
    if (!ncore) {
        xfs_error("Failed to get dinode!");
        goto failed;
    }

    fill_xfs_inode_pvt(parent->fs, ip, ino);
    ip->ino = ino;
    XFS_PVT(ip)->i_ino_blk = ino_to_bytes(parent->fs, ino) >>
        BLOCK_SHIFT(parent->fs);
    ip->size = be64_to_cpu(ncore->di_size);

    if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFDIR) {
        ip->mode = DT_DIR;
        xfs_debug("Found a directory inode!");
    } else if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFREG) {
        ip->mode = DT_REG;
        xfs_debug("Found a file inode!");
        xfs_debug("inode size %llu", ip->size);
    } else if ((be16_to_cpu(ncore->di_mode) & S_IFMT) == S_IFLNK) {
        ip->mode = DT_LNK;
        xfs_debug("Found a symbolic link inode!");
    }

    xfs_debug("entry inode's number %lu", ino);

    return ip;

failed:
    free(ip);

    return NULL;
}
