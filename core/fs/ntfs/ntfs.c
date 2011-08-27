/*
 * Copyright (c) Paulo Alcantara <pcacjr@gmail.com>
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

/* Note: No support for compressed files */

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
#include "ntfs.h"

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

static void ntfs_fixups_writeback(struct fs_info *fs, NTFS_RECORD *nrec)
{
    uint16_t *usa;
    uint16_t usa_no;
    uint16_t usa_count;
    uint16_t *blk;

    if (nrec->magic != NTFS_MAGIC_FILE && nrec->magic != NTFS_MAGIC_INDX)
        return;

    /* get the Update Sequence Array offset */
    usa = (uint16_t *)((uint8_t *)nrec + nrec->usa_ofs);
    /* get the Update Sequence Array Number and skip it */
    usa_no = *usa++;
    /* get the Update Sequene Array count */
    usa_count = nrec->usa_count - 1;    /* exclude the USA number */
    /* make it to point to the last two bytes of the RECORD's first sector */
    blk = (uint16_t *)((uint8_t *)nrec + SECTOR_SIZE(fs) - 2);

    while (usa_count--) {
        if (*blk != usa_no)
            break;

        *blk = *usa++;
        blk = (uint16_t *)((uint8_t *)blk + SECTOR_SIZE(fs));
    }
}

/* read content from cache */
static int ntfs_read(struct fs_info *fs, void *buf, size_t len, uint64_t count,
                    block_t *blk, uint64_t *blk_offset,
                    uint64_t *blk_next_offset, uint64_t *lcn)
{
    uint8_t *data;
    uint64_t offset = *blk_offset;
    const uint32_t clust_byte_shift = NTFS_SB(fs)->clust_byte_shift;
    const uint64_t blk_size = UINT64_C(1) << BLOCK_SHIFT(fs);
    uint64_t bytes;
    uint64_t lbytes;
    uint64_t loffset;
    uint64_t k;

    if (count > len)
        goto out;

    data = (uint8_t *)get_cache(fs->fs_dev, *blk);
    if (!data)
        goto out;

    if (!offset)
        offset = (*lcn << clust_byte_shift) % blk_size;

    dprintf("lcn:            0x%X\n", *lcn);
    dprintf("offset:         0x%X\n", offset);

    bytes = count;              /* bytes to copy */
    lbytes = blk_size - offset; /* bytes left to copy */
    if (lbytes >= bytes) {
        /* so there's room enough, then copy the whole content */
        memcpy(buf, data + offset, bytes);
        loffset = offset;
        offset += count;
    } else {
        dprintf("bytes:      %u\n", bytes);
        dprintf("bytes left: %u\n", lbytes);
        /* otherwise, let's copy it partially... */
        k = 0;
        while (bytes) {
            memcpy(buf + k, data + offset, lbytes);
            bytes -= lbytes;
            loffset = offset;
            offset += lbytes;
            k += lbytes;
            if (offset >= blk_size) {
                /* then fetch a new FS block */
                data = (uint8_t *)get_cache(fs->fs_dev, ++*blk);
                if (!data)
                    goto out;

                loffset = offset;
                offset = 0;
            }
        }
    }

    if (loffset >= blk_size)
        loffset = 0;    /* it must be aligned on a block boundary */

    *blk_offset = loffset;

    if (blk_next_offset)
        *blk_next_offset = offset;

    *lcn += blk_size / count;   /* update LCN */

    return 0;

out:
    return -1;
}

