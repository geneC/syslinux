## -----------------------------------------------------------------------
##
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## Non-COM32 simple Syslinux modules
##

topdir = ..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/embedded.mk

INCLUDES   = -I$(com32)/include

BINS = pxechain.com poweroff.com int18.com ver.com

all: $(BINS)

.PRECIOUS: %.o

.PRECIOUS: %.elf
%.elf: c32entry.o %.o $(LIB)
	$(LD) -Ttext 0x101000 -e _start -o $@ $^

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@

%.com: %.asm
	( $(NASM) -M -DDEPEND -o $@ $< ; echo '' ) > .$@.d ; true
	$(NASM) $(NASMOPT) -f bin -o $@ -l $*.lst $<

$(LIB): $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

%.lss:	%.ppm.gz $(PPMTOLSS16)
	$(GZIPPROG) -cd $< | \
		$(PPMTOLSS16) \#000000=0 \#d0d0d0=7 \#f6f6f6=15	\
		> $@

%.ppm.gz: %.png
	$(PNGTOPNM) $< | gzip -9 > $@

tidy dist:
	rm -f *.o *.a *.lst *.elf *.map .*.d

clean: tidy

spotless: clean
	rm -f $(BINS)

-include .*.d
