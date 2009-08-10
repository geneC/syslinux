#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"

void enter_cmdline(char * cmdline)
{
	int cmdlen;
	kernel_type kt=KT_NONE;
	fgets(cmdline,MAX_CMDLINE_LEN,stdin);
	if((cmdlen=strlen(cmdline))<=3) execute(cmdline,KT_NONE);
	else
	{
		if(!strcmp(cmdline,".com")) 1;
		else if(!strcmp(cmdline,".cbt")) 1;
		else if(!strcmp(cmdline,".c32")) 1;
		else if(!strcmp(cmdline,".bs")) 1;
		else if(!strcmp(cmdline,".0")) 1;
		else if(!strcmp(cmdline,".bin")) 1;
		else if(!strcmp(cmdline,".bss")) 1;
		else if(!strcmp(cmdline,".img")) 1;
	}
}
