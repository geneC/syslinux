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

int parse_xsdt(s_acpi * acpi)
{
    /* Let's seach for XSDT table */
    uint8_t *q;

    /* Let's start for the base address */
    q = (uint64_t *) acpi->xsdt.address;

    /* Searching for MADT with APIC signature */
    if (memcmp(q, "XSDT", 4) == 0) {
	s_xsdt *x = &acpi->xsdt;
	x->valid = true;
	get_acpi_description_header(q, &x->header);

	/* We now have a set of pointers to some tables */
	uint64_t *p = NULL;
	for (p = (uint64_t *) (x->address + ACPI_HEADER_SIZE);
	     p < (uint64_t *) (x->address + x->header.length); p++) {
	    s_acpi_description_header adh;
	    memset(&adh, 0, sizeof(adh));
	    x->entry[x->entry_count] = (uint64_t) * p;

	    /* Let's grab the pointed table header */
	    get_acpi_description_header((uint8_t *) * p, &adh);

	    /* Trying to determine the pointed table */
	    if (memcmp(adh.signature, "FACP", 4) == 0) {
		    s_fadt *f = &acpi->fadt;
		    /* This structure is valid, let's fill it */
		    f->valid=true;
		    f->address=*p;
		    memcpy(&f->header,&adh,sizeof(adh));
		    parse_fadt(f);
	    } else if (memcmp(adh.signature, "APIC", 4) == 0) {
		    s_madt *m = &acpi->madt;
		    /* This structure is valid, let's fill it */
		    m->valid=true;
		    m->address=*p;
		    memcpy(&m->header,&adh,sizeof(adh));
		    parse_madt(acpi);
	    } else if (memcmp(adh.signature, "DSDT", 4) == 0) {
		    s_dsdt *d = &acpi->dsdt;

		    /* This structure is valid, let's fill it */
		    d->valid=true;
		    d->address=*p;
		    memcpy(&d->header,&adh,sizeof(adh));

		    /* Searching how much definition blocks we must copy */
		    uint32_t definition_block_size=adh.length-ACPI_HEADER_SIZE;
		    if ((d->definition_block=malloc(definition_block_size)) != NULL) {
			    memcpy(d->definition_block,(uint64_t *)(d->address+ACPI_HEADER_SIZE),definition_block_size);
		    }
		    /* PSDT have to be considered as SSDT. Intel ACPI Spec @ 5.2.11.3 */
	    } else if ((memcmp(adh.signature, "SSDT", 4) == 0) || (memcmp(adh.signature, "PSDT", 4))) {
		    if ((acpi->ssdt_count >= MAX_SSDT-1)) break;

		    /* We can have many SSDT, so let's allocate a new one */
		    if ((acpi->ssdt[acpi->ssdt_count]=malloc(sizeof(s_ssdt))) == NULL) break;	
		    s_ssdt *s = acpi->ssdt[acpi->ssdt_count];

		    /* This structure is valid, let's fill it */
		    s->valid=true;
		    s->address=*p;
		    memcpy(&s->header,&adh,sizeof(adh));
		    
		    /* Searching how much definition blocks we must copy */
		    uint32_t definition_block_size=adh.length-ACPI_HEADER_SIZE;
		    if ((s->definition_block=malloc(definition_block_size)) != NULL) {
			    memcpy(s->definition_block,(uint64_t *)(s->address+ACPI_HEADER_SIZE),definition_block_size);
		    }
		    /* Increment the number of ssdt we have */
		    acpi->ssdt_count++;
	    }

	    x->entry_count++;
	}
	return XSDT_TABLE_FOUND;
    }

    return -XSDT_TABLE_FOUND;
}
