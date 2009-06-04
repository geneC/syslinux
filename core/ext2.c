#include <stdio.h>
#include <string.h>
#include "cache.h"
#include "core.h"
#include "disk.h"
#include "ext2_fs.h"


#define MAX_OPEN_LG2     6
#define MAX_OPEN         (1 << MAX_OPEN_LG2)

/* 
 * File structure, This holds the information for each currently open file 
 */
struct open_file_t {
        uint32_t file_bytesleft;  /* Number of bytes left (0 = free) */
        uint32_t file_sector;     /* Next linear sector to read */
        uint32_t file_in_sec;     /* Sector where inode lives */
        uint16_t file_in_off;
        uint16_t file_mode;
};

extern char Files[MAX_OPEN * sizeof(struct open_file_t)];


extern char ThisInode[128];
struct ext2_inode *this_inode = ThisInode;

extern uint16_t ClustByteShift,  ClustShift;
extern uint32_t SecPerClust, ClustSize, ClustMask;
extern uint32_t PtrsPerBlock1, PtrsPerBlock2;
uint32_t PtrsPerBlock3;

int DescPerBlock, InodePerBlock;

struct ext2_super_block *sb;    



/**
 * allocate_file:
 * 
 * Allocate a file structure
 *
 * @return: if successful return the file pointer, or return NULL
 *
 */
struct open_file_t *allocate_file()
{
    struct open_file_t *file = (struct open_file_t *)Files;
    int i = 0;
        
    for (; i < MAX_OPEN; i ++) {
        if (file->file_bytesleft == 0) /* found it */
            return file;
        file ++;
    }
    
    return NULL; /* not found */
}


/**
 * get_group_desc:
 *
 * get the group's descriptor of group_num
 *
 * @param: group_num, the group number;
 * 
 * @return: the pointer of the group's descriptor
 *
 */ 
struct ext2_group_desc *get_group_desc(uint32_t group_num)
{
    block_t block_num;
    uint32_t offset;
    struct ext2_group_desc *desc;
    struct cache_struct *cs;

    block_num = group_num / DescPerBlock;
    offset = group_num % DescPerBlock;

    block_num += sb->s_first_data_block + 1;
    cs = get_cache_block(block_num);

    desc = (struct ext2_group_desc *)cs->data + offset;

    return desc;
}


/**
 * read_inode:
 *
 * read the right inode structure to _dst_.
 *
 * @param: inode_offset, the inode offset within a group;
 * @prarm: dst, wher we will store the inode structure;
 * @param: desc, the pointer to the group's descriptor
 * @param: block, a pointer used for retruning the blk number for file structure
 * @param: offset, same as block
 *
 */
void read_inode(uint32_t inode_offset, 
                struct ext2_inode *dst, struct ext2_group_desc *desc,
                block_t *block, uint32_t *offset)
{
    struct cache_struct *cs;
    struct ext2_inode *inode;
    
    *block  = inode_offset / InodePerBlock + desc->bg_inode_table;
    *offset = inode_offset % InodePerBlock;
    
    cs = get_cache_block(*block);
    
    /* well, in EXT4, the inode structure usually be 256 */
    inode = (struct ext2_inode *)(cs->data + (*offset * (sb->s_inode_size)));
    memcpy(dst, inode, EXT2_GOOD_OLD_INODE_SIZE);
    
    /* for file structure */
    *offset = (inode_offset * sb->s_inode_size) % ClustSize;
}


/**
 * open_inode:
 *
 * open a file indicated by an inode number in INR
 *
 * @param : regs, regs->eax stores the inode number
 * @return: a open_file_t structure pointer, stores in regs->esi
 *          file length in bytes, stores in regs->eax
 *          the first 128 bytes of the inode, stores in ThisInode
 *
 */
void open_inode(com32sys_t *regs)
{
    uint32_t inr = regs->eax.l;
    uint32_t file_len;

    struct open_file_t *file;
    struct ext2_group_desc *desc;
        
    uint32_t inode_group, inode_offset;
    block_t block_num;
    uint32_t block_off;    
    
    file = allocate_file();
    if (!file)
        goto err;
    
    file->file_sector = 0;
    
    inr --;
    inode_group  = inr / sb->s_inodes_per_group;
    
    /* get the group desc */
    desc = get_group_desc(inode_group);
    
    inode_offset = inr % sb->s_inodes_per_group;
    read_inode(inode_offset, this_inode, desc, &block_num, &block_off);
    
    /* Finally, we need to convet it to sector for now */
    file->file_in_sec = (block_num<<ClustShift) + (block_off>>SECTOR_SHIFT);
    file->file_in_off = block_off & (SECTOR_SIZE - 1);
    file->file_mode = this_inode->i_mode;
    file_len = file->file_bytesleft = this_inode->i_size;
    
    if (file_len == 0)
        goto err;
    
    regs->esi.w[0] = file;
    regs->eax.l = file_len;
    return;

 err:
    regs->eax.l = 0;
}