static MFT_RECORD *ntfs_mft_record_lookup(struct fs_info *fs, uint32_t file, block_t *blk)
{
    const uint64_t mft_record_size = NTFS_SB(fs)->mft_record_size;
    uint8_t *buf;
    block_t cur_blk;
    block_t right_blk;
    uint64_t offset = 0;
    uint64_t next_offset;
    uint64_t lcn = NTFS_SB(fs)->mft_lcn;
    int err;
    MFT_RECORD *mrec;

    buf = (uint8_t *)malloc(mft_record_size);
    if (!buf)
        malloc_error("uint8_t *");

    cur_blk = blk ? *blk : 0;
    for (;;) {
        right_blk = cur_blk + NTFS_SB(fs)->mft_blk;
        err = ntfs_read(fs, buf, mft_record_size, mft_record_size, &right_blk,
                        &offset, &next_offset, &lcn);
        if (err) {
            printf("Error on reading from cache.\n");
            break;
        }

        ntfs_fixups_writeback(fs, (NTFS_RECORD *)buf);

        mrec = (MFT_RECORD *)buf;
        if (mrec->mft_record_no == file) {
            if (blk)
                *blk = cur_blk;     /* update record starting block */

            return mrec;            /* found MFT record */
        }

        if (next_offset >= BLOCK_SIZE(fs)) {
            /* try the next FS block */
            offset = 0;
            cur_blk = right_blk - NTFS_SB(fs)->mft_blk + 1;
        } else {
            /* there's still content to fetch in the current block */
            cur_blk = right_blk - NTFS_SB(fs)->mft_blk;
            offset = next_offset;   /* update FS block offset */
        }
    }

    free(buf);

    return NULL;
}

static ATTR_RECORD *ntfs_attr_lookup(uint32_t type, const MFT_RECORD *mrec)
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

static bool ntfs_filename_cmp(const char *dname, INDEX_ENTRY *ie)
{
    const uint16_t *entry_fn;
    uint8_t entry_fn_len;
    unsigned i;

    entry_fn = ie->key.file_name.file_name;
    entry_fn_len = ie->key.file_name.file_name_len;

    if (strlen(dname) != entry_fn_len)
        return false;

    /* Do case-sensitive compares for Posix file names */
    if (ie->key.file_name.file_name_type == FILE_NAME_POSIX) {
        for (i = 0; i < entry_fn_len; i++)
            if (entry_fn[i] != dname[i])
                return false;
    } else {
        for (i = 0; i < entry_fn_len; i++)
            if (tolower(entry_fn[i]) != tolower(dname[i]))
                return false;
    }

    return true;
}

static inline uint8_t *mapping_chunk_init(ATTR_RECORD *attr,
                                    struct mapping_chunk *chunk,
                                    uint32_t *offset)
{
    memset(chunk, 0, sizeof *chunk);
    *offset = 0U;

    return (uint8_t *)attr + attr->data.non_resident.mapping_pairs_offset;
}

/* Parse data runs.
 *
 * return 0 on success or -1 on failure.
 */
static int parse_data_run(const void *stream, uint32_t *offset,
                            uint8_t *attr_len, struct mapping_chunk *chunk)
{
    uint8_t *buf;   /* Pointer to the zero-terminated byte stream */
    uint8_t count;  /* The count byte */
    uint8_t v, l;   /* v is the number of changed low-order VCN bytes;
                     * l is the number of changed low-order LCN bytes
                     */
    uint8_t *byte;
    int byte_shift = 8;
    int mask;
    uint8_t val;
    int64_t res;

    (void)attr_len;

    chunk->flags &= ~MAP_MASK;

    buf = (uint8_t *)stream + *offset;
    if (buf > attr_len || !*buf) {
        chunk->flags |= MAP_END;    /* we're done */
        return 0;
    }

    if (!*offset)
        chunk->flags |= MAP_START;  /* initial chunk */

    count = *buf;
    v = count & 0x0F;
    l = count >> 4;

    if (v > 8 || l > 8) /* more than 8 bytes ? */
        goto out;

    byte = (uint8_t *)buf + v;
    count = v;

    res = 0LL;
    while (count--) {
        val = *byte--;
        mask = val >> (byte_shift - 1);
        res = (res << byte_shift) | ((val + mask) ^ mask);
    }

    chunk->len = res;   /* get length data */

    byte = (uint8_t *)buf + v + l;
    count = l;

    mask = 0xFFFFFFFF;
    res = 0LL;
    if (*byte & 0x80)
        res |= (int64_t)mask;   /* sign-extend it */

    while (count--)
        res = (res << byte_shift) | *byte--;

    chunk->lcn += res;
    /* are VCNS from cur_vcn to next_vcn - 1 unallocated ? */
    if (!chunk->lcn)
        chunk->flags |= MAP_UNALLOCATED;
    else
        chunk->flags |= MAP_ALLOCATED;

    *offset += v + l + 1;

    return 0;

out:
    return -1;
}

