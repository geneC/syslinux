#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <console.h>
#include <dprintf.h>
#include <com32.h>
#include <syslinux/adv.h>
#include <syslinux/config.h>
#include <setjmp.h>
#include <linux/list.h>
#include <netinet/in.h>
#include <sys/cpu.h>

#include <sys/exec.h>
#include <sys/module.h>
#include "common.h"
#include "menu.h"

#define LDLINUX	"ldlinux.c32"

typedef void (*constructor_t) (void);
constructor_t __ctors_start[], __ctors_end[];

extern char __dynstr_start[];
extern char __dynstr_len[], __dynsym_len[];
extern char __dynsym_start[];
extern char __got_start[];
extern Elf32_Dyn __dynamic_start[];
extern Elf32_Word __gnu_hash_start[];

struct elf_module core_module = {
    .name		= "(core)",
    .shallow		= true,
    .required		= LIST_HEAD_INIT((core_module.required)),
    .dependants		= LIST_HEAD_INIT((core_module.dependants)),
    .list		= LIST_HEAD_INIT((core_module.list)),
    .module_addr	= (void *)0x0,
    .base_addr		= (Elf32_Addr) 0x0,
    .ghash_table	= __gnu_hash_start,
    .str_table		= __dynstr_start,
    .sym_table		= __dynsym_start,
    .got		= __got_start,
    .dyn_table		= __dynamic_start,
    .strtable_size	= (size_t) __dynstr_len,
    .syment_size	= sizeof(Elf32_Sym),
    .symtable_size	= (size_t) __dynsym_len
};

/*
	Initializes the module subsystem by taking the core module ( shallow module ) and placing
	it on top of the modules_head_list. Since the core module is initialized when declared
	we technically don't need the exec_init() and module_load_shallow() procedures
*/
void init_module_subsystem(struct elf_module *module)
{
    list_add(&module->list, &modules_head);
}

/* call_constr: initializes sme things related */
static void call_constr(void)
{
	constructor_t *p;

	for (p = __ctors_start; p < __ctors_end; p++)
		(*p) ();
}

/* note to self: do _*NOT*_ use static key word on this function */
void load_env32(com32sys_t * regs)
{
	char *argv[] = { LDLINUX, NULL };

	dprintf("Starting 32 bit elf module subsystem...\n");
	call_constr();

	init_module_subsystem(&core_module);

	module_load_dependencies(LDLINUX, "modules.dep");
	spawn_load(LDLINUX, 1, argv);
}
