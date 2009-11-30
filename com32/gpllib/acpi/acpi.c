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

void init_acpi(s_acpi * acpi)
{
    memset(acpi, 0, sizeof(s_acpi));
}

int search_acpi(s_acpi * acpi)
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
	    acpi->base_address = nm[i].addr;
	    acpi->size = nm[i].size;
	    acpi->acpi_valid = true;
	    return ACPI_FOUND;
	}
    }
    return -ENO_ACPI;
}
