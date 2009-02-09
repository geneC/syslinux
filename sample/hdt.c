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

/*
 * hdt.c
 *
 * An Hardware Detection Tool
 */

#include <string.h>
#include <stdio.h>
#include <console.h>
#include "com32io.h"
#include "menu.h"
#include "help.h"
#include "passwords.h"
#include "cpuid.h"

unsigned char MAIN_MENU, CPU_MENU;

#define INFLINE 22
#define PWDLINE 3
#define PWDPROMPT 21
#define PWDCOLUMN 60
#define PWDATTR 0x74
#define EDITPROMPT 21

TIMEOUTCODE ontimeout()
{
	  beep();
	    return CODE_WAIT;
}

void keys_handler(t_menusystem *ms, t_menuitem *mi,unsigned int scancode)
{
   char nc;

   if ((scancode >> 8) == F1) { // If scancode of F1
      runhelpsystem(mi->helpid);
   }
   // If user hit TAB, and item is an "executable" item
   // and user has privileges to edit it, edit it in place.
   if (((scancode & 0xFF) == 0x09) && (mi->action == OPT_RUN)) {
//(isallowed(username,"editcmd") || isallowed(username,"root"))) {
     nc = getnumcols();
     // User typed TAB and has permissions to edit command line
     gotoxy(EDITPROMPT,1,ms->menupage);
     csprint("Command line:",0x07);
     editstring(mi->data,ACTIONLEN);
     gotoxy(EDITPROMPT,1,ms->menupage);
     cprint(' ',0x07,nc-1,ms->menupage);
   }
}

void compute_processor(unsigned char *menu) {
  s_cpu cpu;
  char buffer[MENULEN];
  char buffer1[MENULEN];
  printf("Detecting CPU\n");
  detect_cpu(&cpu);
  *menu = add_menu(" Main Processor ",-1);
  snprintf(buffer,MENULEN,"Vendor    : %s",cpu.vendor);
  add_item(buffer,"Vendor",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Model     : %s",cpu.model);
  add_item(buffer,"Model",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Vendor ID : %d",cpu.vendor_id);
  add_item(buffer,"Vendor ID",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Family ID : %d",cpu.family);
  add_item(buffer,"Family ID",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Model  ID : %d",cpu.model_id);
  add_item(buffer,"Model ID",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Stepping  : %d",cpu.stepping);
  add_item(buffer,"Stepping",OPT_INACTIVE,NULL,0);

  if (cpu.flags.smp)  snprintf(buffer,MENULEN,"SMP       : Yes");
  else snprintf(buffer,MENULEN,"SMP       : No");
  add_item(buffer,"SMP system",OPT_INACTIVE,NULL,0);

  if (cpu.flags.lm)  snprintf(buffer,MENULEN,"x86_64    : Yes");
  else snprintf(buffer,MENULEN,"X86_64     : No");
  add_item(buffer,"x86_64 compatible processor",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu.flags.fpu) strcat(buffer1,"fpu ");
  if (cpu.flags.vme) strcat(buffer1,"vme ");
  if (cpu.flags.de)  strcat(buffer1,"de ");
  if (cpu.flags.pse) strcat(buffer1,"pse ");
  if (cpu.flags.tsc) strcat(buffer1,"tsc ");
  if (cpu.flags.msr) strcat(buffer1,"msr ");
  if (cpu.flags.pae) strcat(buffer1,"pae ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu.flags.mce) strcat(buffer1,"mce ");
  if (cpu.flags.cx8) strcat(buffer1,"cx8 ");
  if (cpu.flags.apic) strcat(buffer1,"apic ");
  if (cpu.flags.sep) strcat(buffer1,"sep ");
  if (cpu.flags.mtrr) strcat(buffer1,"mtrr ");
  if (cpu.flags.pge) strcat(buffer1,"pge ");
  if (cpu.flags.mca) strcat(buffer1,"mca ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu.flags.cmov) strcat(buffer1,"cmov ");
  if (cpu.flags.pat)  strcat(buffer1,"pat ");
  if (cpu.flags.pse_36) strcat(buffer1,"pse_36 ");
  if (cpu.flags.psn)  strcat(buffer1,"psn ");
  if (cpu.flags.clflsh) strcat(buffer1,"clflsh ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu.flags.dts)  strcat(buffer1,"dts ");
  if (cpu.flags.acpi) strcat(buffer1,"acpi ");
  if (cpu.flags.mmx)  strcat(buffer1,"mmx ");
  if (cpu.flags.sse)  strcat(buffer1,"sse ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu.flags.sse2) strcat(buffer1,"sse2 ");
  if (cpu.flags.ss)   strcat(buffer1,"ss ");
  if (cpu.flags.htt)  strcat(buffer1,"ht ");
  if (cpu.flags.acc)  strcat(buffer1,"acc ");
  if (cpu.flags.syscall) strcat(buffer1,"syscall ");
  if (cpu.flags.mp)   strcat(buffer1,"mp ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu.flags.nx)    strcat(buffer1,"nx ");
  if (cpu.flags.mmxext) strcat(buffer1,"mmxext ");
  if (cpu.flags.lm)     strcat(buffer1,"lm ");
  if (cpu.flags.nowext) strcat(buffer1,"3dnowext ");
  if (cpu.flags.now)    strcat(buffer1,"3dnow! ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

}

int main(void)
{
  openconsole(&dev_stdcon_r, &dev_stdcon_w);
  t_menuitem * *prepopt;

  init_menusystem(NULL);
  set_window_size(1,1,23,78); // Leave some space around

 // Register the menusystem handler
//  reg_handler(HDLR_SCREEN,&msys_handler);
  reg_handler(HDLR_KEYS,&keys_handler);

  // Register the ontimeout handler, with a time out of 10 seconds
  reg_ontimeout(ontimeout,1000,0);

  compute_processor(&CPU_MENU);
  MAIN_MENU = add_menu(" Main Menu ",8);
  set_item_options(-1,24);
  add_item("<P>rocessor","Main Processor",OPT_SUBMENU,NULL,CPU_MENU);

  showmenus(MAIN_MENU);

  return 0;
}
