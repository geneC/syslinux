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
#include <stdlib.h>
#include <errno.h>

#include "hdt-cli.h"
#include "hdt-common.h"

void main_show_cpu(int argc __unused, char **argv __unused,
		   struct s_hardware *hardware)
{
	cpu_detect(hardware);
	detect_dmi(hardware);
	more_printf("CPU\n");
	more_printf(" Manufacturer : %s \n", hardware->cpu.vendor);
	more_printf(" Product      : %s \n", del_multi_spaces(hardware->cpu.model));
	if ((hardware->cpu.flags.lm == false)
	    && (hardware->cpu.flags.smp == false)) {
		more_printf(" Features     : %d MhZ : x86 32bits\n",
			    hardware->dmi.processor.current_speed);
	} else if ((hardware->cpu.flags.lm == false)
		   && (hardware->cpu.flags.smp == true)) {
		more_printf(" Features     : %d MhZ : x86 32bits SMP\n",
			    hardware->dmi.processor.current_speed);
	} else if ((hardware->cpu.flags.lm == true)
		   && (hardware->cpu.flags.smp == false)) {
		more_printf(" Features     : %d MhZ : x86_64 64bits\n",
			    hardware->dmi.processor.current_speed);
	} else {
		more_printf(" Features     : %d MhZ : x86_64 64bits SMP\n",
			    hardware->dmi.processor.current_speed);
	}
}

static void show_cpu(int argc __unused, char **argv __unused,
		     struct s_hardware *hardware)
{
	char buffer[81];
	char buffer1[81];
	reset_more_printf();
	more_printf("CPU\n");
	more_printf("Vendor    : %s\n", hardware->cpu.vendor);
	more_printf("Model     : %s\n", hardware->cpu.model);
	more_printf("Vendor ID : %d\n", hardware->cpu.vendor_id);
	more_printf("Family ID : %d\n", hardware->cpu.family);
	more_printf("Model  ID : %d\n", hardware->cpu.model_id);
	more_printf("Stepping  : %d\n", hardware->cpu.stepping);
	more_printf("FSB       : %d MHz\n",
		    hardware->dmi.processor.external_clock);
	more_printf("Cur. Speed: %d MHz\n",
		    hardware->dmi.processor.current_speed);
	more_printf("Max Speed : %d MHz\n", hardware->dmi.processor.max_speed);
	more_printf("Upgrade   : %s\n", hardware->dmi.processor.upgrade);
	if (hardware->cpu.flags.smp) {
		more_printf("SMP       : yes\n");
	} else {
		more_printf("SMP       : no\n");
	}
	if (hardware->cpu.flags.lm) {
		more_printf("x86_64    : yes\n");
	} else {
		more_printf("x86_64    : no\n");
	}

	memset(buffer, 0, sizeof(buffer));
	memset(buffer1, 0, sizeof(buffer1));
	if (hardware->cpu.flags.fpu)
		strcat(buffer1, "fpu ");
	if (hardware->cpu.flags.vme)
		strcat(buffer1, "vme ");
	if (hardware->cpu.flags.de)
		strcat(buffer1, "de ");
	if (hardware->cpu.flags.pse)
		strcat(buffer1, "pse ");
	if (hardware->cpu.flags.tsc)
		strcat(buffer1, "tsc ");
	if (hardware->cpu.flags.msr)
		strcat(buffer1, "msr ");
	if (hardware->cpu.flags.pae)
		strcat(buffer1, "pae ");
	if (hardware->cpu.flags.mce)
		strcat(buffer1, "mce ");
	if (hardware->cpu.flags.cx8)
		strcat(buffer1, "cx8 ");
	if (hardware->cpu.flags.apic)
		strcat(buffer1, "apic ");
	if (hardware->cpu.flags.sep)
		strcat(buffer1, "sep ");
	if (hardware->cpu.flags.mtrr)
		strcat(buffer1, "mtrr ");
	if (hardware->cpu.flags.pge)
		strcat(buffer1, "pge ");
	if (hardware->cpu.flags.mca)
		strcat(buffer1, "mca ");
	if (buffer1[0]) {
		snprintf(buffer, sizeof buffer, "Flags     : %s\n", buffer1);
		more_printf(buffer);
	}

	memset(buffer, 0, sizeof(buffer));
	memset(buffer1, 0, sizeof(buffer1));
	if (hardware->cpu.flags.cmov)
		strcat(buffer1, "cmov ");
	if (hardware->cpu.flags.pat)
		strcat(buffer1, "pat ");
	if (hardware->cpu.flags.pse_36)
		strcat(buffer1, "pse_36 ");
	if (hardware->cpu.flags.psn)
		strcat(buffer1, "psn ");
	if (hardware->cpu.flags.clflsh)
		strcat(buffer1, "clflsh ");
	if (hardware->cpu.flags.dts)
		strcat(buffer1, "dts ");
	if (hardware->cpu.flags.acpi)
		strcat(buffer1, "acpi ");
	if (hardware->cpu.flags.mmx)
		strcat(buffer1, "mmx ");
	if (hardware->cpu.flags.sse)
		strcat(buffer1, "sse ");
	if (hardware->cpu.flags.sse2)
		strcat(buffer1, "sse2 ");
	if (hardware->cpu.flags.ss)
		strcat(buffer1, "ss ");
	if (buffer1[0]) {
		snprintf(buffer, sizeof buffer, "Flags     : %s\n", buffer1);
		more_printf(buffer);
	}

	memset(buffer, 0, sizeof(buffer));
	memset(buffer1, 0, sizeof(buffer1));
	if (hardware->cpu.flags.htt)
		strcat(buffer1, "ht ");
	if (hardware->cpu.flags.acc)
		strcat(buffer1, "acc ");
	if (hardware->cpu.flags.syscall)
		strcat(buffer1, "syscall ");
	if (hardware->cpu.flags.mp)
		strcat(buffer1, "mp ");
	if (hardware->cpu.flags.nx)
		strcat(buffer1, "nx ");
	if (hardware->cpu.flags.mmxext)
		strcat(buffer1, "mmxext ");
	if (hardware->cpu.flags.lm)
		strcat(buffer1, "lm ");
	if (hardware->cpu.flags.nowext)
		strcat(buffer1, "3dnowext ");
	if (hardware->cpu.flags.now)
		strcat(buffer1, "3dnow! ");
	if (buffer1[0]) {
		snprintf(buffer, sizeof buffer, "Flags     : %s\n", buffer1);
		more_printf(buffer);
	}
}

struct cli_module_descr cpu_show_modules = {
	.modules = NULL,
	.default_callback = show_cpu,
};

struct cli_mode_descr cpu_mode = {
	.mode = CPU_MODE,
	.name = CLI_CPU,
	.default_modules = NULL,
	.show_modules = &cpu_show_modules,
	.set_modules = NULL,
};
