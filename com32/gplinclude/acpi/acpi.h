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

enum { ACPI_FOUND, ENO_ACPI, MADT_FOUND, ENO_MADT};

#define WORD(x) (uint16_t)(*(const uint16_t *)(x))
#define DWORD(x) (uint32_t)(*(const uint32_t *)(x))
#define QWORD(x) (*(const uint64_t *)(x))

typedef struct {
    int8_t signature[4+1];
    int32_t length;
    int8_t  revision;
    int8_t  checksum;
    int8_t  oem_id[6+1];
    int8_t  oem_table_id[8+1];
    int32_t oem_revision;
    int8_t  creator_id[4+1];
    int32_t creator_revision;
    uint32_t local_apic_address;
} s_madt;

typedef struct {
    s_madt madt;
    uint64_t base_address;
    uint64_t size;
    bool madt_valid;
    bool acpi_valid;
} s_acpi;

int search_acpi(s_acpi *acpi);
int search_madt(s_acpi *acpi);
void print_madt(s_acpi *acpi);
#endif
