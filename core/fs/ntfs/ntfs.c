/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2011 Paulo Alcantara <pcacjr@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * ntfs.c - The NTFS filesystem functions
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

#include "codepage.h"
#include "ntfs.h"

#define for_each_mft_record(fs, data, block) \
    for ((data) = get_right_block((fs), (block)); \
            (block) < NTFS_SB((fs))->mft_size && \
            ((const MFT_RECORD *)(data))->magic == NTFS_MAGIC_FILE; \
            (block) += ((const MFT_RECORD *)(data))->bytes_allocated >> \
                                                        BLOCK_SHIFT((fs)), \
            (data) = get_right_block((fs), (block)))

/* Check if there are specific zero fields in an NTFS boot sector */
static inline int ntfs_check_zero_fields(const struct ntfs_bpb *sb)
{
    return !sb->res_sectors && (!sb->zero_0[0] && !sb->zero_0[1] &&
            !sb->zero_0[2]) && !sb->zero_1 && !sb->zero_2 &&
            !sb->zero_3;
}

static inline int ntfs_check_sb_fields(const struct ntfs_bpb *sb)
{
    return ntfs_check_zero_fields(sb) &&
            (!memcmp(sb->oem_name, "NTFS    ", 8) ||
             !memcmp(sb->oem_name, "MSWIN4.0", 8) ||
             !memcmp(sb->oem_name, "MSWIN4.1", 8));
}

static inline struct inode *new_ntfs_inode(struct fs_info *fs)
{
    struct inode *inode;

    inode = alloc_inode(fs, 0, sizeof(struct ntfs_inode));
    if (!inode)
        malloc_error("inode structure");

    return inode;
}

static inline const void *get_right_block(struct fs_info *fs,
                                                        block_t block)
{
    return get_cache(fs->fs_dev, NTFS_SB(fs)->mft + block);
}

static MFT_RECORD *mft_record_lookup(uint32_t file, struct fs_info *fs,
                                                        sector_t *block)
{
    const uint8_t *data;
    MFT_RECORD *mrec;

    for_each_mft_record(fs, data, *block) {
        mrec = (MFT_RECORD *)data;
        if (mrec->mft_record_no == file)
            return mrec;
    }

    return NULL;
}

static ATTR_RECORD *attr_lookup(uint32_t type, const MFT_RECORD *mrec)
{
    ATTR_RECORD *attr;

    /* sanity check */
    if (!mrec || type == NTFS_AT_END)
        return NULL;

    attr = (ATTR_RECORD *)((uint8_t *)mrec + mrec->attrs_offset);
    /* walk through the file attribute records */
    for (;; attr = (ATTR_RECORD *)((uint8_t *)attr + attr->len)) {
        if (attr->type == NTFS_AT_END)
            return NULL;

        if (attr->type == type)
            break;
    }

    return attr;
}

static int index_inode_setup(struct fs_info *fs, unsigned long mft_no,
                                struct inode *inode)
{
    MFT_RECORD *mrec;
    sector_t block = 0;
    ATTR_RECORD *attr;
    uint32_t len;
    INDEX_ROOT *ir;
    uint32_t clust_size;

    mrec = mft_record_lookup(mft_no, fs, &block);
    if (!mrec) {
        printf("No MFT record found!\n");
        goto out;
    }

    NTFS_PVT(inode)->mft_no = mft_no;
    NTFS_PVT(inode)->seq_no = mrec->seq_no;

    NTFS_PVT(inode)->start_cluster = block >> NTFS_SB(fs)->clust_shift;
    NTFS_PVT(inode)->here = block;

    attr = attr_lookup(NTFS_AT_INDEX_ROOT, mrec);
    if (!attr) {
        printf("No attribute found!\n");
        goto out;
    }

    NTFS_PVT(inode)->type = attr->type;

    /* note: INDEX_ROOT is always resident */
    ir = (INDEX_ROOT *)((uint8_t *)attr +
                                attr->data.resident.value_offset);
    len = attr->data.resident.value_len;
    if ((uint8_t *)ir + len > (uint8_t *)mrec + NTFS_SB(fs)->mft_record_size) {
        printf("Index is corrupt!\n");
        goto out;
    }

