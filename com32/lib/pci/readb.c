#include <inttypes.h>
#include <sys/io.h>
#include <sys/pci.h>

uint8_t pci_readb(pciaddr_t a)
{
  uint32_t oldcf8 = inl(0xcf8);
  uint8_t r;

  outl(a, 0xcf8);
  r = inb(0xcfc + (a & 3));
  outl(oldcf8, 0xcf8);

  return r;
}
