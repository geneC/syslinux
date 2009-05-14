#include <com32.h>
#include <stddef.h>

void __cdecl core_intcall(uint8_t, const com32sys_t *, com32sys_t *);
void __cdecl core_farcall(uint32_t, const com32sys_t *, com32sys_t *);
int __cdecl core_cfarcall(uint32_t, const void *, uint32_t);

void myputchar(int c)
{
#if 1
  static com32sys_t ireg;

  ireg.eax.b[1] = 0x0e;
  ireg.eax.b[0] = c;
  ireg.ebx.w[0] = 0x0007;
  core_intcall(0x10, &ireg, NULL);
#else
  static uint16_t *vram = (void *)0xb8000;

  *vram++ = (uint8_t)c + 0x1f00;
#endif
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