static inline enum dirent_type get_inode_mode(MFT_RECORD *mrec)
{
    return mrec->flags & MFT_RECORD_IS_DIRECTORY ? DT_DIR : DT_REG;
}

static int index_inode_setup(struct fs_info *fs, unsigned long mft_no,
                            struct inode *inode)
{
    uint64_t start_blk = 0;
    MFT_RECORD *mrec;
    ATTR_RECORD *attr;
    enum dirent_type d_type;
    uint32_t len;
    INDEX_ROOT *ir;
    uint32_t clust_size;
    uint8_t *attr_len;
    struct mapping_chunk chunk;
    int err;
    uint8_t *stream;
    uint32_t offset;

    mrec = ntfs_mft_record_lookup(fs, mft_no, &start_blk);
    if (!mrec) {
        printf("No MFT record found.\n");
        goto out;
    }

    NTFS_PVT(inode)->mft_no = mft_no;
    NTFS_PVT(inode)->seq_no = mrec->seq_no;

    NTFS_PVT(inode)->start_cluster = start_blk >> NTFS_SB(fs)->clust_shift;
    NTFS_PVT(inode)->here = start_blk;

    d_type = get_inode_mode(mrec);
    if (d_type == DT_DIR) {    /* directory stuff */
        dprintf("Got a directory.\n");
        attr = ntfs_attr_lookup(NTFS_AT_INDEX_ROOT, mrec);
        if (!attr) {
            printf("No attribute found.\n");
            goto out;
        }

        /* note: INDEX_ROOT is always resident */
        ir = (INDEX_ROOT *)((uint8_t *)attr +
                                    attr->data.resident.value_offset);
        len = attr->data.resident.value_len;
        if ((uint8_t *)ir + len > (uint8_t *)mrec +
                        NTFS_SB(fs)->mft_record_size) {
            dprintf("Corrupt index\n");
            goto out;
        }

        NTFS_PVT(inode)->itype.index.collation_rule = ir->collation_rule;
        NTFS_PVT(inode)->itype.index.blk_size = ir->index_block_size;
        NTFS_PVT(inode)->itype.index.blk_size_shift =
                            ilog2(NTFS_PVT(inode)->itype.index.blk_size);

        /* determine the size of a vcn in the index */
        clust_size = NTFS_PVT(inode)->itype.index.blk_size;
        if (NTFS_SB(fs)->clust_size <= clust_size) {
            NTFS_PVT(inode)->itype.index.vcn_size = NTFS_SB(fs)->clust_size;
            NTFS_PVT(inode)->itype.index.vcn_size_shift =
                                        NTFS_SB(fs)->clust_shift;
        } else {
            NTFS_PVT(inode)->itype.index.vcn_size = BLOCK_SIZE(fs);
            NTFS_PVT(inode)->itype.index.vcn_size_shift = BLOCK_SHIFT(fs);
        }

        NTFS_PVT(inode)->in_idx_root = true;
    } else if (d_type == DT_REG) {        /* file stuff */
        dprintf("Got a file.\n");
        attr = ntfs_attr_lookup(NTFS_AT_DATA, mrec);
        if (!attr) {
            printf("No attribute found.\n");
            goto out;
        }

        NTFS_PVT(inode)->non_resident = attr->non_resident;
        NTFS_PVT(inode)->type = attr->type;

        if (!attr->non_resident) {
            NTFS_PVT(inode)->data.resident.offset =
                (uint32_t)((uint8_t *)attr + attr->data.resident.value_offset);
            inode->size = attr->data.resident.value_len;
        } else {
            attr_len = (uint8_t *)attr + attr->len;

            stream = mapping_chunk_init(attr, &chunk, &offset);
            for (;;) {
                err = parse_data_run(stream, &offset, attr_len, &chunk);
                if (err) {
                    printf("parse_data_run()\n");
                    goto out;
                }

                if (chunk.flags & MAP_UNALLOCATED)
                    continue;
                if (chunk.flags & (MAP_ALLOCATED | MAP_END))
                    break;
            }

            if (chunk.flags & MAP_END) {
                dprintf("No mapping found\n");
                goto out;
            }

            NTFS_PVT(inode)->data.non_resident.len = chunk.len;
            NTFS_PVT(inode)->data.non_resident.lcn = chunk.lcn;
            inode->size = attr->data.non_resident.initialized_size;
        }
    }

