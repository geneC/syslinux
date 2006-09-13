#ifndef _SYS_PCI_H
#define _SYS_PCI_H

#include <inttypes.h>
#include <sys/io.h>

#define MAX_VENDOR_NAME_SIZE 255
#define MAX_PRODUCT_NAME_SIZE 255
#define MAX_PCI_DEVICES  32 
#define MAX_PCI_BUSES   255

typedef uint32_t pciaddr_t;

typedef struct {
	char	 vendor_name[MAX_VENDOR_NAME_SIZE];
	uint16_t vendor;
	char	 product_name[MAX_PRODUCT_NAME_SIZE];
	uint16_t product;
	uint16_t sub_vendor;
	uint16_t sub_product;
	uint8_t  revision;
} s_pci_device;

typedef struct {
	uint16_t id;
	s_pci_device *pci_device[MAX_PCI_DEVICES];
	uint8_t pci_device_count;
} s_pci_bus;

typedef struct {
	s_pci_device  pci_device[MAX_PCI_DEVICES];
	uint8_t count;
} s_pci_device_list;

typedef struct {
	s_pci_bus pci_bus[MAX_PCI_BUSES];
	uint8_t count;
} s_pci_bus_list;

static inline pciaddr_t pci_mkaddr(uint32_t bus, uint32_t dev,
				   uint32_t func, uint32_t reg)
{
  return 0x80000000 | ((bus & 0xff) << 16) | ((dev & 0x1f) << 11) |
    ((func & 0x07) << 8) | (reg & 0xff);
}

enum pci_config_type {
  PCI_CFG_NONE          = -1,	/* badness */
  PCI_CFG_AUTO		= 0,	/* autodetect */
  PCI_CFG_TYPE1		= 1,
  PCI_CFG_TYPE2		= 2,
  PCI_CFG_BIOS          = 3,
};

enum pci_config_type pci_set_config_type(enum pci_config_type);

uint8_t pci_readb(pciaddr_t);
uint16_t pci_readw(pciaddr_t);
uint32_t pci_readl(pciaddr_t);
void pci_writeb(uint8_t, pciaddr_t);
void pci_writew(uint16_t, pciaddr_t);
void pci_writel(uint32_t, pciaddr_t);

extern int pci_scan(s_pci_bus_list *pci_bus_list, s_pci_device_list *pci_device_list);
#endif /* _SYS_PCI_H */
