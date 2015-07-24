/*
 * Copyright (C) 2013 Raphael S. Carvalho <raphael.scarv@gmail.com>
 * Copyright (C) 2015 Paulo Alcantara <pcacjr@zytor.com>
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

#include <stdio.h>
#include <assert.h>
#include <fs.h>
#include <ilog2.h>
#include <disk.h>
#include <cache.h>
#include <minmax.h>

#include "multifs.h"

#define DISKS_MAX 0x7f

struct part_node {
    int partition;
    struct fs_info *fs;
    struct part_node *next;
};

struct queue_head {
    struct part_node *first;
    struct part_node *last;
};

static struct queue_head *parts_info[DISKS_MAX];

static int add_fs(struct fs_info *fs, uint8_t diskno, uint8_t partno)
{
    struct queue_head *head = parts_info[diskno];
    struct part_node *node;

    node = malloc(sizeof(struct part_node));
    if (!node)
        return -1;
    node->fs = fs;
    node->next = NULL;
    node->partition = partno;

    if (!head) {
        head = malloc(sizeof(struct queue_head));
        if (!head) {
            free(node);
            return -1;
        }
        head->first = head->last = node;
        parts_info[diskno] = head;
        return 0;
    }
    head->last->next = node;
    head->last = node;
    return 0;
}

struct fs_info *multifs_get_fs(uint8_t diskno, uint8_t partno)
{
    struct part_node *i;

    for (i = parts_info[diskno]->first; i; i = i->next) {
        if (i->partition == partno)
            return i->fs;
    }
    return NULL;
}

static const char *get_num(const char *p, char delimiter, uint8_t *data)
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

int multifs_parse_path(const char **path, uint8_t *diskno, uint8_t *partno)
{
    const char *p = *path;
    static const char *cwd = ".";

    *diskno = *partno = 0;
    p++; /* Skip open parentheses */

    /* Get hd number (Range: 0 - (DISKS_MAX - 1)) */
    if (*p != 'h' || *(p + 1) != 'd')
        return -1;

    p += 2; /* Skip 'h' and 'd' */
    p = get_num(p, ',', diskno);
    if (!p)
        return -1;
    if (*diskno >= DISKS_MAX) {
        printf("multifs: disk number is out of range: 0-%d\n", DISKS_MAX - 1);
        return -1;
    }

    /* Get partition number (Range: 0 - 0xFF) */
    p = get_num(p, ')', partno);
    if (!p)
        return -1;

    if (*p == '\0') {
        /* Assume it's a cwd request */
        p = cwd;
    }

    *path = p;
    dprintf("multifs: disk: %u partition: %u path: %s\n", *diskno, *partno,
            *path);
    return 0;
}

int multifs_setup_fs_info(struct fs_info *fsp, uint8_t diskno, uint8_t partno,
                          void *priv)
{
    const struct fs_ops **ops;
    int blk_shift = -1;
    struct device *dev = NULL;

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
                dev = device_init(priv);
                if (!dev)
                    return -1;
            }
            fsp->fs_dev = dev;
        }
        /* invoke the fs-specific init code */
        blk_shift = fsp->fs_ops->fs_init(fsp);
        ops++;
    }
    if (blk_shift < 0) {
        dprintf("%s: no valid file system found!\n", __func__);
        goto out_free;
    }

    if (add_fs(fsp, diskno, partno - 1))
        goto out_free;
    if (fsp->fs_dev && fsp->fs_dev->cache_data)
        cache_init(fsp->fs_dev, blk_shift);
    if (fsp->fs_ops->iget_root) {
        fsp->root = fsp->fs_ops->iget_root(fsp);
        fsp->cwd = get_inode(fsp->root);
    }
    return 0;

out_free:
    free(dev->disk);
    free(dev->cache_data);
    free(dev);
    return -1;
}

void multifs_restore_fs(void)
{
    this_fs = root_fs;
}

int multifs_switch_fs(const char **path)
{
    struct fs_info *fs;

    assert(path && *path);
    if ((*path)[0] != '(') {
        /* If so, don't need to restore chdir */
        if (this_fs == root_fs)
            return 0;

        fs = root_fs;
        goto ret;
    }

    fs = multifs_ops->get_fs_info(path);
    if (!fs)
        return -1;

ret:
    this_fs = fs;
    return 0;
}