    inode->mode = d_type;

    free(mrec);

    return 0;

out:
    free(mrec);

    return -1;
}

static struct inode *ntfs_index_lookup(const char *dname, struct inode *dir)
{
    struct fs_info *fs = dir->fs;
    MFT_RECORD *mrec;
    block_t blk;
    uint64_t blk_offset;
    ATTR_RECORD *attr;
    INDEX_ROOT *ir;
    uint32_t len;
    INDEX_ENTRY *ie;
    const uint64_t blk_size = UINT64_C(1) << BLOCK_SHIFT(fs);
    uint8_t buf[blk_size];
    INDEX_BLOCK *iblk;
    int err;
    uint8_t *stream;
    uint8_t *attr_len;
    struct mapping_chunk chunk;
    uint32_t offset;
    int64_t vcn;
    int64_t lcn;
    int64_t last_lcn;
    struct inode *inode;

    dprintf("ntfs_index_lookup() - mft record number: %d\n",
            NTFS_PVT(dir)->mft_no);
    mrec = ntfs_mft_record_lookup(fs, NTFS_PVT(dir)->mft_no, NULL);
    if (!mrec) {
        printf("No MFT record found.\n");
        goto out;
    }

    attr = ntfs_attr_lookup(NTFS_AT_INDEX_ROOT, mrec);
    if (!attr) {
        printf("No attribute found.\n");
        goto out;
    }

    ir = (INDEX_ROOT *)((uint8_t *)attr +
                            attr->data.resident.value_offset);
    len = attr->data.resident.value_len;
    /* sanity check */
    if ((uint8_t *)ir + len > (uint8_t *)mrec + NTFS_SB(fs)->mft_record_size)
        goto index_err;

    ie = (INDEX_ENTRY *)((uint8_t *)&ir->index +
                                ir->index.entries_offset);
    for (;; ie = (INDEX_ENTRY *)((uint8_t *)ie + ie->len)) {
        /* bounds checks */
        if ((uint8_t *)ie < (uint8_t *)mrec ||
            (uint8_t *)ie + sizeof(INDEX_ENTRY_HEADER) >
            (uint8_t *)&ir->index + ir->index.index_len ||
            (uint8_t *)ie + ie->len >
            (uint8_t *)&ir->index + ir->index.index_len)
            goto index_err;

        /* last entry cannot contain a key. it can however contain
         * a pointer to a child node in the B+ tree so we just break out
         */
        dprintf("(0) ie->flags:          0x%X\n", ie->flags);
        if (ie->flags & INDEX_ENTRY_END)
            break;

        if (ntfs_filename_cmp(dname, ie))
            goto found;
    }

    /* check for the presence of a child node */
    if (!(ie->flags & INDEX_ENTRY_NODE)) {
        printf("No child node, aborting...\n");
        goto out;
    }

    /* then descend into child node */

    attr = ntfs_attr_lookup(NTFS_AT_INDEX_ALLOCATION, mrec);
    if (!attr) {
        printf("No attribute found.\n");
        goto out;
    }

    if (!attr->non_resident) {
        printf("WTF ?! $INDEX_ALLOCATION isn't really resident.\n");
        goto out;
    }

