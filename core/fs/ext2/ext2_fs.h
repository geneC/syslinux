#ifndef __EXT2_FS_H
#define __EXT2_FS_H

#include <stdint.h>

#define	EXT2_SUPER_MAGIC	0xEF53

#define EXT2_GOOD_OLD_REV       0       // The good old (original) format
#define EXT2_DYNAMIC_REV        1       // V2 format w/ dynamic inode sizes
#define EXT2_GOOD_OLD_INODE_SIZE 128

// Special inode numbers
#define	EXT2_BAD_INO		 1	// Bad blocks inode
#define EXT2_ROOT_INO		 2	// Root inode
#define EXT2_BOOT_LOADER_INO	 5	// Boot loader inode
#define EXT2_UNDEL_DIR_INO	 6	// Undelete directory inode
#define EXT3_RESIZE_INO		 7	// Reserved group descriptors inode
#define EXT3_JOURNAL_INO	 8	// Journal inode

// We're readonly, so we only care about incompat features.
#define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT2_FEATURE_INCOMPAT_ANY		0xffffffff

#define EXT2_NDIR_BLOCKS	12
#define	EXT2_IND_BLOCK		EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK		(EXT2_IND_BLOCK+1)
#define	EXT2_TIND_BLOCK		(EXT2_DIND_BLOCK+1)
#define	EXT2_N_BLOCKS		(EXT2_TIND_BLOCK+1)


/* for EXT4 extent */
#define EXT4_EXT_MAGIC     0xf30a
#define EXT4_EXTENTS_FLAG  0x00080000

/*
 * File types and file modes
 */
#define S_IFDIR		0040000	        // Directory
#define S_IFCHR		0020000	        // Character device
#define S_IFBLK		0060000  	// Block device
#define S_IFREG		0100000	        // Regular file
#define S_IFIFO		0010000	        // FIFO
#define S_IFLNK		0120000 	// Symbolic link
#define S_IFSOCK	0140000       	// Socket

#define S_IFSHIFT	12

#define T_IFDIR		(S_IFDIR >> S_IFSHIFT)
#define T_IFCHR		(S_IFCHR >> S_IFSHIFT)
#define T_IFBLK		(S_IFBLK >> S_IFSHIFT)
#define T_IFREG		(S_IFREG >> S_IFSHIFT)
#define T_IFIFO		(S_IFIFO >> S_IFSHIFT)
#define T_IFLNK		(S_IFLNK >> S_IFSHIFT)
#define T_IFSOCK	(S_IFSOCK >> S_IFSHIFT)


#define ext2_group_desc_lg2size 5



/*
 * super block structure:
 * include/linux/ext2_fs.h 
 */
struct ext2_super_block {
    uint32_t s_inodes_count;	        /* Inodes count */
    uint32_t s_blocks_count;	        /* Blocks count */
    uint32_t s_r_blocks_count;	        /* Reserved blocks count */
    uint32_t s_free_blocks_count;	/* Free blocks count */
    uint32_t s_free_inodes_count;	/* Free inodes count */
    uint32_t s_first_data_block;	/* First Data Block */
    uint32_t s_log_block_size;	        /* Block size */
    uint32_t s_log_frag_size;	        /* Fragment size */
    uint32_t s_blocks_per_group;	/* # Blocks per group */
    uint32_t s_frags_per_group;	        /* # Fragments per group */
    uint32_t s_inodes_per_group;	/* # Inodes per group */
    uint32_t s_mtime;		        /* Mount time */
    uint32_t s_wtime;		        /* Write time */
    uint16_t s_mnt_count;		/* Mount count */
    int16_t  s_max_mnt_count;	        /* Maximal mount count */
    uint16_t s_magic;		        /* Magic signature */
    uint16_t s_state;		        /* File system state */
    uint16_t s_errors;		        /* Behaviour when detecting errors */
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;		/* time of last check */
    uint32_t s_checkinterval;	        /* max. time between checks */
    uint32_t s_creator_os;		/* OS */
    uint32_t s_rev_level;		/* Revision level */
    uint16_t s_def_resuid;		/* Default uid for reserved blocks */
    uint16_t s_def_resgid;		/* Default gid for reserved blocks */
    
    uint32_t s_first_ino; 		/* First non-reserved inode */
    uint16_t s_inode_size; 		/* size of inode structure */
    uint16_t s_block_group_nr; 	        /* block group # of this superblock */
    uint32_t s_feature_compat; 	        /* compatible feature set */
    uint32_t s_feature_incompat; 	/* incompatible feature set */
    uint32_t s_feature_ro_compat; 	/* readonly-compatible feature set */
    uint8_t  s_uuid[16];		/* 128-bit uuid for volume */
    char  s_volume_name[16]; 	        /* volume name */
    char  s_last_mounted[64]; 	        /* directory where last mounted */
    uint32_t s_algorithm_usage_bitmap;  /* For compression */
    uint8_t  s_prealloc_blocks;	        /* Nr of blocks to try to preallocate*/
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;
    uint32_t s_reserved[204];   	/* Padding to the end of the block */
};

