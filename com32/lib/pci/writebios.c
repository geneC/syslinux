#include <com32.h>
#include <string.h>
#include "pci/pci.h"

void __pci_write_bios(uint32_t call, uint32_t v, pciaddr_t a)
{
  com32sys_t rs;
  memset(&rs, 0, sizeof rs);
  rs.eax.w[0] = call;
  rs.ebx.w[0] = a >> 8;		/* bus:device:function */
  rs.edi.b[0] = a;		/* address:reg */
  rs.ecx.l = v;
  __intcall(0x1a, &rs, NULL);
}
