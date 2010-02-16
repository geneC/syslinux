#include "fs.h"

void generic_close_file(struct file *file)
{
    if (file->inode) {
	file->offset = 0;
	free_inode(file->inode);
    }
}
