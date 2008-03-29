/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * sanboot.c
 *
 * Invoke the gPXE "sanboot" command, if available.
 */

#include <alloca.h>
#include <inttypes.h>
#include <stdio.h>
#include <console.h>
#include <com32.h>
#include <stdbool.h>
#include <string.h>
#include <syslinux/config.h>

struct segoff16 {
  uint16_t offs, seg;
};

struct s_PXENV_FILE_CHECK_API {
  uint16_t Status;
  uint16_t Size;
  uint32_t Magic;
  uint32_t Provider;
  uint32_t APIMask;
  uint32_t Flags;
};

static bool is_gpxe(void)
{
  const struct syslinux_version *sv;
  com32sys_t reg;
  struct s_PXENV_FILE_CHECK_API *fca;

  sv = syslinux_version();
  if (sv->filesystem != SYSLINUX_FS_PXELINUX)
    return false;		/* Not PXELINUX */

  fca = __com32.cs_bounce;
  memset(fca, 0, sizeof *fca);
  fca->Size  = sizeof *fca;
  fca->Magic = 0x91d447b2;

  memset(&reg, 0, sizeof reg);
  reg.eax.w[0] = 0x0009;
  reg.ebx.w[0] = 0x00e6;	/* PXENV_FILE_API_CHECK */
  reg.edi.w[0] = OFFS(fca);
  reg.es       = SEG(fca);

  __intcall(0x22, &reg, &reg);

  if (reg.eflags.l & EFLAGS_CF)
    return false;		/* Cannot invoke PXE stack */

  if (reg.eax.w[0] || fca->Status)
    return false;		/* PXE failure */

  if (fca->Magic != 0xe9c17b20)
    return false;		/* Incorrect magic */

  if (fca->Size < sizeof *fca)
    return false;		/* Short return */

  if (!(fca->APIMask & (1 << 5)))
    return false;		/* No FILE EXEC */

  return true;
}

struct s_PXENV_FILE_EXEC {
  uint16_t Status;
  struct segoff16 Command;
};

static void sanboot(const char **args)
{
  char *q;
  struct s_PXENV_FILE_EXEC *fx;
  com32sys_t reg;

  memset(&reg, 0, sizeof reg);

  fx = __com32.cs_bounce;
  q = (char *)(fx+1);

  fx->Status = 1;
  fx->Command.offs = OFFS(q);
  fx->Command.seg = SEG(q);

  q = stpcpy(q, "sanboot");

  while (*args) {
    *q++ = ' ';
    q = stpcpy(q, *args);
    args++;
  }

  memset(&reg, 0, sizeof reg);
  reg.eax.w[0] = 0x0009;
  reg.ebx.w[0] = 0x00e5;	/* PXENV_FILE_EXEC */
  reg.edi.w[0] = OFFS(fx);
  reg.es       = SEG(fx);

  __intcall(0x22, &reg, &reg);

  /* This should not return... */
}

int main(int argc, const char *argv[])
{
  openconsole(&dev_null_r, &dev_stdcon_w);

  if (argc < 2) {
    printf("Usage: sanboot rootpath\n");
    return 1;
  }

  if (!is_gpxe()) {
    printf("sanboot: gPXE API not detected\n");
    return 1;
  }

  sanboot(argv+1);

  /* sanboot() should not return... */
  printf("SAN boot failed.\n");
  return 1;
}
