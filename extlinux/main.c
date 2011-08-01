/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * extlinux.c
 *
 * Install the syslinux boot block on an fat, ext2/3/4 and btrfs filesystem
 */

#define  _GNU_SOURCE		/* Enable everything */
#include <inttypes.h>
/* This is needed to deal with the kernel headers imported into glibc 3.3.3. */
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#ifndef __KLIBC__
#include <mntent.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vfs.h>

#include "linuxioctl.h"

#include "btrfs.h"
#include "fat.h"
#include "../version.h"
#include "syslxint.h"
#include "syslxcom.h" /* common functions shared with extlinux and syslinux */
#include "setadv.h"
#include "syslxopt.h" /* unified options */

#ifdef DEBUG
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

#ifndef EXT2_SUPER_OFFSET
#define EXT2_SUPER_OFFSET 1024
#endif

/* the btrfs partition first 64K blank area is used to store boot sector and
   boot image, the boot sector is from 0~512, the boot image starts after */
#define BTRFS_BOOTSECT_AREA	65536
#define BTRFS_EXTLINUX_OFFSET	SECTOR_SIZE
#define BTRFS_SUBVOL_MAX 256	/* By btrfs specification */
static char subvol[BTRFS_SUBVOL_MAX];

#define BTRFS_ADV_OFFSET (BTRFS_BOOTSECT_AREA - 2 * ADV_SIZE)

/*
 * Get the size of a block device
 */
uint64_t get_size(int devfd)
{
    uint64_t bytes;
    uint32_t sects;
    struct stat st;

#ifdef BLKGETSIZE64
    if (!ioctl(devfd, BLKGETSIZE64, &bytes))
	return bytes;
#endif
    if (!ioctl(devfd, BLKGETSIZE, &sects))
	return (uint64_t) sects << 9;
    else if (!fstat(devfd, &st) && st.st_size)
	return st.st_size;
    else
	return 0;
}

/*
 * Get device geometry and partition offset
 */
struct geometry_table {
    uint64_t bytes;
    struct hd_geometry g;
};

static int sysfs_get_offset(int devfd, unsigned long *start)
{
    struct stat st;
    char sysfs_name[128];
    FILE *f;
    int rv;

    if (fstat(devfd, &st))
	return -1;

    if ((size_t)snprintf(sysfs_name, sizeof sysfs_name,
			 "/sys/dev/block/%u:%u/start",
			 major(st.st_rdev), minor(st.st_rdev))
	>= sizeof sysfs_name)
	return -1;

    f = fopen(sysfs_name, "r");
    if (!f)
	return -1;

    rv = fscanf(f, "%lu", start);
    fclose(f);

    return (rv == 1) ? 0 : -1;
}

/* Standard floppy disk geometries, plus LS-120.  Zipdisk geometry
   (x/64/32) is the final fallback.  I don't know what LS-240 has
   as its geometry, since I don't have one and don't know anyone that does,
   and Google wasn't helpful... */
static const struct geometry_table standard_geometries[] = {
    {360 * 1024, {2, 9, 40, 0}},
    {720 * 1024, {2, 9, 80, 0}},
    {1200 * 1024, {2, 15, 80, 0}},
    {1440 * 1024, {2, 18, 80, 0}},
    {1680 * 1024, {2, 21, 80, 0}},
    {1722 * 1024, {2, 21, 80, 0}},
    {2880 * 1024, {2, 36, 80, 0}},
    {3840 * 1024, {2, 48, 80, 0}},
    {123264 * 1024, {8, 32, 963, 0}},	/* LS120 */
    {0, {0, 0, 0, 0}}
};