    attr_len = (uint8_t *)attr + attr->len;
    stream = mapping_chunk_init(attr, &chunk, &offset);
    do {
        err = parse_data_run(stream, &offset, attr_len, &chunk);
        if (err)
            break;

        if (chunk.flags & MAP_UNALLOCATED)
            continue;

        if (chunk.flags & MAP_ALLOCATED) {
            dprintf("%d cluster(s) starting at 0x%08llX\n", chunk.len,
                    chunk.lcn);

            vcn = 0;
            lcn = chunk.lcn;
            while (vcn < chunk.len) {
                blk = (lcn + vcn) << NTFS_SB(fs)->clust_shift <<
                    SECTOR_SHIFT(fs) >> BLOCK_SHIFT(fs);

                blk_offset = 0;
                last_lcn = lcn;
                lcn += vcn;
                err = ntfs_read(fs, &buf, blk_size, blk_size, &blk,
                                &blk_offset, NULL, (uint64_t *)&lcn);
                if (err) {
                    printf("Error on reading from cache.\n");
                    goto not_found;
                }

                ntfs_fixups_writeback(fs, (NTFS_RECORD *)&buf);

                iblk = (INDEX_BLOCK *)&buf;
                if (iblk->magic != NTFS_MAGIC_INDX) {
                    printf("Not a valid INDX record.\n");
                    goto not_found;
                }

                ie = (INDEX_ENTRY *)((uint8_t *)&iblk->index +
                                            iblk->index.entries_offset);
                for (;; ie = (INDEX_ENTRY *)((uint8_t *)ie + ie->len)) {
                    /* bounds checks */
                    if ((uint8_t *)ie < (uint8_t *)iblk || (uint8_t *)ie +
                        sizeof(INDEX_ENTRY_HEADER) >
                        (uint8_t *)&iblk->index + iblk->index.index_len ||
                        (uint8_t *)ie + ie->len >
                        (uint8_t *)&iblk->index + iblk->index.index_len)
                        goto index_err;

                    /* last entry cannot contain a key */
                    if (ie->flags & INDEX_ENTRY_END)
                        break;

                    /* Do case-sensitive compares for Posix file names */
                    if (ie->key.file_name.file_name_type == FILE_NAME_POSIX) {
                        if (ie->key.file_name.file_name[0] > *dname)
                            break;
                    } else {
                        if (tolower(ie->key.file_name.file_name[0]) >
                            tolower(*dname))
                            break;
                    }

                    if (ntfs_filename_cmp(dname, ie))
                        goto found;
                }

                lcn = last_lcn; /* restore the original LCN */
                /* go to the next VCN */
                vcn += (blk_size / (1 << NTFS_SB(fs)->clust_byte_shift));
            }
        }
    } while (!(chunk.flags & MAP_END));

not_found:
    dprintf("Index not found\n");

out:
    dprintf("%s not found!\n", dname);

    free(mrec);

    return NULL;

found:
    dprintf("Index found\n");
    inode = new_ntfs_inode(fs);
    err = index_inode_setup(fs, ie->data.dir.indexed_file, inode);
    if (err) {
        printf("Error in index_inode_setup()\n");
        free(inode);
        goto out;
    }

    dprintf("%s found!\n", dname);

    free(mrec);

    return inode;

index_err:
    printf("Corrupt index. Aborting lookup...\n");
    goto out;
}

/* Convert an UTF-16LE LFN to OEM LFN */
static uint8_t ntfs_cvt_filename(char *filename, INDEX_ENTRY *ie)
{
    const uint16_t *entry_fn;
    uint8_t entry_fn_len;
    unsigned i;

    entry_fn = ie->key.file_name.file_name;
    entry_fn_len = ie->key.file_name.file_name_len;

    for (i = 0; i < entry_fn_len; i++)
        filename[i] = (char)entry_fn[i];

    filename[i] = '\0';

    return entry_fn_len;
}

