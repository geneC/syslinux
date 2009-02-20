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

#include "hdt-common.h"
#include <stdlib.h>
#include <string.h>

void init_hardware(struct s_hardware *hardware) {
  hardware->pci_ids_return_code=0;
  hardware->modules_pcimap_return_code=0;
  hardware->cpu_detection=false;
  hardware->pci_detection=false;
  hardware->disk_detection=false;
  hardware->dmi_detection=false;
  hardware->nb_pci_devices=0;
  hardware->is_dmi_valid=false;
  hardware->pci_domain=NULL;

  /* Cleaning structures */
  memset(&(hardware->disk_info),0,sizeof (struct diskinfo));
  memset(&(hardware->dmi),0,sizeof (s_dmi));
  memset(&(hardware->cpu),0,sizeof (s_cpu));
}

/* Detecting if a DMI table exist
 * if yes, let's parse it */
int detect_dmi(struct s_hardware *hardware) {
  hardware->dmi_detection=true;
  if (dmi_iterate(&(hardware->dmi)) == -ENODMITABLE ) {
	     hardware->is_dmi_valid=false;
             return -ENODMITABLE;
  }

  parse_dmitable(&(hardware->dmi));
  hardware->is_dmi_valid=true;
 return 0;
}

/* Try to detects disk from port 0x80 to 0xff*/
void detect_disks(struct s_hardware *hardware) {
 hardware->disk_detection=true;
 for (int drive = 0x80; drive < 0xff; drive++) {
    if (get_disk_params(drive,hardware->disk_info) != 0)
          continue;
    struct diskinfo d=hardware->disk_info[drive];
    printf("  DISK 0x%X: %s : %s %s: sectors=%d, s/t=%d head=%d : EDD=%s\n",drive,d.aid.model,d.host_bus_type,d.interface_type, d.sectors, d.sectors_per_track,d.heads,d.edd_version);
 }
}

void detect_pci(struct s_hardware *hardware) {
  hardware->pci_detection=true;
  printf("PCI: Detecting Devices\n");
  /* Scanning to detect pci buses and devices */
  hardware->pci_domain = pci_scan();

  struct pci_device *pci_device;
  for_each_pci_func(pci_device, hardware->pci_domain) {
          hardware->nb_pci_devices++;
  }

  printf("PCI: %d Devices Found\n",hardware->nb_pci_devices);

  printf("PCI: Resolving names\n");
  /* Assigning product & vendor name for each device*/
  hardware->pci_ids_return_code=get_name_from_pci_ids(hardware->pci_domain);

  printf("PCI: Resolving class names\n");
  /* Assigning class name for each device*/
  hardware->pci_ids_return_code=get_class_name_from_pci_ids(hardware->pci_domain);


  printf("PCI: Resolving module names\n");
  /* Detecting which kernel module should match each device */
  hardware->modules_pcimap_return_code=get_module_name_from_pci_ids(hardware->pci_domain);

}

void cpu_detect(struct s_hardware *hardware) {
  hardware->cpu_detection=true;
  detect_cpu(&(hardware->cpu));
}

/* Find the last instance of a particular command line argument
   (which should include the final =; do not use for boolean arguments) */
char *find_argument(char **argv, const char *argument)
{
  int la = strlen(argument);
  char **arg;
  char *ptr = NULL;

  for (arg = argv; *arg; arg++) {
    if (!memcmp(*arg, argument, la))
      ptr = *arg + la;
  }

  return ptr;
}
