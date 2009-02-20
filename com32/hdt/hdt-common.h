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

#ifndef DEFINE_HDT_COMMON_H
#define DEFINE_HDT_COMMON_H
#include <stdio.h>
#include "sys/pci.h"
#include "cpuid.h"
#include "dmi/dmi.h"
#include "hdt-ata.h"

struct s_hardware {
  s_dmi dmi; /* DMI table */
  s_cpu cpu; /* CPU information */
  struct pci_domain *pci_domain; /* PCI Devices */
  struct diskinfo disk_info[256];     /* Disk Information*/
  int pci_ids_return_code;
  int modules_pcimap_return_code;
  int nb_pci_devices;
  bool is_dmi_valid;
  bool dmi_detection; /* Does the dmi stuff have been already detected */
  bool pci_detection; /* Does the pci stuff have been already detected */
  bool cpu_detection; /* Does the cpu stuff have been already detected */
  bool disk_detection; /* Does the disk stuff have been already detected */
};

char *find_argument(char **argv, const char *argument);
int detect_dmi(struct s_hardware *hardware);
void detect_disks(struct s_hardware *hardware);
void detect_pci(struct s_hardware *hardware);
void cpu_detect(struct s_hardware *hardware);
void init_hardware(struct s_hardware *hardware);
#endif
