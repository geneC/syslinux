/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
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
#include <dprintf.h>
#include "acpi/acpi.h"

int search_rsdp(s_acpi * acpi)
{
    /* Let's seach for RSDT table */
    uint8_t *p, *q;

    /* Let's start for the base address */
    p = (uint64_t *) RSDP_MIN_ADDRESS;
    for (q = p; q < RSDP_MAX_ADDRESS; q += 16) {
	/* Searching for MADT with APIC signature */
	if (memcmp(q, "RSD PTR", 7) == 0) {
	    s_rsdp *r = &acpi->rsdp;
	    r->valid = true;
	    r->address = (uint64_t) q;
	    cp_str_struct(r->signature);
	    cp_struct(&r->checksum);
	    cp_str_struct(r->oem_id);
	    cp_struct(&r->revision);
	    cp_struct(&r->rsdt_address);
	    cp_struct(&r->length);
	    cp_struct(&r->xsdt_address);
	    cp_struct(&r->extended_checksum);
	    q += 3;		/* reserved field */
	    return RSDP_TABLE_FOUND;
	}
    }
    return -RSDP_TABLE_FOUND;
}

void print_rsdp(s_acpi * acpi)
{
    s_rsdp *r = &acpi->rsdp;

    if (!r->valid)
	return;
    printf("RSDP Table @ 0x%016llx\n", r->address);
    printf(" signature         : %s\n", r->signature);
    printf(" checksum          : %u\n", r->checksum);
    printf(" oem id            : %s\n", r->oem_id);
    printf(" revision          : %u\n", r->revision);
    printf(" RDST address      : 0x%08x\n", r->rsdt_address);
    printf(" length            : %u\n", r->length);
    printf(" XSDT address      : 0x%08x\n", r->xsdt_address);
    printf(" extended checksum : %u\n", r->extended_checksum);
}
