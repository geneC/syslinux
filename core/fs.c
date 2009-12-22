#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "fs.h"
#include "cache.h"

/* The currently mounted filesystem */
struct fs_info *this_fs = NULL;
static struct fs_info fs;

/* Actual file structures (we don't have malloc yet...) */
struct file files[MAX_OPEN];

/*
 * Get an empty file structure
 */
static struct file *alloc_file(void)
{
    int i;
    struct file *file = files;

    for (i = 0; i < MAX_OPEN; i++) {
	if (!file->open_file)
	    return file;
	file++;
    }

    return NULL;
}

/*
 * Close and free a file structure
 */
static inline void free_file(struct file *file)
{
    memset(file, 0, sizeof *file);
}

void _close_file(struct file *file)
{
    if (file->open_file)
	file->fs->fs_ops->close_file(file);
    free_file(file);
}

/*
 * Convert between a 16-bit file handle and a file structure
 */
inline uint16_t file_to_handle(struct file *file)
{
    return file ? (file - files)+1 : 0;
}
inline struct file *handle_to_file(uint16_t handle)
{
    return handle ? &files[handle-1] : NULL;
}

void load_config(void)
{
    int err;
    err = this_fs->fs_ops->load_config();

#if 0
    printf("Loading config file %s\n", err ? "failed" : "successed");
#endif
}

void mangle_name(com32sys_t *regs)
{
    const char *src = MK_PTR(regs->ds, regs->esi.w[0]);
    char       *dst = MK_PTR(regs->es, regs->edi.w[0]);
    
    this_fs->fs_ops->mangle_name(dst, src);
}


void unmangle_name(com32sys_t *regs)
{
    const char *src = MK_PTR(regs->ds, regs->esi.w[0]);
    char       *dst = MK_PTR(regs->es, regs->edi.w[0]);
    
    dst = this_fs->fs_ops->unmangle_name(dst, src);

    /* Update the di register to point to the last null char */
    regs->edi.w[0] = OFFS_WRT(dst, regs->es);
}


void getfssec(com32sys_t *regs)
{
    int sectors;
    bool have_more;
    uint32_t bytes_read;
    char *buf;
    struct file *file;
    uint16_t handle;
    
    sectors = regs->ecx.w[0];

    handle = regs->esi.w[0];
    file = handle_to_file(handle);
    
    buf = MK_PTR(regs->es, regs->ebx.w[0]);
    bytes_read = file->fs->fs_ops->getfssec(file, buf, sectors, &have_more);
    
    /*
     * If we reach EOF, the filesystem driver will have already closed
     * the underlying file... this really should be cleaner.
     */
    if (!have_more) {
	_close_file(file);
        regs->esi.w[0] = 0;
    }
    
    regs->ecx.l = bytes_read;
}


void searchdir(com32sys_t *regs)
{
    char *filename = (char *)MK_PTR(regs->ds, regs->edi.w[0]);;
    struct file *file;
        
#if 0
    printf("filename: %s\n", filename);
#endif

    file = alloc_file();
    
    if (file) {
	file->fs = this_fs;
	file->fs->fs_ops->searchdir(filename, file);
	
	if (file->open_file) {
	    regs->esi.w[0]  = file_to_handle(file);
	    regs->eax.l     = file->file_len;
	    regs->eflags.l &= ~EFLAGS_ZF;
	    return;
	}
    }
    
    /* failure... */
    regs->esi.w[0]  = 0;
    regs->eax.l     = 0;
    regs->eflags.l |= EFLAGS_ZF;
}

void close_file(com32sys_t *regs)
{
    uint16_t handle = regs->esi.w[0];
    struct file *file;
    
    if (handle) {
	file = handle_to_file(handle);
	_close_file(file);
    }
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
    uint8_t disk_devno = regs->edx.b[0];
    uint8_t disk_cdrom = regs->edx.b[1];
    sector_t disk_offset = regs->ecx.l | ((sector_t)regs->ebx.l << 32);
    uint16_t disk_heads = regs->esi.w[0];
    uint16_t disk_sectors = regs->edi.w[0];
    int blk_shift = -1;
    struct device *dev = NULL;
    /* ops is a ptr list for several fs_ops */
    const struct fs_ops **ops = (const struct fs_ops **)regs->eax.l;
    
    while ((blk_shift < 0) && *ops) {
	/* set up the fs stucture */
	fs.fs_ops = *ops;

	/*
	 * This boldly assumes that we don't mix FS_NODEV filesystems
	 * with FS_DEV filesystems...
	 */
	if (fs.fs_ops->fs_flags & FS_NODEV) {
	    fs.fs_dev = NULL;
	} else {
	    if (!dev)
		dev = device_init(disk_devno, disk_cdrom, disk_offset,
				  disk_heads, disk_sectors);
	    fs.fs_dev = dev;
	}
	/* invoke the fs-specific init code */
	blk_shift = fs.fs_ops->fs_init(&fs);
	ops++;
    }
    if (blk_shift < 0) {
	printf("No valid file system found!\n");
	while (1)
		;
    }
    this_fs = &fs;
    /* initialize the cache */
    if (fs.fs_dev && fs.fs_dev->cache_data)
        cache_init(fs.fs_dev, blk_shift);
}