int get_geometry(int devfd, uint64_t totalbytes, struct hd_geometry *geo)
{
    struct floppy_struct fd_str;
    struct loop_info li;
    struct loop_info64 li64;
    const struct geometry_table *gp;
    int rv = 0;

    memset(geo, 0, sizeof *geo);

    if (!ioctl(devfd, HDIO_GETGEO, geo)) {
	goto ok;
    } else if (!ioctl(devfd, FDGETPRM, &fd_str)) {
	geo->heads = fd_str.head;
	geo->sectors = fd_str.sect;
	geo->cylinders = fd_str.track;
	geo->start = 0;
	goto ok;
    }

    /* Didn't work.  Let's see if this is one of the standard geometries */
    for (gp = standard_geometries; gp->bytes; gp++) {
	if (gp->bytes == totalbytes) {
	    memcpy(geo, &gp->g, sizeof *geo);
	    goto ok;
	}
    }

    /* Didn't work either... assign a geometry of 64 heads, 32 sectors; this is
       what zipdisks use, so this would help if someone has a USB key that
       they're booting in USB-ZIP mode. */

    geo->heads = opt.heads ? : 64;
    geo->sectors = opt.sectors ? : 32;
    geo->cylinders = totalbytes / (geo->heads * geo->sectors << SECTOR_SHIFT);
    geo->start = 0;

    if (!opt.sectors && !opt.heads) {
	fprintf(stderr,
		"Warning: unable to obtain device geometry (defaulting to %d heads, %d sectors)\n"
		"         (on hard disks, this is usually harmless.)\n",
		geo->heads, geo->sectors);
	rv = 1;			/* Suboptimal result */
    }

ok:
    /* If this is a loopback device, try to set the start */
    if (!ioctl(devfd, LOOP_GET_STATUS64, &li64))
	geo->start = li64.lo_offset >> SECTOR_SHIFT;
    else if (!ioctl(devfd, LOOP_GET_STATUS, &li))
	geo->start = (unsigned int)li.lo_offset >> SECTOR_SHIFT;
    else if (!sysfs_get_offset(devfd, &geo->start)) {
	/* OK */
    }

    return rv;
}

/*
 * Query the device geometry and put it into the boot sector.
 * Map the file and put the map in the boot sector and file.
 * Stick the "current directory" inode number into the file.
 *
 * Returns the number of modified bytes in the boot file.
 */
int patch_file_and_bootblock(int fd, const char *dir, int devfd)
{
    struct stat dirst, xdst;
    struct hd_geometry geo;
    sector_t *sectp;
    uint64_t totalbytes, totalsectors;
    int nsect;
    struct boot_sector *sbs;
    char *dirpath, *subpath, *xdirpath;
    int rv;

    dirpath = realpath(dir, NULL);
    if (!dirpath || stat(dir, &dirst)) {
	perror("accessing install directory");
	exit(255);		/* This should never happen */
    }

    if (lstat(dirpath, &xdst) ||
	dirst.st_ino != xdst.st_ino ||
	dirst.st_dev != xdst.st_dev) {
	perror("realpath returned nonsense");
	exit(255);
    }

    subpath = strchr(dirpath, '\0');
    for (;;) {
	if (*subpath == '/') {
	    if (subpath > dirpath) {
		*subpath = '\0';
		xdirpath = dirpath;
	    } else {
		xdirpath = "/";
	    }
	    if (lstat(xdirpath, &xdst) || dirst.st_dev != xdst.st_dev) {
		subpath = strchr(subpath+1, '/');
		if (!subpath)
		    subpath = "/"; /* It's the root of the filesystem */
		break;
	    }
	    *subpath = '/';
	}

	if (subpath == dirpath)
	    break;

	subpath--;
    }

    /* Now subpath should contain the path relative to the fs base */
    dprintf("subpath = %s\n", subpath);

    totalbytes = get_size(devfd);
    get_geometry(devfd, totalbytes, &geo);

    if (opt.heads)
	geo.heads = opt.heads;
    if (opt.sectors)
	geo.sectors = opt.sectors;

    /* Patch this into a fake FAT superblock.  This isn't because
       FAT is a good format in any way, it's because it lets the
       early bootstrap share code with the FAT version. */
    dprintf("heads = %u, sect = %u\n", geo.heads, geo.sectors);

    sbs = (struct boot_sector *)syslinux_bootsect;

    totalsectors = totalbytes >> SECTOR_SHIFT;
    if (totalsectors >= 65536) {
	set_16(&sbs->bsSectors, 0);
    } else {
	set_16(&sbs->bsSectors, totalsectors);
    }
    set_32(&sbs->bsHugeSectors, totalsectors);

    set_16(&sbs->bsBytesPerSec, SECTOR_SIZE);
    set_16(&sbs->bsSecPerTrack, geo.sectors);
    set_16(&sbs->bsHeads, geo.heads);
    set_32(&sbs->bsHiddenSecs, geo.start);

    /* Construct the boot file map */

    dprintf("directory inode = %lu\n", (unsigned long)dirst.st_ino);
    nsect = (boot_image_len + SECTOR_SIZE - 1) >> SECTOR_SHIFT;
    nsect += 2;			/* Two sectors for the ADV */
    sectp = alloca(sizeof(sector_t) * nsect);
    if (fs_type == EXT2 || fs_type == VFAT) {
	if (sectmap(fd, sectp, nsect)) {
		perror("bmap");
		exit(1);
	}
    } else if (fs_type == BTRFS) {
	int i;
	sector_t *sp = sectp;

	for (i = 0; i < nsect - 2; i++)
	    *sp++ = BTRFS_EXTLINUX_OFFSET/SECTOR_SIZE + i;
	for (i = 0; i < 2; i++)
	    *sp++ = BTRFS_ADV_OFFSET/SECTOR_SIZE + i;
    }

    /* Create the modified image in memory */
    rv = syslinux_patch(sectp, nsect, opt.stupid_mode,
			opt.raid_mode, subpath, subvol);

    free(dirpath);
    return rv;
}

