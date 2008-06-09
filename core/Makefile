## -----------------------------------------------------------------------
##
##   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
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

CC	 = gcc

TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)
gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) dummy.c -o $$tmpf 2>/dev/null; \
	           then echo '$(1)'; else echo '$(2)'; fi; rm -f $$tmpf)

M32       := $(call gcc_ok,-m32,) $(call gcc_ok,-ffreestanding,) \
	$(call gcc_ok,-fno-stack-protector,) \
	$(call gcc_ok,-fno-top-level-reorder,$(call gcc_ok,-fno-unit-at-a-time))

LD       = ld
LDFLAGS	 = -m elf_i386
OBJCOPY  = objcopy
OBJDUMP  = objdump

OPTFLAGS = -g -Os -march=i386 -falign-functions=0 -falign-jumps=0 -falign-loops=0 -fomit-frame-pointer
INCLUDES = 
CFLAGS	 = $(M32) -mregparm=3 -DREGPARM=3 -W -Wall -msoft-float $(OPTFLAGS) $(INCLUDES)

NASM	 = nasm
NASMOPT  = -O9999
NINCLUDE =

PERL     = perl

VERSION := $(shell cat ../version)

# This is very similar to cp437; technically it's for Norway and Denmark,
# but it's unlikely the characters that are different will be used in
# filenames by other users.
CODEPAGE = cp865

# _bin.c files required by both BTARGET and ITARGET installers
BINFILES = bootsect_bin.c ldlinux_bin.c \
	   extlinux_bss_bin.c extlinux_sys_bin.c

# syslinux.exe is BTARGET so as to not require everyone to have the
# mingw suite installed
BTARGET  = kwdhash.gen \
	   ldlinux.bss ldlinux.sys ldlinux.bin \
	   pxelinux.0 isolinux.bin isolinux-debug.bin \
	   extlinux.bin extlinux.bss extlinux.sys
ITARGET  =

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
HEXDATE := $(shell $(PERL) now.pl $(SRCS))
endif
ifndef DATE
DATE    := $(HEXDATE)
endif

#
# NOTE: If you don't have the mingw compiler suite installed, you probably
# want to remove win32 from this list; otherwise you're going to get an
# error every time you try to build.
#

all:	all-local

all-local: $(BTARGET) $(BINFILES)

installer: installer-local

installer-local: $(ITARGET) $(BINFILES)

kwdhash.gen: keywords genhash.pl
	$(PERL) genhash.pl < keywords > kwdhash.gen

.PRECIOUS: %.elf

# Standard rule for {isolinux,isolinux-debug}.bin
iso%.bin: iso%.elf
	$(OBJCOPY) -O binary $< $@
	$(PERL) checksumiso.pl $@

# Standard rule for {ldlinux,pxelinux,extlinux}.bin
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.o: %.asm kwdhash.gen ../version.gen
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

bootsect_bin.c: ldlinux.bss ../bin2c.pl
	$(PERL) ../bin2c.pl syslinux_bootsect < $< > $@

ldlinux_bin.c: ldlinux.sys ../bin2c.pl
	$(PERL) ../bin2c.pl syslinux_ldlinux < $< > $@

extlinux_bss_bin.c: extlinux.bss ../bin2c.pl
	$(PERL) ../bin2c.pl extlinux_bootsect < $< > $@

extlinux_sys_bin.c: extlinux.sys ../bin2c.pl
	$(PERL) ../bin2c.pl extlinux_image 512 < $< > $@

# NASM prior to 2.03 wouldn't auto-generate this dependency...
ldlinux.o: codepage.cp

codepage.cp: ../codepage/$(CODEPAGE).cp
	cp -f $< $@

install: installer

install-lib: installer

install-all: install install-lib

netinstall: installer

tidy dist:
	rm -f codepage.cp *.o *.elf stupid.* patch.offset
	rm -f *.lsr *.lst *.map *.sec
	rm -f $(OBSOLETE)

clean: tidy
	rm -f $(ITARGET) *_bin.c

spotless: clean
	rm -f $(BTARGET) .depend

.depend:
	rm -f .depend
	for csrc in $(CSRC) ; do $(CC) $(INCLUDE) -MM $$csrc >> .depend ; done
	for nsrc in $(NASMSRC) ; do $(NASM) -DDEPEND $(NINCLUDE) -o `echo $$nsrc | sed -e 's/\.asm/\.o/'` -M $$nsrc >> .depend ; done

depend:
	rm -f .depend
	$(MAKE) .depend

# Include dependencies file
include .depend
