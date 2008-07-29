/*
 * The first prototype of an ELF module, inspired from the Linux kernel
 * module system.
 */

#include <stdio.h>

typedef int (*module_init_t)(void);
typedef void (*module_exit_t)(void);

#define __used				__attribute__((used))

#define MODULE_INIT(fn)	static module_init_t __module_init \
	__used __attribute__((section(".ctors_module")))  = fn

#define MODULE_EXIT(fn) static module_exit_t __module_exit \
	__used __attribute__((section(".dtors_module")))  = fn

static int hello_init(void) {
	printf("Hello, world, from 0x%08X!\n", (unsigned int)&hello_init);
	return 0;
}

static void hello_exit(void) {
	printf("Good bye, cruel world!\n");
}

MODULE_INIT(hello_init);
MODULE_EXIT(hello_exit);
