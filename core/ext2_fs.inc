; -----------------------------------------------------------------------
;
;   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
;   USA; either version 2 of the License, or (at your option) any later
;   version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; ext2_fs.inc
;
; NASM include file for ext2fs data structures
;

%define	EXT2_SUPER_MAGIC	0xEF53

%define EXT2_GOOD_OLD_REV       0       ; The good old (original) format
%define EXT2_DYNAMIC_REV        1       ; V2 format w/ dynamic inode sizes
%define EXT2_GOOD_OLD_INODE_SIZE 128

; Special inode numbers
%define	EXT2_BAD_INO		 1	; Bad blocks inode
%define EXT2_ROOT_INO		 2	; Root inode
%define EXT2_BOOT_LOADER_INO	 5	; Boot loader inode
%define EXT2_UNDEL_DIR_INO	 6	; Undelete directory inode
%define EXT3_RESIZE_INO		 7	; Reserved group descriptors inode
%define EXT3_JOURNAL_INO	 8	; Journal inode

; We're readonly, so we only care about incompat features.
%define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
%define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
%define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
%define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
%define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
%define EXT2_FEATURE_INCOMPAT_ANY		0xffffffff

%define EXT2_NDIR_BLOCKS	12
%define	EXT2_IND_BLOCK		EXT2_NDIR_BLOCKS
%define EXT2_DIND_BLOCK		(EXT2_IND_BLOCK+1)
%define	EXT2_TIND_BLOCK		(EXT2_DIND_BLOCK+1)
%define	EXT2_N_BLOCKS		(EXT2_TIND_BLOCK+1)

;
; File types and file modes
;
%define S_IFDIR		0040000o	; Directory
%define S_IFCHR		0020000o	; Character device
%define S_IFBLK		0060000o	; Block device
%define S_IFREG		0100000o	; Regular file
%define S_IFIFO		0010000o	; FIFO
%define S_IFLNK		0120000o	; Symbolic link
%define S_IFSOCK	0140000o	; Socket

%define S_IFSHIFT	12

%define T_IFDIR		(S_IFDIR >> S_IFSHIFT)
%define T_IFCHR		(S_IFCHR >> S_IFSHIFT)
%define T_IFBLK		(S_IFBLK >> S_IFSHIFT)
%define T_IFREG		(S_IFREG >> S_IFSHIFT)
%define T_IFIFO		(S_IFIFO >> S_IFSHIFT)
%define T_IFLNK		(S_IFLNK >> S_IFSHIFT)
%define T_IFSOCK	(S_IFSOCK >> S_IFSHIFT)

;
; Structure definition for the ext2 superblock
;
			struc ext2_super_block
s_inodes_count          resd 1                  ; Inodes count
s_blocks_count          resd 1                  ; Blocks count
s_r_blocks_count        resd 1                  ; Reserved blocks count
s_free_blocks_count     resd 1                  ; Free blocks count
s_free_inodes_count     resd 1                  ; Free inodes count
s_first_data_block      resd 1                  ; First Data Block
s_log_block_size        resd 1                  ; Block size
s_log_frag_size         resd 1                  ; Fragment size
s_blocks_per_group      resd 1                  ; # Blocks per group
s_frags_per_group       resd 1                  ; # Fragments per group
s_inodes_per_group      resd 1                  ; # Inodes per group
s_mtime                 resd 1                  ; Mount time
s_wtime                 resd 1                  ; Write time
s_mnt_count             resw 1                  ; Mount count
s_max_mnt_count         resw 1                  ; Maximal mount count
s_magic                 resw 1                  ; Magic signature
s_state                 resw 1                  ; File system state
s_errors                resw 1                  ; Behaviour when detecting errors
s_minor_rev_level       resw 1                  ; minor revision level
s_lastcheck             resd 1                  ; time of last check
s_checkinterval         resd 1                  ; max. time between checks
s_creator_os            resd 1                  ; OS
s_rev_level             resd 1                  ; Revision level
s_def_resuid            resw 1                  ; Default uid for reserved blocks
s_def_resgid            resw 1                  ; Default gid for reserved blocks
s_first_ino             resd 1                  ; First non-reserved inode
s_inode_size            resw 1                  ; size of inode structure
s_block_group_nr        resw 1                  ; block group # of this superblock
s_feature_compat        resd 1                  ; compatible feature set
s_feature_incompat      resd 1                  ; incompatible feature set
s_feature_ro_compat     resd 1                  ; readonly-compatible feature set
s_uuid                  resb 16                 ; 128-bit uuid for volume
s_volume_name           resb 16                 ; volume name
s_last_mounted          resb 64                 ; directory where last mounted
s_algorithm_usage_bitmap resd 1                 ; For compression
s_prealloc_blocks       resb 1                  ; Nr of blocks to try to preallocate
s_prealloc_dir_blocks   resb 1                  ; Nr to preallocate for dirs
s_padding1              resw 1
s_reserved              resd 204                ; Padding to the end of the block
			endstruc

%ifndef DEPEND
%if ext2_super_block_size != 1024
%error "ext2_super_block definition bogus"
%endif
%endif

;
; Structure definition for the ext2 inode
;
			struc ext2_inode
i_mode                  resw 1                  ; File mode
i_uid                   resw 1                  ; Owner Uid
i_size                  resd 1                  ; Size in bytes
i_atime                 resd 1                  ; Access time
i_ctime                 resd 1                  ; Creation time
i_mtime                 resd 1                  ; Modification time
i_dtime                 resd 1                  ; Deletion Time
i_gid                   resw 1                  ; Group Id
i_links_count           resw 1                  ; Links count
i_blocks                resd 1                  ; Blocks count
i_flags                 resd 1                  ; File flags
l_i_reserved1           resd 1
i_block			resd EXT2_N_BLOCKS	; Pointer to blocks
i_version               resd 1                  ; File version (for NFS)
i_file_acl              resd 1                  ; File ACL
i_dir_acl               resd 1                  ; Directory ACL
i_faddr                 resd 1                  ; Fragment address
l_i_frag                resb 1                  ; Fragment number
l_i_fsize               resb 1                  ; Fragment size
i_pad1                  resw 1
l_i_reserved2           resd 2
			endstruc

%ifndef DEPEND
%if ext2_inode_size != 128
%error "ext2_inode definition bogus"
%endif
%endif

;
; Structure definition for ext2 block group descriptor
;
			struc ext2_group_desc
bg_block_bitmap		resd 1			; Block bitmap block
bg_inode_bitmap		resd 1			; Inode bitmap block
bg_inode_table		resd 1			; Inode table block
bg_free_blocks_count	resw 1			; Free blocks count
bg_free_inodes_count	resw 1			; Free inodes count
bg_used_dirs_count	resw 1			; Used inodes count
bg_pad			resw 1
bg_reserved		resd 3
			endstruc

%ifndef DEPEND
%if ext2_group_desc_size != 32
%error "ext2_group_desc definition bogus"
%endif
%endif

%define ext2_group_desc_lg2size 5

;
; Structure definition for ext2 directory entry
;
			struc ext2_dir_entry
d_inode			resd 1			; Inode number
d_rec_len		resw 1			; Directory entry length
d_name_len		resb 1			; Name length
d_file_type		resb 1			; File type
d_name			equ $
			endstruc
