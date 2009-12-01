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

#ifndef MADT_H
#define MADT_H
#include <inttypes.h>
#include <stdbool.h>

enum {
    PROCESSOR_LOCAL_APIC = 0,
    IO_APIC = 1,
    INTERRUPT_SOURCE_OVERRIDE = 2,
    NMI = 3,
    LOCAL_APIC_NMI_STRUCTURE = 4,
    LOCAL_APIC_ADDRESS_OVERRIDE_STRUCTURE = 5,
    IO_SAPIC = 6,
    LOCAL_SAPIC = 7,
    PLATEFORM_INTERRUPT_SOURCES = 8
};

#define MAX_SLP 255

typedef struct {
    uint8_t length;
    uint8_t acpi_id;
    uint8_t apic_id;
    uint32_t flags;
} s_processor_local_apic;

typedef struct {
    uint32_t address;
    uint8_t signature[4 + 1];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6 + 1];
    uint8_t oem_table_id[8 + 1];
    uint32_t oem_revision;
    uint8_t creator_id[4 + 1];
    uint32_t creator_revision;
    uint32_t local_apic_address;
    uint32_t flags;
    s_processor_local_apic processor_local_apic[MAX_SLP];
    uint8_t processor_local_apic_count;
    bool valid;
} s_madt;

#endif
