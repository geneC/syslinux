#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "fs.h"
#include "cache.h"


/* The this fs pointer */
struct fs_info *this_fs;
struct fs_info fs;


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
    char *filename = (char *)MK_PTR(regs->ds, regs->edi.w[0]);;
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
 * initialize the device structure 
 */
struct device * device_init(uint8_t devno, bool cdrom, sector_t part_start,
                            uint16_t bsHeads, uint16_t bsSecPerTrack)
{
    static struct device dev;

    dev.disk = disk_init(devno, cdrom, part_start, bsHeads, bsSecPerTrack);
        
    /* for now, isolinux doesn't use cache */
    if (!cdrom) {
        /*
         * FIX!! I can't use __lowmem here, 'cause it will cause the error:
         * "auxseg/lowmem region collides with xfer_buf_seg".
         *
         * static __lowmem char cache_buf[65536];
         */
        dev.cache_data = core_cache_buf;
        dev.cache_size = sizeof core_cache_buf;
    } else 
        dev.cache_data = NULL;

    return &dev;
}


/* debug function */
void dump_dev(struct device *dev)
{
    printf("device type:%s\n", dev->disk->type ? "EDD" : "CHS");
    printf("drive number: 0x%x\n", dev->disk->disk_number);
    printf("cache_data: %p\n", dev->cache_data);
    printf("cache_head: %p\n", dev->cache_head);
    printf("cache_block_size: %d\n", dev->cache_block_size);
    printf("cache_entries: %d\n", dev->cache_entries);
    printf("cache_size: %d\n", dev->cache_size);
}

/*
 * it will do:
 *    set up the vfs fs structure;
 *    initialize the device structure;
 *    invoke the fs-specific init function;
 *    finally, initialize the cache
 *
 */
void fs_init(com32sys_t *regs)
{
    int blk_shift;
    struct fs_ops *ops = (struct fs_ops*)regs->eax.l;
    
    /* set up the fs stucture */    
    fs.fs_name = ops->fs_name;
    fs.fs_ops = ops;
    fs.fs_dev = device_init(regs->edx.b[0], regs->edx.b[1], regs->ecx.l,
                            regs->esi.w[0], regs->edi.w[0]);
    this_fs = &fs;

    /* invoke the fs-specific init code */
    blk_shift = fs.fs_ops->fs_init(&fs);    

    /* initialize the cache */
    if (fs.fs_dev->cache_data)
        cache_init(fs.fs_dev, blk_shift);
    dump_dev(fs.fs_dev);
}
