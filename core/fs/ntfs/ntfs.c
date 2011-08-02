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
 * 59 Temple Place - Suite 330, Boston, MA 02111EXIT_FAILURE307, USA.
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

static inline const void *get_right_block(struct fs_info *fs,
                                                        block_t block)
{
    return get_cache(fs->fs_dev, NTFS_SB(fs)->mft_block + block);
}

static int fixups_writeback(struct fs_info *fs, NTFS_RECORD *nrec)
{
    uint16_t *usa;
    uint16_t usa_no;
    uint16_t usa_count;
    uint16_t *block;

    if (nrec->magic != NTFS_MAGIC_FILE && nrec->magic != NTFS_MAGIC_INDX) {
        printf("Not a valid NTFS record\n");
        goto out;
    }

    /* get the Update Sequence Array offset */
    usa = (uint16_t *)((uint8_t *)nrec + nrec->usa_ofs);
    /* get the Update Sequence Array Number and skip it */
    usa_no = *usa++;
    /* get the Update Sequene Array count */
    usa_count = nrec->usa_count - 1;    /* exclude the USA number */
    /* make it to point to the last two bytes of the RECORD's first sector */
    block = (uint16_t *)((uint8_t *)nrec + SECTOR_SIZE(fs) - 2);

    while (usa_count--) {
        if (*block != usa_no)
            break;

        *block = *usa++;
        block = (uint16_t *)((uint8_t *)block + SECTOR_SIZE(fs));
    }

    return 0;

out:
    return -1;
}

