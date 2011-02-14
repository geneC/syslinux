#include <stdlib.h>
#include <string.h>

#define ldmilib_c       /* Define the library */

/* Include the Lua API header files */
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "dmi/dmi.h"

static int dmi_gettable(lua_State *L)
{
  s_dmi dmi;

  lua_newtable(L);

  if ( ! dmi_iterate(&dmi) ) {
          printf("No DMI Structure found\n");
          return -1;
  }

  parse_dmitable(&dmi);

  /* bios */
  lua_pushstring(L, "bios.vendor");
  lua_pushstring(L, dmi.bios.vendor);
  lua_settable(L,-3);

  lua_pushstring(L, "bios.version");
  lua_pushstring(L, dmi.bios.version);
  lua_settable(L,-3);

  lua_pushstring(L, "bios.release_date");
  lua_pushstring(L, dmi.bios.release_date);
  lua_settable(L,-3);

  lua_pushstring(L, "bios.bios_revision");
  lua_pushstring(L, dmi.bios.bios_revision);
  lua_settable(L,-3);

  lua_pushstring(L, "bios.firmware_revision");
  lua_pushstring(L, dmi.bios.firmware_revision);
  lua_settable(L,-3);

  lua_pushstring(L, "bios.address");
  lua_pushnumber(L, dmi.bios.address);
  lua_settable(L,-3);

  lua_pushstring(L, "bios.runtime_size");
  lua_pushnumber(L, dmi.bios.runtime_size);
  lua_settable(L,-3);

  lua_pushstring(L, "bios.runtime_size_unit");
  lua_pushstring(L, dmi.bios.runtime_size_unit);
  lua_settable(L,-3);

  lua_pushstring(L, "bios.rom_size");
  lua_pushnumber(L, dmi.bios.rom_size);
  lua_settable(L,-3);

  lua_pushstring(L, "bios.rom_size_unit");
  lua_pushstring(L, dmi.bios.rom_size_unit);
  lua_settable(L,-3);

  /* system */
  lua_pushstring(L, "system.manufacturer");
  lua_pushstring(L, dmi.system.manufacturer);
  lua_settable(L,-3);

  lua_pushstring(L, "system.product_name");
  lua_pushstring(L, dmi.system.product_name);
  lua_settable(L,-3);

  lua_pushstring(L, "system.version");
  lua_pushstring(L, dmi.system.version);
  lua_settable(L,-3);

  lua_pushstring(L, "system.serial");
  lua_pushstring(L, dmi.system.serial);
  lua_settable(L,-3);

  lua_pushstring(L, "system.uuid");
  lua_pushstring(L, dmi.system.uuid);
  lua_settable(L,-3);

  lua_pushstring(L, "system.wakeup_type");
  lua_pushstring(L, dmi.system.wakeup_type);
  lua_settable(L,-3);

  lua_pushstring(L, "system.sku_number");
  lua_pushstring(L, dmi.system.sku_number);
  lua_settable(L,-3);

  lua_pushstring(L, "system.family");
  lua_pushstring(L, dmi.system.family);
  lua_settable(L,-3);

  /* base_board */
  lua_pushstring(L, "base_board.manufacturer");
  lua_pushstring(L, dmi.base_board.manufacturer);
  lua_settable(L,-3);

  lua_pushstring(L, "base_board.product_name");
  lua_pushstring(L, dmi.base_board.product_name);
  lua_settable(L,-3);

  lua_pushstring(L, "base_board.version");
  lua_pushstring(L, dmi.base_board.version);
  lua_settable(L,-3);

  lua_pushstring(L, "base_board.serial");
  lua_pushstring(L, dmi.base_board.serial);
  lua_settable(L,-3);

  lua_pushstring(L, "base_board.asset_tag");
  lua_pushstring(L, dmi.base_board.asset_tag);
  lua_settable(L,-3);

  lua_pushstring(L, "base_board.location");
  lua_pushstring(L, dmi.base_board.location);
  lua_settable(L,-3);

  lua_pushstring(L, "base_board.type");
  lua_pushstring(L, dmi.base_board.type);
  lua_settable(L,-3);

  /* chassis */
  lua_pushstring(L, "chassis.manufacturer");
  lua_pushstring(L, dmi.chassis.manufacturer);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.type");
  lua_pushstring(L, dmi.chassis.type);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.lock");
  lua_pushstring(L, dmi.chassis.lock);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.version");
  lua_pushstring(L, dmi.chassis.version);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.serial");
  lua_pushstring(L, dmi.chassis.serial);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.asset_tag");
  lua_pushstring(L, dmi.chassis.asset_tag);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.boot_up_state");
  lua_pushstring(L, dmi.chassis.boot_up_state);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.power_supply_state");
  lua_pushstring(L, dmi.chassis.power_supply_state);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.thermal_state");
  lua_pushstring(L, dmi.chassis.thermal_state);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.security_status");
  lua_pushstring(L, dmi.chassis.security_status);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.oem_information");
  lua_pushstring(L, dmi.chassis.oem_information);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.height");
  lua_pushnumber(L, dmi.chassis.height);
  lua_settable(L,-3);

  lua_pushstring(L, "chassis.nb_power_cords");
  lua_pushnumber(L, dmi.chassis.nb_power_cords);
  lua_settable(L,-3);

  /* processor */
  lua_pushstring(L, "processor.socket_designation");
  lua_pushstring(L, dmi.processor.socket_designation);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.type");
  lua_pushstring(L, dmi.processor.type);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.family");
  lua_pushstring(L, dmi.processor.family);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.manufacturer");
  lua_pushstring(L, dmi.processor.manufacturer);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.version");
  lua_pushstring(L, dmi.processor.version);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.external_clock");
  lua_pushnumber(L, dmi.processor.external_clock);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.max_speed");
  lua_pushnumber(L, dmi.processor.max_speed);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.current_speed");
  lua_pushnumber(L, dmi.processor.current_speed);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.signature.type");
  lua_pushnumber(L, dmi.processor.signature.type);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.signature.family");
  lua_pushnumber(L, dmi.processor.signature.family);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.signature.model");
  lua_pushnumber(L, dmi.processor.signature.model);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.signature.stepping");
  lua_pushnumber(L, dmi.processor.signature.stepping);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.signature.minor_stepping");
  lua_pushnumber(L, dmi.processor.signature.minor_stepping);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.voltage_mv");
  lua_pushnumber(L, dmi.processor.voltage_mv);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.status");
  lua_pushstring(L, dmi.processor.status);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.upgrade");
  lua_pushstring(L, dmi.processor.upgrade);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.cache1");
  lua_pushstring(L, dmi.processor.cache1);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.cache2");
  lua_pushstring(L, dmi.processor.cache2);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.cache3");
  lua_pushstring(L, dmi.processor.cache3);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.serial");
  lua_pushstring(L, dmi.processor.serial);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.part_number");
  lua_pushstring(L, dmi.processor.part_number);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.id");
  lua_pushstring(L, dmi.processor.id);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.core_count");
  lua_pushnumber(L, dmi.processor.core_count);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.core_enabled");
  lua_pushnumber(L, dmi.processor.core_enabled);
  lua_settable(L,-3);

  lua_pushstring(L, "processor.thread_count");
  lua_pushnumber(L, dmi.processor.thread_count);
  lua_settable(L,-3);

  /* set global variable: lua_setglobal(L, "dmitable"); */

  /* return number of return values on stack */
  return 1;
}


static int dmi_supported(lua_State *L)
{
  s_dmi dmi;

  if ( dmi_iterate(&dmi) ) {
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}


static const luaL_Reg dmilib[] = {
  {"gettable", dmi_gettable},
  {"supported", dmi_supported},
  {NULL, NULL}
};


LUALIB_API int luaopen_dmi (lua_State *L) {
  luaL_openlib(L, LUA_DMILIBNAME, dmilib, 0);
  return 1;
}

