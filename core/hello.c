#include <stddef.h>
#include <com32.h>
#include <stdio.h>
#include <string.h>

void itoa(char *str, int num)
{
    char buf[10];
    int i = 0;
    
    do {
        buf[i++] = num % 10 + 0x30;
    }while ( num /= 10 );

    str[i] = '\0';
    for (; i > 0; i -- )
        *str++ = buf[i-1];
}

void myputchar(int c)
{
    static com32sys_t ireg;
    //static uint16_t *vram = 0xb8000;

    ireg.eax.b[1] = 0x02;
    ireg.edx.b[0] = c;
    __intcall(0x21, &ireg, NULL);

    //*vram++ = c + 0x0700;
}

void myputs(const char *str)
{
    while (*str)
	myputchar(*str++);
}

void hello(void)
{
    static char hello_str[] = "Hello, World!  (hello.c)\r\n";

    myputs(hello_str);
}
