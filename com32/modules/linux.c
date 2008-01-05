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
 * linux.c
 *
 * Sample module to load Linux kernels.  This module can also create
 * a file out of the DHCP return data if running under PXELINUX.
 *
 * If -dhcpinfo is specified, the DHCP info is written into the file
 * /dhcpinfo.dat in the initramfs.
 *
 * Usage: linux.c32 [-dhcpinfo] kernel arguments...
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <syslinux/loadfile.h>
#include <syslinux/linux.h>
#include <syslinux/pxe.h>

const char *progname = "linux.c32";

/* Find the last instance of a particular command line argument
   (which should include the final =; do not use for boolean arguments) */
static char *find_argument(char **argv, const char *argument)
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

/* Get a value with a potential suffix (k/m/g/t/p/e) */
static unsigned long long suffix_number(const char *str)
{
  char *ep;
  unsigned long long v;
  int shift;

  v = strtoull(str, &ep, 0);
  switch (*ep|0x20) {
  case 'k':
    shift = 10;
    break;
  case 'm':
    shift = 20;
    break;
  case 'g':
    shift = 30;
    break;
  case 't':
    shift = 40;
    break;
  case 'p':
    shift = 50;
    break;
  case 'e':
    shift = 60;
    break;
  default:
    shift = 0;
    break;
  }
  v <<= shift;

  return v;
}

/* Truncate to 32 bits, with saturate */
static inline uint32_t saturate32(unsigned long long v)
{
  return (v > 0xffffffff) ? 0xffffffff : (uint32_t)v;
}

/* Stitch together the command line from a set of argv's */
static char *make_cmdline(char **argv)
{
  char **arg;
  size_t bytes;
  char *cmdline, *p;

  bytes = 1;			/* Just in case we have a zero-entry cmdline */
  for (arg = argv; *arg; arg++) {
    bytes += strlen(*arg)+1;
  }

  p = cmdline = malloc(bytes);
  if (!cmdline)
    return NULL;

  for (arg = argv; *arg; arg++) {
    int len = strlen(*arg);
    memcpy(p, *arg, len);
    p[len] = ' ';
    p += len+1;
  }

  if (p > cmdline)
    p--;			/* Remove the last space */
  *p = '\0';

  return cmdline;
}

int main(int argc, char *argv[])
{
  uint32_t mem_limit = 0;
  uint16_t video_mode = 0;
  const char *kernel_name;
  struct initramfs *initramfs;
  char *cmdline;
  char *boot_image;
  void *kernel_data;
  size_t kernel_len;
  int opt_dhcpinfo = 0;
  void *dhcpdata;
  size_t dhcplen;
  char **argp, *arg, *p;

  openconsole(&dev_null_r, &dev_stdcon_w);

  (void)argc;
  argp = argv+1;

  while ((arg = *argp) && arg[0] == '-') {
    if (!strcmp("-dhcpinfo", arg)) {
      opt_dhcpinfo = 1;
    } else {
      fprintf(stderr, "%s: unknown option: %s\n", progname, arg);
      return 1;
    }
    argp++;
  }

  if (!arg) {
    fprintf(stderr, "%s: missing kernel name\n", progname);
    return 1;
  }

  kernel_name = arg;

  printf("Loading %s... ", kernel_name);
  if (loadfile(kernel_name, &kernel_data, &kernel_len)) {
    printf("failed!\n");
    goto bail;
  }
  printf("ok\n");

  boot_image = malloc(strlen(kernel_name)+12);
  if (!boot_image)
    goto bail;

  strcpy(boot_image, "BOOT_IMAGE=");
  strcpy(boot_image+11, kernel_name);

  /* argp now points to the kernel name, and the command line follows.
     Overwrite the kernel name with the BOOT_IMAGE= argument, and thus
     we have the final argument. */
  *argp = boot_image;

  cmdline = make_cmdline(argp);
  if (!cmdline)
    goto bail;

  /* Initialize the initramfs chain */
  initramfs = initramfs_init();
  if (!initramfs)
    goto bail;

  /* Look for specific command-line arguments we care about */
  if ((arg = find_argument(argp, "mem=")))
    mem_limit = saturate32(suffix_number(arg));

  if ((arg = find_argument(argp, "vga="))) {
    switch (arg[0] | 0x20) {
    case 'a':			/* "ask" */
      video_mode = 0xfffd;
      break;
    case 'e':			/* "ext" */
      video_mode = 0xfffe;
      break;
    case 'n':			/* "normal" */
      video_mode = 0xffff;
      break;
    default:
      video_mode = strtoul(arg, NULL, 0);
      break;
    }
  }

  if ((arg = find_argument(argp, "initrd="))) {
    do {
      p = strchr(arg, ',');
      if (p)
	*p = '\0';

      printf("Loading %s... ", arg);
      if (initramfs_load_archive(initramfs, arg)) {
	printf("failed!\n");
	goto bail;
      }
      printf("ok\n");

      if (p)
	*p++ = ',';
    } while ((arg = p));
  }

  /* Append the DHCP info */
  if (opt_dhcpinfo &&
      !pxe_get_cached_info(PXENV_PACKET_TYPE_DHCP_ACK, &dhcpdata, &dhcplen)) {
    if (initramfs_add_file(initramfs, dhcpdata, dhcplen, dhcplen,
			   "/dhcpinfo.dat", 0, 0755))
      goto bail;
  }

  /* This should not return... */
  syslinux_boot_linux(kernel_data, kernel_len, initramfs, cmdline,
		      video_mode, mem_limit);

 bail:
  fprintf(stderr, "Kernel load failure (insufficient memory?)\n");
  return 1;
}
