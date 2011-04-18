#ifndef _H_SYSLXCOM_
#define _H_SYSLXCOM_

#include "syslinux.h"

/* Global fs_type for handling fat, ext2/3/4 and btrfs */
enum filesystem {
    NONE,
    EXT2,
    BTRFS,
    VFAT,
};

extern int fs_type;
extern const char *program;
ssize_t xpread(int fd, void *buf, size_t count, off_t offset);
ssize_t xpwrite(int fd, const void *buf, size_t count, off_t offset);
void clear_attributes(int fd);
void set_attributes(int fd);
int sectmap(int fd, sector_t *sectors, int nsectors);
int syslinux_already_installed(int dev_fd);

#endif
