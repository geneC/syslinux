#include <stddef.h>
#include "core.h"

void myputchar(int c)
{
  static com32sys_t ireg;
  static uint16_t *vram = 0xb8000;

  ireg.eax.b[1] = 0x02;
  ireg.edx.b[0] = c;
  core_intcall(0x21, &ireg, NULL);

  *vram++ = c + 0x1f00;
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
