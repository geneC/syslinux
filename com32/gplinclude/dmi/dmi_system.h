/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006 Erwan Velu - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef DMI_SYSTEM_H
#define DMI_SYSTEM_H

#define SYSTEM_MANUFACTURER_SIZE	32
#define SYSTEM_PRODUCT_NAME_SIZE	32
#define SYSTEM_VERSION_SIZE		16
#define SYSTEM_SERIAL_SIZE		32
#define SYSTEM_UUID_SIZE		40
#define SYSTEM_WAKEUP_TYPE_SIZE		32
#define SYSTEM_SKU_NUMBER_SIZE		32
#define SYSTEM_FAMILY_SIZE		32

typedef struct {
char manufacturer[SYSTEM_MANUFACTURER_SIZE];
char product_name[SYSTEM_PRODUCT_NAME_SIZE];
char version[SYSTEM_VERSION_SIZE];
char serial[SYSTEM_SERIAL_SIZE];
char uuid[SYSTEM_UUID_SIZE];
char wakeup_type[SYSTEM_WAKEUP_TYPE_SIZE];
char sku_number[SYSTEM_SKU_NUMBER_SIZE];
char family[SYSTEM_FAMILY_SIZE];
/* The filled field have to be set to true when the dmitable implement that item */
bool filled;
} s_system;

#endif
