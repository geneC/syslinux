## -----------------------------------------------------------------------
##
##   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Makefile for MBR
#

topdir = ..
include $(topdir)/MCONFIG

GCCOPT    := $(call gcc_ok,-m32,) \
	     $(call gcc_ok,-ffreestanding,) \
	     $(call gcc_ok,-fno-stack-protector) \
	     -march=i386 -Os

LDFLAGS	   = -m elf_i386
SFLAGS	   = $(GCCOPT)

.SUFFIXES: .S .s .o .elf

all:	mbr.bin gptmbr.bin

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -Wa,-a=$*.lst -c -o $@ $<

mbr.elf: mbr.o mbr.ld
	$(LD) $(LDFLAGS) -T mbr.ld -e _start -o $@ $<

mbr.bin: mbr.elf checksize.pl
	$(OBJCOPY) -O binary $< $@
	$(PERL) checksize.pl mbr.bin 440

mbr_bin.c: mbr.bin

gptmbr.elf: gptmbr.o mbr.ld
	$(LD) $(LDFLAGS) -T mbr.ld -e _start -o $@ $<

gptmbr.bin: gptmbr.elf checksize.pl
	$(OBJCOPY) -O binary $< $@
	$(PERL) checksize.pl gptmbr.bin 424

tidy dist:
	rm -f *.o *.elf *.lst

clean: tidy

spotless: clean
	rm -f *.bin
