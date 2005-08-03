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

enum pci_config_type {
  PCI_CFG_AUTO		= 0,	/* autodetect */
  PCI_CFG_TYPE1		= 1,
  PCI_CFG_TYPE2		= 2,
  PCI_CFG_BIOS          = 3,
};

void pci_set_config_type(enum pci_config_type);

uint8_t pci_readb(pciaddr_t);
uint16_t pci_readw(pciaddr_t);
uint32_t pci_readl(pciaddr_t);
void pci_writeb(uint8_t, pciaddr_t);
void pci_writew(uint16_t, pciaddr_t);
void pci_writel(uint32_t, pciaddr_t);

#endif /* _SYS_PCI_H */