/*
 * Install the boot block on the specified device.
 * Must be run AFTER install_file()!
 */
int install_bootblock(int fd, const char *device)
{
    struct ext2_super_block sb;
    struct btrfs_super_block sb2;
    struct boot_sector sb3;
    bool ok = false;

    if (fs_type == EXT2) {
	if (xpread(fd, &sb, sizeof sb, EXT2_SUPER_OFFSET) != sizeof sb) {
		perror("reading superblock");
		return 1;
	}
	if (sb.s_magic == EXT2_SUPER_MAGIC)
		ok = true;
    } else if (fs_type == BTRFS) {
	if (xpread(fd, &sb2, sizeof sb2, BTRFS_SUPER_INFO_OFFSET)
			!= sizeof sb2) {
		perror("reading superblock");
		return 1;
	}
	if (sb2.magic == *(u64 *)BTRFS_MAGIC)
		ok = true;
    } else if (fs_type == VFAT) {
	if (xpread(fd, &sb3, sizeof sb3, 0) != sizeof sb3) {
		perror("reading fat superblock");
		return 1;
	}
	if (fat_check_sb_fields(&sb3))
		ok = true;
    }
    if (!ok) {
	fprintf(stderr, "no fat, ext2/3/4 or btrfs superblock found on %s\n",
			device);
	return 1;
    }
    if (fs_type == VFAT) {
	struct boot_sector *sbs = (struct boot_sector *)syslinux_bootsect;
        if (xpwrite(fd, &sbs->bsHead, bsHeadLen, 0) != bsHeadLen ||
	    xpwrite(fd, &sbs->bsCode, bsCodeLen,
		    offsetof(struct boot_sector, bsCode)) != bsCodeLen) {
	    perror("writing fat bootblock");
	    return 1;
	}
    } else {
	if (xpwrite(fd, syslinux_bootsect, syslinux_bootsect_len, 0)
	    != syslinux_bootsect_len) {
	    perror("writing bootblock");
	    return 1;
	}
    }

    return 0;
}