static int ntfs_next_extent(struct inode *inode, uint32_t lstart)
{
    struct fs_info *fs = inode->fs;
    struct ntfs_sb_info *sbi = NTFS_SB(fs);
    uint32_t mcluster = lstart >> sbi->clust_shift;
    uint32_t tcluster;
    const uint32_t cluster_bytes = UINT32_C(1) << sbi->clust_byte_shift;
    sector_t pstart;
    const uint32_t sec_size = SECTOR_SIZE(fs);
    const uint32_t sec_shift = SECTOR_SHIFT(fs);

    tcluster = (inode->size + cluster_bytes - 1) >> sbi->clust_byte_shift;
    if (mcluster >= tcluster)
        goto out;       /* Requested cluster beyond end of file */

    if (!NTFS_PVT(inode)->non_resident) {
        pstart = sbi->mft_blk + NTFS_PVT(inode)->here;
        pstart <<= BLOCK_SHIFT(fs) >> sec_shift;
    } else {
        pstart = NTFS_PVT(inode)->data.non_resident.lcn << sbi->clust_shift;
    }

    inode->next_extent.pstart = pstart;
    inode->next_extent.len = (inode->size + sec_size - 1) >> sec_shift;

    return 0;

out:
    return -1;
}

static uint32_t ntfs_getfssec(struct file *file, char *buf, int sectors,
                                bool *have_more)
{
    uint8_t non_resident;
    uint32_t ret;
    struct fs_info *fs = file->fs;
    struct inode *inode = file->inode;
    MFT_RECORD *mrec;
    ATTR_RECORD *attr;
    char *p;

    non_resident = NTFS_PVT(inode)->non_resident;

    ret = generic_getfssec(file, buf, sectors, have_more);
    if (!ret)
        return ret;

    if (!non_resident) {
        mrec = ntfs_mft_record_lookup(fs, NTFS_PVT(inode)->mft_no, NULL);
        if (!mrec) {
            printf("No MFT record found.\n");
            goto out;
        }

        attr = ntfs_attr_lookup(NTFS_AT_DATA, mrec);
        if (!attr) {
            printf("No attribute found.\n");
            goto out;
        }

        p = (char *)((uint8_t *)attr + attr->data.resident.value_offset);

        /* p now points to the data offset, so let's copy it into buf */
        memcpy(buf, p, inode->size);

        ret = inode->size;

        free(mrec);
    }


    return ret;

out:
    free(mrec);

    return 0;
}

static inline bool is_filename_printable(const char *s)
{
    return s && (*s != '.' && *s != '$');
}

