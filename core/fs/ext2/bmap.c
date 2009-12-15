/*
 * The logical block -> physical block routine.
 *
 * Copyright (C) 2009 Liu Aleaxander -- All rights reserved. This file
 * may be redistributed under the terms of the GNU Public License.
 */

#include <stdio.h>
#include <fs.h>
#include <disk.h>
#include <cache.h>
#include "ext2_fs.h"


static struct ext4_extent_header * 
ext4_find_leaf(struct fs_info *fs, struct ext4_extent_header *eh, block_t block)
{
    struct ext4_extent_idx *index;
    struct cache_struct *cs;
    block_t blk;
    int i;
    
    while (1) {                              
	if (eh->eh_magic != EXT4_EXT_MAGIC)
	    return NULL;
	if (eh->eh_depth == 0) 
	    return eh;
	
	index = EXT4_FIRST_INDEX(eh);  
	for (i = 0; i < (int)eh->eh_entries; i++) {
	    if (block < index[i].ei_block)
		break;
	}
	if (--i < 0)
	    return NULL;
	
	blk = index[i].ei_leaf_hi;
	blk = (blk << 32) + index[i].ei_leaf_lo;                
	cs = get_cache_block(fs->fs_dev, blk);
	eh = (struct ext4_extent_header *)cs->data;
    }
}

/* handle the ext4 extents to get the phsical block number */
static uint64_t bmap_extent(struct fs_info *fs, 
			    struct inode *inode, 
			    uint32_t block)
{
    struct ext4_extent_header *leaf;
    struct ext4_extent *ext;
    int i;
    block_t start;
    
    leaf = ext4_find_leaf(fs, (struct ext4_extent_header *)inode->data, block);
    if (!leaf) {
	printf("ERROR, extent leaf not found\n");
	return 0;
    }

    ext = EXT4_FIRST_EXTENT(leaf);        
    for (i = 0; i < leaf->eh_entries; i++) {
	if (block < ext[i].ee_block)
	    break;
    }
    if (--i < 0) {
	printf("ERROR, not find the right block\n");
	return 0;
    }
    
    /* got it */
    block -= ext[i].ee_block;
    if (block >= ext[i].ee_len)
	return 0;
    start = ext[i].ee_start_hi;
    start = (start << 32) + ext[i].ee_start_lo;
    
    return start + block;
}


/* 
 * handle the traditional block map, like indirect, double indirect 
 * and triple indirect 
 */
static unsigned int bmap_traditional(struct fs_info *fs, 
				     struct inode *inode, 
				     uint32_t block)
{
    int addr_per_block = BLOCK_SIZE(fs) >> 2;
    uint32_t direct_blocks = EXT2_NDIR_BLOCKS,
	indirect_blocks = addr_per_block,
	double_blocks = addr_per_block * addr_per_block,
	triple_blocks = double_blocks * addr_per_block;
    struct cache_struct *cs;
    
    /* direct blocks */
    if (block < direct_blocks)
	return inode->data[block];
    
    /* indirect blocks */
    block -= direct_blocks;
    if (block < indirect_blocks) {
	block_t ind_block = inode->data[EXT2_IND_BLOCK];
        
	if (!ind_block)
	    return 0;
	cs = get_cache_block(fs->fs_dev, ind_block);
        
	return ((uint32_t *)cs->data)[block];
    }
    
    
    /* double indirect blocks */
    block -= indirect_blocks;
    if (block < double_blocks) {
	block_t dou_block = inode->data[EXT2_DIND_BLOCK];
        
	if (!dou_block)
	    return 0;                
	cs = get_cache_block(fs->fs_dev, dou_block);
	
	dou_block = ((uint32_t *)cs->data)[block / indirect_blocks];
	if (!dou_block)
	    return 0;
	cs = get_cache_block(fs->fs_dev, dou_block);
	
	return ((uint32_t *)cs->data)[block % addr_per_block];
    }
    

    /* triple indirect block */
    block -= double_blocks;
    if (block < triple_blocks) {
	block_t tri_block = inode->data[EXT2_TIND_BLOCK];
        
	if (!tri_block)
	    return 0;
	cs = get_cache_block(fs->fs_dev, tri_block);
	
	tri_block = ((uint32_t *)cs->data)[block / double_blocks];
	if (!tri_block)
	    return 0;
	cs = get_cache_block(fs->fs_dev, tri_block);
	
	tri_block = (block / addr_per_block) % addr_per_block;
	tri_block = ((uint32_t *)cs->data)[tri_block];
	if (!tri_block)
	    return 0;
	cs = get_cache_block(fs->fs_dev, tri_block);
        
	return ((uint32_t *)cs->data)[block % addr_per_block];
    }
    
    
    /* File too big, can not handle */
    printf("ERROR, file too big\n");
    return 0;
}


/**
 * Map the logical block to physic block where the file data stores.
 * In EXT4, there are two ways to handle the map process, extents and indirect.
 * EXT4 uses a inode flag to mark extent file and indirect block file.
 *
 * @fs:    the fs_info structure.
 * @inode: the inode structure.
 * @block: the logical blcok needed to be maped.
 * @retrun: the physic block number.
 *
 */
block_t bmap(struct fs_info *fs, struct inode * inode, int block)
{
    block_t ret;
    
    if (block < 0)
	return 0;
    
    if (inode->flags & EXT4_EXTENTS_FLAG)
	ret = bmap_extent(fs, inode, block);
    else
	ret = bmap_traditional(fs, inode, block);
    
    if (!ret) {
        printf("ERROR: something error happend at linsector..\n");
        return 0;
    }

    return ret;
}
