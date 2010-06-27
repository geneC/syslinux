/*
 * linuxioctl.h
 *
 * Wrapper for Linux ioctl definitions, including workarounds
 */

#ifndef LIBINSTALLER_LINUXIOCTL_H
#define LIBINSTALLER_LINUXIOCTL_H

#include <sys/ioctl.h>

#define statfs _kernel_statfs	/* HACK to deal with broken 2.4 distros */

#include <linux/fd.h>		/* Floppy geometry */
#include <linux/hdreg.h>	/* Hard disk geometry */

#include <linux/fs.h>		/* FIGETBSZ, FIBMAP, FS_IOC_FIEMAP */
#include <linux/msdos_fs.h>	/* FAT_IOCTL_SET_ATTRIBUTES */

#undef SECTOR_SIZE		/* Defined in msdos_fs.h for no good reason */
#undef SECTOR_BITS
#include <linux/ext2_fs.h>	/* EXT2_IOC_* */

#ifndef FAT_IOCTL_GET_ATTRIBUTES
# define FAT_IOCTL_GET_ATTRIBUTES	_IOR('r', 0x10, __u32)
#endif

#ifndef FAT_IOCTL_SET_ATTRIBUTES
# define FAT_IOCTL_SET_ATTRIBUTES	_IOW('r', 0x11, __u32)
#endif

#include <linux/fiemap.h>	/* FIEMAP definitions */

#ifndef FS_IOC_FIEMAP
# define FS_IOC_FIEMAP		_IOWR('f', 11, struct fiemap)
#endif

#undef statfs

#if defined(__linux__) && !defined(BLKGETSIZE64)
/* This takes a u64, but the size field says size_t.  Someone screwed big. */
# define BLKGETSIZE64 _IOR(0x12,114,size_t)
#endif

#include <linux/loop.h>

#endif /* LIBINSTALLER_LINUXIOCTL_H */
