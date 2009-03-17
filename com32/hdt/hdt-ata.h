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

#ifndef DEFINE_HDT_ATA_H
#define DEFINE_HDT_ATA_H
#include <com32io.h>

#include "hdt.h"

struct ata_identify_device {
  unsigned short words000_009[10];
  unsigned char serial_no[20];
  unsigned short words020_022[3];
  unsigned char fw_rev[8];
  unsigned char model[40];
  unsigned short words047_079[33];
  unsigned short major_rev_num;
  unsigned short minor_rev_num;
  unsigned short command_set_1;
  unsigned short command_set_2;
  unsigned short command_set_extension;
  unsigned short cfs_enable_1;
  unsigned short word086;
  unsigned short csf_default;
  unsigned short words088_255[168];
} ATTR_PACKED;

struct diskinfo {
  int disk;
  int ebios;                      /* EBIOS supported on this disk */
  int cbios;                      /* CHS geometry is valid */
  int heads;
  int sectors_per_track;
  int sectors;
  int cylinders;
  char edd_version[4];
  struct ata_identify_device aid; /* IDENTIFY xxx DEVICE data */
  char host_bus_type[5];
  char interface_type[9];
  char interface_port;
} ATTR_PACKED;

/*
 * Get a disk block and return a malloc'd buffer.
 * Uses the disk number and information from disk_info.
 */
struct ebios_dapa {
  uint16_t len;
  uint16_t count;
  uint16_t off;
  uint16_t seg;
  uint64_t lba;
};

// BYTE=8
// WORD=16
// DWORD=32
// QWORD=64
struct device_parameter {
  uint16_t len;
  uint16_t info;
  uint32_t cylinders;
  uint32_t heads;
  uint32_t sectors_per_track;
  uint64_t sectors;
  uint16_t bytes_per_sector;
  uint32_t dpte_pointer;
  uint16_t device_path_information;
  uint8_t device_path_lenght;
  uint8_t device_path_reserved;
  uint16_t device_path_reserved_2;
  uint8_t host_bus_type[4];
  uint8_t interface_type[8];
  uint64_t interace_path;
  uint64_t device_path[2];
  uint8_t reserved;
  uint8_t cheksum;
} ATTR_PACKED;

/* Useless stuff until I manage how to send ata packets */
#ifdef ATA
enum {
  ATA_ID_FW_REV = 23,
  ATA_ID_PROD = 27,
  ATA_ID_FW_REV_LEN = 8,
  ATA_ID_PROD_LEN = 40,
};
void ata_id_c_string(const uint16_t * id, unsigned char *s, unsigned int ofs,
                     unsigned int len);
void ata_id_string(const uint16_t * id, unsigned char *s, unsigned int ofs,
                   unsigned int len);
int int13_retry(const com32sys_t * inreg, com32sys_t * outreg);
void printregs(const com32sys_t * r);
#endif

int get_disk_params(int disk, struct diskinfo *disk_info);
#endif
