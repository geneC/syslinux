#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <dprintf.h>
#include "fs.h"
#include "cache.h"

/* The currently mounted filesystem */
struct fs_info *this_fs = NULL;		/* Root filesystem */

/* Actual file structures (we don't have malloc yet...) */
struct file files[MAX_OPEN];

/* Symlink hard limits */
#define MAX_SYMLINK_CNT	20
#define MAX_SYMLINK_BUF 4096

/*
 * Get a new inode structure
 */
struct inode *alloc_inode(struct fs_info *fs, uint32_t ino, size_t data)
{
    struct inode *inode = zalloc(sizeof(struct inode) + data);
    if (inode) {
	inode->fs = fs;
	inode->ino = ino;
	inode->refcnt = 1;
    }
    return inode;
}

/*
 * Free a refcounted inode
 */
void put_inode(struct inode *inode)
{
    while (inode && --inode->refcnt == 0) {
	struct inode *dead = inode;
	inode = inode->parent;
	if (dead->name)
	    free((char *)dead->name);
	free(dead);
    }
}

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

void pm_load_config(com32sys_t *regs)
{
    int err;

    err = this_fs->fs_ops->load_config();

    if (err)
	printf("ERROR: No configuration file found\n");

    set_flags(regs, err ? EFLAGS_ZF : 0);
}

void pm_mangle_name(com32sys_t *regs)
{
    const char *src = MK_PTR(regs->ds, regs->esi.w[0]);
    char       *dst = MK_PTR(regs->es, regs->edi.w[0]);

    mangle_name(dst, src);
}

