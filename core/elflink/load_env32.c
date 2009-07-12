#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <string.h>

#include <sys/module.h>
#include <sys/exec.h>

typedef void (*constructor_t)(void);
constructor_t __ctors_start[], __ctors_end[];

extern char 		__dynstr_start[];
extern char 		__dynstr_len[], __dynsym_len[];
extern char 		__dynsym_start[];
extern char		__got_start[];
extern Elf32_Dyn 	__dynamic_start[];
extern Elf32_Word	__gnu_hash_start[];

struct elf_module core_module =
{
	.name = "(core)",
	.shallow = 1,
	.required=LIST_HEAD_INIT(required),
	.dependants=LIST_HEAD_INIT(dependants),
	.list=LIST_HEAD_INIT(list),
	.module_addr = (void *)0,
	.base_addr = 0,
	.ghash_table = __gnu_hash_start,
	.str_table = __dynstr_start,
	.sym_table = (void *)__dynsym_start,
	.got = (void *)__got_start,
	.dyn_table = __dynamic_start,
	.strtable_size = (size_t)__dynstr_len,
	.symtable_size = (size_t)__dynsym_len
};

/*
	call_constr: initializes sme things related
*/
static void call_constr()
{
	constructor_t *p;
	for (p = __ctors_start; p < __ctors_end; p++)
	{
		(*p)();
	}
}
/* note to self: do _*NOT*_ use static key word on this function */
void load_env32()
{
	char *screen=0;
    	screen = (char *)0xB8000;
	*(screen+2) = 'Q';
    	*(screen+3) = 0x1C;
	openconsole(&dev_stdcon_r, &dev_stdcon_w);
	printf("Calling initilization constructor procedures...\n");
	call_constr();
	printf("Starting 32 bit elf environment...\n");
	exec_init();
	char *str=malloc(16*sizeof(char));
	strcpy(str,"malloc works :)");
	printf("%s ",str);
	free(str);

	while(1) 1; /* we don't have anything better to do so hang around for a bit */
}

