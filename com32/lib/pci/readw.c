#include <inttypes.h>
#include <sys/io.h>
#include <sys/pci.h>

uint16_t pci_readw(pciaddr_t a)
{
  uint32_t oldcf8 = inl(0xcf8);
  uint16_t r;

  outl(a, 0xcf8);
  r = inw(0xcfc + (a & 3));
  outl(oldcf8, 0xcf8);

  return r;
}
