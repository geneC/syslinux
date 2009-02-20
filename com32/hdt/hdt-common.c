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
  if (dmi_iterate(&(hardware->dmi)) == -ENODMITABLE ) {
             printf("No DMI Structure found\n");
	     hardware->is_dmi_valid=false;
             return -ENODMITABLE;
  }

  parse_dmitable(&(hardware->dmi));
  hardware->is_dmi_valid=true;
 return 0;
}

/* Try to detects disk from port 0x80 to 0xff*/
void detect_disks(struct diskinfo *disk_info) {
 for (int drive = 0x80; drive < 0xff; drive++) {
    if (get_disk_params(drive,disk_info) != 0)
          continue;
    struct diskinfo d=disk_info[drive];
    printf("  DISK 0x%X: %s : %s %s: sectors=%d, s/t=%d head=%d : EDD=%s\n",drive,d.aid.model,d.host_bus_type,d.interface_type, d.sectors, d.sectors_per_track,d.heads,d.edd_version);
 }
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
