/*
 * Copyright (C) 2012 Andre Ericson <de.ericson@gmail.com>
 * Copyright (C) 2012 Paulo Cavalcanti <pcacjr@zytor.com>
 * Copyright (C) 2013 Raphael S. Carvalho <raphael.scarv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <syslinux/multifs_utils.h>
#include "core.h"
#include "disk.h"
#include "cache.h"
#include "minmax.h"

/* 0x80 - 0xFF
 * BIOS limitation */
#define DISK_ID_OFFSET 0x80
#define DISKS_MAX 128

/* MaxTransfer for MultiFS access */
#define MAX_TRANSFER 127

static struct queue_head *parts_info[DISKS_MAX];

/*
 * Store info about the FS into a specific queue to be used later.
 *
 * @ret: 0 on success, -1 on failure.
 */
static int add_fs(struct fs_info *fs, uint8_t disk, uint8_t partition)
{
    struct queue_head *head = parts_info[disk];
    struct part_node *node;

    node = malloc(sizeof(struct part_node));
    if (!node)
        return -1;
    node->fs = fs;
    node->next = NULL;
    node->partition = partition;

    if (!head) {
        head = malloc(sizeof(struct queue_head));
        if (!head) {
	    free(node);
	    return -1;
	}
        head->first = head->last = node;
        parts_info[disk] = head;
        return 0;
    }
    head->last->next = node;
    head->last = node;
    return 0;
}

/*
 * Check if the FS was previously allocated.
 *
 * @ret: return the fs on success, NULL on failure.
 */
static struct fs_info *get_fs(uint8_t disk, uint8_t partition)
{
    struct part_node *i;

    for (i = parts_info[disk]->first; i; i = i->next) {
        if (i->partition == partition)
            return i->fs;
    }
    return NULL;
}

/*
 * Attempt to find a partition based on drive and partition numbers.
 *
 * @ret: 0 on success, -1 on failure.
 */
static int find_partition(struct part_iter **_iter, struct disk_info *diskinfo,
			  int partition)
{
    struct part_iter *iter = NULL;

    if (!(iter = pi_begin(diskinfo, 0)))
	return -1;

    do {
        if (iter->index == partition)
            break;
    } while (!pi_next(iter));

    if (iter->status) {
        dprintf("MultiFS: Request disk/partition combination not found.\n");
        goto bail;
    }
    dprintf("MultiFS: found 0x%llx at idex: %i and partition %i\n",
	    iter->abs_lba, iter->index, partition);

    *_iter = iter;
    return 0;
bail:
    pi_del(&iter);
    return -1;
}

/*
 * Get a number till the delimiter is found.
 *
 * @ret: addr to delimiter+1 on success, NULL on failure.
 */
static const char *get_num(const char *p, char delimiter, uint8_t *data)
{
    uint32_t n = 0;

    while (*p) {
	if (*p < '0' || *p > '9')
	    break;
	
	n = (n * 10) + (*p - '0');
	p++;

	if (*p == delimiter) {
	    p++; /* Skip delimiter */
	    *data = min(n, UINT8_MAX); /* Avoid overflow */
	    return p;
	}
    }
    return NULL;
}

/*
 * Parse MultiFS path. Syntax:
 * (hd[disk number],[partition number])/path/to/file
 * 
 * @ret: Returns syntax validity.
 */
static int parse_multifs_path(const char **path, uint8_t *hdd,
			      uint8_t *partition)
{
    const char *p = *path;
    static const char *cwd = ".";

    *hdd = *partition = 0;
    p++; /* Skip open parentheses */

    /* Get hd number (Range: 0 - (DISKS_MAX - 1)) */
    if (*p != 'h' || *(p + 1) != 'd')
	return -1; 

    p += 2; /* Skip 'h' and 'd' */
    p = get_num(p, ',', hdd);
    if (!p)
	return -1;
    if (*hdd >= DISKS_MAX) {
	printf("MultiFS: hdd is out of range: 0-%d\n", DISKS_MAX - 1);
	return -1;
    }

    /* Get partition number (Range: 0 - 0xFF) */
    p = get_num(p, ')', partition);
    if (!p)
	return -1;

    if (*p == '\0') {
	/* Assume it's a cwd request */
	p = cwd;
    }

    *path = p;
    dprintf("MultiFS: hdd: %u partition: %u path: %s\n",
	    *hdd, *partition, *path);
    return 0;
}

