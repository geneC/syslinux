#include <inttypes.h>
#include <sys/io.h>
#include <sys/pci.h>

uint32_t pci_readl(pciaddr_t a)
{
  uint32_t oldcf8 = inl(0xcf8);
  uint32_t r;

  outl(a, 0xcf8);
  r = inl(0xcfc + (a & 3));
  outl(oldcf8, 0xcf8);

  return r;
}