int ext2_fat_install_file(const char *path, int devfd, struct stat *rst)
{
    char *file, *oldfile;
    int fd = -1, dirfd = -1;
    int modbytes;
    int r1, r2;

    r1 = asprintf(&file, "%s%sldlinux.sys",
		  path, path[0] && path[strlen(path) - 1] == '/' ? "" : "/");
    r2 = asprintf(&oldfile, "%s%sextlinux.sys",
		  path, path[0] && path[strlen(path) - 1] == '/' ? "" : "/");
    if (r1 < 0 || !file || r2 < 0 || !oldfile) {
	perror(program);
	return 1;
    }

    dirfd = open(path, O_RDONLY | O_DIRECTORY);
    if (dirfd < 0) {
	perror(path);
	goto bail;
    }

    fd = open(file, O_RDONLY);
    if (fd < 0) {
	if (errno != ENOENT) {
	    perror(file);
	    goto bail;
	}
    } else {
	clear_attributes(fd);
    }
    close(fd);

    fd = open(file, O_WRONLY | O_TRUNC | O_CREAT | O_SYNC,
	      S_IRUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
	perror(file);
	goto bail;
    }

    /* Write it the first time */
    if (xpwrite(fd, boot_image, boot_image_len, 0) != boot_image_len ||
	xpwrite(fd, syslinux_adv, 2 * ADV_SIZE,
		boot_image_len) != 2 * ADV_SIZE) {
	fprintf(stderr, "%s: write failure on %s\n", program, file);
	goto bail;
    }

    /* Map the file, and patch the initial sector accordingly */
    modbytes = patch_file_and_bootblock(fd, path, devfd);

    /* Write the patch area again - this relies on the file being
       overwritten in place! */
    if (xpwrite(fd, boot_image, modbytes, 0) != modbytes) {
	fprintf(stderr, "%s: write failure on %s\n", program, file);
	goto bail;
    }

    /* Attempt to set immutable flag and remove all write access */
    /* Only set immutable flag if file is owned by root */
    set_attributes(fd);

    if (fstat(fd, rst)) {
	perror(file);
	goto bail;
    }

    close(dirfd);
    close(fd);

    /* Look if we have the old filename */
    fd = open(oldfile, O_RDONLY);
    if (fd >= 0) {
	clear_attributes(fd);
	close(fd);
	unlink(oldfile);
    }

    free(file);
    free(oldfile);
    return 0;

bail:
    if (dirfd >= 0)
	close(dirfd);
    if (fd >= 0)
	close(fd);

    free(file);
    free(oldfile);
    return 1;
}

/* btrfs has to install the ldlinux.sys in the first 64K blank area, which
   is not managered by btrfs tree, so actually this is not installed as files.
   since the cow feature of btrfs will move the ldlinux.sys every where */
int btrfs_install_file(const char *path, int devfd, struct stat *rst)
{
    patch_file_and_bootblock(-1, path, devfd);
    if (xpwrite(devfd, boot_image, boot_image_len, BTRFS_EXTLINUX_OFFSET)
		!= boot_image_len) {
	perror("writing bootblock");
	return 1;
    }
    dprintf("write boot_image to 0x%x\n", BTRFS_EXTLINUX_OFFSET);
    if (xpwrite(devfd, syslinux_adv, 2 * ADV_SIZE, BTRFS_ADV_OFFSET)
	!= 2 * ADV_SIZE) {
	perror("writing adv");
	return 1;
    }
    dprintf("write adv to 0x%x\n", BTRFS_ADV_OFFSET);
    if (stat(path, rst)) {
	perror(path);
	return 1;
    }
    return 0;
}

/*
 *  * test if path is a subvolume:
 *   * this function return
 *    * 0-> path exists but it is not a subvolume
 *     * 1-> path exists and it is  a subvolume
 *      * -1 -> path is unaccessible
 *       */
static int test_issubvolume(char *path)
{

        struct stat     st;
        int             res;

        res = stat(path, &st);
        if(res < 0 )
                return -1;

        return (st.st_ino == 256) && S_ISDIR(st.st_mode);

}

/*
 * Get file handle for a file or dir
 */
