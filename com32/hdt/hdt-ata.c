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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <getkey.h>

#include "com32io.h"
#include "hdt-common.h"
#include "hdt-ata.h"

#ifdef ATA
/**
 *      ata_id_string - Convert IDENTIFY DEVICE page into string
 *      @id: IDENTIFY DEVICE results we will examine
 *      @s: string into which data is output
 *      @ofs: offset into identify device page
 *      @len: length of string to return. must be an even number.
 *
 *      The strings in the IDENTIFY DEVICE page are broken up into
 *      16-bit chunks.  Run through the string, and output each
 *      8-bit chunk linearly, regardless of platform.
 *
 *      LOCKING:
 *      caller.
 */
void ata_id_string(const uint16_t * id, unsigned char *s,
       unsigned int ofs, unsigned int len)
{
  unsigned int c;

  while (len > 0) {
    c = id[ofs] >> 8;
    *s = c;
    s++;

    c = id[ofs] & 0xff;
    *s = c;
    s++;

    ofs++;
    len -= 2;
  }
}

/**
 *      ata_id_c_string - Convert IDENTIFY DEVICE page into C string
 *      @id: IDENTIFY DEVICE results we will examine
 *      @s: string into which data is output
 *      @ofs: offset into identify device page
 *      @len: length of string to return. must be an odd number.
 *
 *      This function is identical to ata_id_string except that it
 *      trims trailing spaces and terminates the resulting string with
 *      null.  @len must be actual maximum length (even number) + 1.
 *
 *      LOCKING:
 *      caller.
 */
void ata_id_c_string(const uint16_t * id, unsigned char *s,
         unsigned int ofs, unsigned int len)
{
  unsigned char *p;

  //WARN_ON(!(len & 1));

  ata_id_string(id, s, ofs, len - 1);

  p = s + strnlen(s, len - 1);
  while (p > s && p[-1] == ' ')
    p--;
  *p = '\0';
}
#endif

/**
 * Call int 13h, but with retry on failure.  Especially floppies need this.
 */
int int13_retry(const com32sys_t * inreg, com32sys_t * outreg)
{
  int retry = 6;    /* Number of retries */
  com32sys_t tmpregs;

  if (!outreg)
    outreg = &tmpregs;

  while (retry--) {
    __intcall(0x13, inreg, outreg);
    if (!(outreg->eflags.l & EFLAGS_CF))
      return 0; /* CF=0, OK */
  }

  return -1;    /* Error */
}

/* Display CPU registers for debugging purposes */
void printregs(const com32sys_t * r)
{
  printf("eflags = %08x  ds = %04x  es = %04x  fs = %04x  gs = %04x\n"
         "eax = %08x  ebx = %08x  ecx = %08x  edx = %08x\n"
         "ebp = %08x  esi = %08x  edi = %08x  esp = %08x\n",
         r->eflags.l, r->ds, r->es, r->fs, r->gs,
         r->eax.l, r->ebx.l, r->ecx.l, r->edx.l,
         r->ebp.l, r->esi.l, r->edi.l, r->_unused_esp.l);
}

