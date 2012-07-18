/*
 * Copyright (c) 2012 Paulo Alcantara <pcacjr@zytor.com>
 *
 * Some parts borrowed from Linux kernel tree (linux/fs/xfs):
 *
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
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

#define XFS_INO_MASK(k)                 (uint32_t)((1ULL << (k)) - 1)
#define XFS_INO_OFFSET_BITS(fs)		(fs)->inopb_shift
#define XFS_INO_AGINO_BITS(fs) \
    (XFS_INFO((fs))->inopb_shift + XFS_INFO((fs))->agblk_shift)

#define XFS_INO_TO_AGINO(fs, i) \
    ((xfs_agino_t)(i) & XFS_INO_MASK(XFS_INO_AGINO_BITS(fs)))

#define XFS_INO_TO_AGNO(fs, ino) \
    ((xfs_agnumber_t)((ino) >> (XFS_INFO((fs))->inopb_shift + \
				XFS_INFO((fs))->agblk_shift)))

#define XFS_INO_TO_OFFSET(fs, i) \
	((int)(i) & XFS_INO_MASK(XFS_INO_OFFSET_BITS(fs)))

#define XFS_AGNO_TO_FSB(fs, agno) \
    ((block_t)((agno) << XFS_INFO((fs))->agblocks_shift))

#define XFS_AGI_OFFS(fs, mp) \
    ((xfs_agi_t *)((uint8_t *)(mp) + 2 * SECTOR_SIZE((fs))))

#define XFS_GET_DIR_INO4(di) \
    (((uint32_t)(di).i[0] << 24) | ((di).i[1] << 16) | ((di).i[2] << 8) | \
		((di).i[3]))

#define XFS_DI_HI(di) \
    (((uint32_t)(di).i[1] << 16) | ((di).i[2] << 8) | ((di).i[3]))

#define XFS_DI_LO(di) \
    (((uint32_t)(di).i[4] << 24) | ((di).i[5] << 16) | ((di).i[6] << 8) | \
		((di).i[7]))

#define XFS_GET_DIR_INO8(di) \
    (((xfs_ino_t)XFS_DI_LO(di) & 0xffffffffULL) | \
     ((xfs_ino_t)XFS_DI_HI(di) << 32))

#define XFS_FSB_TO_AGNO(fs, fsbno) \
    ((xfs_agnumber_t)((fsbno) >> XFS_INFO((fs))->agblk_shift))
#define XFS_FSB_TO_AGBNO(fs, fsbno) \
    ((xfs_agblock_t)((fsbno) & (uint32_t)((1ULL << \
					   XFS_INFO((fs))->agblk_shift) - 1)))

#define agblock_to_bytes(fs, x) \
    ((uint64_t)(x) << BLOCK_SHIFT((fs)))
#define agino_to_bytes(fs, x) \
    ((uint64_t)(x) << XFS_INFO((fs))->inode_shift)
#define agnumber_to_bytes(fs, x) \
    agblock_to_bytes(fs, (uint64_t)(x) * XFS_INFO((fs))->agblocks)
#define fsblock_to_bytes(fs,x)				\
    (agnumber_to_bytes(fs, XFS_FSB_TO_AGNO(fs, (x))) +	\
     agblock_to_bytes(fs, XFS_FSB_TO_AGBNO(fs, (x))))
#define ino_to_bytes(fs, x)			   \
    (agnumber_to_bytes(fs, XFS_INO_TO_AGNO(fs, (x))) +	\
     agino_to_bytes(fs, XFS_INO_TO_AGINO(fs, (x))))

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
    uint8_t		inopb_shift;
    uint8_t		agblk_shift;

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
    uint8_t		di_literal_area[1];
} __attribute__((packed)) xfs_dinode_t;

struct xfs_inode {
    xfs_agblock_t 	i_agblock;
    block_t		i_ino_blk;
    uint64_t		i_block_offset;
    uint64_t		i_offset;
    uint32_t		i_cur_extent;
};

typedef struct { uint8_t i[8]; } __attribute__((__packed__)) xfs_dir2_ino8_t;
typedef struct { uint8_t i[4]; } __attribute__((__packed__)) xfs_dir2_ino4_t;

typedef union {
    xfs_dir2_ino8_t i8;
    xfs_dir2_ino4_t i4;
} __attribute__((__packed__)) xfs_dir2_inou_t;

typedef struct { uint8_t i[2]; } __attribute__((__packed__)) xfs_dir2_sf_off_t;

typedef struct xfs_dir2_sf_hdr {
    uint8_t		count;		/* count of entries */
    uint8_t           	i8count;        /* count of 8-byte inode #s */
    xfs_dir2_inou_t     parent;         /* parent dir inode number */
} __attribute__((__packed__)) xfs_dir2_sf_hdr_t;

typedef struct xfs_dir2_sf_entry {
    uint8_t             namelen;        /* actual name length */
    xfs_dir2_sf_off_t   offset;         /* saved offset */
    uint8_t             name[1];        /* name, variable size */
    xfs_dir2_inou_t	inumber;	/* inode number, var. offset */
} __attribute__((__packed__)) xfs_dir2_sf_entry_t;

typedef struct xfs_dir2_sf {
    xfs_dir2_sf_hdr_t       hdr;            /* shortform header */
    xfs_dir2_sf_entry_t     list[1];        /* shortform entries */
} __attribute__((__packed__)) xfs_dir2_sf_t;

typedef enum {
    XFS_EXT_NORM,
    XFS_EXT_UNWRITTEN,
    XFS_EXT_DMAPI_OFFLINE,
    XFS_EXT_INVALID,
} xfs_exntst_t;

typedef struct xfs_bmbt_rec {
    uint64_t l0;
    uint64_t l1;
} __attribute__((__packed__)) xfs_bmbt_rec_t;

typedef xfs_ino_t	xfs_intino_t;

static inline xfs_intino_t xfs_dir2_sf_get_inumber(xfs_dir2_sf_t *sfp,
						   xfs_dir2_inou_t *from)
{
    return ((sfp)->hdr.i8count == 0 ? \
	    (xfs_intino_t)XFS_GET_DIR_INO4((from)->i4) : \
	    (xfs_intino_t)XFS_GET_DIR_INO8((from)->i8));
}

static inline bool xfs_is_valid_magicnum(const xfs_sb_t *sb)
{
    return sb->sb_magicnum == *(uint32_t *)XFS_SB_MAGIC;
}

static inline bool xfs_is_valid_agi(xfs_agi_t *agi)
{
    return agi->agi_magicnum == *(uint32_t *)XFS_AGI_MAGIC;
}

#endif /* XFS_H_ */
