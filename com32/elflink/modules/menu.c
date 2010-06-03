/*
 * menu.c -- simple program to test menu_main()
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/module.h>

#include "menu.h"

/*
 * useage: menu.c32 [config file]
 */
static int menu(int argc, char **argv)
{
	menu_main(argc, argv);
	return 0;
}
MODULE_MAIN(menu);
