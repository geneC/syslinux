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

#ifndef ACPI_H
#define ACPI_H
#include <inttypes.h>
#include <stdbool.h>
#include <acpi/madt.h>

enum { ACPI_FOUND, ENO_ACPI, MADT_FOUND, ENO_MADT };

typedef struct {
    s_madt madt;
    uint64_t base_address;
    uint64_t size;
    bool madt_valid;
    bool acpi_valid;
} s_acpi;

int search_acpi(s_acpi * acpi);
int search_madt(s_acpi * acpi);
void print_madt(s_acpi * acpi);
#endif