static int open_file_or_dir(const char *fname)
{
        int ret;
        struct stat st;
        DIR *dirstream;
        int fd;

        ret = stat(fname, &st);
        if (ret < 0) {
                return -1;
        }
        if (S_ISDIR(st.st_mode)) {
                dirstream = opendir(fname);
                if (!dirstream) {
                        return -2;
                }
                fd = dirfd(dirstream);
        } else {
                fd = open(fname, O_RDWR);
        }
        if (fd < 0) {
                return -3;
        }
        return fd;
}

/*
 * Get the default subvolume of a btrfs filesystem
 *   rootdir: btrfs root dir
 *   subvol:  this function will save the default subvolume name here
 */
static char * get_default_subvol(char * rootdir, char * subvol)
{
    struct btrfs_ioctl_search_args args;
    struct btrfs_ioctl_search_key *sk = &args.key;
    struct btrfs_ioctl_search_header *sh;
    int ret, i;
    int fd;
    struct btrfs_root_ref *ref;
    struct btrfs_dir_item *dir_item;
    unsigned long off = 0;
    int name_len;
    char *name;
    char dirname[4096];
    u64 defaultsubvolid = 0;

    ret = test_issubvolume(rootdir);
    if (ret == 1) {
        fd = open_file_or_dir(rootdir);
        if (fd < 0) {
            fprintf(stderr, "ERROR: failed to open %s\n", rootdir);
        }
        ret = fd;
    }
    if (ret <= 0) {
        subvol[0] = '\0';
        return NULL;
    }

    memset(&args, 0, sizeof(args));

   /* search in the tree of tree roots */
   sk->tree_id = 1;

   /*
    * set the min and max to backref keys.  The search will
    * only send back this type of key now.
    */
   sk->max_type = BTRFS_DIR_ITEM_KEY;
   sk->min_type = BTRFS_DIR_ITEM_KEY;

   /*
    * set all the other params to the max, we'll take any objectid
    * and any trans
    */
   sk->min_objectid = BTRFS_ROOT_TREE_DIR_OBJECTID;
   sk->max_objectid = BTRFS_ROOT_TREE_DIR_OBJECTID;

   sk->max_offset = (u64)-1;
   sk->min_offset = 0;
   sk->max_transid = (u64)-1;

   /* just a big number, doesn't matter much */
   sk->nr_items = 4096;

   while(1) {
       ret = ioctl(fd, BTRFS_IOC_TREE_SEARCH, &args);
       if (ret < 0) {
           fprintf(stderr, "ERROR: can't perform the search\n");
           subvol[0] = '\0';
           return NULL;
       }
       /* the ioctl returns the number of item it found in nr_items */
       if (sk->nr_items == 0) {
           break;
       }

       off = 0;

       /*
        * for each item, pull the key out of the header and then
        * read the root_ref item it contains
        */
       for (i = 0; i < sk->nr_items; i++) {
           sh = (struct btrfs_ioctl_search_header *)(args.buf + off);
           off += sizeof(*sh);
           if (sh->type == BTRFS_DIR_ITEM_KEY) {
               dir_item = (struct btrfs_dir_item *)(args.buf + off);
               name_len = dir_item->name_len;
               name = (char *)(dir_item + 1);


               /*add_root(&root_lookup, sh->objectid, sh->offset,
                        dir_id, name, name_len);*/
               strncpy(dirname, name, name_len);
               dirname[name_len] = '\0';
               if (strcmp(dirname, "default") == 0) {
                   defaultsubvolid = dir_item->location.objectid;
                   break;
               }
           }
           off += sh->len;

           /*
            * record the mins in sk so we can make sure the
            * next search doesn't repeat this root
            */
           sk->min_objectid = sh->objectid;
           sk->min_type = sh->type;
           sk->max_type = sh->type;
           sk->min_offset = sh->offset;
       }
       if (defaultsubvolid != 0)
           break;
       sk->nr_items = 4096;
       /* this iteration is done, step forward one root for the next
        * ioctl
        */
       if (sk->min_objectid < (u64)-1) {
           sk->min_objectid = BTRFS_ROOT_TREE_DIR_OBJECTID;
           sk->max_objectid = BTRFS_ROOT_TREE_DIR_OBJECTID;
           sk->max_type = BTRFS_ROOT_BACKREF_KEY;
           sk->min_type = BTRFS_ROOT_BACKREF_KEY;
           sk->min_offset = 0;
       } else
           break;
   }

   if (defaultsubvolid == 0) {
       subvol[0] = '\0';
       return NULL;
   }

   memset(&args, 0, sizeof(args));

   /* search in the tree of tree roots */
   sk->tree_id = 1;

   /*
    * set the min and max to backref keys.  The search will
    * only send back this type of key now.
    */
   sk->max_type = BTRFS_ROOT_BACKREF_KEY;
   sk->min_type = BTRFS_ROOT_BACKREF_KEY;

   /*
    * set all the other params to the max, we'll take any objectid
    * and any trans
    */
   sk->max_objectid = (u64)-1;
   sk->max_offset = (u64)-1;
   sk->max_transid = (u64)-1;

   /* just a big number, doesn't matter much */
   sk->nr_items = 4096;

   while(1) {
       ret = ioctl(fd, BTRFS_IOC_TREE_SEARCH, &args);
       if (ret < 0) {
           fprintf(stderr, "ERROR: can't perform the search\n");
           subvol[0] = '\0';
           return NULL;
       }
       /* the ioctl returns the number of item it found in nr_items */
       if (sk->nr_items == 0)
           break;

       off = 0;

       /*
        * for each item, pull the key out of the header and then
        * read the root_ref item it contains
        */
       for (i = 0; i < sk->nr_items; i++) {
           sh = (struct btrfs_ioctl_search_header *)(args.buf + off);
           off += sizeof(*sh);
           if (sh->type == BTRFS_ROOT_BACKREF_KEY) {
               ref = (struct btrfs_root_ref *)(args.buf + off);
               name_len = ref->name_len;
               name = (char *)(ref + 1);

               if (sh->objectid == defaultsubvolid) {
                   strncpy(subvol, name, name_len);
                   subvol[name_len] = '\0';
                   dprintf("The default subvolume: %s, ID: %llu\n",
			   subvol, sh->objectid);
                   break;
               }

           }

           off += sh->len;

           /*
            * record the mins in sk so we can make sure the
            * next search doesn't repeat this root
            */
           sk->min_objectid = sh->objectid;
           sk->min_type = sh->type;
           sk->min_offset = sh->offset;
       }
       if (subvol[0] != '\0')
           break;
       sk->nr_items = 4096;
       /* this iteration is done, step forward one root for the next
        * ioctl
        */
       if (sk->min_objectid < (u64)-1) {
           sk->min_objectid++;
           sk->min_type = BTRFS_ROOT_BACKREF_KEY;
           sk->min_offset = 0;
       } else
           break;
   }
   return subvol;
}

