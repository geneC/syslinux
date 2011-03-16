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
#include <core-elf.h>
#include <syslinux/adv.h>
#include <syslinux/config.h>
#include <sys/module.h>

#include "menu.h"

static int mytest_main(int argc, char **argv)
{
    console_ansi_raw();
    //edit_cmdline("",1);
    menu_main(argc, argv);
    return 0;
}

MODULE_MAIN(mytest_main);
