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
#include <syslinux/pxe.h>
#include <syslinux/config.h>

#include "hdt-cli.h"
#include "hdt-common.h"

void main_show_syslinux(struct s_hardware *hardware)
{
  more_printf("SYSLINUX\n");
  more_printf(" Bootloader : %s\n", hardware->syslinux_fs);
  more_printf(" Version    : %s\n", hardware->sv->version_string + 2);
  more_printf(" Version    : %u\n", hardware->sv->version);
  more_printf(" Max API    : %u\n", hardware->sv->max_api);
  more_printf(" Copyright  : %s\n", hardware->sv->copyright_string + 1);
}

static void show_syslinux_help()
{
  more_printf("Show supports the following commands : %s\n",
              CLI_SHOW_LIST);
}

static void syslinux_show(char *item, struct s_hardware *hardware)
{
  if (!strncmp(item, CLI_SHOW_LIST, sizeof(CLI_SHOW_LIST) - 1)) {
    main_show_syslinux(hardware);
    return;
  }
  show_syslinux_help();
}

void handle_syslinux_commands(char *cli_line, struct s_hardware *hardware)
{
  if (!strncmp(cli_line, CLI_SHOW, sizeof(CLI_SHOW) - 1)) {
    syslinux_show(strstr(cli_line, "show") + sizeof(CLI_SHOW),
                  hardware);
    return;
  }
}
