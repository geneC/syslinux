/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef DMI_MEMORY_H
#define DMI_MEMORY_H

#define MEMORY_MANUFACTURER_SIZE	32
#define MEMORY_ERROR_SIZE		16
#define MEMORY_TOTAL_WIDTH_SIZE		16
#define MEMORY_DATA_WIDTH_SIZE		16
#define MEMORY_SIZE_SIZE		32
#define MEMORY_FORM_FACTOR_SIZE		32
#define MEMORY_DEVICE_SET_SIZE		32
#define MEMORY_DEVICE_LOCATOR_SIZE	32
#define MEMORY_BANK_LOCATOR_SIZE	32
#define MEMORY_TYPE_SIZE		32
#define MEMORY_TYPE_DETAIL_SIZE		16
#define MEMORY_SPEED_SIZE		16
#define MEMORY_SERIAL_SIZE		16
#define MEMORY_ASSET_TAG_SIZE		16
#define MEMORY_PART_NUMBER_SIZE		16

typedef struct {
char manufacturer[MEMORY_MANUFACTURER_SIZE];
char error[MEMORY_ERROR_SIZE];
char total_width[MEMORY_TOTAL_WIDTH_SIZE];
char data_width[MEMORY_DATA_WIDTH_SIZE];
char size[MEMORY_SIZE_SIZE];
char form_factor[MEMORY_FORM_FACTOR_SIZE];
char device_set[MEMORY_DEVICE_SET_SIZE];
char device_locator[MEMORY_DEVICE_LOCATOR_SIZE];
char bank_locator[MEMORY_BANK_LOCATOR_SIZE];
char type[MEMORY_TYPE_SIZE];
char type_detail[MEMORY_TYPE_DETAIL_SIZE];
char speed[MEMORY_SPEED_SIZE];
char serial[MEMORY_SERIAL_SIZE];
char asset_tag[MEMORY_ASSET_TAG_SIZE];
char part_number[MEMORY_PART_NUMBER_SIZE];
/* The filled field have to be set to true when the dmitable implement that item */
bool filled;
} s_memory;

void dmi_memory_array_error_handle(uint16_t code,char *array);
void dmi_memory_device_width(uint16_t code, char *width);
void dmi_memory_device_size(uint16_t code, char *size);
const char *dmi_memory_device_form_factor(uint8_t code);
void dmi_memory_device_set(uint8_t code, char *set);
const char *dmi_memory_device_type(uint8_t code);
void dmi_memory_device_type_detail(uint16_t code,char *type_detail);
void dmi_memory_device_speed(uint16_t code, char *speed);

#endif
