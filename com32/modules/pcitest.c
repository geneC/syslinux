/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006 Erwan Velu - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * pcitest.c
 *
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <console.h>
#include <com32.h>
#include <sys/pci.h>
#include <stdbool.h>

#ifdef DEBUG
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

char display_line;
#define moreprintf(...)				\
  do {						\
    display_line++;				\
    if (display_line == 24) {			\
      char tempbuf[10];				\
      display_line=0;				\
      printf("Press enter to continue\n");	\
      fgets(tempbuf, sizeof tempbuf, stdin);	\
    }						\
    printf ( __VA_ARGS__);			\
  } while (0);

void display_pci_devices(struct pci_device_list *pci_device_list) {
  int pci_dev;
  for (pci_dev=0; pci_dev < pci_device_list->count; pci_dev++) {
    struct pci_device *pci_device = &pci_device_list->pci_device[pci_dev];
    printf("PCI: Vendor=%04x(%s) Product=%04x(%s) Sub_vendor=%04x Sub_Product=%04x Release=%02x\n",
	   pci_device->vendor,pci_device->pci_dev_info->vendor_name, pci_device->product, pci_device->pci_dev_info->product_name,
	   pci_device->sub_vendor, pci_device->sub_product,
	   pci_device->revision);
  }
  printf("PCI: %d devices found\n",pci_device_list->count);
}

void display_pci_bus(struct pci_bus_list *pci_bus_list, bool display_pci_devices) {
  int bus;
  for (bus=0; bus<pci_bus_list->count;bus++) {
    struct pci_bus pci_bus = pci_bus_list->pci_bus[bus];
    printf("\nPCI BUS No %d:\n", pci_bus.id);
    if (display_pci_devices) {
      int pci_dev;
      for (pci_dev=0; pci_dev < pci_bus.pci_device_count; pci_dev++) {
	struct pci_device pci_device=*(pci_bus.pci_device[pci_dev]);
	printf("%s :%04x:%04x[%04x:%04x]) %s:%s\n",
		pci_device.pci_dev_info->linux_kernel_module,
	       pci_device.vendor, pci_device.product,
	        pci_device.sub_vendor, pci_device.sub_product, pci_device.pci_dev_info->vendor_name,pci_device.pci_dev_info->product_name);
      }
    }
  }
  printf("PCI: %d buse(s) found\n",pci_bus_list->count);
}

int main(int argc, char *argv[])
{
  struct pci_device_list pci_device_list;
  struct pci_bus_list pci_bus_list;
  openconsole(&dev_null_r, &dev_stdcon_w);

  /* Scanning to detect pci buses and devices */
  pci_scan(&pci_bus_list,&pci_device_list);

  /* Assigning product & vendor name for each device*/
  get_name_from_pci_ids(&pci_device_list);

  /* Detecting which kernel module should match each device */
  get_module_name_from_pci_ids(&pci_device_list);

  /* display the pci devices we found */
  display_pci_bus(&pci_bus_list,true);
  return 1;
}
