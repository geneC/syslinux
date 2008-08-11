/*
 * The first prototype of an ELF module, inspired from the Linux kernel
 * module system.
 */

#include <stdio.h>
#include <sys/module.h>


static int hello_init(void) {
	printf("Hello, world, from 0x%08X!\n", (unsigned int)&hello_init);
	return 0;
}

static void hello_exit(void) {
	printf("Good bye, cruel world!\n");
}

MODULE_INIT(hello_init);
MODULE_EXIT(hello_exit);