static MFT_RECORD *mft_record_lookup(uint32_t file, struct fs_info *fs,
                                    block_t *block)
{
    uint8_t *data;
    int64_t offset = 0;
    int err;
    MFT_RECORD *mrec;

    goto jump_in;

    for (;;) {
        err = fixups_writeback(fs, (NTFS_RECORD *)(data + offset));
        if (err)
            break;

        mrec = (MFT_RECORD *)(data + offset);
        if (mrec->mft_record_no == file)
            return mrec;   /* MFT record found! */

        offset += mrec->bytes_allocated;
        if (offset >= BLOCK_SIZE(fs)) {
            ++*block;
            offset -= BLOCK_SIZE(fs);
jump_in:
            data = (uint8_t *)get_right_block(fs, *block);
            if (!data)
                break;
        }
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

static bool ntfs_cmp_filename(const char *dname, INDEX_ENTRY *ie)
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

static int index_inode_setup(struct fs_info *fs, block_t start_block,
                            unsigned long mft_no, struct inode *inode)
{
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

    mrec = mft_record_lookup(mft_no, fs, &start_block);
    if (!mrec) {
        printf("No MFT record found.\n");
        goto out;
    }

    NTFS_PVT(inode)->mft_no = mft_no;
    NTFS_PVT(inode)->seq_no = mrec->seq_no;

    NTFS_PVT(inode)->start_cluster = start_block >> NTFS_SB(fs)->clust_shift;
    NTFS_PVT(inode)->here = start_block;

    d_type = get_inode_mode(mrec);
    if (d_type == DT_DIR) {    /* directory stuff */
        dprintf("Got a directory.\n");
        attr = attr_lookup(NTFS_AT_INDEX_ROOT, mrec);
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
        NTFS_PVT(inode)->itype.index.block_size = ir->index_block_size;
        NTFS_PVT(inode)->itype.index.block_size_shift =
                            ilog2(NTFS_PVT(inode)->itype.index.block_size);

        /* determine the size of a vcn in the index */
        clust_size = NTFS_PVT(inode)->itype.index.block_size;
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
        attr = attr_lookup(NTFS_AT_DATA, mrec);
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

    return 0;

out:
    return -1;
}

static struct inode *index_lookup(const char *dname, struct inode *dir)
{
    struct fs_info *fs = dir->fs;
    MFT_RECORD *mrec;
    block_t block;
    ATTR_RECORD *attr;
    INDEX_ROOT *ir;
    uint32_t len;
    INDEX_ENTRY *ie;
    uint8_t *data;
    INDEX_BLOCK *iblock;
    int err;
    uint8_t *stream;
    uint8_t *attr_len;
    struct mapping_chunk chunk;
    uint32_t offset;
    int64_t vcn;
    int64_t lcn;
    struct inode *inode;

    block = NTFS_PVT(dir)->start;
    dprintf("index_lookup() - mft record number: %d\n", NTFS_PVT(dir)->mft_no);
    mrec = mft_record_lookup(NTFS_PVT(dir)->mft_no, fs, &block);
    if (!mrec) {
        printf("No MFT record found.\n");
        goto out;
    }

    attr = attr_lookup(NTFS_AT_INDEX_ROOT, mrec);
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

        if (ntfs_cmp_filename(dname, ie))
            goto found;
    }

    /* check for the presence of a child node */
    if (!(ie->flags & INDEX_ENTRY_NODE)) {
        printf("No child node, aborting...\n");
        goto out;
    }

    /* then descend into child node */

    attr = attr_lookup(NTFS_AT_INDEX_ALLOCATION, mrec);
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
            dprintf("%d cluster(s) starting at 0x%X\n", chunk.len, chunk.lcn);

            vcn = 0;
            lcn = chunk.lcn;
            while (vcn < chunk.len) {
                block = ((lcn + vcn) << NTFS_SB(fs)->clust_shift) <<
                        SECTOR_SHIFT(fs) >> BLOCK_SHIFT(fs);

                data = (uint8_t *)get_cache(fs->fs_dev, block);
                if (!data) {
                    printf("get_cache() returned NULL.\n");
                    goto not_found;
                }

                err = fixups_writeback(fs, (NTFS_RECORD *)data);
                if (err)
                    goto not_found;

                iblock = (INDEX_BLOCK *)&data[0];
                if (iblock->magic != NTFS_MAGIC_INDX) {
                    printf("Not a valid INDX record.\n");
                    goto not_found;
                }

                ie = (INDEX_ENTRY *)((uint8_t *)&iblock->index +
                                            iblock->index.entries_offset);
                for (;; ie = (INDEX_ENTRY *)((uint8_t *)ie + ie->len)) {
                    /* bounds checks */
                    if ((uint8_t *)ie < (uint8_t *)iblock || (uint8_t *)ie +
                        sizeof(INDEX_ENTRY_HEADER) >
                        (uint8_t *)&iblock->index + iblock->index.index_len ||
                        (uint8_t *)ie + ie->len >
                        (uint8_t *)&iblock->index + iblock->index.index_len)
                        goto index_err;

                    /* last entry cannot contain a key */
                    if (ie->flags & INDEX_ENTRY_END)
                        break;

                    if (ntfs_cmp_filename(dname, ie))
                        goto found;
                }

                vcn++;  /* go to the next VCN */
            }
        }
    } while (!(chunk.flags & MAP_END));

not_found:
    dprintf("Index not found\n");

out:
    dprintf("%s not found!\n", dname);

    return NULL;

found:
    dprintf("Index found\n");
    inode = new_ntfs_inode(fs);
    err = index_inode_setup(fs, NTFS_PVT(dir)->here, ie->data.dir.indexed_file,
                            inode);
    if (err) {
        free(inode);
        goto out;
    }

    dprintf("%s found!\n", dname);

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
        pstart = sbi->mft_block + NTFS_PVT(inode)->here;
        pstart <<= BLOCK_SHIFT(fs) >> sec_shift;
    } else {
        pstart = NTFS_PVT(inode)->data.non_resident.lcn << sbi->clust_shift;
    }

    inode->next_extent.len = (inode->size + sec_size - 1) >> sec_shift;
    inode->next_extent.pstart = pstart;

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
    block_t block;
    MFT_RECORD *mrec;
    ATTR_RECORD *attr;
    char *p;

    non_resident = NTFS_PVT(inode)->non_resident;

    ret = generic_getfssec(file, buf, sectors, have_more);
    if (!ret)
        return ret;

    if (!non_resident) {
        block = NTFS_PVT(inode)->here;
        mrec = mft_record_lookup(NTFS_PVT(inode)->mft_no, fs, &block);
        if (!mrec) {
            printf("No MFT record found.\n");
            goto out;
        }

        attr = attr_lookup(NTFS_AT_DATA, mrec);
        if (!attr) {
            printf("No attribute found.\n");
            goto out;
        }

        p = (char *)((uint8_t *)attr + attr->data.resident.value_offset);

        /* p now points to the data offset, so let's copy it into buf */
        memcpy(buf, p, inode->size);

        ret = inode->size;
    }

    return ret;