int install_file(const char *path, int devfd, struct stat *rst)
{
	if (fs_type == EXT2 || fs_type == VFAT)
		return ext2_fat_install_file(path, devfd, rst);
	else if (fs_type == BTRFS)
		return btrfs_install_file(path, devfd, rst);
	return 1;
}

#ifdef __KLIBC__
static char devname_buf[64];

static void device_cleanup(void)
{
    unlink(devname_buf);
}
#endif

/* Verify that a device fd and a pathname agree.
   Return 0 on valid, -1 on error. */
static int validate_device(const char *path, int devfd)
{
    struct stat pst, dst;
    struct statfs sfs;

    if (stat(path, &pst) || fstat(devfd, &dst) || statfs(path, &sfs))
	return -1;
    /* btrfs st_dev is not matched with mnt st_rdev, it is a known issue */
    if (fs_type == BTRFS && sfs.f_type == BTRFS_SUPER_MAGIC)
	return 0;
    return (pst.st_dev == dst.st_rdev) ? 0 : -1;
}

#ifndef __KLIBC__
static const char *find_device(const char *mtab_file, dev_t dev)
{
    struct mntent *mnt;
    struct stat dst;
    FILE *mtab;
    const char *devname = NULL;
    bool done;

    mtab = setmntent(mtab_file, "r");
    if (!mtab)
	return NULL;

    done = false;
    while ((mnt = getmntent(mtab))) {
	/* btrfs st_dev is not matched with mnt st_rdev, it is a known issue */
	switch (fs_type) {
	case BTRFS:
		if (!strcmp(mnt->mnt_type, "btrfs") &&
		    !stat(mnt->mnt_dir, &dst) &&
		    dst.st_dev == dev) {
	                if (!subvol[0]) {
			    get_default_subvol(mnt->mnt_dir, subvol);
                        }
			done = true;
		}
		break;
	case EXT2:
		if ((!strcmp(mnt->mnt_type, "ext2") ||
		     !strcmp(mnt->mnt_type, "ext3") ||
		     !strcmp(mnt->mnt_type, "ext4")) &&
		    !stat(mnt->mnt_fsname, &dst) &&
		    dst.st_rdev == dev) {
		    done = true;
		    break;
		}
	case VFAT:
		if ((!strcmp(mnt->mnt_type, "vfat")) &&
		    !stat(mnt->mnt_fsname, &dst) &&
		    dst.st_rdev == dev) {
		    done = true;
		    break;
		}
	case NONE:
	    break;
	}
	if (done) {
		devname = strdup(mnt->mnt_fsname);
		break;
	}
    }
    endmntent(mtab);

    return devname;
}
#endif