/* Try to get information for a given disk */
int get_disk_params(int disk, struct diskinfo *disk_info)
{
  static com32sys_t getparm, parm, getebios, ebios, inreg, outreg;
  struct device_parameter dp;
#ifdef ATA
  struct ata_identify_device aid;
#endif

  memset(&(disk_info[disk]), 0, sizeof(struct diskinfo));

  disk_info[disk].disk = disk;
  disk_info[disk].ebios = disk_info[disk].cbios = 0;

  /* Sending int 13h func 41h to query EBIOS information */
  memset(&getebios, 0, sizeof(com32sys_t));
  memset(&ebios, 0, sizeof(com32sys_t));

  /* Get EBIOS support */
  getebios.eax.w[0] = 0x4100;
  getebios.ebx.w[0] = 0x55aa;
  getebios.edx.b[0] = disk;
  getebios.eflags.b[0] = 0x3; /* CF set */

  __intcall(0x13, &getebios, &ebios);

  /* Detecting EDD support */
  if (!(ebios.eflags.l & EFLAGS_CF) &&
      ebios.ebx.w[0] == 0xaa55 && (ebios.ecx.b[0] & 1)) {
    disk_info[disk].ebios = 1;
    switch (ebios.eax.b[1]) {
    case 32:
      strlcpy(disk_info[disk].edd_version, "1.0", 3);
      break;
    case 33:
      strlcpy(disk_info[disk].edd_version, "1.1", 3);
      break;
    case 48:
      strlcpy(disk_info[disk].edd_version, "3.0", 3);
      break;
    default:
      strlcpy(disk_info[disk].edd_version, "0", 1);
      break;
    }
  }
  /* Get disk parameters -- really only useful for
   * hard disks, but if we have a partitioned floppy
   * it's actually our best chance...
   */
  memset(&getparm, 0, sizeof(com32sys_t));
  memset(&parm, 0, sizeof(com32sys_t));
  getparm.eax.b[1] = 0x08;
  getparm.edx.b[0] = disk;

  __intcall(0x13, &getparm, &parm);

  if (parm.eflags.l & EFLAGS_CF)
    return disk_info[disk].ebios ? 0 : -1;

  disk_info[disk].heads = parm.edx.b[1] + 1;
  disk_info[disk].sectors_per_track = parm.ecx.b[0] & 0x3f;
  if (disk_info[disk].sectors_per_track == 0) {
    disk_info[disk].sectors_per_track = 1;
  } else {
    disk_info[disk].cbios = 1;  /* Valid geometry */
  }

  /* If geometry isn't valid, no need to try to get more info about the drive*/
  /* Looks like in can confuse some optical drives */
  if (disk_info[disk].cbios != 1) return 0;

/* FIXME: memset to 0 make it fails
 * memset(__com32.cs_bounce, 0, sizeof(struct device_pairameter)); */
  memset(&dp, 0, sizeof(struct device_parameter));
  memset(&inreg, 0, sizeof(com32sys_t));

  /* Requesting Extended Read Drive Parameters via int13h func 48h */
  inreg.esi.w[0] = OFFS(__com32.cs_bounce);
  inreg.ds = SEG(__com32.cs_bounce);
  inreg.eax.w[0] = 0x4800;
  inreg.edx.b[0] = disk;

  __intcall(0x13, &inreg, &outreg);

  /* Saving bounce buffer before anything corrupt it */
  memcpy(&dp, __com32.cs_bounce, sizeof(struct device_parameter));

  if (outreg.eflags.l & EFLAGS_CF) {
    more_printf("Disk 0x%X doesn't supports EDD 3.0\n", disk);
    return -1;
  }

  /* Copying result to the disk_info structure
   * host_bus_type, interface_type, sectors & cylinders */
  snprintf(disk_info[disk].host_bus_type,
     sizeof disk_info[disk].host_bus_type, "%c%c%c%c",
     dp.host_bus_type[0], dp.host_bus_type[1], dp.host_bus_type[2],
     dp.host_bus_type[3]);
  snprintf(disk_info[disk].interface_type,
     sizeof disk_info[disk].interface_type, "%c%c%c%c%c%c%c%c",
     dp.interface_type[0], dp.interface_type[1],
     dp.interface_type[2], dp.interface_type[3],
     dp.interface_type[4], dp.interface_type[5],
     dp.interface_type[6], dp.interface_type[7]);
  disk_info[disk].sectors = dp.sectors;
  disk_info[disk].cylinders = dp.cylinders;

  /*FIXME: we have to find a way to grab the model & fw
   * We do put dummy data until we found a solution */
  snprintf(disk_info[disk].aid.model, sizeof disk_info[disk].aid.model,
     "0x%X", disk);
  snprintf(disk_info[disk].aid.fw_rev, sizeof disk_info[disk].aid.fw_rev,
     "%s", "N/A");
  snprintf(disk_info[disk].aid.serial_no,
     sizeof disk_info[disk].aid.serial_no, "%s", "N/A");

  /* Useless stuff before I figure how to send ata packets */
#ifdef ATA
  memset(__com32.cs_bounce, 0, sizeof(struct device_parameter));
  memset(&aid, 0, sizeof(struct ata_identify_device));
  memset(&inreg, 0, sizeof inreg);
  inreg.ebx.w[0] = OFFS(__com32.cs_bounce + 1024);
  inreg.es = SEG(__com32.cs_bounce + 1024);
  inreg.eax.w[0] = 0x2500;
  inreg.edx.b[0] = disk;

  __intcall(0x13, &inreg, &outreg);

  memcpy(&aid, __com32.cs_bounce, sizeof(struct ata_identify_device));

  if (outreg.eflags.l & EFLAGS_CF) {
    more_printf("Disk 0x%X: Failed to Identify Device\n", disk);
    //FIXME
    return 0;
  }
//   ata_id_c_string(aid, disk_info[disk].fwrev, ATA_ID_FW_REV, sizeof(disk_info[disk].fwrev));
//   ata_id_c_string(aid, disk_info[disk].model, ATA_ID_PROD,  sizeof(disk_info[disk].model));

  char buff[sizeof(struct ata_identify_device)];
  memcpy(buff, &aid, sizeof(struct ata_identify_device));
  for (int j = 0; j < sizeof(struct ata_identify_device); j++)
    more_printf("model=|%c|\n", buff[j]);
  more_printf("Disk 0x%X : %s %s %s\n", disk, aid.model, aid.fw_rev,
         aid.serial_no);
#endif

  return 0;
}