out:
    return 0;
}

static int ntfs_readdir(struct file *file, struct dirent *dirent)
{
    struct fs_info *fs = file->fs;
    struct inode *inode = file->inode;
    MFT_RECORD *mrec;
    block_t block;
    ATTR_RECORD *attr;
    INDEX_ROOT *ir;
    uint32_t count;
    int len;
    INDEX_ENTRY *ie = NULL;
    uint8_t *data;
    INDEX_BLOCK *iblock;
    int err;
    uint8_t *stream;
    uint8_t *attr_len;
    struct mapping_chunk chunk;
    uint32_t offset;
    int64_t vcn;
    int64_t lcn;
    char filename[NTFS_MAX_FILE_NAME_LEN + 1];

    block = NTFS_PVT(inode)->here;
    mrec = mft_record_lookup(NTFS_PVT(inode)->mft_no, fs, &block);
    if (!mrec) {
        printf("No MFT record found.\n");
        goto out;
    }

    attr = attr_lookup(NTFS_AT_INDEX_ROOT, mrec);
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

    if (NTFS_PVT(inode)->in_idx_root) {
        ie = (INDEX_ENTRY *)(uint8_t *)file->offset;
        if (ie->flags & INDEX_ENTRY_END) {
            file->offset = 0;
            NTFS_PVT(inode)->in_idx_root = false;
            NTFS_PVT(inode)->idx_blocks_count = 1;
            NTFS_PVT(inode)->entries_count = 0;
            NTFS_PVT(inode)->last_vcn = 0;
            goto descend_into_child_node;
        }

        file->offset = (uint32_t)((uint8_t *)ie + ie->len);
        goto done;
    }

descend_into_child_node:
    if (!(ie->flags & INDEX_ENTRY_NODE))
        goto out;

    attr = attr_lookup(NTFS_AT_INDEX_ALLOCATION, mrec);
    if (!attr)
        goto out;

    if (!attr->non_resident) {
        printf("WTF ?! $INDEX_ALLOCATION isn't really resident.\n");
        goto out;
    }

    attr_len = (uint8_t *)attr + attr->len;

next_run:
    stream = mapping_chunk_init(attr, &chunk, &offset);
    count = NTFS_PVT(inode)->idx_blocks_count;
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
       NTFS_PVT(inode)->idx_blocks_count++;
       goto next_run;
    }

next_vcn:
    vcn = NTFS_PVT(inode)->last_vcn;
    if (vcn >= chunk.len) {
        NTFS_PVT(inode)->last_vcn = 0;
        NTFS_PVT(inode)->idx_blocks_count++;
        goto next_run;
    }

    lcn = chunk.lcn;
    block = ((lcn + vcn) << NTFS_SB(fs)->clust_shift) << SECTOR_SHIFT(fs) >>
            BLOCK_SHIFT(fs);

    data = (uint8_t *)get_cache(fs->fs_dev, block);
    if (!data) {
        printf("get_cache() returned NULL.\n");
        goto not_found;
    }

    err = fixups_writeback(fs, (NTFS_RECORD *)data);
    if (err)
        goto not_found;

    iblock = (INDEX_BLOCK *)data;
    if (iblock->magic != NTFS_MAGIC_INDX) {
        printf("Not a valid INDX record.\n");
        goto not_found;
    }

    ie = (INDEX_ENTRY *)((uint8_t *)&iblock->index +
                        iblock->index.entries_offset);
    count = NTFS_PVT(inode)->entries_count;
    for ( ; count--; ie = (INDEX_ENTRY *)((uint8_t *)ie + ie->len)) {
        /* bounds checks */
        if ((uint8_t *)ie < (uint8_t *)iblock || (uint8_t *)ie +
            sizeof(INDEX_ENTRY_HEADER) >
            (uint8_t *)&iblock->index + iblock->index.index_len ||
            (uint8_t *)ie + ie->len >
            (uint8_t *)&iblock->index + iblock->index.index_len)
            goto index_err;

        /* last entry cannot contain a key */
        if (ie->flags & INDEX_ENTRY_END) {
            NTFS_PVT(inode)->last_vcn++;
            NTFS_PVT(inode)->entries_count = 0;
            goto next_vcn;
        }
    }

    NTFS_PVT(inode)->entries_count++;
    goto done;