static const char *get_devname(const char *path)
{
    const char *devname = NULL;
    struct stat st;
    struct statfs sfs;

    if (stat(path, &st) || !S_ISDIR(st.st_mode)) {
	fprintf(stderr, "%s: Not a directory: %s\n", program, path);
	return devname;
    }
    if (statfs(path, &sfs)) {
	fprintf(stderr, "%s: statfs %s: %s\n", program, path, strerror(errno));
	return devname;
    }
#ifdef __KLIBC__

    /* klibc doesn't have getmntent and friends; instead, just create
       a new device with the appropriate device type */
    snprintf(devname_buf, sizeof devname_buf, "/tmp/dev-%u:%u",
	     major(st.st_dev), minor(st.st_dev));

    if (mknod(devname_buf, S_IFBLK | 0600, st.st_dev)) {
	fprintf(stderr, "%s: cannot create device %s\n", program, devname);
	return devname;
    }

    atexit(device_cleanup);	/* unlink the device node on exit */
    devname = devname_buf;

#else

    devname = find_device("/proc/mounts", st.st_dev);
    if (!devname) {
	/* Didn't find it in /proc/mounts, try /etc/mtab */
        devname = find_device("/etc/mtab", st.st_dev);
    }
    if (!devname) {
	fprintf(stderr, "%s: cannot find device for path %s\n", program, path);
	return devname;
    }

    fprintf(stderr, "%s is device %s\n", path, devname);
#endif
    return devname;
}

static int open_device(const char *path, struct stat *st, const char **_devname)
{
    int devfd;
    const char *devname = NULL;
    struct statfs sfs;

    if (st)
	if (stat(path, st) || !S_ISDIR(st->st_mode)) {
		fprintf(stderr, "%s: Not a directory: %s\n", program, path);
		return -1;
	}

    if (statfs(path, &sfs)) {
	fprintf(stderr, "%s: statfs %s: %s\n", program, path, strerror(errno));
	return -1;
    }
    if (sfs.f_type == EXT2_SUPER_MAGIC)
	fs_type = EXT2;
    else if (sfs.f_type == BTRFS_SUPER_MAGIC)
	fs_type = BTRFS;
    else if (sfs.f_type == MSDOS_SUPER_MAGIC)
	fs_type = VFAT;

    if (!fs_type) {
	fprintf(stderr, "%s: not a fat, ext2/3/4 or btrfs filesystem: %s\n",
		program, path);
	return -1;
    }

    devfd = -1;
    devname = get_devname(path);
    if (_devname)
	*_devname = devname;

    if ((devfd = open(devname, O_RDWR | O_SYNC)) < 0) {
	fprintf(stderr, "%s: cannot open device %s\n", program, devname);
	return -1;
    }

    /* Verify that the device we opened is the device intended */
    if (validate_device(path, devfd)) {
	fprintf(stderr, "%s: path %s doesn't match device %s\n",
		program, path, devname);
	close(devfd);
	return -1;
    }
    return devfd;
}

