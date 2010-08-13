#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <console.h>

//#include <syslinux/loadfile.h>
//#include <syslinux/linux.h>
//#include <syslinux/pxe.h>

#include "core.h"
#include "core-elf.h"
#include "menu.h"

void abort_load_new(com32sys_t *reg)
{
	char *str;

	str = (void *)reg->esi.l;

	printf("Error!\n");
	if (str)
		printf("%s\n", str);

	if (onerrorlen)
		execute(start_menu->onerror, KT_NONE);
	enter_cmdline();
	return;
}
