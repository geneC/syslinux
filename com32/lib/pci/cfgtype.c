#include "pci/pci.h"

enum pci_config_type __pci_cfg_type;

void pci_set_config_type(enum pci_config_type type)
{
  uint32_t oldcf8;

  if ( type == PCI_CFG_AUTO ) {
    /* Try to detect CM #1 */
    cli();
    oldcf8 = inl(0xcf8);
    outl(~0, 0xcf8);
    if ( inl(0xcf8) == pci_mkaddr(255,31,7,252) )
      type = PCI_CFG_TYPE1;
    else
      type = PCI_CFG_TYPE2;	/* ... it better be ... */
    outl(oldcf8, 0xcf8);
    sti();
  }

  __pci_cfg_type = type;
}
