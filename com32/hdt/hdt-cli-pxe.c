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

void main_show_pxe(struct s_hardware *hardware, struct s_cli_mode *cli_mode)
{
  char buffer[81];
  memset(buffer, 0, sizeof(81));
  if (hardware->sv->filesystem != SYSLINUX_FS_PXELINUX) {
    more_printf("You are not currently using PXELINUX\n");
    return;
  }

  detect_pxe(hardware);
  more_printf("PXE\n");
  if (hardware->is_pxe_valid == false) {
    more_printf(" No valid PXE ROM found\n");
    return;
  }

  struct s_pxe *p = &hardware->pxe;
  more_printf(" PCI device no: %d \n", p->pci_device_pos);

  if (hardware->pci_ids_return_code == -ENOPCIIDS) {
    snprintf(buffer, sizeof(buffer),
       " PCI ID       : %04x:%04x[%04x:%04X] rev(%02x)\n",
       p->vendor_id, p->product_id, p->subvendor_id,
       p->subproduct_id, p->rev);
    snprintf(buffer, sizeof(buffer),
       " PCI Bus pos. : %02x:%02x.%02x\n", p->pci_bus,
       p->pci_dev, p->pci_func);
    more_printf(buffer);
  } else {
    snprintf(buffer, sizeof(buffer), " Manufacturer : %s \n",
       p->pci_device->dev_info->vendor_name);
    more_printf(buffer);
    snprintf(buffer, sizeof(buffer), " Product      : %s \n",
       p->pci_device->dev_info->product_name);
    more_printf(buffer);
  }
  more_printf(" Addresses    : %d.%d.%d.%d @ %s\n", p->ip_addr[0],
        p->ip_addr[1], p->ip_addr[2], p->ip_addr[3], p->mac_addr);
}

static void show_pxe_help()
{
  more_printf("Show supports the following commands : %s\n",
        CLI_SHOW_LIST);
}

void pxe_show(char *item, struct s_hardware *hardware)
{
  if (!strncmp(item, CLI_SHOW_LIST, sizeof(CLI_SHOW_LIST) - 1)) {
    main_show_pxe(hardware, NULL);
    return;
  }
  show_pxe_help();
}

void handle_pxe_commands(char *cli_line, struct s_cli_mode *cli_mode,
       struct s_hardware *hardware)
{
  if (!strncmp(cli_line, CLI_SHOW, sizeof(CLI_SHOW) - 1)) {
    pxe_show(strstr(cli_line, "show") + sizeof(CLI_SHOW), hardware);
    return;
  }
}
