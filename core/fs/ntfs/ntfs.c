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
            goto out;

        *block = *usa++;
        block = (uint16_t *)((uint8_t *)block + SECTOR_SIZE(fs));
    }

    return 0;

out:
    return -1;
}

static int64_t mft_record_lookup(uint32_t file, struct fs_info *fs,
                                    block_t *block, void *data)
{
    int64_t offset = 0;
    const uint8_t *ret;
    int err;
    const uint64_t blk_size = UINT64_C(1) << BLOCK_SHIFT(fs);
    MFT_RECORD *mrec;

    goto jump_in;

    for (;;) {
        err = fixups_writeback(fs, (NTFS_RECORD *)((uint8_t *)data + offset));
        if (err) {
            printf("Error in fixups_writeback()\n");
            break;
        }

        mrec = (MFT_RECORD *)((uint8_t *)data + offset);
        if (mrec->mft_record_no == file)
            return offset;   /* MFT record found! */

        offset += mrec->bytes_allocated;
        if (offset >= BLOCK_SIZE(fs)) {
            ++*block;
            offset -= BLOCK_SIZE(fs);
jump_in:
            ret = get_right_block(fs, *block);
            if (!ret)
                break;

            memcpy(data, ret, blk_size);
        }
    }

    return -1;
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

static bool ntfs_match_longname(const char *str, unsigned long mft_no,
                                    struct fs_info *fs)
{
    uint8_t data[BLOCK_SIZE(fs)];
    int64_t offset;
    MFT_RECORD *mrec;
    block_t block = 0;
    ATTR_RECORD *attr;
    FILE_NAME_ATTR *fn;
    uint8_t len;
    unsigned char c = -1;	/* Nonzero: we have not yet seen NULL */
    uint16_t cp;
    const uint16_t *match;

    dprintf("Matching: %s\n", str);

    offset = mft_record_lookup(mft_no, fs, &block, &data);
    if (offset < 0) {
        printf("No MFT record found.\n");
        goto out;
    }

    mrec = (MFT_RECORD *)&data[offset];

    attr = attr_lookup(NTFS_AT_FILENAME, mrec);
    if (!attr) {
        printf("No attribute found.\n");
        goto out;
    }

    fn = (FILE_NAME_ATTR *)((uint8_t *)attr + attr->data.resident.value_offset);
    len = fn->file_name_len;
    match = fn->file_name;

    while (len) {
        cp = *match++;
        len--;
        if (!cp)
            break;

        c = *str++;
        if (cp != codepage.uni[0][c] && cp != codepage.uni[1][c])
            goto out;
    }

    if (*str)
        goto out;

    while (len--)
        if (*match++ != 0xffff)
            goto out;

    return true;

out:
    return false;
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

static enum dirent_type get_inode_mode(MFT_RECORD *mrec)
{
    ATTR_RECORD *attr;
    FILE_NAME_ATTR *fn;
    bool infile = false;
    uint32_t dir_mask, root_mask, file_mask;
    uint32_t dir, root, file;

    attr = attr_lookup(NTFS_AT_FILENAME, mrec);
    if (!attr) {
        dprintf("No attribute found.\n");
        return DT_UNKNOWN;
    }

    fn = (FILE_NAME_ATTR *)((uint8_t *)attr +
                                attr->data.resident.value_offset);
    dprintf("File attributes:        0x%X\n", fn->file_attrs);

    dir_mask = NTFS_FILE_ATTR_ARCHIVE |
                NTFS_FILE_ATTR_DUP_FILE_NAME_INDEX_PRESENT;
    root_mask = NTFS_FILE_ATTR_READONLY | NTFS_FILE_ATTR_HIDDEN |
                NTFS_FILE_ATTR_SYSTEM |
                NTFS_FILE_ATTR_DUP_FILE_NAME_INDEX_PRESENT;
    file_mask = NTFS_FILE_ATTR_ARCHIVE;

    dir = fn->file_attrs & ~dir_mask;
    root = fn->file_attrs & ~root_mask;
    file = fn->file_attrs & ~file_mask;

    dprintf("dir = 0x%X\n", dir);
    dprintf("root= 0x%X\n", root);
    dprintf("file = 0x%X\n", file);
    if (((!dir && root) || (!dir && !root)) && !file)
        infile = true;

    return infile ? DT_REG : DT_DIR;
}

static int index_inode_setup(struct fs_info *fs, unsigned long mft_no,
                                struct inode *inode)
{
    uint8_t data[BLOCK_SIZE(fs)];
    int64_t offset;
    MFT_RECORD *mrec;
    block_t block = 0;
    ATTR_RECORD *attr;
    enum dirent_type d_type;
    uint32_t len;
    INDEX_ROOT *ir;
    uint32_t clust_size;
    uint8_t *attr_len;
    struct mapping_chunk chunk;
    int err;
    uint8_t *stream;
    uint32_t droffset;

    offset = mft_record_lookup(mft_no, fs, &block, &data);
    if (offset < 0) {
        printf("No MFT record found.\n");
        goto out;
    }

    mrec = (MFT_RECORD *)&data[offset];

    NTFS_PVT(inode)->mft_no = mft_no;
    NTFS_PVT(inode)->seq_no = mrec->seq_no;

    NTFS_PVT(inode)->start_cluster = block >> NTFS_SB(fs)->clust_shift;
    NTFS_PVT(inode)->here = block;

    d_type = get_inode_mode(mrec);
    if (d_type == DT_UNKNOWN) {
        dprintf("Failed on determining inode's mode\n");
        goto out;
    }

    if (d_type == DT_DIR) {    /* directory stuff */
        dprintf("Got a directory.\n");
        attr = attr_lookup(NTFS_AT_INDEX_ROOT, mrec);
        if (!attr) {
            dprintf("No attribute found.\n");
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
    } else if (d_type == DT_REG) {        /* file stuff */
        dprintf("Got a file.\n");
        attr = attr_lookup(NTFS_AT_DATA, mrec);
        if (!attr) {
            dprintf("No attribute found.\n");
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

            stream = mapping_chunk_init(attr, &chunk, &droffset);
            for (;;) {
                err = parse_data_run(stream, &droffset, attr_len, &chunk);
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
    const uint64_t blk_size = BLOCK_SIZE(fs);
    uint8_t data[blk_size];
    int64_t offset;
    MFT_RECORD *mrec;
    block_t block;
    ATTR_RECORD *attr;
    INDEX_ROOT *ir;
    uint32_t len;
    INDEX_ENTRY *ie;
    uint8_t *ret;
    INDEX_BLOCK *iblock;
    int err;
    uint8_t *stream;
    uint8_t *attr_len;
    struct mapping_chunk chunk;
    uint32_t droffset;
    struct mapping_chunk *chunks;
    unsigned chunks_count;
    unsigned i;
    int64_t vcn;
    int64_t lcn;
    struct inode *inode;

    block = NTFS_PVT(dir)->start;
    dprintf("index_lookup() - mft record number: %d\n", NTFS_PVT(dir)->mft_no);
    offset = mft_record_lookup(NTFS_PVT(dir)->mft_no, fs, &block, &data);
    if (offset < 0) {
        printf("No MFT record found.\n");
        goto out;
    }

    mrec = (MFT_RECORD *)&data[offset];

    attr = attr_lookup(NTFS_AT_INDEX_ROOT, mrec);
    if (!attr) {
        dprintf("No attribute found.\n");
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

        if (ntfs_match_longname(dname, ie->data.dir.indexed_file, fs)) {
            dprintf("Filename matches up!\n");
            dprintf("MFT record number = %d\n", ie->data.dir.indexed_file);
            goto found;
        }
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

    stream = mapping_chunk_init(attr, &chunk, &droffset);
    chunks_count = 0;
    do {
        err = parse_data_run(stream, &droffset, attr_len, &chunk);
        if (err)
            break;

        if (chunk.flags & MAP_UNALLOCATED)
            continue;

        if (chunk.flags & MAP_ALLOCATED)
            chunks_count++;

    } while (!(chunk.flags & MAP_END));

    if (!chunks_count) {
        printf("No data runs found, but claims non-resident attribute\n");
        goto out;
    }

    chunks = malloc(chunks_count * sizeof *chunks);
    if (!chunks)
        malloc_error("mapping_chunk structure");

    stream = mapping_chunk_init(attr, &chunk, &droffset);
    i = 0;
    do {
        err = parse_data_run(stream, &droffset, attr_len, &chunk);
        if (err)
            break;

        if (chunk.flags & MAP_UNALLOCATED)
            continue;

        if (chunk.flags & MAP_ALLOCATED) {
            dprintf("%d cluster(s) starting at 0x%X\n", chunk.len, chunk.lcn);
            memcpy(&chunks[i++], &chunk, sizeof chunk);
        }

    } while (!(chunk.flags & MAP_END));

    i = 0;
    while (chunks_count--) {
        vcn = 0;
        lcn = chunks[i].lcn;
        while (vcn < chunks[i].len) {
            block = ((lcn + vcn) << NTFS_SB(fs)->clust_shift) <<
                    SECTOR_SHIFT(fs) >> BLOCK_SHIFT(fs);

            ret = (uint8_t *)get_cache(fs->fs_dev, block);
            if (!ret) {
                printf("get_cache() returned NULL\n");
                goto not_found;
            }

            memcpy(data, ret, blk_size);

            err = fixups_writeback(fs, (NTFS_RECORD *)&data[0]);
            if (err) {
                printf("Error in fixups_writeback()\n");
                goto not_found;
            }

            iblock = (INDEX_BLOCK *)&data[0];
            if (iblock->magic != NTFS_MAGIC_INDX) {
                printf("Not a valid INDX record\n");
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

                if (ntfs_match_longname(dname, ie->data.dir.indexed_file, fs)) {
                    dprintf("Filename matches up!\n");
                    goto found;
                }
            }

            vcn++;  /* go to the next VCN */
        }

        i++;        /* go to the next chunk */
    }

not_found:
    dprintf("Index not found\n");

out:
    dprintf("%s not found!\n", dname);

    return NULL;

found:
    dprintf("Index found\n");
    inode = new_ntfs_inode(fs);
    err = index_inode_setup(fs, ie->data.dir.indexed_file, inode);
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

/*
 * Convert an UTF-16LE longname to the system codepage; return
 * the length on success or -1 on failure.
 */
static int ntfs_cvt_longname(char *entry_name, const uint16_t *long_name)
{
    struct unicache {
        uint16_t utf16;
        uint8_t cp;
    };
    static struct unicache unicache[256];
    struct unicache *uc;
    uint16_t cp;
    unsigned int c;
    char *p = entry_name;

    do {
        cp = *long_name++;
        uc = &unicache[cp % 256];

        if (__likely(uc->utf16 == cp)) {
            *p++ = uc->cp;
        } else {
            for (c = 0; c < 512; c++) {
                if (codepage.uni[0][c] == cp) {
                    uc->utf16 = cp;
                    *p++ = uc->cp = (uint8_t)c;
                    goto found;
                }
            }

            return -1;
            found:
                ;
        }
    } while (cp);

    return (p - entry_name) - 1;
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
    int64_t offset;
    struct fs_info *fs = file->fs;
    uint8_t data[BLOCK_SIZE(fs)];
    struct inode *inode = file->inode;
    block_t block = 0;
    MFT_RECORD *mrec;
    ATTR_RECORD *attr;
    char *p;

    non_resident = NTFS_PVT(inode)->non_resident;

    ret = generic_getfssec(file, buf, sectors, have_more);
    if (!ret)
        return ret;

    if (!non_resident) {
        dprintf("mft_no:     %d\n", NTFS_PVT(inode)->mft_no);
        offset = mft_record_lookup(NTFS_PVT(inode)->mft_no, fs, &block, &data);
        if (offset < 0) {
            printf("No MFT record found.\n");
            goto out;
        }

        mrec = (MFT_RECORD *)&data[offset];

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
    int64_t offset;
    struct fs_info *fs = file->fs;
    uint8_t data[BLOCK_SIZE(fs)];
    MFT_RECORD *mrec;
    struct inode *inode = file->inode;
    block_t block = 0;
    ATTR_RECORD *attr;
    FILE_NAME_ATTR *fn;
    char filename[NTFS_MAX_FILE_NAME_LEN + 1];
    int len;

    offset = mft_record_lookup(NTFS_PVT(inode)->mft_no, fs, &block, &data);
    if (offset < 0) {
        printf("No MFT record found.\n");
        goto out;
    }

    mrec = (MFT_RECORD *)&data[offset];

    attr = attr_lookup(NTFS_AT_FILENAME, mrec);
    if (!attr) {
        printf("No attribute found.\n");
        goto out;
    }

    fn = (FILE_NAME_ATTR *)((uint8_t *)attr +
                            attr->data.resident.value_offset);

    len = ntfs_cvt_longname(filename, fn->file_name);
    if (len < 0 || len != fn->file_name_len) {
        printf("Failed on converting UTF-16LE LFN to OEM LFN\n");
        goto out;
    }

    dirent->d_ino = NTFS_PVT(inode)->mft_no;
    dirent->d_off = file->offset++;
    dirent->d_reclen = offsetof(struct dirent, d_name) + len + 1;
    dirent->d_type = get_inode_mode(mrec);
    memcpy(dirent->d_name, filename, len + 1);

    return 0;

out:
    return -1;
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
