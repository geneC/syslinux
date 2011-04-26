## -----------------------------------------------------------------------
##
##   Copyright 1998-2009 H. Peter Anvin - All Rights Reserved
##   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
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
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/embedded.mk
-include $(topdir)/version.mk

OPTFLAGS =
INCLUDES = -I./include -I$(com32)/include

# This is very similar to cp437; technically it's for Norway and Denmark,
# but it's unlikely the characters that are different will be used in
# filenames by other users.
CODEPAGE = cp865

# The targets to build in this directory...
BTARGET  = kwdhash.gen \
	   ldlinux.bss ldlinux.sys ldlinux.bin \
	   isolinux.bin isolinux-debug.bin pxelinux.0

# All primary source files for the main syslinux files
NASMSRC	 := $(wildcard *.asm)
NASMHDR  := $(wildcard *.inc)
CSRC	 := $(wildcard *.c */*.c */*/*.c)
SSRC	 := $(wildcard *.S */*.S */*/*.S)
CHDR	 := $(wildcard *.h)
OTHERSRC := keywords
ALLSRC    = $(NASMSRC) $(NASMHDR) $(CSRC) $(SSRC) $(CHDR) $(OTHERSRC)

COBJ	 := $(patsubst %.c,%.o,$(CSRC))
SOBJ	 := $(patsubst %.S,%.o,$(SSRC))

LIB	 = libcore.a
LIBS	 = $(LIB) $(com32)/lib/libcomcore.a $(LIBGCC)
LIBOBJS	 = $(COBJ) $(SOBJ)

NASMDEBUG = -g -F dwarf
NASMOPT  += $(NASMDEBUG)

PREPCORE = ../lzo/prepcore

# CFLAGS	+= -DDEBUG=1

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

%.raw: %.elf
	$(OBJCOPY) -O binary $< $(@:.bin=.raw)

# GNU make 3.82 gets confused by the first form
.PRECIOUS: %.raw

%.bin: %.raw $(PREPCORE)
	$(PREPCORE) $< $@

%.o: %.asm kwdhash.gen ../version.gen
	$(NASM) -f elf $(NASMOPT) -DDATE_STR="'$(DATE)'" \
		-DHEXDATE="$(HEXDATE)" \
		-l $(@:.o=.lsr) -o $@ -MP -MD .$@.d $<

%.elf: %.o $(LIBS) syslinux.ld
	$(LD) $(LDFLAGS) -T syslinux.ld -M -o $@ $< \
		--start-group $(LIBS) --end-group \
		> $(@:.elf=.map)
	$(OBJDUMP) -h $@ > $(@:.elf=.sec)
	$(PERL) lstadjust.pl $(@:.elf=.lsr) $(@:.elf=.sec) $(@:.elf=.lst)

$(LIB): $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

pxelinux.0: pxelinux.bin
	cp -f $< $@

ldlinux.bss: ldlinux.bin
	dd if=$< of=$@ bs=512 count=1

ldlinux.sys: ldlinux.bin
	dd if=$< of=$@ bs=512 skip=2

codepage.cp: ../codepage/$(CODEPAGE).cp
	cp -f $< $@

codepage.o: codepage.S codepage.cp

install: installer

install-lib: installer

install-all: install install-lib

netinstall: installer

tidy dist:
	rm -f codepage.cp *.o *.elf *.a stupid.* patch.offset .depend .*.d
	rm -f *.elf.tmp *.sym
	rm -f *.lsr *.lst *.map *.sec *.raw
	rm -f */*.o */*/*.o */*.lst */*/*.lst */.*.d */*/.*.d
	rm -f $(OBSOLETE) $(LIB)

clean: tidy

spotless: clean
	rm -f $(BTARGET) *.bin *_bin.c

# Include dependencies file
-include .*.d */.*.d */*/.*.d
