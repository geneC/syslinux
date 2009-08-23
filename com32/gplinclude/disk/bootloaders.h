/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#ifndef __BOOTLOADERS_H_
#define __BOOTLOADERS_H_

#include <stdint.h>

void get_bootloader_string(const uint16_t, void *, const int);
uint16_t get_bootloader_id(const struct driveinfo *);
#endif /* _BOOTLOADERS_H_ */
