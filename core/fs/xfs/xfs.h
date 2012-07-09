/*
 * Copyright (c) 2012 Paulo Alcantara <pcacjr@zytor.com>
 *
 * Some parts borrowed from Linux kernel tree (linux/fs/xfs):
 *
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
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

#ifndef XFS_H_
#define XFS_H_

#include <disk.h>
#include <fs.h>

#include "xfs_types.h"
#include "xfs_ag.h"

#define xfs_error(fmt, args...) \
    printf("xfs: " fmt "\n", ## args);

#define xfs_debug(fmt, args...) \
    printf("%s: " fmt "\n", __func__, ## args);

struct xfs_fs_info;

#define XFS_INFO(fs) ((struct xfs_fs_info *)((fs)->fs_info))
#define XFS_PVT(ino) ((struct xfs_inode *)((ino)->pvt))

#define XFS_INO_TO_AGNO(fs, ino) \
    (xfs_agnumber_t)((ino) >> XFS_INFO((fs))->ag_relative_ino_shift)

#define XFS_AGNO_TO_FSB(fs, agno) \
    (block_t)((agno) << XFS_INFO((fs))->agblocks_shift)

#define XFS_AGI_OFFS(fs, mp) \
    (xfs_agi_t *)((uint8_t *)(mp) + 2 * SECTOR_SIZE((fs)))

/* Superblock's LBA */
#define XFS_SB_DADDR ((xfs_daddr_t)0) /* daddr in filesystem/ag */

/* Magic numbers */
#define	XFS_AGI_MAGIC 		"XAGI"
#define XFS_IBT_MAGIC 		"IABT"
#define XFS_DINODE_MAGIC	"IN"

/* File types and modes */
#define S_IFMT  	00170000
#define S_IFSOCK 	0140000
#define S_IFLNK 	0120000
#define S_IFREG  	0100000
#define S_IFBLK  	0060000
#define S_IFDIR  	0040000
#define S_IFCHR  	0020000
#define S_IFIFO  	0010000
#define S_ISUID  	0004000
#define S_ISGID  	0002000
#define S_ISVTX  	0001000

/*
 * NOTE: The fields in the superblock are stored in big-endian format on disk.
 */
