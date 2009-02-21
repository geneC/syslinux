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

#include "hdt-cli.h"
#include "hdt-common.h"
#include <stdio.h>
#include <string.h>

void dmi_show(char *item, struct s_hardware *hardware) {
 if ( !strncmp(item, CLI_DMI_BASE_BOARD, sizeof CLI_DMI_BASE_BOARD - 1) ) {
   show_dmi_base_board(hardware);
   return;
 } else
 if ( !strncmp(item, CLI_DMI_SYSTEM, sizeof CLI_DMI_SYSTEM - 1) ) {
   show_dmi_system(hardware);
   return;
 } else
 if ( !strncmp(item, CLI_DMI_BIOS, sizeof CLI_DMI_BIOS - 1) ) {
   show_dmi_bios(hardware);
   return;
 } else
 if ( !strncmp(item, CLI_DMI_CHASSIS, sizeof CLI_DMI_CHASSIS - 1) ) {
   show_dmi_chassis(hardware);
   return;
 } else
 if ( !strncmp(item, CLI_DMI_PROCESSOR, sizeof CLI_DMI_PROCESSOR - 1) ) {
   show_dmi_cpu(hardware);
   return;
 }
 if ( !strncmp(item, CLI_DMI_MODULES, sizeof CLI_DMI_MODULES - 1) ) {
   show_dmi_modules(hardware);
   return;
 }

}

void handle_dmi_commands(char *cli_line, struct s_cli_mode *cli_mode, struct s_hardware *hardware) {
 if ( !strncmp(cli_line, CLI_SHOW, sizeof CLI_SHOW - 1) ) {
    dmi_show(strstr(cli_line,"show")+ sizeof CLI_SHOW, hardware);
    return;
 }
}

void show_dmi_modules(struct s_hardware *hardware) {
 char available_dmi_commands[1024];
 memset(available_dmi_commands,0,sizeof available_dmi_commands);

 if (hardware->dmi.base_board.filled==true) {
	 strncat(available_dmi_commands,CLI_DMI_BASE_BOARD,sizeof CLI_DMI_BASE_BOARD-1);
	 strncat(available_dmi_commands," ",1);
 }
 if (hardware->dmi.battery.filled==true) {
	 strncat(available_dmi_commands,CLI_DMI_BATTERY,sizeof CLI_DMI_BATTERY-1);
	 strncat(available_dmi_commands," ",1);
 }
 if (hardware->dmi.bios.filled==true) {
	 strncat(available_dmi_commands,CLI_DMI_BIOS,sizeof CLI_DMI_BIOS-1);
	 strncat(available_dmi_commands," ",1);
 }
 if (hardware->dmi.chassis.filled==true) {
	 strncat(available_dmi_commands,CLI_DMI_CHASSIS,sizeof CLI_DMI_CHASSIS-1);
	 strncat(available_dmi_commands," ",1);
 }
 for (int i=0;i<hardware->dmi.memory_count;i++) {
	if (hardware->dmi.memory[i].filled==true) {
		strncat(available_dmi_commands,CLI_DMI_MEMORY,sizeof CLI_DMI_MEMORY-1);
		strncat(available_dmi_commands," ",1);
		break;
	}
 }
 if (hardware->dmi.processor.filled==true) {
	 strncat(available_dmi_commands,CLI_DMI_PROCESSOR,sizeof CLI_DMI_PROCESSOR-1);
	 strncat(available_dmi_commands," ",1);
 }
 if (hardware->dmi.system.filled==true) {
	 strncat(available_dmi_commands,CLI_DMI_SYSTEM,sizeof CLI_DMI_SYSTEM-1);
	 strncat(available_dmi_commands," ",1);
 }
 printf("Available DMI modules: %s\n",available_dmi_commands);
}

