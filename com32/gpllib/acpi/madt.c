/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006 Erwan Velu - All Rights Reserved
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

static void print_local_apic_structure(s_processor_local_apic * sla,
				       uint8_t index)
{
    printf("Local APIC Structure no%u (%03u bytes)\n", index, sla->length);
    printf(" ACPI_ID / APIC_ID = %03u / %03u\n", sla->acpi_id, sla->apic_id);
}

/* Parse the apic structures */
static uint8_t *add_apic_structure(s_acpi * acpi, uint8_t * q)
{
    uint8_t type = *q;
    q++;
    uint8_t length = *q;
    q++;
    s_processor_local_apic *sla;
    switch (type) {
    case PROCESSOR_LOCAL_APIC:
	sla =
	    &acpi->madt.processor_local_apic[acpi->madt.
					     processor_local_apic_count];
	sla->length = length;
	sla->acpi_id = *q;
	q++;
	sla->apic_id = *q;
	q++;
	memcpy(&sla->flags, q, 4);
	q += 4;
	print_local_apic_structure(sla, acpi->madt.processor_local_apic_count);
	acpi->madt.processor_local_apic_count++;
	break;
    default:
	printf("APIC structure type %u, size=%u \n", type, length);
	q += length - 2;
	break;
    }
    return q;
}

#define cp_struct(dest,quantity) memcpy(dest,q,quantity); q+=quantity
#define cp_str_struct(dest,quantity) memcpy(dest,q,quantity); dest[quantity]=0;q+=quantity
int search_madt(s_acpi * acpi)
{
    uint8_t *p, *q;
    if (!acpi->acpi_valid)
	return -ENO_ACPI;

    p = (uint8_t *) acpi->base_address;	/* The start address to look at the APIC table */
    for (q = p; q < p + acpi->size; q += 1) {
	uint8_t *save = q;
	/* Searching for MADT with APIC signature */
	if (memcmp(q, "APIC", 4) == 0) {
	    s_madt *m = &acpi->madt;
	    cp_str_struct(m->signature, 4);
	    cp_struct(&m->length, 4);
	    cp_struct(&m->revision, 1);
	    cp_struct(&m->checksum, 1);
	    cp_str_struct(m->oem_id, 6);
	    cp_str_struct(m->oem_table_id, 8);
	    cp_struct(&m->oem_revision, 4);
	    cp_str_struct(m->creator_id, 4);
	    cp_struct(&m->creator_revision, 4);
	    cp_struct(&m->local_apic_address, 4);
	    cp_struct(&m->flags, 4);

	    /* Let's parse APIC Structures */
	    while (q < (save + m->length)) {
		q = add_apic_structure(acpi, q);
	    }
	    acpi->madt_valid = true;
	    return MADT_FOUND;
	}
    }
    return -ENO_MADT;
}

void print_madt(s_acpi * acpi)
{
    if (!acpi->madt_valid)
	return;
    printf("MADT Table\n");
    printf(" signature      : %s\n", acpi->madt.signature);
    printf(" length         : %d\n", acpi->madt.length);
    printf(" revision       : %u\n", acpi->madt.revision);
    printf(" checksum       : %u\n", acpi->madt.checksum);
    printf(" oem id         : %s\n", acpi->madt.oem_id);
    printf(" oem table id   : %s\n", acpi->madt.oem_table_id);
    printf(" oem revision   : %u\n", acpi->madt.oem_revision);
    printf(" oem creator id : %s\n", acpi->madt.creator_id);
    printf(" oem creator rev: %u\n", acpi->madt.creator_revision);
    printf(" APIC address   : 0x%08x\n", acpi->madt.local_apic_address);
}
