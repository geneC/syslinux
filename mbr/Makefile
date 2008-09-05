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
include $(topdir)/MCONFIG.embedded

all:	mbr.bin gptmbr.bin isohdpfx.bin

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -Wa,-a=$*.lst -c -o $@ $<

.PRECIOUS: %.elf
%.elf: %.o mbr.ld
	$(LD) $(LDFLAGS) -T mbr.ld -e _start -o $@ $<

mbr.bin: mbr.elf checksize.pl
	$(OBJCOPY) -O binary $< $@
	$(PERL) checksize.pl mbr.bin 440

isohdpfx.bin: isohdpfx.elf checksize.pl
	$(OBJCOPY) -O binary $< $@
	$(PERL) checksize.pl mbr.bin 432

mbr_bin.c: mbr.bin

gptmbr.bin: gptmbr.elf checksize.pl
	$(OBJCOPY) -O binary $< $@
	$(PERL) checksize.pl gptmbr.bin 424

tidy dist:
	rm -f *.o *.elf *.lst

clean: tidy

spotless: clean
	rm -f *.bin
