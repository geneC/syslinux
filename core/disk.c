#include <stdio.h>
#include <string.h>
#include "core.h"
#include "disk.h"

void read_sectors(char *buf, sector_t sector_num, int sectors)
{
    com32sys_t regs;
    static __lowmem char low_buf[65536]; 
    /* for safe, we use buf + (sectors << SECTOR_SHIFT) here */
    int high_addr = (buf + (sectors << SECTOR_SHIFT)) > (char *)0x100000;
        
    memset(&regs, 0, sizeof regs);
    regs.eax.l = sector_num;
    regs.ebp.l = sectors;
    
    if (high_addr) {
        regs.es = SEG(low_buf);
        regs.ebx.w[0] = OFFS(low_buf);
    } else {
        regs.es = SEG(buf);
        regs.ebx.w[0] = OFFS(buf);
    }

    call16(getlinsec, &regs, NULL);

    if (high_addr)
        memcpy(buf, core_xfer_buf, sectors << SECTOR_SHIFT);
}


void getoneblk(char *buf, block_t block, int block_size)
{
    int sec_per_block = block_size >> SECTOR_SHIFT;
        
    read_sectors(buf, block * sec_per_block, sec_per_block);
}


