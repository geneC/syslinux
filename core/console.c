#include <stddef.h>
#include <com32.h>
#include <stdio.h>
#include <string.h>

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
