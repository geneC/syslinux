/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007 H. Peter Anvin - All Rights Reserved
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
 * ----------------------------------------------------------------------- */

/*
 * syslinux/config.h
 *
 * Query syslinux configuration information.
 */

#ifndef _SYSLINUX_CONFIG_H
#define _SYSLINUX_CONFIG_H

#include <stdint.h>

enum syslinux_filesystem {
  SYSLINUX_FS_UNKNOWN    = 0x30,
  SYSLINUX_FS_SYSLINUX   = 0x31,
  SYSLINUX_FS_PXELINUX   = 0x32,
  SYSLINUX_FS_ISOLINUX   = 0x33,
  SYSLINUX_FS_EXTLINUX   = 0x34,
};

struct syslinux_version {
  uint16_t version;		/* (major << 8)+minor */
  uint16_t max_api;
  enum syslinux_filesystem filesystem;
  const char *version_string;
  const char *copyright_string;
};

extern struct syslinux_version __syslinux_version;
static inline const struct syslinux_version *
syslinux_version(void)
{
  return &__syslinux_version;
}

union syslinux_derivative_info {
  struct {
    uint8_t filesystem;
    uint8_t ah;
  } c;				/* common */
  struct {
    uint16_t ax;
    uint16_t cx;
    uint16_t dx;
    const void *esbx;
    const void *fssi;
    const void *gsdi;
  } r;				/* raw */
  struct {
    uint8_t filesystem;
    uint8_t ah;
    uint8_t sector_shift;
    uint8_t ch;
    uint8_t drive_number;
    uint8_t dh;
    const void *ptab_ptr;
  } disk;			/* syslinux/extlinux */
  struct {
    uint8_t filesystem;
    uint8_t ah;
    uint16_t cx;
    uint16_t apiver;
    const void *pxenvptr;
    const void *stack;
  } pxe;			/* pxelinux */
  struct {
    uint8_t filesystem;
    uint8_t ah;
    uint8_t sector_shift;
    uint8_t ch;
    uint8_t drive_number;
    uint8_t dh;
    const void *spec_packet;
  } iso;			/* isolinux */
};

union syslinux_derivative_info __syslinux_derivative_info;
static inline const union syslinux_derivative_info *
syslinux_derivative_info(void)
{
  return &__syslinux_derivative_info;
}

struct syslinux_serial_console_info {
  uint16_t iobase;
  uint16_t divisor;
  uint16_t flowctl;
};

extern struct syslinux_serial_console_info __syslinux_serial_console_info;
static inline const struct syslinux_serial_console_info *
syslinux_serial_console_info(void)
{
  return &__syslinux_serial_console_info;
}

extern const char *__syslinux_config_file;
static inline const char *syslinux_config_file(void)
{
  return __syslinux_config_file;
}

struct syslinux_ipappend_strings {
  int count;
  const char * const *ptr;
};
extern struct syslinux_ipappend_strings __syslinux_ipappend_strings;
static inline const struct syslinux_ipappend_strings *
syslinux_ipappend_strings(void)
{
  return &__syslinux_ipappend_strings;
}

#endif /* _SYSLINUX_CONFIG_H */
