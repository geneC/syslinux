/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006 Erwan Velu - All Rights Reserved
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
      printf("Press Enter to continue\n");	\
      fgets(tempbuf, sizeof tempbuf, stdin);	\
    }						\
    printf ( __VA_ARGS__);			\
  } while (0);

void display_pci_devices(struct pci_domain *pci_domain) {
  struct pci_device *pci_device;
  int ndev = 0;
  for_each_pci_func(pci_device, pci_domain) {
	printf("[%02x:%02x.%01x]: %s: %04x:%04x[%04x:%04x]) %s:%s\n",
	       __pci_bus, __pci_slot, __pci_func,
	       pci_device->dev_info->linux_kernel_module,
	       pci_device->vendor, pci_device->product,
	       pci_device->sub_vendor, pci_device->sub_product,
	       pci_device->dev_info->vendor_name,
	       pci_device->dev_info->product_name);
    ndev++;
  }
  printf("PCI: %d devices found\n", ndev);
}

int main(int argc, char *argv[])
{
  struct pci_domain *pci_domain;

  openconsole(&dev_null_r, &dev_stdcon_w);

  /* Scanning to detect pci buses and devices */
  pci_domain = pci_scan();

  /* Assigning product & vendor name for each device*/
  get_name_from_pci_ids(pci_domain);

  /* Detecting which kernel module should match each device */
  get_module_name_from_pci_ids(pci_domain);

  /* display the pci devices we found */
  display_pci_devices(pci_domain);
  return 1;
}
