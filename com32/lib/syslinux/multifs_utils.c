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

#include <syslinux/multifs_utils.h>

/* 0x80 - 0xFF
 * BIOS limitation */
#define DISK_ID_OFFSET 0x80

/* MaxTransfer for MultiFS access */
#define MAX_TRANSFER 127

static void *get_private(uint8_t devno, uint64_t part_start,
                         uint16_t bsHeads, uint16_t bsSecPerTrack)
{
    com32sys_t *regs;
    struct bios_disk_private *priv;

    regs = malloc(sizeof(*regs));
    if (!regs)
        return NULL;
    memset(regs, 0, sizeof(*regs));

    regs->edx.b[0] = devno;
    regs->edx.b[1] = 0; /* XXX: cdrom ->->-> always 0? */
    regs->ecx.l = part_start & 0xFFFFFFFF;
    regs->ebx.l = part_start >> 32;
    regs->esi.w[0] = bsHeads;
    regs->edi.w[0] = bsSecPerTrack;
    regs->ebp.l = MAX_TRANSFER; /* XXX: should it be pre-defined? */

    priv = malloc(sizeof(*priv));
    if (!priv) {
        free(regs);
        priv = NULL;
    } else {
        priv->regs = regs;
    }
    return priv;
}

void *bios_find_partition(uint8_t diskno, uint8_t partno)
{
    uint8_t disk_devno;
    struct part_iter *iter = NULL;
    struct disk_info diskinfo;

    dprintf("%s: diskno %d partition %d\n", __func__, diskno, partno);

    disk_devno = DISK_ID_OFFSET + diskno;
    if (disk_get_params(disk_devno, &diskinfo))
        return NULL;
    if (!(iter = pi_begin(&diskinfo, 0)))
        return NULL;

    do {
        if (iter->index == partno)
            break;
    } while (!pi_next(iter));

    if (iter->status) {
        dprintf("MultiFS: Request disk/partition combination not found.\n");
        pi_del(&iter);
        return NULL;
    }
    dprintf("MultiFS: found 0x%llx at index: %i and partition %i\n",
            iter->abs_lba, iter->index, partno);
    return get_private(disk_devno, iter->abs_lba, diskinfo.head, diskinfo.spt);
}
