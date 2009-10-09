/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
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

#include <stdlib.h>
#include <string.h>

#define lnetlib_c       /* Define the library */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "syslinux/boot.h"
#include "syslinux/loadfile.h"
#include "syslinux/linux.h"


/*
 * Most code taken from:
 *	com32/modules/linux.c
 */

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


static int sl_run_command(lua_State *L)
{
  const char *cmd = luaL_checkstring(L, 1);  /* Reads the string parameter */
  syslinux_run_command(cmd);
  return 0;
}

static int sl_run_default(lua_State *L)
{
  syslinux_run_default();
  return 0;
}

static int sl_local_boot(lua_State *L)
{
  uint16_t flags = luaL_checkint(L, 1);
  syslinux_local_boot(flags);
  return 0;
}

static int sl_final_cleanup(lua_State *L)
{
  uint16_t flags = luaL_checkint(L, 1);
  syslinux_local_boot(flags);
  return 0;
}

static int sl_boot_linux(lua_State *L)
{
  const char *kernel = luaL_checkstring(L, 1);
  const char *initrd = luaL_checkstring(L, 2);
  const char *cmdline = luaL_optstring(L, 3, "");
  void *kernel_data;
  size_t kernel_len;
  struct initramfs *initramfs;
  char *newcmdline;
  uint32_t mem_limit = luaL_optint(L, 4, 0);
  uint16_t video_mode = luaL_optint(L, 5, 0);
  int ret;

  newcmdline = malloc(strlen(kernel)+12+1+7+1+strlen(initrd)+1+strlen(cmdline));
  if (!newcmdline)
    printf("Mem alloc failed: cmdline\n");

  strcpy(newcmdline, "BOOT_IMAGE=");
  strcpy(newcmdline+strlen(newcmdline), kernel);
  strcpy(newcmdline+strlen(newcmdline), " initrd=");
  strcpy(newcmdline+strlen(newcmdline), initrd);
  strcpy(newcmdline+strlen(newcmdline), " ");
  strcpy(newcmdline+strlen(newcmdline), cmdline);

  printf("Command line: %s\n", newcmdline);

//  /* Look for specific command-line arguments we care about */
//  if ((arg = find_argument(argp, "mem=")))
//    mem_limit = saturate32(suffix_number(arg));
//
//  if ((arg = find_argument(argp, "vga="))) {
//    switch (arg[0] | 0x20) {
//    case 'a':			/* "ask" */
//      video_mode = 0xfffd;
//      break;
//    case 'e':			/* "ext" */
//      video_mode = 0xfffe;
//      break;
//    case 'n':			/* "normal" */
//      video_mode = 0xffff;
//      break;
//    default:
//      video_mode = strtoul(arg, NULL, 0);
//      break;
//    }
//  }
// 

  printf("Loading kernel %s...\n", kernel);
  if (loadfile(kernel, &kernel_data, &kernel_len)) {
    printf("Loading kernel failed!\n");
  }

  initramfs = initramfs_init();
  if (!initramfs)
    printf("Initializing initrd failed!\n");

  printf("Loading initrd %s...\n", initrd);
  if (initramfs_load_archive(initramfs, initrd)) {
    printf("Loading initrd failed!\n");
  }

  ret = syslinux_boot_linux(kernel_data, kernel_len, initramfs, newcmdline,
                      video_mode, mem_limit);
  printf("syslinux_boot_linux returned %d\n", ret);

  return 0;
}

static int sl_run_kernel_image(lua_State *L)
{
  const char *filename = luaL_checkstring(L, 1);
  const char *cmdline = luaL_checkstring(L, 2);
  uint32_t ipappend_flags = luaL_checkint(L, 3);
  uint32_t type = luaL_checkint(L, 4);

  syslinux_run_kernel_image(filename, cmdline, ipappend_flags, type);
  return 0;
}

static const luaL_reg syslinuxlib[] = {
  {"run_command", sl_run_command},
  {"run_default", sl_run_default},
  {"local_boot", sl_local_boot},
  {"final_cleanup", sl_final_cleanup},
  {"boot_linux", sl_boot_linux},
  {"run_kernel_image", sl_run_kernel_image},
  {NULL, NULL}
};

/* This defines a function that opens up your library. */

LUALIB_API int luaopen_syslinux (lua_State *L) {
  luaL_openlib(L, LUA_SYSLINUXLIBNAME, syslinuxlib, 0);
  return 1;
}
