/*
 * Copyright (C) 2012 Andre Ericson <de.ericson@gmail.com>
 * Copyright (C) 2012-2015 Paulo Alcantara <pcacjr@zytor.com>
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
#include "core.h"
#include "fs.h"
#include "disk.h"
#include "cache.h"
#include "multifs.h"

#include <syslinux/multifs_utils.h>

/* MaxTransfer for multifs access */
#define MAX_TRANSFER 127

static bios_find_partition_t find_partition = NULL;

__export struct fs_info *bios_multifs_get_fs_info(const char **path)
{
    struct fs_info *fsp;
    void *private;
    uint8_t hdd, partition;
    struct multifs_utils_part_info *pinfo;
    int ret;

    if (multifs_parse_path(path, &hdd, &partition)) {
        printf("multifs: Syntax invalid: %s\n", *path);
        return NULL;
    }

    fsp = multifs_get_fs(hdd, partition - 1);
    if (fsp)
        return fsp;

    fsp = malloc(sizeof(struct fs_info));
    if (!fsp)
        return NULL;

    private = find_partition(hdd, partition);
    if (!private) {
        printf("multifs: Failed to get disk/partition: %s\n", *path);
        goto bail;
    }
    ret = multifs_setup_fs_info(fsp, hdd, partition, private);
    if (ret) {
        goto bail;
    }
    return fsp;

bail:
    free(fsp);
    return NULL;
}

__export void bios_multifs_init(void *addr)
{
    find_partition = addr;
    dprintf("%s: initialised multifs support\n", __func__);
}
