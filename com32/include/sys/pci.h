#ifndef _SYS_PCI_H
#define _SYS_PCI_H

#include <inttypes.h>
#include <sys/io.h>

typedef uint32_t pciaddr_t;

static inline pciaddr_t pci_mkaddr(uint32_t bus, uint32_t dev,
				   uint32_t func, uint32_t reg)
{
  return 0x80000000 | ((bus & 0xff) << 16) | ((dev & 0x1f) << 11) |
    ((func & 0x07) << 8) | (reg & 0xff);
}

uint8_t pci_read8(pciaddr_t a);
uint16_t pci_read16(pciaddr_t a);
uint32_t pci_read32(pciaddr_t a);
void pci_write8(uint8_t v, pciaddr_t a);
void pci_write16(uint16_t v, pciaddr_t a);
void pci_write32(uint32_t v, pciaddr_t a);

#endif /* _SYS_PCI_H */