/*******************************************************************************
#ifndef DEPEND
#if ext2_super_block_size != 1024
#error ext2_super_block definition bogus
#endif
#endif
*******************************************************************************/

/*  
 *  ext2 group desc structure:
 */
struct ext2_group_desc {
    uint32_t bg_block_bitmap;	/* Blocks bitmap block */
    uint32_t bg_inode_bitmap;	/* Inodes bitmap block */
    uint32_t bg_inode_table;	/* Inodes table block */
    uint16_t bg_free_blocks_count;	/* Free blocks count */
    uint16_t bg_free_inodes_count;	/* Free inodes count */
    uint16_t bg_used_dirs_count;	/* Directories count */
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
};

/*******************************************************************************
#ifndef DEPEND
#if ext2_group_desc_size != 32
#error ext2_group_desc definition bogus
#endif
#endif
*******************************************************************************/


/*
 * ext2 inode structure:
 */
struct ext2_inode {
    uint16_t i_mode;		/* File mode */
    uint16_t i_uid;		/* Owner Uid */
    uint32_t i_size;		/* 4: Size in bytes */
    uint32_t i_atime;		/* Access time */
    uint32_t i_ctime;		/* 12: Creation time */
    uint32_t i_mtime;		/* Modification time */
    uint32_t i_dtime;		/* 20: Deletion Time */
    uint16_t i_gid;		/* Group Id */
    uint16_t i_links_count;	/* 24: Links count */
    uint32_t i_blocks;		/* Blocks count */
    uint32_t i_flags;		/* 32: File flags */
    uint32_t l_i_reserved1;
    uint32_t i_block[EXT2_N_BLOCKS];	/* 40: Pointers to blocks */
    uint32_t i_version;		/* File version (for NFS) */
    uint32_t i_file_acl;	/* File ACL */
    uint32_t i_dir_acl;		/* Directory ACL */
    uint32_t i_faddr;		/* Fragment address */
    uint8_t  l_i_frag;	        /* Fragment number */
    uint8_t  l_i_fsize;	        /* Fragment size */
    uint16_t i_pad1;
    uint32_t l_i_reserved2[2];
};

/*******************************************************************************
#ifndef DEPEND
#if ext2_inode_size != 128
#error ext2_inode definition bogus
#endif
#endif
*******************************************************************************/


#define EXT2_NAME_LEN 255
struct ext2_dir_entry {
    unsigned int	d_inode;		/* Inode number */
    unsigned short	d_rec_len;		/* Directory entry length */
    unsigned char	d_name_len;		/* Name length */
    unsigned char	d_file_type;
    char	d_name[EXT2_NAME_LEN];	        /* File name */
};

/*******************************************************************************
#define EXT2_DIR_PAD	 4
#define EXT2_DIR_ROUND 	(EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT2_DIR_ROUND) & \
					 ~EXT2_DIR_ROUND)
*******************************************************************************/






/*
 * This is the extent on-disk structure.
 * It's used at the bottom of the tree.
 */
struct ext4_extent {
    uint32_t ee_block;	        /* first logical block extent covers */
    uint16_t ee_len;	        /* number of blocks covered by extent */
    uint16_t ee_start_hi;	/* high 16 bits of physical block */
    uint32_t ee_start_lo;	/* low 32 bits of physical block */
};

/*
 * This is index on-disk structure.
 * It's used at all the levels except the bottom.
 */
struct ext4_extent_idx {
    uint32_t ei_block;	        /* index covers logical blocks from 'block' */
    uint32_t ei_leaf_lo;	/* pointer to the physical block of the next *
				 * level. leaf or next index could be there */
    uint16_t ei_leaf_hi;	/* high 16 bits of physical block */
    uint16_t ei_unused;
};

/*
 * Each block (leaves and indexes), even inode-stored has header.
 */
struct ext4_extent_header {
    uint16_t eh_magic;	        /* probably will support different formats */
    uint16_t eh_entries;	/* number of valid entries */
    uint16_t eh_max;	        /* capacity of store in entries */
    uint16_t eh_depth;	        /* has tree real underlying blocks? */
    uint32_t eh_generation;	/* generation of the tree */
};



#define EXT4_FIRST_EXTENT(header) ( (struct ext4_extent *)(header + 1) )
#define EXT4_FIRST_INDEX(header)  ( (struct ext4_extent_idx *) (header + 1) )







/* function declartion */
/*******************************************************************************
extern struct open_file_t * ext2_read(char *);
extern int ext2_read(struct open_file_t *, char *, int);
*******************************************************************************/


#endif /* ext2_fs.h */
