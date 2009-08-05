## -----------------------------------------------------------------------
##
##   Copyright 1998-2009 H. Peter Anvin - All Rights Reserved
##   Copyright 2009 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Makefile for the SYSLINUX core
#

# No builtin rules
MAKEFLAGS += -r
MAKE      += -r

topdir = ..
include $(topdir)/MCONFIG.embedded
-include $(topdir)/version.mk

OPTFLAGS = 
INCLUDES =

# This is very similar to cp437; technically it's for Norway and Denmark,
# but it's unlikely the characters that are different will be used in
# filenames by other users.
CODEPAGE = cp865

# The targets to build in this directory...
BTARGET  = kwdhash.gen \
	   ldlinux.bss ldlinux.sys ldlinux.bin \
	   pxelinux.0 isolinux.bin isolinux-debug.bin \
	   extlinux.bin extlinux.bss extlinux.sys

# All primary source files for the main syslinux files
NASMSRC	 = $(wildcard *.asm)
NASMHDR  = $(wildcard *.inc)
CSRC	 = $(wildcard *.c)
CHDR	 = $(wildcard *.h)
OTHERSRC = keywords
ALLSRC   = $(NASMSRC) $(NASMHDR) $(CSRC) $(CHDR) $(OTHERSRC)

# The DATE is set on the make command line when building binaries for
# official release.  Otherwise, substitute a hex string that is pretty much
# guaranteed to be unique to be unique from build to build.
ifndef HEXDATE
HEXDATE := $(shell $(PERL) ../now.pl $(SRCS))
endif
ifndef DATE
DATE    := $(shell sh ../gen-id.sh $(VERSION) $(HEXDATE))
endif

all: $(BTARGET)

kwdhash.gen: keywords genhash.pl
	$(PERL) genhash.pl < keywords > kwdhash.gen

.PRECIOUS: %.elf

# Standard rule for {isolinux,isolinux-debug}.bin
iso%.bin: iso%.elf checksumiso.pl
	$(OBJCOPY) -O binary $< $@
	$(PERL) checksumiso.pl $@

# Standard rule for {ldlinux,pxelinux,extlinux}.bin
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.o: %.asm kwdhash.gen ../version.gen
	( $(NASM) -M -DDEPEND $(NINCLUDE) -o $@ $< ; echo '' ) > .$@.d; true
	$(NASM) $(NASMOPT) -f elf -g -F stabs -DDATE_STR="'$(DATE)'" \
		-DHEXDATE="$(HEXDATE)" \
		-l $(@:.o=.lsr) -o $@ $<

%.elf: %.o syslinux.ld
	$(LD) $(LDFLAGS) -T syslinux.ld -M -o $@ $< > $(@:.elf=.map)
	$(OBJDUMP) -h $@ > $(@:.elf=.sec)
	$(PERL) lstadjust.pl $(@:.elf=.lsr) $(@:.elf=.sec) $(@:.elf=.lst)

pxelinux.0: pxelinux.bin
	cp -f $< $@

ldlinux.bss: ldlinux.bin
	dd if=$< of=$@ bs=512 count=1

ldlinux.sys: ldlinux.bin
	dd if=$< of=$@ bs=512 skip=1

extlinux.bss: extlinux.bin
	dd if=$< of=$@ bs=512 count=1

extlinux.sys: extlinux.bin
	dd if=$< of=$@ bs=512 skip=1

# NASM prior to 2.03 wouldn't auto-generate this dependency...
ldlinux.o: codepage.cp

codepage.cp: ../codepage/$(CODEPAGE).cp
	cp -f $< $@

install: installer

install-lib: installer

install-all: install install-lib

netinstall: installer

tidy dist:
	rm -f codepage.cp *.o *.elf stupid.* patch.offset .depend .*.d
	rm -f *.lsr *.lst *.map *.sec
	rm -f $(OBSOLETE)

clean: tidy

spotless: clean
	rm -f $(BTARGET) *.bin *_bin.c

# Include dependencies file
-include .*.d
