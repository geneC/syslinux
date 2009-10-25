#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fs.h>
#include <cache.h>

/* The currently mounted filesystem */
struct fs_info *this_fs = NULL;
static struct fs_info fs;
struct inode *this_inode = NULL;

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
	if (!file->fs)
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
    if (file->fs)
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
    char *name = MK_PTR(regs->ds, regs->edi.w[0]);
    struct inode *inode;
    struct inode *parent;
    struct file *file;
    char part[256];
    char *p;
    int symlink_count = 6;
    
#if 0
    printf("filename: %s\n", name);
#endif

    if (!(file = alloc_file()))
	goto err;
    file->fs = this_fs;

    /* for now, we just applied the universal path_lookup to EXTLINUX */
    if (strcmp(this_fs->fs_ops->fs_name, "ext2") != 0) {
	file->fs->fs_ops->searchdir(name, file);
	
	if (file->u1.open_file) {
	    regs->esi.w[0]  = file_to_handle(file);
	    regs->eax.l     = file->u2.file_len;
	    regs->eflags.l &= ~EFLAGS_ZF;
	    return;
	}

	goto err;
    }



    if (*name == '/') {
	inode = this_fs->fs_ops->iget_root();
	while(*name == '/')
	    name++;
    } else {
	inode = this_inode;
    }
    parent = inode;
    
    while (*name) {
	p = part;
	while(*name && *name != '/')
	    *p++ = *name++;
	*p = '\0';
	inode = this_fs->fs_ops->iget(part, parent);
	if (!inode)
	    goto err;
	if (inode->mode == I_SYMLINK) {
	    if (!this_fs->fs_ops->follow_symlink || 
		--symlink_count == 0               ||      /* limit check */
		inode->size >= (uint32_t)inode->blksize)
		goto err;
	    name = this_fs->fs_ops->follow_symlink(inode, name);
	    free_inode(inode);
	    continue;
	}
	
	if (parent != this_inode)
	    free_inode(parent);
	parent = inode;
	if (! *name)
	    break;
	while(*name == '/')
	    name++;
    }
    
    file->u1.inode  = inode;
    file->u2.offset = 0;
    
    regs->esi.w[0]  = file_to_handle(file);
    regs->eax.l     = inode->size;
    regs->eflags.l &= ~EFLAGS_ZF;
    return;
    
err:
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
 *    initialize the memory management function;
 *    set up the vfs fs structure;
 *    initialize the device structure;
 *    invoke the fs-specific init function;
 *    initialize the cache if we need one;
 *    finally, get the current inode for relative path looking.
 *
 */
void fs_init(com32sys_t *regs)
{
    int blk_shift;
    const struct fs_ops *ops = (const struct fs_ops *)regs->eax.l;
    
    if (strcmp(ops->fs_name, "ext2") == 0)
	mem_init();

    /* set up the fs stucture */    
    fs.fs_ops = ops;
    this_fs = &fs;

    /* this is a total hack... */
    if (ops->fs_flags & FS_NODEV)
        fs.fs_dev = NULL;	/* Non-device filesystem */
    else 
        fs.fs_dev = device_init(regs->edx.b[0], regs->edx.b[1], regs->ecx.l,
				regs->esi.w[0], regs->edi.w[0]);

    /* invoke the fs-specific init code */
    blk_shift = fs.fs_ops->fs_init(&fs);
    if (blk_shift < 0) {
	printf("%s fs init error\n", fs.fs_ops->fs_name);
	for(;;) ; /* halt */
    }

    /* initialize the cache */
    if (fs.fs_dev && fs.fs_dev->cache_data)
        cache_init(fs.fs_dev, blk_shift);

    if (fs.fs_ops->iget_current)
	this_inode = fs.fs_ops->iget_current();
}
