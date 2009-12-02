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

#ifndef ACPI_H
#define ACPI_H
#include <inttypes.h>
#include <stdbool.h>
#include <acpi/structs.h>
#include <acpi/rsdp.h>
#include <acpi/rsdt.h>
#include <acpi/madt.h>
#include <acpi/xsdt.h>

enum { ACPI_FOUND, ENO_ACPI, MADT_FOUND, ENO_MADT };

/* This macro are used to extract ACPI structures 
 * please be careful about the q (interator) naming */
#define cp_struct(dest) memcpy(dest,q,sizeof(*dest)); q+=sizeof(*dest)
#define cp_str_struct(dest) memcpy(dest,q,sizeof(dest)-1); dest[sizeof(dest)-1]=0;q+=sizeof(dest)-1

typedef struct {
    s_rsdp rsdp;
    s_rsdt rsdt;
    s_xsdt xsdt;
    s_madt madt;
} s_acpi;

int parse_acpi(s_acpi * acpi);
int search_madt(s_acpi * acpi);
int search_rsdp(s_acpi * acpi);
int parse_rsdt(s_acpi * acpi);
int parse_xsdt(s_acpi * acpi);
void print_madt(s_acpi * acpi);
void get_acpi_description_header(uint8_t *q, s_acpi_description_header * adh);
#endif
