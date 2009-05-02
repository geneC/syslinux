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

#include <memory.h>

#include "hdt-cli.h"
#include "hdt-common.h"

#define E820MAX	128

static void show_memory_e820(int argc __unused, char **argv __unused,
			     struct s_hardware *hardware __unused)
{
	struct e820entry map[E820MAX];
	int count = 0;
	char type[14];

	detect_memory_e820(map, E820MAX, &count);
	printf("BIOS-provided physical RAM e820 map:\n");
	for (int i = 0; i < count; i++) {
		get_type(map[i].type, type, 14);
		more_printf("%016llx - %016llx %016llx (%s)\n",
			    map[i].addr, map[i].size, map[i].addr+map[i].size,
			    remove_spaces(type));
	}
}

struct cli_callback_descr list_memory_show_modules[] = {
	{
		.name = "e820",
		.exec = show_memory_e820,
	},
	{
		.name = NULL,
		.exec = NULL,
	},
};

struct cli_module_descr memory_show_modules = {
	.modules = list_memory_show_modules,
	.default_callback = NULL,
};

struct cli_mode_descr memory_mode = {
	.mode = MEMORY_MODE,
	.name = CLI_MEMORY,
	.default_modules = NULL,
	.show_modules = &memory_show_modules,
	.set_modules = NULL,
};