    NTFS_PVT(inode)->itype.index.collation_rule = ir->collation_rule;
    NTFS_PVT(inode)->itype.index.block_size = ir->index_block_size;
    NTFS_PVT(inode)->itype.index.block_size_shift =
                        ilog2(NTFS_PVT(inode)->itype.index.block_size);

    /* determine the size of a vcn in the index */
    clust_size = NTFS_PVT(inode)->itype.index.block_size;
    if (NTFS_SB(fs)->clust_size <= clust_size) {
        NTFS_PVT(inode)->itype.index.vcn_size = NTFS_SB(fs)->clust_size;
        NTFS_PVT(inode)->itype.index.vcn_size_shift = NTFS_SB(fs)->clust_shift;
    } else {
        NTFS_PVT(inode)->itype.index.vcn_size = BLOCK_SIZE(fs);
        NTFS_PVT(inode)->itype.index.vcn_size_shift = BLOCK_SHIFT(fs);
    }

    inode->mode = DT_DIR;

    return 0;

out:
    return -1;
}

static struct inode *ntfs_iget_root(struct fs_info *fs)
{
    struct inode *inode = new_ntfs_inode(fs);
    int err;

    inode->fs = fs;

    err = index_inode_setup(fs, FILE_root, inode);
    if (err)
        goto free_out;

    NTFS_PVT(inode)->start = NTFS_PVT(inode)->here;

    return inode;

free_out:
    free(inode);

    return NULL;
}

/* Initialize the filesystem metadata and return block size in bits */
static int ntfs_fs_init(struct fs_info *fs)
{
    struct ntfs_bpb ntfs;
    struct ntfs_sb_info *sbi;
    struct disk *disk = fs->fs_dev->disk;

    disk->rdwr_sectors(disk, &ntfs, 0, 1, 0);

    /* sanity check */
    if (!ntfs_check_sb_fields(&ntfs))
        return -1;

    SECTOR_SHIFT(fs) = BLOCK_SHIFT(fs) = disk->sector_shift;
    SECTOR_SIZE(fs) = 1 << SECTOR_SHIFT(fs);
    fs->block_size = 1 << BLOCK_SHIFT(fs);

    sbi = malloc(sizeof(*sbi));
    if (!sbi)
        malloc_error("ntfs_sb_info structure");

    fs->fs_info = sbi;

    sbi->clust_shift        = ilog2(ntfs.sec_per_clust);
    sbi->clust_byte_shift   = sbi->clust_shift + SECTOR_SHIFT(fs);
    sbi->clust_mask         = ntfs.sec_per_clust - 1;
    sbi->clust_size         = ntfs.sec_per_clust << SECTOR_SHIFT(fs);
    sbi->mft_record_size    = ntfs.clust_per_mft_record <<
                                            sbi->clust_byte_shift;
    sbi->root = sbi->mft + sbi->mft_size;

    sbi->mft = ntfs.mft_lclust << sbi->clust_shift;
    /* 16 MFT entries reserved for metadata files (approximately 16 KiB) */
    sbi->mft_size = (ntfs.clust_per_mft_record << sbi->clust_shift) << 4;

    sbi->clusters = (ntfs.total_sectors - sbi->root) >> sbi->clust_shift;
    if (sbi->clusters > 0xFFFFFFFFFFF4ULL)
        sbi->clusters = 0xFFFFFFFFFFF4ULL;

	/* Initialize the cache */
    cache_init(fs->fs_dev, BLOCK_SHIFT(fs));

    return BLOCK_SHIFT(fs);
}

const struct fs_ops ntfs_fs_ops = {
    .fs_name        = "ntfs",
    .fs_flags       = FS_USEMEM | FS_THISIND,
    .fs_init        = ntfs_fs_init,
    .searchdir      = NULL,
    .getfssec       = NULL,
    .close_file     = NULL,
    .mangle_name    = NULL,
    .load_config    = NULL,
    .readdir        = NULL,
    .iget_root      = ntfs_iget_root,
    .iget           = NULL,
    .next_extent    = NULL,
};
