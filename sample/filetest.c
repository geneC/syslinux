#include <com32.h>

#define NULL ((void *)0)

static inline void memset(void *buf, int ch, unsigned int len)
{
  asm volatile("cld; rep; stosb"
               : "+D" (buf), "+c" (len) : "a" (ch) : "memory");
}

static void strcpy(char *dst, const char *src)
{
  while ( *src )
    *dst++ = *src++;

  *dst = '\0';
}

int __start(void)
{
  unsigned int ax,cx,dx,es,si,di,t;
  com32sys_t  inreg,outreg;
  
  memset(&inreg, 0, sizeof inreg);
  memset(&outreg, 0, sizeof outreg);
  strcpy(__com32.cs_bounce, "test.txt");
  inreg.eax.w[0] = 0x0006;  // Open file
  inreg.esi.w[0] = OFFS(__com32.cs_bounce);
  inreg.es = SEG(__com32.cs_bounce);
  __com32.cs_intcall(0x22, &inreg, &outreg);
  
  si = outreg.esi.w[0];
  cx = outreg.ecx.w[0];
  ax = outreg.eax.l;

  if ( ax > 65536 )
    ax = 65536;			/* Max in one call */

  while ( si ) {
    t = (ax+cx-1)/cx;
  
    memset(&inreg, 0, sizeof inreg);
    inreg.esi.w[0] = si;
    inreg.ecx.w[0] = t;
    inreg.eax.w[0] = 0x0007;  // Read file
    inreg.ebx.w[0] = OFFS(__com32.cs_bounce);
    inreg.es = SEG(__com32.cs_bounce);
    __com32.cs_intcall(0x22, &inreg, &inreg);
    si = inreg.esi.w[0];

    // This is broken if we hit null, but works for (DOS) text files
    memset(&inreg, 0, sizeof inreg);
    inreg.eax.w[0] = 0x0002;
    inreg.ebx.w[0] = OFFS(__com32.cs_bounce);
    inreg.es = SEG(__com32.cs_bounce);
    __com32.cs_intcall(0x22, &inreg, NULL);
  }
  
  return 0;
}