void mangle_name(char *dst, const char *src)
{
    this_fs->fs_ops->mangle_name(dst, src);
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

void getfsbytes(com32sys_t *regs)
{
    int sectors;
    bool have_more;
    uint32_t bytes_read;
    char *buf;
    struct file *file;
    uint16_t handle;

    handle = regs->esi.w[0];
    file = handle_to_file(handle);

    sectors = regs->ecx.w[0] >> SECTOR_SHIFT(file->fs);

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

size_t pmapi_read_file(uint16_t *handle, void *buf, size_t sectors)
{
    bool have_more;
    size_t bytes_read;
    struct file *file;

    file = handle_to_file(*handle);
    bytes_read = file->fs->fs_ops->getfssec(file, buf, sectors, &have_more);

    /*
     * If we reach EOF, the filesystem driver will have already closed
     * the underlying file... this really should be cleaner.
     */
    if (!have_more) {
	_close_file(file);
	*handle = 0;
    }

    return bytes_read;
}

void pm_searchdir(com32sys_t *regs)
{
    char *name = MK_PTR(regs->ds, regs->edi.w[0]);
    int rv;

    rv = searchdir(name);
    if (rv < 0) {
	regs->esi.w[0]  = 0;
	regs->eax.l     = 0;
	regs->eflags.l |= EFLAGS_ZF;
    } else {
	regs->esi.w[0]  = rv;
	regs->eax.l     = handle_to_file(rv)->inode->size;
	regs->eflags.l &= ~EFLAGS_ZF;
    }
}

int searchdir(const char *name)
{
    static char root_name[] = "/";
    struct file *file;
    char *path, *inode_name, *next_inode_name;
    struct inode *tmp, *inode = NULL;
    int symlink_count = MAX_SYMLINK_CNT;

    dprintf("searchdir: %s  root: %p  cwd: %p\n",
	    name, this_fs->root, this_fs->cwd);

    if (!(file = alloc_file()))
	goto err_no_close;
    file->fs = this_fs;

    /* if we have ->searchdir method, call it */
    if (file->fs->fs_ops->searchdir) {
	file->fs->fs_ops->searchdir(name, file);

	if (file->inode)
	    return file_to_handle(file);
	else
	    goto err;
    }

    /* else, try the generic-path-lookup method */

    /* Copy the path */
    path = strdup(name);
    if (!path) {
	dprintf("searchdir: Couldn't copy path\n");
	goto err_path;
    }

    /* Work with the current directory, by default */
    inode = get_inode(this_fs->cwd);
    if (!inode) {
	dprintf("searchdir: Couldn't use current directory\n");
	goto err_curdir;
    }

    for (inode_name = path; inode_name; inode_name = next_inode_name) {
	/* Root directory? */
	if (inode_name[0] == '/') {
	    next_inode_name = inode_name + 1;
	    inode_name = root_name;
	} else {
	    /* Find the next inode name */
	    next_inode_name = strchr(inode_name + 1, '/');
	    if (next_inode_name) {
		/* Terminate the current inode name and point to next */
		*next_inode_name++ = '\0';
	    }
	}
	if (next_inode_name) {
	    /* Advance beyond redundant slashes */
	    while (*next_inode_name == '/')
		next_inode_name++;

	    /* Check if we're at the end */
	    if (*next_inode_name == '\0')
		next_inode_name = NULL;
	}
	dprintf("searchdir: inode_name: %s\n", inode_name);
	if (next_inode_name)
	    dprintf("searchdir: Remaining: %s\n", next_inode_name);

	/* Root directory? */
	if (inode_name[0] == '/') {
	    /* Release any chain that's already been established */
	    put_inode(inode);
	    inode = get_inode(this_fs->root);
	    continue;
	}

	/* Current directory? */
	if (!strncmp(inode_name, ".", sizeof "."))
	    continue;

	/* Parent directory? */
	if (!strncmp(inode_name, "..", sizeof "..")) {
	    /* If there is no parent, just ignore it */
	    if (!inode->parent)
		continue;

	    /* Add a reference to the parent so we can release the child */
	    tmp = get_inode(inode->parent);

	    /* Releasing the child will drop the parent back down to 1 */
	    put_inode(inode);

	    inode = tmp;
	    continue;
	}

	/* Anything else */
	tmp = inode;
	inode = this_fs->fs_ops->iget(inode_name, inode);
	if (!inode) {
	    /* Failure.  Release the chain */
	    put_inode(tmp);
	    break;
	}

	/* Sanity-check */
	if (inode->parent && inode->parent != tmp) {
	    dprintf("searchdir: iget returned a different parent\n");
	    put_inode(inode);
	    inode = NULL;
	    put_inode(tmp);
	    break;
	}
	inode->parent = tmp;
	inode->name = strdup(inode_name);
	dprintf("searchdir: path component: %s\n", inode->name);

	/* Symlink handling */
	if (inode->mode == DT_LNK) {
	    char *new_path;
	    int new_len, copied;

	    /* target path + NUL */
	    new_len = inode->size + 1;

	    if (next_inode_name) {
		/* target path + slash + remaining + NUL */
		new_len += strlen(next_inode_name) + 1;
	    }

	    if (!this_fs->fs_ops->readlink ||
		/* limit checks */
		--symlink_count == 0 ||
		new_len > MAX_SYMLINK_BUF)
		goto err_new_len;

	    new_path = malloc(new_len);
	    if (!new_path)
		goto err_new_path;

	    copied = this_fs->fs_ops->readlink(inode, new_path);
	    if (copied <= 0)
		goto err_copied;
	    new_path[copied] = '\0';
	    dprintf("searchdir: Symlink: %s\n", new_path);

	    if (next_inode_name) {
		new_path[copied] = '/';
		strcpy(new_path + copied + 1, next_inode_name);
		dprintf("searchdir: New path: %s\n", new_path);
	    }

	    free(path);
	    path = next_inode_name = new_path;

            /* Add a reference to the parent so we can release the child */
            tmp = get_inode(inode->parent);

            /* Releasing the child will drop the parent back down to 1 */
            put_inode(inode);

            inode = tmp;
	    continue;
err_copied:
	    free(new_path);
err_new_path:
err_new_len:
	    put_inode(inode);
	    inode = NULL;
	    break;
	}

	/* If there's more to process, this should be a directory */
	if (next_inode_name && inode->mode != DT_DIR) {
	    dprintf("searchdir: Expected a directory\n");
	    put_inode(inode);
	    inode = NULL;
	    break;
	}
    }
err_curdir:
    free(path);
err_path:
    if (!inode) {
	dprintf("searchdir: Not found\n");
	goto err;
    }

    file->inode  = inode;
    file->offset = 0;

    return file_to_handle(file);

err:
    _close_file(file);
err_no_close:
    return -1;
}

int open_file(const char *name, struct com32_filedata *filedata)
{
    int rv;
    struct file *file;
    char mangled_name[FILENAME_MAX];

    dprintf("open_file %s\n", name);

    mangle_name(mangled_name, name);
    rv = searchdir(mangled_name);

    if (rv < 0)
	return rv;

    file = handle_to_file(rv);

    if (file->inode->mode != DT_REG) {
	_close_file(file);
	return -1;
    }

    filedata->size	= file->inode->size;
    filedata->blocklg2	= SECTOR_SHIFT(file->fs);
    filedata->handle	= rv;

    return rv;
}

void pm_open_file(com32sys_t *regs)
{
    int rv;
    struct file *file;
    const char *name = MK_PTR(regs->es, regs->esi.w[0]);
    char mangled_name[FILENAME_MAX];

    dprintf("pm_open_file %s\n", name);

    mangle_name(mangled_name, name);
    rv = searchdir(mangled_name);
    if (rv < 0) {
	regs->eflags.l |= EFLAGS_CF;
    } else {
	file = handle_to_file(rv);
	regs->eflags.l &= ~EFLAGS_CF;
	regs->eax.l = file->inode->size;
	regs->ecx.w[0] = SECTOR_SIZE(file->fs);
	regs->esi.w[0] = rv;
    }
}

void close_file(uint16_t handle)
{
    struct file *file;

    if (handle) {
	file = handle_to_file(handle);
	_close_file(file);
    }
}

void pm_close_file(com32sys_t *regs)
{
    close_file(regs->esi.w[0]);
}

/*
 * it will do:
 *    initialize the memory management function;
 *    set up the vfs fs structure;
 *    initialize the device structure;
 *    invoke the fs-specific init function;
 *    initialize the cache if we need one;
 *    finally, get the current inode for relative path looking.
 */
__bss16 uint16_t SectorSize, SectorShift;

void fs_init(com32sys_t *regs)
{
    static struct fs_info fs;	/* The actual filesystem buffer */
    uint8_t disk_devno = regs->edx.b[0];
    uint8_t disk_cdrom = regs->edx.b[1];
    sector_t disk_offset = regs->ecx.l | ((sector_t)regs->ebx.l << 32);
    uint16_t disk_heads = regs->esi.w[0];
    uint16_t disk_sectors = regs->edi.w[0];
    uint32_t maxtransfer = regs->ebp.l;
    int blk_shift = -1;
    struct device *dev = NULL;
    /* ops is a ptr list for several fs_ops */
    const struct fs_ops **ops = (const struct fs_ops **)regs->eax.l;

    /* Initialize malloc() */
    mem_init();

    /* Default name for the root directory */
    fs.cwd_name[0] = '/';

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
				  disk_heads, disk_sectors, maxtransfer);
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

    /* start out in the root directory */
    if (fs.fs_ops->iget_root) {
	fs.root = fs.fs_ops->iget_root(&fs);
	fs.cwd = get_inode(fs.root);
	dprintf("init: root inode %p, cwd inode %p\n", fs.root, fs.cwd);
    }

    SectorShift = fs.sector_shift;
    SectorSize  = fs.sector_size;
}