out:
    NTFS_PVT(inode)->in_idx_root = true;

    return -1;

done:
    dirent->d_ino = ie->data.dir.indexed_file;
    dirent->d_off = file->offset;

    len = ntfs_cvt_filename(filename, ie);
    dirent->d_reclen = offsetof(struct dirent, d_name) + len + 1;

    if (NTFS_PVT(inode)->mft_no == FILE_root)
        block = 0;
    else
        block = NTFS_SB(fs)->mft_block;

    mrec = mft_record_lookup(ie->data.dir.indexed_file, fs, &block);
    if (!mrec) {
        printf("No MFT record found.\n");
        goto out;
    }

    dirent->d_type = get_inode_mode(mrec);
    memcpy(dirent->d_name, filename, len + 1);

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
    return index_lookup(dname, parent);
}

static struct inode *ntfs_iget_root(struct fs_info *fs)
{
    struct inode *inode = new_ntfs_inode(fs);
    int err;

    inode->fs = fs;

    err = index_inode_setup(fs, 0, FILE_root, inode);
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
    uint8_t clust_per_mft_record;

    disk->rdwr_sectors(disk, &ntfs, 0, 1, 0);

    /* sanity check */
    if (!ntfs_check_sb_fields(&ntfs))
        return -1;

    /* Note: clust_per_mft_record can be a negative number */
    clust_per_mft_record = ntfs.clust_per_mft_record < 0 ?
                    -ntfs.clust_per_mft_record : ntfs.clust_per_mft_record;

    SECTOR_SHIFT(fs) = disk->sector_shift;

    /* We need _at least_ 1 KiB to read the whole MFT record */
    BLOCK_SHIFT(fs) = ilog2(ntfs.sec_per_clust) + SECTOR_SHIFT(fs);
    if (BLOCK_SHIFT(fs) < clust_per_mft_record)
        BLOCK_SHIFT(fs) = clust_per_mft_record;

    SECTOR_SIZE(fs) = 1 << SECTOR_SHIFT(fs);
    BLOCK_SIZE(fs) = 1 << BLOCK_SHIFT(fs);

    sbi = malloc(sizeof *sbi);
    if (!sbi)
        malloc_error("ntfs_sb_info structure");

    fs->fs_info = sbi;

    sbi->clust_shift        = ilog2(ntfs.sec_per_clust);
    sbi->clust_byte_shift   = sbi->clust_shift + SECTOR_SHIFT(fs);
    sbi->clust_mask         = ntfs.sec_per_clust - 1;
    sbi->clust_size         = ntfs.sec_per_clust << SECTOR_SHIFT(fs);
    sbi->mft_record_size    = 1 << clust_per_mft_record;

    sbi->mft_block = ntfs.mft_lclust << sbi->clust_shift <<
                    SECTOR_SHIFT(fs) >> BLOCK_SHIFT(fs);
    /* 16 MFT entries reserved for metadata files (approximately 16 KiB) */
    sbi->mft_size = (clust_per_mft_record << sbi->clust_shift) << 4;

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
