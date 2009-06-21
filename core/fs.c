#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "fs.h"
#include "cache.h"


/* The this fs pointer */
struct fs_info *this_fs;
struct fs_info fs;
struct device dev;


void load_config(com32sys_t *regs)
{
    this_fs->fs_ops->load_config(regs);
}

void mangle_name(com32sys_t *regs)
{
    char *src = (char *)MK_PTR(regs->ds, regs->esi.w[0]);
    char *dst = (char *)MK_PTR(regs->es, regs->edi.w[0]);

    this_fs->fs_ops->mangle_name(dst, src);
}

/****
void unmangle_name(com32sys_t *regs)
{
    
}
****/

void getfssec(com32sys_t *regs)
{
    int sectors;
    int have_more;
    uint32_t bytes_read;
    char *buf;
    struct file file;
    
    sectors = regs->ecx.w[0];
    buf = (char *)MK_PTR(regs->es, regs->ebx.w[0]);
    file.open_file = MK_PTR(regs->ds, regs->esi.w[0]); 
    file.fs = this_fs;

    bytes_read = this_fs->fs_ops->getfssec(file.fs, buf, file.open_file, sectors, &have_more);
    
    /* if we reach the EOF, set the si to be NULL */
    if (!have_more)
        regs->esi.w[0] = 0;
    
    regs->ecx.l = bytes_read;
}


void searchdir(com32sys_t *regs)
{
    char *filename = (char *)MK_PTR(regs->ds, regs->edi.w[0]);
    struct file file;

#if 0    
    printf("filename: %s\n", filename);
#endif

    memset(&file, 0, sizeof file);
    file.fs = this_fs;
    
    this_fs->fs_ops->searchdir(filename, &file);
    regs->esi.w[0] = OFFS_WRT(file.open_file, 0);
    regs->eax.l = file.file_len;
    if (file.file_len)
        regs->eflags.l &= ~EFLAGS_ZF;
    else
        regs->eflags.l |= EFLAGS_ZF;
}

/*
 * well, I find that in the diskstart.inc, there's no room fow us to
 * get the edd check result, so we implement a new one here.
 */
uint8_t detect_edd(uint8_t device_num)
{
    com32sys_t iregs, oregs;
    
    /* Sending int 13h func 41h to query EBIOS information */
    memset(&iregs, 0, sizeof iregs);
    memset(&oregs, 0, sizeof oregs);
    
    /* Get EBIOS support */
    iregs.eax.w[0] = 0x4100;
    iregs.ebx.w[0] = 0x55aa;
    iregs.edx.b[0] = device_num;
    iregs.eflags.b[0] = 0x3; /* CF set */
    
    __intcall(0x13, &iregs, &oregs);
    
    /* Detecting EDD support */
    if (!(oregs.eflags.l & EFLAGS_CF) &&
        oregs.ebx.w[0] == 0xaa55 && (oregs.ecx.b[0] & 1))
        return 1;
    else
        return 0;
}

/* 
 * initialize the device structure 
 */
void device_init(struct device *dev, uint8_t device_num, 
                 bool cdrom, sector_t offset)
{
    dev->device_number = device_num;
    dev->part_start = offset;

    dev->type = detect_edd(device_num);
        
    if (!cdrom) {
        /* I can't use __lowmem here, 'cause it will cause the error:
           "auxseg/lowmem region collides with xfer_buf_seg"
 
           static __lowmem char cache_buf[65536];
        */
        dev->cache_data = core_cache_buf;
        dev->cache_size = sizeof core_cache_buf;
    } else 
        dev->cache_data = NULL;
}


/* debug function */
void dump_dev(struct device *dev)
{
    printf("device type:%s\n", dev->type ? "CHS" : "EDD");
    printf("cache_data: %p\n", dev->cache_data);
    printf("cache_head: %p\n", dev->cache_head);
    printf("cache_block_size: %d\n", dev->cache_block_size);
    printf("cache_entries: %d\n", dev->cache_entries);
    printf("cache_size: %d\n", dev->cache_size);
}

/*
 * it will do:
 *    initialize the device structure;
 *    set up the vfs fs structure;
 *    invoke the fs-specific init function;
 *    finally, initialize the cache
 *
 */
void fs_init(com32sys_t *regs)
{
    int blk_shift;
    struct fs_ops *ops = (struct fs_ops*)regs->eax.l;
    
    device_init(&dev, regs->edx.b[0], regs->edx.b[1], regs->ecx.l);
    
    /* set up the fs stucture */    
    fs.fs_name = ops->fs_name;
    fs.fs_ops = ops;
    fs.fs_dev = &dev;
    this_fs = &fs;

    /* invoke the fs-specific init code */
    blk_shift = fs.fs_ops->fs_init();    

    /* initialize the cache */
    if (dev.cache_data)
        cache_init(&dev, blk_shift);
}