void main_show_dmi(struct s_hardware *hardware,struct s_cli_mode *cli_mode) {

 if (hardware->dmi_detection==false) detect_dmi(hardware);

 if (hardware->is_dmi_valid==false) {
	 printf("No valid DMI table found, exiting.\n");
	 do_exit(cli_mode);
	 return;
 }
 printf("DMI Table version %d.%d found\n",hardware->dmi.dmitable.major_version,hardware->dmi.dmitable.minor_version);

 show_dmi_modules(hardware);
}


void show_dmi_base_board(struct s_hardware *hardware) {
 if (hardware->dmi.base_board.filled==false) {
	 printf("Base_board module not available\n");
	 return;
 }
 clear_screen();
 more_printf("Base board\n");
 more_printf(" Manufacturer : %s\n",hardware->dmi.base_board.manufacturer);
 more_printf(" Product Name : %s\n",hardware->dmi.base_board.product_name);
 more_printf(" Version      : %s\n",hardware->dmi.base_board.version);
 more_printf(" Serial       : %s\n",hardware->dmi.base_board.serial);
 more_printf(" Asset Tag    : %s\n",hardware->dmi.base_board.asset_tag);
 more_printf(" Location     : %s\n",hardware->dmi.base_board.location);
 more_printf(" Type         : %s\n",hardware->dmi.base_board.type);
 for (int i=0;i<BASE_BOARD_NB_ELEMENTS; i++) {
   if (((bool *)(& hardware->dmi.base_board.features))[i] == true) {
     more_printf(" %s\n", base_board_features_strings[i]);
    }
 }
}

void show_dmi_system(struct s_hardware *hardware) {
 if (hardware->dmi.system.filled==false) {
	 printf("System module not available\n");
	 return;
 }
 clear_screen();
 more_printf("System\n");
 more_printf(" Manufacturer : %s\n",hardware->dmi.system.manufacturer);
 more_printf(" Product Name : %s\n",hardware->dmi.system.product_name);
 more_printf(" Version      : %s\n",hardware->dmi.system.version);
 more_printf(" Serial       : %s\n",hardware->dmi.system.serial);
 more_printf(" UUID         : %s\n",hardware->dmi.system.uuid);
 more_printf(" Wakeup Type  : %s\n",hardware->dmi.system.wakeup_type);
 more_printf(" SKU Number   : %s\n",hardware->dmi.system.sku_number);
 more_printf(" Family       : %s\n",hardware->dmi.system.family);
}

void show_dmi_bios(struct s_hardware *hardware) {
 if (hardware->dmi.bios.filled==false) {
	 printf("Bios module not available\n");
	 return;
 }
 clear_screen();
 more_printf("BIOS\n");
 more_printf(" Vendor            : %s\n",hardware->dmi.bios.vendor);
 more_printf(" Version           : %s\n",hardware->dmi.bios.version);
 more_printf(" Release           : %s\n",hardware->dmi.bios.release_date);
 more_printf(" Bios Revision     : %s\n",hardware->dmi.bios.bios_revision);
 more_printf(" Firmware Revision : %s\n",hardware->dmi.bios.firmware_revision);
 more_printf(" Address           : 0x%04X0\n",hardware->dmi.bios.address);
 more_printf(" Runtime address   : %u %s\n",hardware->dmi.bios.runtime_size,hardware->dmi.bios.runtime_size_unit);
 more_printf(" Rom size          : %u %s\n",hardware->dmi.bios.rom_size,hardware->dmi.bios.rom_size_unit);

 for (int i=0;i<BIOS_CHAR_NB_ELEMENTS; i++) {
  if (((bool *)(& hardware->dmi.bios.characteristics))[i] == true) {
     more_printf(" %s\n", bios_charac_strings[i]);
  }
 }
 for (int i=0;i<BIOS_CHAR_X1_NB_ELEMENTS; i++) {
  if (((bool *)(& hardware->dmi.bios.characteristics_x1))[i] == true) {
      more_printf(" %s\n", bios_charac_x1_strings[i]);
  }
 }

 for (int i=0;i<BIOS_CHAR_X2_NB_ELEMENTS; i++) {
  if (((bool *)(& hardware->dmi.bios.characteristics_x2))[i] == true) {
       more_printf(" %s\n", bios_charac_x2_strings[i]);
  }
 }

}

