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

void main_show_kernel(struct s_hardware *hardware,struct s_cli_mode *cli_mode) {
 char buffer[1024];
 struct pci_device *pci_device;
 bool found=false;
 char kernel_modules [LINUX_KERNEL_MODULE_SIZE*MAX_KERNEL_MODULES_PER_PCI_DEVICE];

 memset(buffer,0,sizeof(buffer));

 detect_pci(hardware);
 more_printf("Kernel modules\n");

// more_printf(" PCI device no: %d \n", p->pci_device_pos);

 if (hardware->modules_pcimap_return_code == -ENOMODULESPCIMAP) {
	 more_printf(" Modules.pcimap is missing\n");
	 return;
 }

  /* For every detected pci device, compute its submenu */
 for_each_pci_func(pci_device, hardware->pci_domain) {
   memset(kernel_modules,0,sizeof kernel_modules);

   for (int kmod=0; kmod<pci_device->dev_info->linux_kernel_module_count;kmod++) {
     if (kmod>0) {
       strncat(kernel_modules," | ",3);
     }
     strncat(kernel_modules, pci_device->dev_info->linux_kernel_module[kmod],LINUX_KERNEL_MODULE_SIZE-1);
   }

   if (pci_device->dev_info->linux_kernel_module_count>0) {
	   found=true;
	   if (pci_device->dev_info->linux_kernel_module_count>1) strncat(buffer,"(",1);
	   strncat(buffer, kernel_modules, sizeof(kernel_modules));
	   if (pci_device->dev_info->linux_kernel_module_count>1) strncat(buffer,")",1);
	   strncat(buffer," # ", 3);
   }

 }
 if (found ==true) {
	 strncat(buffer,"\n",1);
	 more_printf(buffer);
 }
}
