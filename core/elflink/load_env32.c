#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <console.h>
#include <com32.h>
#include <syslinux/adv.h>
#include <syslinux/config.h>
#include <setjmp.h>
#include <linux/list.h>
#include <netinet/in.h>	

#include <sys/exec.h>
#include <sys/module.h>
#include "common.h"
#include "menu.h"
#include "cli.h"

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
	.name = 		"(core)",
	.shallow = 		1,
	.required= 		LIST_HEAD_INIT( (core_module.required) ),
	.dependants= 		LIST_HEAD_INIT( (core_module.dependants) ),
	.list= 			LIST_HEAD_INIT( (core_module.list) ),
	.module_addr = 		(void *)0x0,
	.base_addr = 		(Elf32_Addr)0x0,
	.ghash_table = 		__gnu_hash_start,
	.str_table = 		__dynstr_start,
	.sym_table = 		__dynsym_start,
	.got = 			__got_start,
	.dyn_table = 		__dynamic_start,
	.strtable_size = 	(size_t)__dynstr_len,
	.syment_size =		sizeof(Elf32_Sym),
	.symtable_size = 	(size_t)__dynsym_len
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
void load_env32(com32sys_t * regs)
{
	call_constr();
	char *cmdline;
	struct cli_command c1,c2,c3,*comm,*aux;
	openconsole(&dev_rawcon_r, &dev_ansiserial_w);
	INIT_LIST_HEAD(&cli_history_head);

	printf("Calling initilization constructor procedures...\n");
	printf("Starting 32 bit elf module subsystem...\n");
	init_module_subsystem(&core_module);

	printf("Str table address: %d\n",core_module.str_table);
	printf("Sym table address: %d\n",core_module.sym_table);
	printf("Str table size: %d\n",core_module.strtable_size);
	printf("Sym table size: %d\n",core_module.symtable_size);


	c1.command=(char*)malloc(sizeof(char)*16);
	c2.command=(char*)malloc(sizeof(char)*16);
	c3.command=(char*)malloc(sizeof(char)*16);
	strcpy(c1.command,"command 1");
	strcpy(c2.command,"command 2");
	strcpy(c3.command,"command 3");
	list_add(&(c1.list),&cli_history_head);
	list_add(&(c2.list),&cli_history_head);
	list_add(&(c3.list),&cli_history_head);
	

	/*list_for_each_entry(aux,&cli_history_head,list)
	{
		printf("%s\n",aux->command);
	}*/
	
	while(1)
	{
		cmdline=edit_cmdline("",1,NULL,NULL);
		aux=list_entry(cli_history_head.next, typeof(*aux),list);
		if(strcmp(aux->command,cmdline))		
		{
			comm=(struct cli_command*)malloc(sizeof(struct cli_command*));
			comm->command=(char*)malloc(sizeof(char)*(strlen(cmdline)+1));
			strcpy(comm->command,cmdline);
			list_add(&(comm->list),&cli_history_head);
			process_command(cmdline);
		}
	}

	int i,n=5;
	char **argv;
	argv=(char**)calloc(n,sizeof(char*));
	argv[0]=(char*)calloc(100,sizeof(char));//(char *)(regs->edi.w[0]);
	argv[1]=(char*)calloc(100,sizeof(char));//(char *)(regs->edi.w[0]);
	//strcpy(argv[1],(regs->edi.w[0]));
	strcpy(argv[0],"extlinux.conf");
	strcpy(argv[1],"extlinux.conf");
	
	printf("%d\n",load_library("dyn/sort.dyn"));
	printf("Loading ansiraw.c32\n%d\n",load_library("dyn/ansiraw.c32"));
	printf("Loading background.c32\n%d\n",load_library("dyn/background.c32"));
	printf("Loading printmsg.c32\n%d\n",load_library("dyn/printmsg.c32"));
	printf("Loading drain.c32\n%d\n",load_library("dyn/drain.c32"));
	printf("Loading sha1hash.c32\n%d\n",load_library("dyn/sha1hash.c32"));
	printf("Loading unbase64.c32\n%d\n",load_library("dyn/unbase64.c32"));
	printf("Loading sha512crypt\n%d\n",load_library("dyn/sha512crypt.c32"));
	printf("Loading sha256crypt\n%d\n",load_library("dyn/sha256crypt.c32"));
	printf("Loading md5.c32\n%d\n",load_library("dyn/md5.c32"));
	printf("Loading crypt-md5.c32\n%d\n",load_library("dyn/crypt-md5.c32"));
	printf("Loading passwd.c32\n%d\n",load_library("dyn/passwd.c32"));
	printf("Loading get_key.c32\n%d\n",load_library("dyn/get_key.c32"));
	//printf("Loading menumain.c32\n%d\n",load_library("dyn/menumain.c32"));
	spawn_load("dyn/menumain.c32",0);

	/*printf("\n\nTrying to spawn 'dyn/hello.dyn'\n\n"); 
	spawnv("dyn/hello.dyn",0);
	printf("\nTest done\n");*/
	//printf("%d\n",spawnv("mytest.c32",argv));
	spawn_load("mytest.c32",argv);
	//spawnl("mytest.c32", "mytest",(regs->edi.w[0]), NULL);
	//printf("Done\n");
	
	while(1) 1; /* we don't have anything better to do so hang around for a bit */
}

