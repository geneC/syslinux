#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "fs.h"
#include "cache.h"

/*
 * Convert a relative pathname to an absolute pathname
 * In the future this might also resolve symlinks...
 */
void pm_realpath(com32sys_t *regs)
{
    const char *src = MK_PTR(regs->ds, regs->esi.w[0]);
    char       *dst = MK_PTR(regs->es, regs->edi.w[0]);

    realpath(dst, src, FILENAME_MAX);
}

size_t realpath(char *dst, const char *src, size_t bufsize)
{
    if (this_fs->fs_ops->realpath) {
	return this_fs->fs_ops->realpath(this_fs, dst, src, bufsize);
    } else {
	/* Filesystems with "common" pathname resolution */
	return snprintf(dst, bufsize, "%s%s",
			src[0] == '/' ? "" : this_fs->cwd_name,
			src);
    }
}

int chdir(const char *src)
{
    int rv;
    struct file *file;
    char *p;

    if (this_fs->fs_ops->chdir)
	return this_fs->fs_ops->chdir(this_fs, src);

    /* Otherwise it is a "conventional filesystem" */
    rv = searchdir(src);
    if (rv < 0)
	return rv;

    file = handle_to_file(rv);
    if (file->inode->mode != I_DIR) {
	_close_file(file);
	return -1;
    }

    this_fs->cwd = file->inode;
    file->inode = NULL;		/* "Steal" the inode */
    _close_file(file);

    /* Save the current working directory */
    realpath(this_fs->cwd_name, src, CURRENTDIR_MAX);
    p = strchr(this_fs->cwd_name, '\0');

    /* Make sure the cwd_name ends in a slash, it's supposed to be a prefix */
    if (p < this_fs->cwd_name + CURRENTDIR_MAX - 1 &&
	(p == this_fs->cwd_name || p[1] != '/')) {
	p[0] = '/';
	p[1] = '\0';
    }
    return 0;
}
