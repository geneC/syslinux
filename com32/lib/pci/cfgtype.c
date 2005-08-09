#include "pci/pci.h"
#include <com32.h>
#include <string.h>

enum pci_config_type __pci_cfg_type;

int pci_set_config_type(enum pci_config_type type)
{
  static const com32sys_t ireg = {
    .eax.l    = 0xb101,
    .edi.l    = 0,
    .eflags.l = EFLAGS_CF,
  };
  com32sys_t oreg;

  if ( type == PCI_CFG_AUTO ) {
    type = PCI_CFG_NONE;

    /* Try to detect PCI BIOS */
    __intcall(0x1a, &ireg, &oreg);

    if ( !(oreg.eflags.l & EFLAGS_CF) &&
	 oreg.eax.b[1] == 0 && oreg.edx.l == 0x20494250 ) {
      /* PCI BIOS present.  Use direct access if we know how to do it. */
      if ( oreg.eax.b[0] & 1 )
	type = PCI_CFG_TYPE1;
      else if ( oreg.eax.b[0] & 2 )
	type = PCI_CFG_TYPE2;
      else
	type = PCI_CFG_BIOS;
    } else {
      /* Try to detect CM #1 */
      uint32_t oldcf8, newcf8;
      
      cli();
      outb(1, 0xcfb);		/* Linux does this for some reason? */
      oldcf8 = inl(0xcf8);
      outl(0x80000000, 0xcf8);
      newcf8 = inl(0xcf8);
      outl(oldcf8, 0xcf8);
      sti();

      if ( newcf8 == 0x80000000 )
	type = PCI_CFG_TYPE1;
      else {
	uint8_t oldcf8, oldcfa;
	/* Try to detect CM#2 */
	cli();
	outb(0, 0xcfb);		/* Linux does this for some reason? */
	oldcf8 = inb(0xcf8);
	outb(0, 0xcf8);
	oldcfa = inb(0xcfa);
	outb(0, 0xcfa);
	
	if ( inb(0xcf8) == 0 && inb(0xcfa) == 0 )
	  type = PCI_CFG_TYPE2;

	outb(oldcf8, 0xcf8);
	outb(oldcfa, 0xcfa);
	sti();
      }
    }
  }

  return (__pci_cfg_type = type);
}

