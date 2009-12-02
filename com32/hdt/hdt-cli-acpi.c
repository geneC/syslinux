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

#include "hdt-cli.h"
#include "hdt-common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void show_header_32(uint32_t address, s_acpi_description_header * h)
{
    more_printf("%-4s v%03x %-6s %-7s 0x%08x %-4s    0x%08x @ 0x%016x\n",
		h->signature, h->revision, h->oem_id, h->oem_table_id,
		h->oem_revision, h->creator_id, h->creator_revision, address)
}

static void show_header(uint32_t address, s_acpi_description_header * h)
{
    more_printf("%-4s v%03x %-6s %-7s 0x%08x %-4s    0x%08x @ 0x%016lx\n",
		h->signature, h->revision, h->oem_id, h->oem_table_id,
		h->oem_revision, h->creator_id, h->creator_revision, address)
}
void main_show_acpi(int argc __unused, char **argv __unused,
		    struct s_hardware *hardware)
{
    reset_more_printf();
    detect_acpi(hardware);
    if (hardware->is_acpi_valid == false) {
	more_printf("No ACPI Tables detected\n");
	return;
    }
    more_printf
	("ACPI rev  oem    table_id oem_rev    creator creat_rev  @ address \n");
    more_printf
	("----|----|------|--------|----------|-------|-----------|--------------------\n");
    if (hardware->acpi.rsdp.valid) {
	s_rsdp *r = &hardware->acpi.rsdp;
	more_printf
	    ("RSDP v%03x %-6s                                        @ 0x%016llx\n",
	     r->revision, r->oem_id, r->address);
    }
    if (hardware->acpi.rsdt.valid)
	show_header_32(hardware->acpi.rsdt.address, &hardware->acpi.rsdt.header);

    if (hardware->acpi.xsdt.valid)
	show_header_32(hardware->acpi.xsdt.address, &hardware->acpi.xsdt.header);
 
    if (hardware->acpi.fadt.valid)
	show_header(hardware->acpi.fadt.address, &hardware->acpi.fadt.header);

    if (hardware->acpi.madt.valid)
	show_header(hardware->acpi.madt.address, &hardware->acpi.madt.header);

    if (hardware->acpi.dsdt.valid)
	show_header(hardware->acpi.dsdt.address, &hardware->acpi.dsdt.header);

    for (int i=0;i<hardware->acpi.ssdt_count;i++) {
    	if ((hardware->acpi.ssdt[i] != NULL) && (hardware->acpi.ssdt[i]->valid))
		show_header(hardware->acpi.ssdt[i]->address, &hardware->acpi.ssdt[i]->header);
    }

}

struct cli_module_descr acpi_show_modules = {
    .modules = NULL,
    .default_callback = main_show_acpi,
};

struct cli_mode_descr acpi_mode = {
    .mode = ACPI_MODE,
    .name = CLI_ACPI,
    .default_modules = NULL,
    .show_modules = &acpi_show_modules,
    .set_modules = NULL,
};
