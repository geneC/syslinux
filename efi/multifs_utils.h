/*
 * Copyright (C) 2012-2015 Paulo Alcantara <pcacjr@zytor.com>
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

#ifndef MULTIDISK_UTILS_H
#define MULTIDISK_UTILS_H

#include <fs.h>

#include "efi.h"

struct part_node {
    int partition;
    struct fs_info *fs;
    struct part_node *next;
};

struct queue_head {
    struct part_node *first;
    struct part_node *last;
};

/*
 * Needs to keep ROOT_FS_OPS after fs_init()
 * to be used by multidisk
 */
extern const struct fs_ops **p_ops;

/*
 * Used to initialize multifs support
 */
extern void enable_multifs(void *);
extern EFI_STATUS init_multifs(void);

#endif /* MULTIDISK_UTILS_H */
