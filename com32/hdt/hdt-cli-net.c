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
#include <syslinux/pxe.h>

void main_show_net(struct s_hardware *hardware,struct s_cli_mode *cli_mode) {
 char buffer[1024];
 memset(buffer,0,sizeof(buffer));

 if (hardware->pxe_detection==false) detect_pxe(hardware);
 if (hardware->is_pxe_valid==false) {
	 more_printf("No valid PXE rom found\n");
	 return;
 }
 t_PXENV_UNDI_GET_NIC_TYPE *g = &hardware->gnt;
 switch(hardware->gnt.NicType) {
	 case PCI_NIC: more_printf("PCI NIC: %04x:%04x[%04x:%04X] rev(%02x) Bus(%02x:%02x.%02x) %02x.%02x %02x %02x\n",
				       g->info.pci.Vendor_ID, g->info.pci.Dev_ID, g->info.pci.SubVendor_ID, g->info.pci.SubDevice_ID,
				       g->info.pci.Rev, (g->info.pci.BusDevFunc >> 8) & 0xff, (g->info.pci.BusDevFunc >> 3) & 0x7,
				       g->info.pci.BusDevFunc & 0x03,
				       g->info.pci.Base_Class,
				       g->info.pci.Sub_Class, g->info.pci.Prog_Intf);break;
	 case PnP_NIC: break;
	 case CardBus_NIC:break;
 }
}


