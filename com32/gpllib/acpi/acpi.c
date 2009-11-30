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

void init_acpi(s_acpi *acpi)
{
   acpi->acpi_valid=false;
   acpi->madt_valid=false;
   memset(acpi->madt.oem_id,0,sizeof(acpi->madt.oem_id));
   memset(acpi->madt.oem_table_id,0,sizeof(acpi->madt.oem_table_id));
   memset(acpi->madt.oem_revision,0,sizeof(acpi->madt.oem_revision));
   memset(acpi->madt.creator_id,0,sizeof(acpi->madt.creator_id));
   memset(acpi->madt.creator_revision,0,sizeof(acpi->madt.creator_revision));
}

int search_acpi(s_acpi *acpi)
{
    init_acpi(acpi);
    struct e820entry map[E820MAX];
    struct e820entry nm[E820MAX];
    int count = 0;

    detect_memory_e820(map, E820MAX, &count);
    /* Clean up, adjust and copy the BIOS-supplied E820-map. */
    int nr = sanitize_e820_map(map, nm, count);
    for (int i = 0; i < nr; i++) {
	/* Type is ACPI Reclaim */
	if (nm[i].type == E820_ACPI) {
		printf("ACPI Table Found\n");
		acpi->base_address=nm[i].addr;
		acpi->size=nm[i].size;
		acpi->acpi_valid=true;
		return ACPI_FOUND;
	}
    }
   return -ENO_ACPI;
}

#define cp_struct(dest,quantity) memcpy(dest,q,quantity); q+=quantity
#define cp_str_struct(dest,quantity) memcpy(dest,q,quantity); dest[quantity]=0;q+=quantity
int search_madt(s_acpi *acpi)
{
    uint8_t *p, *q;
    if (!acpi->acpi_valid) return -ENO_ACPI;

    p = (uint64_t *) acpi->base_address;	/* The start address to look at the dmi table */
    /* The anchor-string is 16-bytes aligned */
    for (q = p; q < p + acpi->size; q += 1) {
	/* To validate the presence of SMBIOS:
	 * + the overall checksum must be correct
	 * + the intermediate anchor-string must be _DMI_
	 * + the intermediate checksum must be correct
	 */
	if (memcmp(q, "APIC", 4) == 0) {
	    /* Do not return, legacy_decode will need to be called
	     * on the intermediate structure to get the table length
	     * and address
	     */
	     s_madt *m = &acpi->madt;
	     cp_str_struct(m->signature,4);
	     cp_struct(&m->length,4);
	     cp_struct(&m->revision,1);
	     cp_struct(&m->checksum,1);
	     cp_str_struct(m->oem_id,6);
	     cp_str_struct(m->oem_table_id,8);
	     cp_struct(&m->oem_revision,4);
	     cp_str_struct(m->creator_id,4);
	     cp_struct(&m->creator_revision,4);
	     cp_struct(&m->local_apic_address,4);
	     acpi->madt_valid=true;
	return MADT_FOUND;
	}
    }
   return -ENO_MADT;
}

void print_madt(s_acpi *acpi)
{
   if (!acpi->madt_valid) return;
   printf("MADT\n");
   printf(" signature      : %s\n",acpi->madt.signature);
   printf(" length         : %d\n",acpi->madt.length);
   printf(" revision       : %d\n",acpi->madt.revision);
   printf(" checksum       : %d\n",acpi->madt.checksum);
   printf(" oem id         : %s\n",acpi->madt.oem_id);
   printf(" oem table id   : %s\n",acpi->madt.oem_table_id);
   printf(" oem revision   : %d\n",acpi->madt.oem_revision);
   printf(" oem creator id : %s\n",acpi->madt.creator_id);
   printf(" oem creator rev: %d\n",acpi->madt.creator_revision);
}
