/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer - All Rights Reserved
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

#include <disk/geom.h>

#include "hdt-cli.h"
#include "hdt-common.h"

void main_show_disk(int argc __unused, char **argv __unused,
		    struct s_hardware *hardware)
{
	int i = -1;

	detect_disks(hardware);

	for (int drive = 0x80; drive < 0xff; drive++) {
		i++;
		if (!hardware->disk_info[i].cbios)
			continue; /* Invalid geometry */
		struct driveinfo *d = &hardware->disk_info[i];
		more_printf
		    ("DISK 0x%X:\n\ts/t=%d, sectors=%d, cylinders=%d, heads=%d, b/s=%d\n"
		     "\tBus type: %s, Interface type: %s\n\tEDD=%X (ebios=%d, cbios=%d)\n",
		     d->disk, d->sectors_per_track, d->sectors, d->cylinder, d->heads,
		     d->bytes_per_sector, d->host_bus_type, d->interface_type,
		     d->edd_version, d->ebios, d->cbios);
	}
}

struct cli_module_descr disk_show_modules = {
	.modules = NULL,
	.default_callback = main_show_disk,
};

struct cli_mode_descr disk_mode = {
	.mode = DISK_MODE,
	.name = CLI_DISK,
	.default_modules = NULL,
	.show_modules = &disk_show_modules,
	.set_modules = NULL,
};
