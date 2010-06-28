#include <stddef.h>
#include <com32.h>
#include <stdio.h>
#include <string.h>

#include "core.h"

#include <console.h>

static int console_init = 0;

void myputchar(int c)
{
    static com32sys_t ireg;

    if (c == '\n')
	myputchar('\r');

    ireg.eax.b[1] = 0x02;
    ireg.edx.b[0] = c;
    __intcall(0x21, &ireg, NULL);
}

void myputs(const char *str)
{
    while (*str)
	myputchar(*str++);
}

void hello(void)
{
    static char hello_str[] = "Hello, World!";

    printf("%s from (%s)\n", hello_str, __FILE__);  /* testing */
}

void hexdump(void *buf, int bytelen, const char *str)
{
	unsigned int *p32, i;
	
	if (str)
		printf("Dump %s:\n", str);
		
	p32 = (unsigned int *)buf;
	for (i = 0; i < (bytelen / 4); i++){
		printf(" 0x%08x ", p32[i]);
	}
	printf("\n\n");		
}

static inline void myprint(int num)
{
	uint32_t i;

	for (i = 0; i < 5; i ++)
		printf("%d", num);
	printf("\n");
}

void mp1(void)
{
	myprint(1);
}

void mp2(void)
{
	myprint(2);
}

void mp3(void)
{
	myprint(3);
}

void mp4(void)
{
	myprint(4);
}

void mp5(void)
{
	myprint(5);
}

void printf_init(void)
{
	openconsole(&dev_null_r, &dev_stdcon_w);
}

