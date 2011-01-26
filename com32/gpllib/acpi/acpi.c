/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009-2011 Erwan Velu - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * -----------------------------------------------------------------------
*/

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "acpi/acpi.h"

void dbg_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void init_acpi(s_acpi * acpi)
{
    memset(acpi, 0, sizeof(s_acpi));
}

int parse_acpi(s_acpi * acpi)
{
    int ret_val;
    init_acpi(acpi);

    /* Let's seach for RSDP table */
    if ((ret_val = search_rsdp(acpi)) != RSDP_TABLE_FOUND)
	return ret_val;

    /* Let's seach for RSDT table 
     * That's not a big deal not having it, XSDT is far more relevant */
    parse_rsdt(&acpi->rsdt);
    if ((ret_val = parse_xsdt(acpi)) != XSDT_TABLE_FOUND)
	return ret_val;

    return ACPI_FOUND;
}

void get_acpi_description_header(uint8_t * q, s_acpi_description_header * adh)
{
    cp_str_struct(adh->signature);
    cp_struct(&adh->length);
    cp_struct(&adh->revision);
    cp_struct(&adh->checksum);
    cp_str_struct(adh->oem_id);
    cp_str_struct(adh->oem_table_id);
    cp_struct(&adh->oem_revision);
    cp_str_struct(adh->creator_id);
    cp_struct(&adh->creator_revision);
    DEBUG_PRINT(("acpi_header at %p = %s | %s | %s\n", q, adh->signature,adh->oem_id, adh->creator_id ));
}