typedef struct xfs_sb {
    uint32_t	sb_magicnum;	/* magic number == XFS_SB_MAGIC */
    uint32_t	sb_blocksize;	/* logical block size, bytes */
    xfs_drfsbno_t	sb_dblocks;	/* number of data blocks */
    xfs_drfsbno_t	sb_rblocks;	/* number of realtime blocks */
    xfs_drtbno_t	sb_rextents;	/* number of realtime extents */
    uuid_t		sb_uuid;	/* file system unique id */
    xfs_dfsbno_t	sb_logstart;	/* starting block of log if internal */
    xfs_ino_t	sb_rootino;	/* root inode number */
    xfs_ino_t	sb_rbmino;	/* bitmap inode for realtime extents */
    xfs_ino_t	sb_rsumino;	/* summary inode for rt bitmap */
    xfs_agblock_t	sb_rextsize;	/* realtime extent size, blocks */
    xfs_agblock_t	sb_agblocks;	/* size of an allocation group */
    xfs_agnumber_t	sb_agcount;	/* number of allocation groups */
    xfs_extlen_t	sb_rbmblocks;	/* number of rt bitmap blocks */
    xfs_extlen_t	sb_logblocks;	/* number of log blocks */
    uint16_t	sb_versionnum;	/* header version == XFS_SB_VERSION */
    uint16_t	sb_sectsize;	/* volume sector size, bytes */
    uint16_t	sb_inodesize;	/* inode size, bytes */
    uint16_t	sb_inopblock;	/* inodes per block */
    char	sb_fname[12];	/* file system name */
    uint8_t	sb_blocklog;	/* log2 of sb_blocksize */
    uint8_t	sb_sectlog;	/* log2 of sb_sectsize */
    uint8_t	sb_inodelog;	/* log2 of sb_inodesize */
    uint8_t	sb_inopblog;	/* log2 of sb_inopblock */
    uint8_t	sb_agblklog;	/* log2 of sb_agblocks (rounded up) */
    uint8_t	sb_rextslog;	/* log2 of sb_rextents */
    uint8_t	sb_inprogress;	/* mkfs is in progress, don't mount */
    uint8_t	sb_imax_pct;	/* max % of fs for inode space */
					/* statistics */
    /*
     * These fields must remain contiguous.  If you really
     * want to change their layout, make sure you fix the
     * code in xfs_trans_apply_sb_deltas().
     */
    uint64_t	sb_icount;	/* allocated inodes */
    uint64_t	sb_ifree;	/* free inodes */
    uint64_t	sb_fdblocks;	/* free data blocks */
    uint64_t	sb_frextents;	/* free realtime extents */
    /*
     * End contiguous fields.
     */
    xfs_ino_t	sb_uquotino;	/* user quota inode */
    xfs_ino_t	sb_gquotino;	/* group quota inode */
    uint16_t	sb_qflags;	/* quota flags */
    uint8_t	sb_flags;	/* misc. flags */
    uint8_t	sb_shared_vn;	/* shared version number */
    xfs_extlen_t	sb_inoalignmt;	/* inode chunk alignment, fsblocks */
    uint32_t	sb_unit;	/* stripe or raid unit */
    uint32_t	sb_width;	/* stripe or raid width */
    uint8_t	sb_dirblklog;	/* log2 of dir block size (fsbs) */
    uint8_t	sb_logsectlog;	/* log2 of the log sector size */
    uint16_t	sb_logsectsize;	/* sector size for the log, bytes */
    uint32_t	sb_logsunit;	/* stripe unit size for the log */
    uint32_t	sb_features2;	/* additional feature bits */

    /*
     * bad features2 field as a result of failing to pad the sb
     * structure to 64 bits. Some machines will be using this field
     * for features2 bits. Easiest just to mark it bad and not use
     * it for anything else.
     */
    uint32_t	sb_bad_features2;
    uint8_t	pad[304]; /* must be padded to a sector boundary */
} __attribute__((__packed__)) xfs_sb_t;

/* In-memory structure that stores filesystem-specific information.
 * The information stored is basically retrieved from the XFS superblock
 * to be used statically around the driver.
 */
struct xfs_fs_info {
    uint32_t 		blocksize; /* Filesystem block size */
    uint8_t		block_shift; /* Filesystem block size in bits */

    /* AG relative inode number bits (found within AG's inode structures) */
    uint8_t		ag_relative_ino_shift;
    /* AG number bits (MSB of the inode number) */
    uint8_t		ag_number_ino_shift;

    xfs_ino_t 		rootino; /* Root inode number for the filesystem */

    xfs_agblock_t	agblocks; /* Size of each AG in blocks */
    uint8_t		agblocks_shift; /* agblocks in bits */
    xfs_agnumber_t	agcount; /* Number of AGs in the filesytem */

    uint16_t		inodesize; /* Size of the inode in bytes */
    uint8_t		inode_shift; /* Inode size in bits */
} __attribute__((__packed__));

typedef struct xfs_agi {
	/*
	 * Common allocation group header information
	 */
    uint32_t		agi_magicnum;	/* magic number == XFS_AGI_MAGIC */
    uint32_t		agi_versionnum;	/* header version == XFS_AGI_VERSION */
    uint32_t		agi_seqno;	/* sequence # starting from 0 */
    uint32_t		agi_length;	/* size in blocks of a.g. */
    /*
     * Inode information
     * Inodes are mapped by interpreting the inode number, so no
     * mapping data is needed here.
     */
    uint32_t		agi_count;	/* count of allocated inodes */
    uint32_t		agi_root;	/* root of inode btree */
    uint32_t		agi_level;	/* levels in inode btree */
    uint32_t		agi_freecount;	/* number of free inodes */
    uint32_t		agi_newino;	/* new inode just allocated */
    uint32_t		agi_dirino;	/* last directory inode chunk */
    /*
     * Hash table of inodes which have been unlinked but are
     * still being referenced.
     */
    uint32_t		agi_unlinked[XFS_AGI_UNLINKED_BUCKETS];
} __attribute__((__packed__)) xfs_agi_t;

