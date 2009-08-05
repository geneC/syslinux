#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <minmax.h>
#include <console.h>
#include <consoles.h>
#include <alloca.h>
#include <inttypes.h>
#include <colortbl.h>
#include <getkey.h>
#include <setjmp.h>
#include <limits.h>
#include <com32.h>
#include <syslinux/adv.h>
#include <syslinux/config.h>
#include <sys/module.h>

#include "menu.h"

int n=10;

static int mytest_main(int argc, char **argv)
{
	openconsole(&dev_stdcon_r, &dev_stdcon_w);
	//openconsole(&dev_rawcon_r, &dev_vesaserial_w);
	menu_main(argc, argv);
	//printf("Something's fishy...\n");

	return 0;
}

MODULE_MAIN(mytest_main);
