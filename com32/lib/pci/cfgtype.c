#include "pci/pci.h"
#include <com32.h>
#include <string.h>

enum pci_config_type __pci_cfg_type;

void pci_set_config_type(enum pci_config_type type)
{
  uint32_t oldcf8;
  static const com32sys_t ireg = {
    .eax.l    = 0xb101,
    .edi.l    = 0,
    .eflags.l = EFLAGS_CF,
  };
  com32sys_t oreg;

  if ( type == PCI_CFG_AUTO ) {
    /* Try to detect PCI BIOS */
    __intcall(0x1a, &ireg, &oreg);

    if ( !(oreg.eflags.l & EFLAGS_CF) &&
	 oreg.eax.b[1] == 0 && oreg.edx.l == 0x20494250 ) {
      /* Use CM#1 if it is present, otherwise BIOS calls. CM#2 is evil. */
      type = (oreg.eax.b[0] & 1) ? PCI_CFG_TYPE1 : PCI_CFG_BIOS;
    } else {
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
  }

  __pci_cfg_type = type;
}