/*
 * Set up private struct based on paramaters.
 * This structure will be used later to set up a device
 * to (disk x,partition y).
 *
 * @devno: Device number (range: 0 - (DISKS_MAX - 1)).
 * @part_start: Start LBA.
 * @bsHeads: Number of heads.
 * @bsSecPerTrack: Sectors per track.
 */
static void *get_private(uint8_t devno, uint64_t part_start,
			 uint16_t bsHeads, uint16_t bsSecPerTrack)
{
    static com32sys_t regs;
    static struct bios_disk_private priv;

    priv.regs = &regs;

    regs.edx.b[0] = devno;
    regs.edx.b[1] = 0; // TODO: cdrom ... always 0???
    regs.ecx.l = part_start & 0xFFFFFFFF;
    regs.ebx.l = part_start >> 32;
    regs.esi.w[0] = bsHeads;
    regs.edi.w[0] = bsSecPerTrack;
    regs.ebp.l = MAX_TRANSFER; // TODO: should it be pre-defined???

    return (void *) &priv;
}

/*
 * 1) Set up a new device based on the disk and the partition.
 * 2) Find which file system is installed in this device.
 * 3) Set up fs_info based on the fs, add to queue, and return it.
 * 4) Subsequent accesses to the same disk and partition will get
 *    fs_info from the queue.
 *
 * It handles the important stuff to get the MultiFS support working.
 */
static struct fs_info *get_fs_info(const char **path)
{
    const struct fs_ops **ops;
    struct fs_info *fsp;
    struct disk_info diskinfo;
    struct part_iter *iter = NULL;
    struct device *dev = NULL;
    void *private;
    int blk_shift = -1;
    uint8_t disk_devno, hdd, partition;

    if (parse_multifs_path(path, &hdd, &partition)) {
	printf("MultiFS: Syntax invalid: %s\n", *path);
	return NULL;
    }

    fsp = get_fs(hdd, partition - 1);
    if (fsp)
        return fsp;

    fsp = malloc(sizeof(struct fs_info));
    if (!fsp)
        return NULL;

    disk_devno = DISK_ID_OFFSET + hdd;
    if (disk_get_params(disk_devno, &diskinfo))
        goto bail;

    if (find_partition(&iter, &diskinfo, partition)) {
        printf("MultiFS: Failed to get disk/partition: %s\n", *path);
        goto bail;
    }
    private = get_private(disk_devno, iter->abs_lba, diskinfo.head,
			  diskinfo.spt);

    /* Default name for the root directory */
    fsp->cwd_name[0] = '/';
    fsp->cwd_name[1] = '\0';

    ops = p_ops;
    while ((blk_shift < 0) && *ops) {
        /* set up the fs stucture */
        fsp->fs_ops = *ops;

        /*
         * This boldly assumes that we don't mix FS_NODEV filesystems
         * with FS_DEV filesystems...
         */
        if (fsp->fs_ops->fs_flags & FS_NODEV) {
            fsp->fs_dev = NULL;
        } else {
	    if (!dev) {
		dev = device_init(private);
		if (!dev)
		    goto bail;
	    }
	    fsp->fs_dev = dev;
        }
        /* invoke the fs-specific init code */
        blk_shift = fsp->fs_ops->fs_init(fsp);
        ops++;
    }
    if (blk_shift < 0) {
	printf("MultiFS: No valid file system found!\n");
        goto free_dev;
    }

    /* add fs_info into hdd queue  */
    if (add_fs(fsp, hdd, partition - 1))
	goto free_dev;

    /* initialize the cache */
    if (fsp->fs_dev && fsp->fs_dev->cache_data)
        cache_init(fsp->fs_dev, blk_shift);

    /* start out in the root directory */
    if (fsp->fs_ops->iget_root) {
        fsp->root = fsp->fs_ops->iget_root(fsp);
        fsp->cwd = get_inode(fsp->root);
    }

    return fsp;
free_dev:
    free(dev->disk);
    free(dev->cache_data);
    free(dev);
bail:
    free(fsp);
    return NULL;
}

/*
 * Initialize MultiFS support
 */
void init_multifs(void)
{
    enable_multifs(&get_fs_info);
    dprintf("MultiFS support was enabled successfully!\n");
}