struct ext4_extent_header * 
ext4_find_leaf (struct ext4_extent_header *eh, block_t block)
{
    struct ext4_extent_idx *index;
    struct cache_struct *cs;
    uint64_t blk;
    int i;
    
    while (1) {        
        if (eh->eh_magic != EXT4_EXT_MAGIC)
            return NULL;
        
        /* got it */
        if (eh->eh_depth == 0)
            return eh;
        
        index = EXT4_FIRST_INDEX(eh);        
        for ( i = 0; i < eh->eh_entries; i++ ) {
            if ( block < index[i].ei_block )
                break;
        }
        if ( --i < 0 )
            return NULL;
        
        blk = index[i].ei_leaf_hi;
        blk = (blk << 32) + index[i].ei_leaf_lo;
        
        /* read the blk to memeory */
        cs = get_cache_block(blk);
        eh = (struct ext4_extent_header *)(cs->data);
    }
}

/* handle the ext4 extents to get the phsical block number */
uint64_t linsector_extent(block_t block, struct ext2_inode *inode)
{
    struct ext4_extent_header *leaf;
    struct ext4_extent *ext;
    int i;
    uint64_t start;
    
    leaf = ext4_find_leaf((struct ext4_extent_header*)inode->i_block,block);
    if (! leaf) {
        printf("ERROR, extent leaf not found\n");
        return 0;
    }
    
    ext = EXT4_FIRST_EXTENT(leaf);
    for ( i = 0; i < leaf->eh_entries; i++ ) {
        if ( block < ext[i].ee_block)
            break;
    }
    if ( --i < 0 ) {
        printf("ERROR, not find the right block\n");
        return 0;
    }
    
    
    /* got it */
    block -= ext[i].ee_block;
    if ( block >= ext[i].ee_len)
        return 0;
    
    start = ext[i].ee_start_hi;
    start = (start << 32) + ext[i].ee_start_lo;
    
    return start + block;
}


/**
 * linsector_direct:
 * 
 * @param: block, the block index
 * @param: inode, the inode structure
 *
 * @return: the physic block number
 */
block_t linsector_direct(block_t block, struct ext2_inode *inode)
{
    struct cache_struct *cs;
    
    /* direct blocks */
    if (block < EXT2_NDIR_BLOCKS) 
        return inode->i_block[block];
    

    /* indirect blocks */
    block -= EXT2_NDIR_BLOCKS;
    if (block < PtrsPerBlock1) {
        block_t ind_block = inode->i_block[EXT2_IND_BLOCK];
        cs = get_cache_block(ind_block);
        
        return ((block_t *)cs->data)[block];
    }
    
    /* double indirect blocks */
    block -= PtrsPerBlock1;
    if (block < PtrsPerBlock2) {
        block_t dou_block = inode->i_block[EXT2_DIND_BLOCK];
        cs = get_cache_block(dou_block);
        
        dou_block = ((block_t *)cs->data)[block / PtrsPerBlock1];
        cs = get_cache_block(dou_block);
        
        return ((block_t*)cs->data)[block % PtrsPerBlock1];
    }
    
    /* triple indirect block */
    block -= PtrsPerBlock2;
    if (block < PtrsPerBlock3) {
        block_t tri_block = inode->i_block[EXT2_TIND_BLOCK];
        cs = get_cache_block(tri_block);
        
        tri_block = ((block_t *)cs->data)[block / PtrsPerBlock2];
        cs = get_cache_block(tri_block);
        
        tri_block = ((block_t *)cs->data)[block % PtrsPerBlock2];
        cs = get_cache_block(tri_block);

        return ((uint32_t*)cs->data)[block % PtrsPerBlock1];
    }
    
    /* File too big, can not handle */
    printf("ERROR, file too big\n");
    return 0;
}


/**
 * linsector:
 *
 * Convert a linear sector index in a file to linear sector number
 *
 * well, alought this function converts a linear sector number to 
 * physic sector number, it uses block cache in the implemention.
 * 
 * @param: lin_sector, the lineral sector index
 * 
 * @return: physic sector number
 */
void linsector(com32sys_t *regs)
{
    sector_t lin_sector = regs->eax.l;
    block_t block = lin_sector >> ClustShift;
    struct ext2_inode *inode;

    /* well, this is what I think the variable this_inode used for */
    inode = this_inode;

    if (inode->i_flags & EXT4_EXTENTS_FLAG)
        block = linsector_extent(block, inode);
    else
        block = (uint32_t)linsector_direct(block, inode);
    
    if (!block) {
        printf("ERROR: something error happend at linsector..\n");
        regs->eax.l = 0;
        return;
    }
    
    /* finally convert it to sector */
    regs->eax.l = ((block << ClustShift) + (lin_sector & ClustMask));
}



/**
 * init. the fs meta data, return the block size in eax
 */
void init_fs(com32sys_t *regs)
{
    extern char SuperBlock[1024];
    
    /* read the super block */
    read_sectors(SuperBlock, 2, 2);
    sb = (struct ext2_super_block *) SuperBlock;
    
    ClustByteShift = sb->s_log_block_size + 10;
    ClustSize = 1 << ClustByteShift;
    ClustShift = ClustByteShift - SECTOR_SHIFT;
    
    DescPerBlock  = ClustSize >> ext2_group_desc_lg2size;
    InodePerBlock = ClustSize / sb->s_inode_size;
        
    SecPerClust = ClustSize >> SECTOR_SHIFT;
    ClustMask = SecPerClust - 1;
    
    PtrsPerBlock1 = 1 << ( ClustByteShift - 2 );
    PtrsPerBlock2 = 1 << ( (ClustByteShift - 2) * 2);
    PtrsPerBlock3 = 1 << ( (ClustByteShift - 2) * 3);
    
    regs->eax.l = ClustByteShift;
}
