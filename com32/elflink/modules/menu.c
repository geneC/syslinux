/*
 * menu.c -- simple program to test menu_main()
 */

#include <stdio.h>
#include <stdlib.h>
#include <core-elf.h>

#include "menu.h"

/*
 * useage: menu.c32 [config file]
 */
int menu(int argc, char **argv)
{
	menu_main(argc, argv);
	return 0;
}
