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

#ifndef ACPI_STRUCTS_H
#define ACPI_STRUCTS_H
#include <inttypes.h>
#include <stdbool.h>

/* This value define the real size of the acpi structure 
 * Our is bigger as we manage the \0 of strings
 * */
#define ACPI_HEADER_SIZE 36

typedef struct {
    uint8_t signature[4 + 1];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6 + 1];
    uint8_t oem_table_id[8 + 1];
    uint32_t oem_revision;
    uint8_t creator_id[4 + 1];
    uint32_t creator_revision;
} s_acpi_description_header;

#endif