void show_dmi_chassis(struct s_hardware *hardware) {
 if (hardware->dmi.chassis.filled==false) {
	 printf("Chassis module not available\n");
	 return;
 }
 clear_screen();
 more_printf("Chassis\n");
 more_printf(" Manufacturer       : %s\n",hardware->dmi.chassis.manufacturer);
 more_printf(" Type               : %s\n",hardware->dmi.chassis.type);
 more_printf(" Lock               : %s\n",hardware->dmi.chassis.lock);
 more_printf(" Version            : %s\n",hardware->dmi.chassis.version);
 more_printf(" Serial             : %s\n",hardware->dmi.chassis.serial);
 more_printf(" Asset Tag          : %s\n",hardware->dmi.chassis.asset_tag);
 more_printf(" Boot up state      : %s\n",hardware->dmi.chassis.boot_up_state);
 more_printf(" Power supply state : %s\n",hardware->dmi.chassis.power_supply_state);
 more_printf(" Thermal state      : %s\n",hardware->dmi.chassis.thermal_state);
 more_printf(" Security Status    : %s\n",hardware->dmi.chassis.security_status);
 more_printf(" OEM Information    : %s\n",hardware->dmi.chassis.oem_information);
 more_printf(" Height             : %u\n",hardware->dmi.chassis.height);
 more_printf(" NB Power Cords     : %u\n",hardware->dmi.chassis.nb_power_cords);
}

void show_dmi_cpu(struct s_hardware *hardware) {
 if (hardware->dmi.processor.filled==false) {
	 printf("Processor module not available\n");
	 return;
 }
 clear_screen();
 more_printf("CPU\n");
 more_printf(" Socket Designation : %s\n",hardware->dmi.processor.socket_designation);
 more_printf(" Type               : %s\n",hardware->dmi.processor.type);
 more_printf(" Family             : %s\n",hardware->dmi.processor.family);
 more_printf(" Manufacturer       : %s\n",hardware->dmi.processor.manufacturer);
 more_printf(" Version            : %s\n",hardware->dmi.processor.version);
 more_printf(" External Clock     : %u\n",hardware->dmi.processor.external_clock);
 more_printf(" Max Speed          : %u\n",hardware->dmi.processor.max_speed);
 more_printf(" Current Speed      : %u\n",hardware->dmi.processor.current_speed);
 more_printf(" Cpu Type           : %u\n",hardware->dmi.processor.signature.type);
 more_printf(" Cpu Family         : %u\n",hardware->dmi.processor.signature.family);
 more_printf(" Cpu Model          : %u\n",hardware->dmi.processor.signature.model);
 more_printf(" Cpu Stepping       : %u\n",hardware->dmi.processor.signature.stepping);
 more_printf(" Cpu Minor Stepping : %u\n",hardware->dmi.processor.signature.minor_stepping);
 //more_printf(" Voltage      %f\n",hardware->dmi.processor.voltage);
 more_printf(" Status             : %s\n",hardware->dmi.processor.status);
 more_printf(" Upgrade            : %s\n",hardware->dmi.processor.upgrade);
 more_printf(" Cache L1 Handle    : %s\n",hardware->dmi.processor.cache1);
 more_printf(" Cache L2 Handle    : %s\n",hardware->dmi.processor.cache2);
 more_printf(" Cache L3 Handle    : %s\n",hardware->dmi.processor.cache3);
 more_printf(" Serial             : %s\n",hardware->dmi.processor.serial);
 more_printf(" Part Number        : %s\n",hardware->dmi.processor.part_number);
 more_printf(" ID                 : %s\n",hardware->dmi.processor.id);
 for (int i=0;i<PROCESSOR_FLAGS_ELEMENTS; i++) {
      if (((bool *)(& hardware->dmi.processor.cpu_flags))[i] == true) {
            more_printf(" %s\n", cpu_flags_strings[i]);
      }
 }

}