static int ntfs_readdir(struct file *file, struct dirent *dirent)
{
    struct fs_info *fs = file->fs;
    struct inode *inode = file->inode;
    MFT_RECORD *mrec;
    block_t blk;
    uint64_t blk_offset;
    const uint64_t blk_size = UINT64_C(1) << BLOCK_SHIFT(fs);
    ATTR_RECORD *attr;
    INDEX_ROOT *ir;
    uint32_t count;
    int len;
    INDEX_ENTRY *ie = NULL;
    uint8_t buf[BLOCK_SIZE(fs)];
    INDEX_BLOCK *iblk;
    int err;
    uint8_t *stream;
    uint8_t *attr_len;
    struct mapping_chunk chunk;
    uint32_t offset;
    int64_t vcn;
    int64_t lcn;
    char filename[NTFS_MAX_FILE_NAME_LEN + 1];

    mrec = ntfs_mft_record_lookup(fs, NTFS_PVT(inode)->mft_no, NULL);
    if (!mrec) {
        printf("No MFT record found.\n");
        goto out;
    }

    attr = ntfs_attr_lookup(NTFS_AT_INDEX_ROOT, mrec);
    if (!attr) {
        printf("No attribute found.\n");
        goto out;
    }

    ir = (INDEX_ROOT *)((uint8_t *)attr +
                            attr->data.resident.value_offset);
    len = attr->data.resident.value_len;
    /* sanity check */
    if ((uint8_t *)ir + len > (uint8_t *)mrec + NTFS_SB(fs)->mft_record_size)
        goto index_err;

    if (!file->offset && NTFS_PVT(inode)->in_idx_root) {
        file->offset = (uint32_t)((uint8_t *)&ir->index +
                                        ir->index.entries_offset);
    }

idx_root_next_entry:
    if (NTFS_PVT(inode)->in_idx_root) {
        ie = (INDEX_ENTRY *)(uint8_t *)file->offset;
        if (ie->flags & INDEX_ENTRY_END) {
            file->offset = 0;
            NTFS_PVT(inode)->in_idx_root = false;
            NTFS_PVT(inode)->idx_blks_count = 1;
            NTFS_PVT(inode)->entries_count = 0;
            NTFS_PVT(inode)->last_vcn = 0;
            goto descend_into_child_node;
        }

        file->offset = (uint32_t)((uint8_t *)ie + ie->len);
        len = ntfs_cvt_filename(filename, ie);
        if (!is_filename_printable(filename))
            goto idx_root_next_entry;

        goto done;
    }

descend_into_child_node:
    if (!(ie->flags & INDEX_ENTRY_NODE))
        goto out;

    attr = ntfs_attr_lookup(NTFS_AT_INDEX_ALLOCATION, mrec);
    if (!attr)
        goto out;

    if (!attr->non_resident) {
        printf("WTF ?! $INDEX_ALLOCATION isn't really resident.\n");
        goto out;
    }

    attr_len = (uint8_t *)attr + attr->len;

next_run:
    stream = mapping_chunk_init(attr, &chunk, &offset);
    count = NTFS_PVT(inode)->idx_blks_count;
    while (count--) {
        err = parse_data_run(stream, &offset, attr_len, &chunk);
        if (err) {
            printf("Error on parsing data runs.\n");
            goto out;
        }

        if (chunk.flags & MAP_UNALLOCATED)
            break;
        if (chunk.flags & MAP_END)
            goto out;
    }

    if (chunk.flags & MAP_UNALLOCATED) {
       NTFS_PVT(inode)->idx_blks_count++;
       goto next_run;
    }

next_vcn:
    vcn = NTFS_PVT(inode)->last_vcn;
    if (vcn >= chunk.len) {
        NTFS_PVT(inode)->last_vcn = 0;
        NTFS_PVT(inode)->idx_blks_count++;
        goto next_run;
    }

    lcn = chunk.lcn;
    blk = (lcn + vcn) << NTFS_SB(fs)->clust_shift << SECTOR_SHIFT(fs) >>
            BLOCK_SHIFT(fs);

    blk_offset = 0;
    err = ntfs_read(fs, &buf, blk_size, blk_size, &blk, &blk_offset, NULL,
                    (uint64_t *)&lcn);
    if (err) {
        printf("Error on reading from cache.\n");
        goto not_found;
    }

    ntfs_fixups_writeback(fs, (NTFS_RECORD *)&buf);

    iblk = (INDEX_BLOCK *)&buf;
    if (iblk->magic != NTFS_MAGIC_INDX) {
        printf("Not a valid INDX record.\n");
        goto not_found;
    }

idx_block_next_entry:
    ie = (INDEX_ENTRY *)((uint8_t *)&iblk->index +
                        iblk->index.entries_offset);
    count = NTFS_PVT(inode)->entries_count;
    for ( ; count--; ie = (INDEX_ENTRY *)((uint8_t *)ie + ie->len)) {
        /* bounds checks */
        if ((uint8_t *)ie < (uint8_t *)iblk || (uint8_t *)ie +
            sizeof(INDEX_ENTRY_HEADER) >
            (uint8_t *)&iblk->index + iblk->index.index_len ||
            (uint8_t *)ie + ie->len >
            (uint8_t *)&iblk->index + iblk->index.index_len)
            goto index_err;

        /* last entry cannot contain a key */
        if (ie->flags & INDEX_ENTRY_END) {
            /* go to the next VCN */
            NTFS_PVT(inode)->last_vcn += (blk_size / (1 <<
                                NTFS_SB(fs)->clust_byte_shift));
            NTFS_PVT(inode)->entries_count = 0;
            goto next_vcn;
        }
    }

    NTFS_PVT(inode)->entries_count++;
    len = ntfs_cvt_filename(filename, ie);
    if (!is_filename_printable(filename))
        goto idx_block_next_entry;

    goto done;

out:
    NTFS_PVT(inode)->in_idx_root = true;

    free(mrec);

    return -1;

done:
    dirent->d_ino = ie->data.dir.indexed_file;
    dirent->d_off = file->offset;
    dirent->d_reclen = offsetof(struct dirent, d_name) + len + 1;

    free(mrec);

    mrec = ntfs_mft_record_lookup(fs, ie->data.dir.indexed_file, NULL);
    if (!mrec) {
        printf("No MFT record found.\n");
        goto out;
    }

    dirent->d_type = get_inode_mode(mrec);
    memcpy(dirent->d_name, filename, len + 1);

    free(mrec);

    return 0;

not_found:
    printf("Index not found\n");
    goto out;

index_err:
    printf("Corrupt index. Aborting lookup...\n");
    goto out;
}

