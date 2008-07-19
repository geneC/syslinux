#ifndef _SYS_PCI_H
#define _SYS_PCI_H

#include <inttypes.h>
#include <sys/io.h>

#define MAX_PCI_FUNC      8
#define MAX_PCI_DEVICES  32
#define MAX_PCI_BUSES   256

typedef uint32_t pciaddr_t;

/* a structure for extended pci information */
struct pci_dev_info {
	char     vendor_name[255];
	char     product_name[255];
	char	 linux_kernel_module[64];
};

/* a struct to represent a pci device */
struct pci_device {
	uint16_t vendor;
	uint16_t product;
	uint16_t sub_vendor;
	uint16_t sub_product;
	uint8_t  revision;
	struct pci_dev_info *pci_dev_info;
};

struct pci_bus {
	uint16_t id;
	struct pci_device *pci_device[MAX_PCI_DEVICES * MAX_PCI_FUNC];
	uint32_t pci_device_count;
};

struct pci_device_list {
	struct pci_device pci_device[MAX_PCI_BUSES * MAX_PCI_DEVICES * MAX_PCI_FUNC];
	uint32_t count;
};

struct pci_bus_list {
	struct pci_bus pci_bus[MAX_PCI_BUSES];
	uint32_t count;
};

struct match {
  struct match *next;
  uint32_t did;
  uint32_t did_mask;
  uint32_t sid;
  uint32_t sid_mask;
  uint8_t rid_min, rid_max;
  char *filename;
};

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

extern int pci_scan(struct pci_bus_list *pci_bus_list, struct pci_device_list *pci_device_list);
extern struct match * find_pci_device(struct pci_device_list *pci_device_list, struct match *list);
extern void get_name_from_pci_ids(struct pci_device_list *pci_device_list);
extern void get_module_name_from_pci_ids(struct pci_device_list *pci_device_list);
#endif /* _SYS_PCI_H */