static int btrfs_read_adv(int devfd)
{
    if (xpread(devfd, syslinux_adv, 2 * ADV_SIZE, BTRFS_ADV_OFFSET)
	!= 2 * ADV_SIZE)
	return -1;

    return syslinux_validate_adv(syslinux_adv) ? 1 : 0;
}

static int ext_read_adv(const char *path, int devfd, const char **namep)
{
    int err;
    const char *name;

    if (fs_type == BTRFS) {
	/* btrfs "ldlinux.sys" is in 64k blank area */
	return btrfs_read_adv(devfd);
    } else {
	err = read_adv(path, name = "ldlinux.sys");
	if (err == 2)		/* ldlinux.sys does not exist */
	    err = read_adv(path, name = "extlinux.sys");
	if (namep)
	    *namep = name; 
	return err;
    }
}

static int ext_write_adv(const char *path, const char *cfg, int devfd)
{
    if (fs_type == BTRFS) { /* btrfs "ldlinux.sys" is in 64k blank area */
	if (xpwrite(devfd, syslinux_adv, 2 * ADV_SIZE,
		BTRFS_ADV_OFFSET) != 2 * ADV_SIZE) {
		perror("writing adv");
		return 1;
	}
	return 0;
    }
    return write_adv(path, cfg);
}

int install_loader(const char *path, int update_only)
{
    struct stat st, fst;
    int devfd, rv;
    const char *devname;

    devfd = open_device(path, &st, &devname);
    if (devfd < 0)
	return 1;

    if (update_only && !syslinux_already_installed(devfd)) {
	fprintf(stderr, "%s: no previous syslinux boot sector found\n",
		program);
	close(devfd);
	return 1;
    }

    /* Read a pre-existing ADV, if already installed */
    if (opt.reset_adv) {
	syslinux_reset_adv(syslinux_adv);
    } else if (ext_read_adv(path, devfd, NULL) < 0) {
	close(devfd);
	return 1;
    }

    if (modify_adv() < 0) {
	close(devfd);
	return 1;
    }

    /* Install ldlinux.sys */
    if (install_file(path, devfd, &fst)) {
	close(devfd);
	return 1;
    }
    if (fst.st_dev != st.st_dev) {
	fprintf(stderr, "%s: file system changed under us - aborting!\n",
		program);
	close(devfd);
	return 1;
    }

    sync();
    rv = install_bootblock(devfd, devname);
    close(devfd);
    sync();

    return rv;
}

/*
 * Modify the ADV of an existing installation
 */
int modify_existing_adv(const char *path)
{
    const char *filename;
    int devfd;

    devfd = open_device(path, NULL, NULL);
    if (devfd < 0)
	return 1;

    if (opt.reset_adv)
	syslinux_reset_adv(syslinux_adv);
    else if (ext_read_adv(path, devfd, &filename) < 0) {
	close(devfd);
	return 1;
    }
    if (modify_adv() < 0) {
	close(devfd);
	return 1;
    }
    if (ext_write_adv(path, filename, devfd) < 0) {
	close(devfd);
	return 1;
    }
    close(devfd);
    return 0;
}

int main(int argc, char *argv[])
{
    parse_options(argc, argv, MODE_EXTLINUX);

    if (!opt.directory || opt.install_mbr || opt.activate_partition)
	usage(EX_USAGE, 0);

    if (opt.update_only == -1) {
	if (opt.reset_adv || opt.set_once || opt.menu_save)
	    return modify_existing_adv(opt.directory);
	else
	    usage(EX_USAGE, MODE_EXTLINUX);
    }

    return install_loader(opt.directory, opt.update_only);
}