typedef struct xfs_btree_sblock {
    uint32_t bb_magic;
    uint16_t bb_level;
    uint16_t bb_numrecs;
    uint32_t bb_leftsib;
    uint32_t bb_rightsib;
} __attribute__((__packed__)) xfs_btree_sblock_t;

typedef struct xfs_inobt_rec {
    uint32_t ir_startino;
    uint32_t ir_freecount;
    uint64_t ir_free;
} __attribute__((__packed__)) xfs_inobt_rec_t;

typedef struct xfs_timestamp {
    int32_t t_sec;
    int32_t t_nsec;
} __attribute__((__packed__)) xfs_timestamp_t;

typedef enum xfs_dinode_fmt {
    XFS_DINODE_FMT_DEV,
    XFS_DINODE_FMT_LOCAL,
    XFS_DINODE_FMT_EXTENTS,
    XFS_DINODE_FMT_BTREE,
    XFS_DINODE_FMT_UUID,
} xfs_dinode_fmt_t;

typedef struct xfs_dinode {
    uint16_t		di_magic;	/* inode magic # = XFS_DINODE_MAGIC */
    uint16_t		di_mode;	/* mode and type of file */
    uint8_t		di_version;	/* inode version */
    uint8_t		di_format;	/* format of di_c data */
    uint16_t		di_onlink;	/* old number of links to file */
    uint32_t		di_uid;		/* owner's user id */
    uint32_t		di_gid;		/* owner's group id */
    uint32_t		di_nlink;	/* number of links to file */
    uint16_t		di_projid_lo;	/* lower part of owner's project id */
    uint16_t		di_projid_hi;	/* higher part owner's project id */
    uint8_t		di_pad[6];	/* unused, zeroed space */
    uint16_t		di_flushiter;	/* incremented on flush */
    xfs_timestamp_t	di_atime;	/* time last accessed */
    xfs_timestamp_t	di_mtime;	/* time last modified */
    xfs_timestamp_t	di_ctime;	/* time created/inode modified */
    uint64_t		di_size;	/* number of bytes in file */
    uint64_t		di_nblocks;	/* # of direct & btree blocks used */
    uint32_t		di_extsize;	/* basic/minimum extent size for file */
    uint32_t		di_nextents;	/* number of extents in data fork */
    uint16_t		di_anextents;	/* number of extents in attribute fork*/
    uint8_t		di_forkoff;	/* attr fork offs, <<3 for 64b align */
    int8_t		di_aformat;	/* format of attr fork's data */
    uint32_t		di_dmevmask;	/* DMIG event mask */
    uint16_t		di_dmstate;	/* DMIG state info */
    uint16_t		di_flags;	/* random flags, XFS_DIFLAG_... */
    uint32_t		di_gen;		/* generation number */

    /* di_next_unlinked is the only non-core field in the old dinode */
    uint32_t		di_next_unlinked;/* agi unlinked list ptr */
} __attribute__((packed)) xfs_dinode_t;

struct xfs_inode {
    xfs_agblock_t i_agblock;
};

static inline bool xfs_is_valid_magicnum(const xfs_sb_t *sb)
{
    return sb->sb_magicnum == *(uint32_t *)XFS_SB_MAGIC;
}

static inline bool xfs_is_valid_agi(xfs_agi_t *agi)
{
    return agi->agi_magicnum == *(uint32_t *)XFS_AGI_MAGIC;
}

#endif /* XFS_H_ */
