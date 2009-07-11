#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <string.h>

#include <sys/module.h>
#include <sys/exec.h>

typedef void (*constructor_t)(void);
constructor_t __ctors_start[], __ctors_end[];

/*
	call_constr: initializes sme things related
*/
void call_constr()
{
	constructor_t *p;
	printf("begin\n");
	for (p = __ctors_start; p < __ctors_end; p++)
	{
		(*p)();
	}
	printf("end\n");
}
/* note to self: do _*NOT*_ use static key word on this function */
void load_env32()
{
	char *screen;
    	screen = (char *)0xB8000;
	*(screen+2) = 'Q';
    	*(screen+3) = 0x1C;
	openconsole(&dev_stdcon_r, &dev_stdcon_w);
	printf("Calling initilization constructor procedures...\n");
	call_constr();
	printf("Starting 32 bit elf environment...\n");
	exec_init();
	char *str=malloc(12*sizeof(char));
	strcpy(str,"hello :)");
	printf("%s ",str);

	while(1) 1; /* we don't have anything better to do so hang around for a bit */
}

