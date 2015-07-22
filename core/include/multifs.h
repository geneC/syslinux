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

#ifndef MULTIFS_H
#define MULTIFS_H

#include "fs.h"

/*
 * MULTIFS SYNTAX:
 * (hd[disk number],[partition number])/path/to/file
 *
 * E.G.: (hd0,1)/dir/file means /dir/file at partition 1 of the disk 0.
 * Disk and Partition numbering starts from 0 and 1 respectivelly.
 */

struct multifs_ops {
    struct fs_info *(*get_fs_info)(const char **);
    void (*init)(void *);
};

extern struct fs_info *root_fs;
extern const struct fs_ops **p_ops;
extern const struct multifs_ops *multifs_ops;

struct fs_info *multifs_get_fs(uint8_t diskno, uint8_t partno);
int multifs_parse_path(const char **path, uint8_t *diskno, uint8_t *partno);
int multifs_setup_fs_info(struct fs_info *fsp, uint8_t diskno, uint8_t partno,
                          void *priv);
void multifs_restore_fs(void);
int multifs_switch_fs(const char **path);

#endif /* MULTIFS_H */
