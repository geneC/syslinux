#include <stdlib.h>
#include <string.h>
#include <syslinux/boot.h>

#define lnetlib_c       /* Define the library */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static int sl_run_command(lua_State *L)
{
  const char *cmd = luaL_checkstring(L, 1);  /* Reads the string parameter */
  syslinux_run_command(cmd);
  return 0;
}


static const luaL_reg syslinuxlib[] = {
  {"run_command", sl_run_command},
  {NULL, NULL}
};

/* This defines a function that opens up your library. */

LUALIB_API int luaopen_syslinux (lua_State *L) {
  luaL_openlib(L, LUA_SYSLINUXLIBNAME, syslinuxlib, 0);
  return 1;
}

