/*
 * Copyright (c) 2015 Paulo Alcantara <pcacjr@zytor.com>
 * Copyright (C) 2012 Andre Ericson <de.ericson@gmail.com>
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
#include <fs.h>
#include <ilog2.h>
#include <disk.h>

#include "cache.h"
#include "minmax.h"
#include "multifs_utils.h"
#include "efi.h"

#define DISKS_MAX 0xff

static EFI_HANDLE *_logical_parts = NULL;
static unsigned int _logical_parts_no = 0;
static struct queue_head *parts_info[DISKS_MAX];

/* Find all BlockIo device handles which is a logical partition */
static EFI_STATUS find_all_logical_parts(void)
{
    EFI_STATUS status;
    unsigned long len = 0;
    EFI_HANDLE *handles = NULL;
    unsigned long i;
    EFI_BLOCK_IO *bio;

    if (_logical_parts) {
        status = EFI_SUCCESS;
        goto out;
    }

    status = uefi_call_wrapper(BS->LocateHandle, 5, ByProtocol,
                               &BlockIoProtocol, NULL, &len, NULL);
    if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL)
        goto out;

    handles = malloc(len);
    if (!handles) {
        status = EFI_OUT_OF_RESOURCES;
        goto out;
    }

    _logical_parts = malloc(len);
    if (!_logical_parts) {
        status = EFI_OUT_OF_RESOURCES;
        goto out_free;
    }

    status = uefi_call_wrapper(BS->LocateHandle, 5, ByProtocol,
                               &BlockIoProtocol, NULL, &len,
                               (void **)handles);
    if (EFI_ERROR(status))
        goto out_free;

    for (i = 0; i < len / sizeof(EFI_HANDLE); i++) {
        status = uefi_call_wrapper(BS->HandleProtocol, 3, handles[i],
                                   &BlockIoProtocol, (void **)&bio);
        if (EFI_ERROR(status))
            goto out_free;
        if (bio->Media->LogicalPartition) {
            _logical_parts[_logical_parts_no++] = handles[i];
        }
    }

    free(handles);
    return status;

out_free:
    if (handles)
        free(handles);
    if (_logical_parts)
        free(_logical_parts);
out:
    return status;
}

static inline EFI_HANDLE get_logical_part(unsigned int partno)
{
    if (!_logical_parts || partno > _logical_parts_no)
        return NULL;
    return _logical_parts[partno - 1];
}

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

static struct fs_info *get_fs(uint8_t disk, uint8_t partition)
{
    struct part_node *i;

    for (i = parts_info[disk]->first; i; i = i->next) {
        if (i->partition == partition)
            return i->fs;
    }
    return NULL;
}

static EFI_HANDLE find_partition(unsigned int diskno, unsigned int partno)
{
    return get_logical_part(partno);
}

static const char *get_num(const char *p, char delimiter, unsigned int *data)
{
    uint32_t n = 0;

    while (*p) {
        if (*p < '0' || *p > '9')
            break;
        n = (n * 10) + (*p - '0');
        p++;
        if (*p == delimiter) {
            p++; /* skip delimiter */
            *data = min(n, UINT8_MAX); /* avoid overflow */
            return p;
        }
    }
    return NULL;
}

static int parse_multifs_path(const char **path, unsigned int *hdd,
                              unsigned int *partition)
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

static inline void *get_private(EFI_HANDLE lpart)
{
    static struct efi_disk_private priv;
    priv.dev_handle = lpart;
    return (void *)&priv;
}

static struct fs_info *get_fs_info(const char **path)
{
    const struct fs_ops **ops;
    struct fs_info *fsp;
    EFI_HANDLE dh;
    struct device *dev = NULL;
    void *private;
    int blk_shift = -1;
    unsigned int hdd, partition;

    if (parse_multifs_path(path, &hdd, &partition)) {
        return NULL;
    }

    fsp = get_fs(hdd, partition - 1);
    if (fsp)
        return fsp;

    fsp = malloc(sizeof(struct fs_info));
    if (!fsp)
        return NULL;

    dh = find_partition(hdd, partition);
    if (!dh)
        goto bail;
    dprintf("\nMultiFS: found partition %d\n", partition);
    private = get_private(dh);

    /* set default name for the root directory */
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
        dprintf("MultiFS: No valid file system found!\n");
        goto free_dev;
    }

    if (add_fs(fsp, hdd, partition - 1))
        goto free_dev;
    if (fsp->fs_dev && fsp->fs_dev->cache_data && !fsp->fs_dev->cache_init)
        cache_init(fsp->fs_dev, blk_shift);
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

EFI_STATUS init_multifs(void)
{
    EFI_STATUS status;

    status = find_all_logical_parts();
    enable_multifs(get_fs_info);
    dprintf("MultiFS: initialised\n");

    return status;
}
