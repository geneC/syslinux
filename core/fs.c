#include <stdio.h>
#include <string.h>
#include "fs.h"
#include "cache.h"


/* The this pointer */
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

    bytes_read = this_fs->fs_ops->getfssec(buf, file.open_file, sectors, &have_more);
    
    /* if we reach the EOF, set the si to be NULL */
    if (!have_more)
        regs->esi.w[0] = 0;
    
    regs->ecx.l = bytes_read;
}


void searchdir(com32sys_t *regs)
{
    char *filename = (char *)MK_PTR(regs->ds, regs->edi.w[0]);
    struct file file;
    
    memset(&file, 0, sizeof file);
    file.fs = this_fs;
    
    this_fs->fs_ops->searchdir(filename, &file);
    regs->esi.w[0] = OFFS_WRT(file.open_file, 0);
    regs->eax.l = file.file_len;
}

/* 
 * initialize the device structure 
 */
void device_init(struct device *dev, uint8_t drive_num, 
                 uint8_t type, sector_t offset)
{
    dev->device_number = drive_num;
    dev->part_start = offset;
    dev->type = type;
    
    /* 
     * check if we use cache or not, for now I just know ISO fs 
     * does not use the cache, and I hope the USE_CACHE can detect
     * it correctly.
     *
     */
    if ( USE_CACHE(dev->device_number) ) {
        static __lowmem char cache_buf[65536];
        dev->cache_data = cache_buf;
    } else 
        dev->cache_data = NULL;

    /* I just considered the floppy and disk now */
    dev->read_sectors = read_sectors;
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
