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
#include <stdlib.h>
#include "acpi/acpi.h"

int parse_xsdt(s_acpi * acpi)
{
    /* Let's seach for XSDT table */
    uint8_t *q;

    /* Let's start for the base address */
    q = acpi->xsdt.address;

    /* Searching for MADT with APIC signature */
    if (memcmp(q, XSDT, sizeof(XSDT) - 1) == 0) {
	DEBUG_PRINT(("XSDT table found\n"));
	s_xsdt *x = &acpi->xsdt;
	x->valid = true;
	get_acpi_description_header(q, &x->header);

	/* We now have a set of pointers to some tables */
	uint8_t *p = NULL;
	for (p = (x->address + ACPI_HEADER_SIZE);
	     p < (x->address + x->header.length); p++) {
	    s_acpi_description_header adh;
	    memset(&adh, 0, sizeof(adh));
	    x->entry[x->entry_count] = p;

	    /* Let's grab the pointed table header */
	    get_acpi_description_header(p, &adh);

	    /* Trying to determine the pointed table */
	    /* Looking for FADT */
	    if (memcmp(adh.signature, FACP, sizeof(FACP) - 1) == 0) {
		DEBUG_PRINT(("FACP table found\n"));
		s_fadt *f = &acpi->fadt;
		s_facs *fa = &acpi->facs;
		s_dsdt *d = &acpi->dsdt;
		/* This structure is valid, let's fill it */
		f->valid = true;
		f->address = p;
		memcpy(&f->header, &adh, sizeof(adh));
		parse_fadt(f);

		/* FACS wasn't already detected
		 * FADT points to it, let's try to detect it */
		if (fa->valid == false) {
		    fa->address = f->x_firmware_ctrl;
		    parse_facs(fa);
		    if (fa->valid == false) {
			/* Let's try again */
			fa->address = f->firmware_ctrl;
			parse_facs(fa);
		    }
		}

		/* DSDT wasn't already detected
		 * FADT points to it, let's try to detect it */
		if (d->valid == false) {
		    s_acpi_description_header new_adh;
		    get_acpi_description_header(f->x_dsdt,
						&new_adh);
		    if (memcmp(new_adh.signature, DSDT, sizeof(DSDT) - 1) == 0) {
			DEBUG_PRINT(("DSDT table found\n"));
			d->valid = true;
			d->address = f->x_dsdt;
			memcpy(&d->header, &new_adh, sizeof(new_adh));
			parse_dsdt(d);
		    } else {
			/* Let's try again */
			get_acpi_description_header(f->dsdt_address,
						    &new_adh);
			if (memcmp(new_adh.signature, DSDT, sizeof(DSDT) - 1) ==
			    0) {
			    d->valid = true;
			    d->address = f->dsdt_address;
			    memcpy(&d->header, &new_adh, sizeof(new_adh));
			    parse_dsdt(d);
			}
		    }
		}
	    } /* Looking for MADT */
	    else if (memcmp(adh.signature, APIC, sizeof(APIC) - 1) == 0) {
		DEBUG_PRINT(("MADT table found\n"));
		s_madt *m = &acpi->madt;
		/* This structure is valid, let's fill it */
		m->valid = true;
		m->address = p;
		memcpy(&m->header, &adh, sizeof(adh));
		parse_madt(acpi);
	    } else if (memcmp(adh.signature, DSDT, sizeof(DSDT) - 1) == 0) {
		DEBUG_PRINT(("DSDT table found\n"));
		s_dsdt *d = &acpi->dsdt;
		/* This structure is valid, let's fill it */
		d->valid = true;
		d->address = p;
		memcpy(&d->header, &adh, sizeof(adh));
		parse_dsdt(d);
		/* PSDT have to be considered as SSDT. Intel ACPI Spec @ 5.2.11.3 */
	    } else if ((memcmp(adh.signature, SSDT, sizeof(SSDT) - 1) == 0)
		       || (memcmp(adh.signature, PSDT, sizeof(PSDT) - 1) == 0)) {
		
		DEBUG_PRINT(("SSDT table found with %s \n",adh.signature));

		if ((acpi->ssdt_count >= MAX_SSDT - 1))
		    break;

		/* We can have many SSDT, so let's allocate a new one */
		if ((acpi->ssdt[acpi->ssdt_count] =
		     malloc(sizeof(s_ssdt))) == NULL)
		    break;
		s_ssdt *s = acpi->ssdt[acpi->ssdt_count];

		/* This structure is valid, let's fill it */
		s->valid = true;
		s->address = p;
		memcpy(&s->header, &adh, sizeof(adh));

		/* Searching how much definition blocks we must copy */
		uint32_t definition_block_size = adh.length - ACPI_HEADER_SIZE;
		if ((s->definition_block =
		     malloc(definition_block_size)) != NULL) {
		    memcpy(s->definition_block,
			   (s->address + ACPI_HEADER_SIZE),
			   definition_block_size);
		}
		/* Increment the number of ssdt we have */
		acpi->ssdt_count++;
	    } else if (memcmp(adh.signature, SBST, sizeof(SBST) - 1) == 0) {
		DEBUG_PRINT(("SBST table found\n"));
		s_sbst *s = &acpi->sbst;
		/* This structure is valid, let's fill it */
		s->valid = true;
		s->address = p;
		memcpy(&s->header, &adh, sizeof(adh));
		parse_sbst(s);
	    } else if (memcmp(adh.signature, ECDT, sizeof(ECDT) - 1) == 0) {
		DEBUG_PRINT(("ECDT table found\n"));
		s_ecdt *e = &acpi->ecdt;
		/* This structure is valid, let's fill it */
		e->valid = true;
		e->address = p;
		memcpy(&e->header, &adh, sizeof(adh));
		parse_ecdt(e);
	    }
	    x->entry_count++;
	}
	return XSDT_TABLE_FOUND;
    }

    return -XSDT_TABLE_FOUND;
}
