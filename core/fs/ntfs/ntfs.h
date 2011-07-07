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
 * ntfs.h - The NTFS filesystem structures
 */

#ifndef _NTFS_H_
#define _NTFS_H_

struct ntfs_bpb {
    uint8_t jmp_boot[3];
    char oem_name[8];
    uint16_t sector_size;
    uint8_t sec_per_clust;
    uint16_t res_sectors;
    uint8_t zero_0[3];
    uint16_t zero_1;
    uint8_t media;
    uint16_t zero_2;
    uint16_t unused_0;
    uint16_t unused_1;
    uint32_t unused_2;
    uint32_t zero_3;
    uint32_t unused_3;
    uint64_t total_sectors;
    uint64_t mft_lclust;
    uint64_t mft_mirr_lclust;
    uint8_t clust_per_mft_record;
    uint8_t unused_4[3];
    uint8_t clust_per_idx_buf;
    uint8_t unused_5[3];
    uint64_t vol_serial;
    uint32_t unused_6;

    uint8_t pad[428];       /* padding to a sector boundary (512 bytes) */
} __attribute__((packed));

struct ntfs_sb_info {
    sector_t mft;                   /* The MFT region */
    sector_t root;                  /* The root dir region */

    unsigned mft_size;              /* The MFT size in sectors */
    unsigned mft_record_size;       /* MFT record size in bytes */

    unsigned long long clusters;    /* Total number of clusters */

    unsigned clust_shift;           /* Based on sectors */
    unsigned clust_byte_shift;      /* Based on bytes */
    unsigned clust_mask;
    unsigned clust_size;

} __attribute__((packed));

#endif /* _NTFS_H_ */
