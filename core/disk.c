#include <stdio.h>
#include <string.h>
#include "core.h"
#include "disk.h"

void read_sectors(char *buf, int sector_num, int sectors)
{
    com32sys_t regs;
        
    memset(&regs, 0, sizeof regs);
    regs.eax.l = sector_num;
    regs.ebp.l = sectors;
    regs.es = SEG(core_xfer_buf);
    regs.ebx.w[0] = OFFS(core_xfer_buf);
    call16(getlinsec, &regs, NULL);

    memcpy(buf, core_xfer_buf, sectors << SECTOR_SHIFT);
}


void getoneblk(char *buf, uint32_t block, int block_size)
{
    int sec_per_block = block_size >> SECTOR_SHIFT;
        
    read_sectors(buf, block * sec_per_block, sec_per_block);
}


