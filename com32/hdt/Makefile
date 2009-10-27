## -----------------------------------------------------------------------
##
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
##   Copyright 2009 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
##   Boston MA 02110-1301, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## Hardware Detection Tool
##

topdir = ../..
include ../MCONFIG

LIBS	   = ../cmenu/libmenu/libmenu.a ../libutil/libutil_com.a \
	     ../lib/libcom32.a $(LIBGCC)
CFLAGS    += -I$(com32)/cmenu/libmenu

MODULES	  = hdt.c32
TESTFILES =

OBJS	  = $(patsubst %.c,%.o,$(wildcard *.c))

KERNEL_VERSION          ?= $(shell uname -r)
MODULES_ALIAS_FILE      ?= /lib/modules/$(KERNEL_VERSION)/modules.alias
MODULES_PCIMAP_FILE     ?= /lib/modules/$(KERNEL_VERSION)/modules.pcimap
ISO_DIR                 ?= iso
ISOLINUX_DIR            ?= isolinux
FLOPPY_DIR		?= floppy
PCI_IDS_FILE            ?= $(PWD)/$(FLOPPY_DIR)/pci.ids

all: $(MODULES) $(TESTFILES)

hdt.elf : $(OBJS) $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

hdt.img: hdt.c32 $(FLOPPY_DIR)/hdt.cfg $(FLOPPY_DIR)/mtools.conf $(topdir)/mtools/syslinux
	rm -f hdt.img
	MTOOLSRC=$(PWD)/$(FLOPPY_DIR)/mtools.conf $(MFORMAT) -v HDT -f 2880 -C a:
	$(topdir)/mtools/syslinux hdt.img
	-[ ! -f $(PCI_IDS_FILE) ] && cp /usr/share/hwdata/pci.ids $(PCI_IDS_FILE)
	-[ ! -f $(PCI_IDS_FILE) ] && cp /usr/share/pci.ids $(PCI_IDS_FILE)
	-[ -f $(MODULES_ALIAS_FILE) ] && MTOOLSRC=$(PWD)/$(FLOPPY_DIR)/mtools.conf $(MCOPY) $(MODULES_ALIAS_FILE) a:
	-[ -f $(MODULES_PCIMAP_FILE) ] && MTOOLSRC=$(PWD)/$(FLOPPY_DIR)/mtools.conf $(MCOPY) $(MODULES_PCIMAP_FILE) a:
	MTOOLSRC=$(PWD)/$(FLOPPY_DIR)/mtools.conf $(MCOPY) hdt.c32 a:
	@ [ -f $(PCI_IDS_FILE) ] && MTOOLSRC=$(PWD)/$(FLOPPY_DIR)/mtools.conf $(MCOPY) $(PCI_IDS_FILE) a: || printf "\nThe $(FLOPPY_DIR)/pci.ids file is missing and can be downloaded from http://pciids.sourceforge.net and put in\nthe ./com32/hdt/$(FLOPPY_DIR) directory of the extracted Syslinux source.\n\n"
	MTOOLSRC=$(PWD)/$(FLOPPY_DIR)/mtools.conf $(MCOPY) $(FLOPPY_DIR)/hdt.cfg a:syslinux.cfg

hdt.iso: hdt.c32 $(topdir)/core/isolinux.bin $(FLOPPY_DIR)/hdt.cfg
	rm -rf $(ISO_DIR)
	rm -f hdt.iso
	mkdir -p $(ISO_DIR)/$(ISOLINUX_DIR)
	cp $(topdir)/core/isolinux.bin $(ISO_DIR)/$(ISOLINUX_DIR)
	cp $(FLOPPY_DIR)/hdt.cfg $(ISO_DIR)/$(ISOLINUX_DIR)/isolinux.cfg
	cp hdt.c32 $(ISO_DIR)/$(ISOLINUX_DIR)
	-[ ! -f $(PCI_IDS_FILE) ] && cp /usr/share/hwdata/pci.ids $(ISO_DIR)/$(ISOLINUX_DIR)
	-[ ! -f $(PCI_IDS_FILE) ] && cp /usr/share/pci.ids $(ISO_DIR)/$(ISOLINUX_DIR)
	-[ -f $(MODULES_ALIAS_FILE) ] && cp $(MODULES_ALIAS_FILE) $(ISO_DIR)/$(ISOLINUX_DIR)
	-[ -f $(MODULES_PCIMAP_FILE) ] && cp $(MODULES_PCIMAP_FILE) $(ISO_DIR)/$(ISOLINUX_DIR)
	-[ ! -f $(ISO_DIR)/$(ISOLINUX_DIR)/pci.ids ] && printf "\nThe $(FLOPPY_DIR)/pci.ids file is missing and can be downloaded from http://pciids.sourceforge.net and put in\nthe ./com32/hdt/$(FLOPPY_DIR) directory of the extracted Syslinux source.\n\n"
	$(MKISOFS) -o hdt.iso -b $(ISOLINUX_DIR)/isolinux.bin -c $(ISOLINUX_DIR)/boot.cat \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		$(ISO_DIR)

tidy dist:
	rm -f *.o *.lo *.a *.lst *.elf .*.d *.tmp

clean: tidy
	rm -f *.lnx

spotless: clean
	rm -f *.lss *.c32 *.com hdt.img hdt.iso
	rm -rf $(ISO_DIR)
	rm -f *~ \#*

install:

-include .*.d
