#include <stdio.h>
#include <string.h>

#include "core.h"
#include "ext2_fs.h"



/**
 * init. the fs meta data, return the block size in eax
 */
void init_fs(com32sys_t *regs)
{

    extern uint16_t ClustByteShift,  ClustShift;
    extern uint32_t SecPerClust, ClustSize, ClustMask;
    extern uint32_t PtrsPerBlock1, PtrsPerBlock2;
    extern char SuperBlock[1024];

    struct ext2_super_block *sb;
    
    
    /* read the super block */
    read_sectors(SuperBlock, 2, 2);
    sb = (struct ext2_super_block *) SuperBlock;
    
    ClustByteShift = sb->s_log_block_size + 10;
    ClustSize = 1 << ClustByteShift;
    ClustShift = ClustByteShift - 9;
    
    //DescPerBlock  = blk_size / ( 1 << ext2_group_desc_lg2size);
    //InodePerBlock = blk_size / sb->s_inode_size;
        
    SecPerClust = ClustSize >> 9;
    ClustMask = SecPerClust - 1;
    
    PtrsPerBlock1 = 1 << ( ClustByteShift - 2 );
    PtrsPerBlock2 = 1 << ( (ClustByteShift - 2) * 2);
    //PtrsPerBlock3 = 1 << ( (ClustByteShift - 2) * 3);
    
    regs->eax.l = 9;
}
