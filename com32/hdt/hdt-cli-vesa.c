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

void main_show_vesa(int argc __unused, char **argv __unused,
		    struct s_hardware *hardware)
{
	detect_vesa(hardware);
	if (hardware->is_vesa_valid == false) {
		printf("No VESA BIOS detected\n");
		return;
	}
	printf("VESA\n");
	printf(" Vesa version : %d.%d\n", hardware->vesa.major_version,
		    hardware->vesa.minor_version);
	printf(" Vendor       : %s\n", hardware->vesa.vendor);
	printf(" Product      : %s\n", hardware->vesa.product);
	printf(" Product rev. : %s\n", hardware->vesa.product_revision);
	printf(" Software rev.: %d\n", hardware->vesa.software_rev);
	printf(" Memory (KB)  : %d\n", hardware->vesa.total_memory * 64);
	printf(" Modes        : %d\n", hardware->vesa.vmi_count);
}

static void show_vesa_modes(int argc __unused, char **argv __unused,
			    struct s_hardware *hardware)
{
	detect_vesa(hardware);
	if (hardware->is_vesa_valid == false) {
		printf("No VESA BIOS detected\n");
		return;
	}
	reset_more_printf();
	printf(" ResH. x ResV x Bits : vga= : Vesa Mode\n");
	printf("----------------------------------------\n");

	for (int i = 0; i < hardware->vesa.vmi_count; i++) {
		struct vesa_mode_info *mi = &hardware->vesa.vmi[i].mi;
		/*
		 * Sometimes, vesa bios reports 0x0 modes.
		 * We don't need to display that ones.
		 */
		if ((mi->h_res == 0) || (mi->v_res == 0)) continue;
		printf("%5u %5u    %3u     %3d     0x%04x\n",
			    mi->h_res, mi->v_res, mi->bpp,
			    hardware->vesa.vmi[i].mode + 0x200,
			    hardware->vesa.vmi[i].mode);
	}
}

struct cli_callback_descr list_vesa_show_modules[] = {
	{
	 .name = CLI_MODES,
	 .exec = show_vesa_modes,
	 },
	{
	 .name = NULL,
	 .exec = NULL,
	 },
};

struct cli_module_descr vesa_show_modules = {
	.modules = list_vesa_show_modules,
	.default_callback = main_show_vesa,
};

struct cli_mode_descr vesa_mode = {
	.mode = VESA_MODE,
	.name = CLI_VESA,
	.default_modules = NULL,
	.show_modules = &vesa_show_modules,
	.set_modules = NULL,
};
