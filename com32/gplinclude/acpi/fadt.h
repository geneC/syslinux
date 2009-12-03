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

#ifndef FADT_H
#define FADT_H
#include <inttypes.h>
#include <stdbool.h>

enum { FADT_TABLE_FOUND };

#define FACP "FACP"
#define FADT "FADT"

typedef struct {
    uint64_t address;
    s_acpi_description_header header;
    bool valid;
    uint32_t firmware_ctrl;
    uint32_t dsdt_address;
    /* To be filled later */
} s_fadt;

void parse_fadt(s_fadt * fadt);
#endif
