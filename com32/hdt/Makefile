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
PCI_IDS_FILE            ?= $(PWD)/floppy/pci.ids

all: $(MODULES) $(TESTFILES)

hdt.elf : $(OBJS) $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

hdt.img: hdt.c32 floppy/hdt.cfg floppy/mtools.conf $(topdir)/mtools/syslinux 
	rm -f hdt.img
	MTOOLSRC=$(PWD)/floppy/mtools.conf $(MFORMAT) -v HDT -f 2880 -C a:
	$(topdir)/mtools/syslinux hdt.img
	-[ ! -f $(PCI_IDS_FILE) ] && cp /usr/share/hwdata/pci.ids $(PCI_IDS_FILE)
	-[ ! -f $(PCI_IDS_FILE) ] && cp /usr/share/pci.ids $(PCI_IDS_FILE)
	-[ -f $(MODULES_ALIAS_FILE) ] && MTOOLSRC=$(PWD)/floppy/mtools.conf $(MCOPY) $(MODULES_ALIAS_FILE) a:
	-[ -f $(MODULES_PCIMAP_FILE) ] && MTOOLSRC=$(PWD)/floppy/mtools.conf $(MCOPY) $(MODULES_PCIMAP_FILE) a:
	MTOOLSRC=$(PWD)/floppy/mtools.conf $(MCOPY) hdt.c32 a:
	@ [ -f $(PCI_IDS_FILE) ] && MTOOLSRC=$(PWD)/floppy/mtools.conf $(MCOPY) $(PCI_IDS_FILE) a: || printf "\nThe floppy/pci.ids file is missing and can be downloaded from http://pciids.sourceforge.net and put in\nthe ./com32/hdt/floppy directory of the extracted Syslinux source.\n\n"
	MTOOLSRC=$(PWD)/floppy/mtools.conf $(MCOPY) floppy/hdt.cfg a:syslinux.cfg

tidy dist:
	rm -f *.o *.lo *.a *.lst *.elf .*.d *.tmp

clean: tidy
	rm -f *.lnx

spotless: clean
	rm -f *.lss *.c32 *.com hdt.img
	rm -f *~ \#*

install:

-include .*.d
