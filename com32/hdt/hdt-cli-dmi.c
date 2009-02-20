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
#include <string.h>

void main_show_dmi(struct s_hardware *hardware) {
 char available_dmi_commands[1024];
 memset(available_dmi_commands,0,sizeof available_dmi_commands);

 if (hardware->dmi_detection==false) {
	          detect_dmi(hardware);
 }

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
 printf("Available commands: %s\n",available_dmi_commands);
}
