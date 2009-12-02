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
    s_madt *madt = &acpi->madt;
    switch (type) {
    case PROCESSOR_LOCAL_APIC:
	sla=&madt->processor_local_apic[madt->processor_local_apic_count];
	sla->length = length;
	sla->acpi_id = *q;
	q++;
	sla->apic_id = *q;
	q++;
	memcpy(&sla->flags, q, 4);
	q += 4;
#ifdef DEBUG 
	print_local_apic_structure(sla, madt->processor_local_apic_count);
#endif
	madt->processor_local_apic_count++;
	break;
    default:
	dprintf("APIC structure type %u, size=%u \n", type, length);
	q += length - 2;
	break;
    }
    return q;
}

int parse_madt(s_acpi *acpi)
{
    /* Let's seach for FADT table */
    uint8_t *q;
    s_madt *m = &acpi->madt;

    /* Fixing table name */
    strcpy(m->header.signature,"MADT");

    /* Copying remaining structs */
    q = (uint64_t *) (m->address+ACPI_HEADER_SIZE);
    while (q < (m->address + m->header.length)) {
	q=add_apic_structure(acpi, q);
    }
}

void print_madt(s_madt * madt)
{
    if (!madt->valid)
	return;
    printf("MADT Table @ 0x%08x\n",madt->address);
    printf(" signature      : %s\n", madt->header.signature);
    printf(" length         : %d\n", madt->header.length);
    printf(" revision       : %u\n", madt->header.revision);
    printf(" checksum       : %u\n", madt->header.checksum);
    printf(" oem id         : %s\n", madt->header.oem_id);
    printf(" oem table id   : %s\n", madt->header.oem_table_id);
    printf(" oem revision   : %u\n", madt->header.oem_revision);
    printf(" oem creator id : %s\n", madt->header.creator_id);
    printf(" oem creator rev: %u\n", madt->header.creator_revision);
    printf(" APIC address   : 0x%08x\n", madt->local_apic_address);
}
