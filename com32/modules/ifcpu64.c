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
 * ifcpu64.c
 *
 * Run one command if the CPU has 64-bit support, and another if it doesn't.
 * Eventually this and other features should get folded into some kind
 * of scripting engine.
 *
 * Usage:
 *
 *    label boot_kernel
 *        kernel ifcpu64.c
 *        append boot_kernel_64 [-- boot_kernel_32pae] -- boot_kernel_32
 *    label boot_kernel_32
 *        kernel vmlinuz_32
 *        append ...
 *    label boot_kernel_64
 *        kernel vmlinuz_64
 *        append ...
 */

#include <alloca.h>
#include <stdlib.h>
#include <string.h>
#include <cpuid.h>
#include <syslinux/boot.h>

static bool cpu_has_cpuid(void)
{
  return cpu_has_eflag(X86_EFLAGS_ID);
}

static bool cpu_has_level(uint32_t level)
{
  uint32_t group = level & 0xffff0000;
  uint32_t limit = cpuid_eax(group);
  if ((limit & 0xffff0000) != group)
    return false;
  if (level > limit)
    return false;

  return true;
}

static bool cpu_has_pae(void)
{
  if (!cpu_has_cpuid())
    return false;

  if (!cpu_has_level(0x00000001))
    return false;

  return !!(cpuid_edx(0x00000001) & (X86_FEATURE_PAE & 31));
}

static bool cpu_has_lm(void)
{
  if (!cpu_has_cpuid())
    return false;

  if (!cpu_has_level(0x80000001))
    return false;

  return !!(cpuid_edx(0x80000001) & (X86_FEATURE_LM & 31));
}

/* XXX: this really should be librarized */
static void boot_args(char **args)
{
  int len = 0;
  char **pp;
  const char *p;
  char c, *q, *str;

  for (pp = args; *pp; pp++)
    len += strlen(*pp);

  q = str = alloca(len+1);
  for (pp = args; *pp; pp++) {
    p = *pp;
    while ((c = *p++))
      *q++ = c;
  }
  *q = '\0';

  if (!str[0])
    syslinux_run_default();
  else
    syslinux_run_command(str);
}

int main(int argc, char *argv[])
{
  char **args[3];
  int i;
  int n;

  for (i = 0; i < 3; i++)
    args[i] = &argv[1];

  n = 1;
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--")) {
      argv[i] = NULL;
      args[n++] = &argv[i+1];
    }
    if (n >= 3)
      break;
  }

  boot_args(cpu_has_lm()  ? args[0] :
	    cpu_has_pae() ? args[1] :
	    args[2]);
  return -1;
}