static struct inode *ntfs_iget(const char *dname, struct inode *parent)
{
    return ntfs_index_lookup(dname, parent);
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

/* Initialize the filesystem metadata and return blk size in bits */
static int ntfs_fs_init(struct fs_info *fs)
{
    struct ntfs_bpb ntfs;
    struct ntfs_sb_info *sbi;
    struct disk *disk = fs->fs_dev->disk;
    uint8_t mft_record_shift;

    disk->rdwr_sectors(disk, &ntfs, 0, 1, 0);

    /* sanity check */
    if (!ntfs_check_sb_fields(&ntfs))
        return -1;

    SECTOR_SHIFT(fs) = disk->sector_shift;

    /* Note: ntfs.clust_per_mft_record can be a negative number.
     * If negative, it represents a shift count, else it represents
     * a multiplier for the cluster size.
     */
    mft_record_shift = ntfs.clust_per_mft_record < 0 ?
                    -ntfs.clust_per_mft_record :
                    ilog2(ntfs.sec_per_clust) + SECTOR_SHIFT(fs) +
                    ilog2(ntfs.clust_per_mft_record);

    SECTOR_SIZE(fs) = 1 << SECTOR_SHIFT(fs);

    sbi = malloc(sizeof *sbi);
    if (!sbi)
        malloc_error("ntfs_sb_info structure");

    fs->fs_info = sbi;

    sbi->clust_shift            = ilog2(ntfs.sec_per_clust);
    sbi->clust_byte_shift       = sbi->clust_shift + SECTOR_SHIFT(fs);
    sbi->clust_mask             = ntfs.sec_per_clust - 1;
    sbi->clust_size             = ntfs.sec_per_clust << SECTOR_SHIFT(fs);
    sbi->mft_record_size        = 1 << mft_record_shift;
    sbi->clust_per_idx_record   = ntfs.clust_per_idx_record;

    BLOCK_SHIFT(fs) = ilog2(ntfs.clust_per_idx_record) + sbi->clust_byte_shift;
    BLOCK_SIZE(fs) = 1 << BLOCK_SHIFT(fs);

    sbi->mft_lcn = ntfs.mft_lclust;
    sbi->mft_blk = ntfs.mft_lclust << sbi->clust_shift <<
                    SECTOR_SHIFT(fs) >> BLOCK_SHIFT(fs);
    /* 16 MFT entries reserved for metadata files (approximately 16 KiB) */
    sbi->mft_size = mft_record_shift << sbi->clust_shift << 4;

    sbi->clusters = ntfs.total_sectors << SECTOR_SHIFT(fs) >> sbi->clust_shift;
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
    .getfssec       = ntfs_getfssec,
    .close_file     = generic_close_file,
    .mangle_name    = generic_mangle_name,
    .load_config    = generic_load_config,
    .readdir        = ntfs_readdir,
    .iget_root      = ntfs_iget_root,
    .iget           = ntfs_iget,
    .next_extent    = ntfs_next_extent,
};
