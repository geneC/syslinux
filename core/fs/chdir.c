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

#define EMIT(x)		     		\
do {		     			\
    if (++n < bufsize)	 		\
    	*q++ = (x);			\
} while (0)

static size_t join_paths(char *dst, size_t bufsize,
			 const char *s1, const char *s2)
{
    const char *list[2];
    int i;
    char c;
    const char *p;
    char *q  = dst;
    size_t n = 0;
    bool slash = false;
    
    list[0] = s1;
    list[1] = s2;

    for (i = 0; i < 2; i++) {
	p = list[i];

	while ((c = *p++)) {
	    if (c == '/') {
		if (!slash)
		    EMIT(c);
		slash = true;
	    } else {
		EMIT(c);
		slash = false;
	    }
	}
    }

    if (bufsize)
	*q = '\0';

    return n;
}

size_t realpath(char *dst, const char *src, size_t bufsize)
{
    if (this_fs->fs_ops->realpath) {
	return this_fs->fs_ops->realpath(this_fs, dst, src, bufsize);
    } else {
	/* Filesystems with "common" pathname resolution */
	return join_paths(dst, bufsize, 
			  src[0] == '/' ? "" : this_fs->cwd_name,
			  src);
    }
}

int chdir(const char *src)
{
    int rv;
    struct file *file;
    char cwd_buf[CURRENTDIR_MAX];

    if (this_fs->fs_ops->chdir)
	return this_fs->fs_ops->chdir(this_fs, src);

    /* Otherwise it is a "conventional filesystem" */
    rv = searchdir(src);
    if (rv < 0)
	return rv;

    file = handle_to_file(rv);
    if (file->inode->mode != DT_DIR) {
	_close_file(file);
	return -1;
    }

    put_inode(this_fs->cwd);
    this_fs->cwd = get_inode(file->inode);
    _close_file(file);

    /* Save the current working directory */
    realpath(cwd_buf, src, CURRENTDIR_MAX);

    /* Make sure the cwd_name ends in a slash, it's supposed to be a prefix */
    join_paths(this_fs->cwd_name, CURRENTDIR_MAX, cwd_buf, "/");

    return 0;
}
