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

TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)

gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) -c -x c /dev/null -o $$tmpf 2>/dev/null; \
	           then echo $(1); else echo $(2); fi; rm -f $$tmpf)

M32       := $(call gcc_ok,-m32,) $(call gcc_ok,-ffreestanding,) $(call gcc_ok,-fno-stack-protector)

CC         = gcc
LD         = ld 
LDFLAGS	   = -m elf_i386
SFLAGS	   = $(M32) -march=i386
OBJCOPY	   = objcopy
PERL	   = perl

.SUFFIXES: .S .s .o .elf

all:	mbr.bin mbr_bin.c gptmbr.bin gptmbr_bin.c 

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -Wa,-a=$*.lst -c -o $@ $<

%_bin.c: %.bin ../bin2c.pl
	$(PERL) ../bin2c.pl syslinux_$* < $< > $@

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
	rm -f *.o *.elf *.lst *_bin.c

clean: tidy

spotless: clean
	rm -f *.bin
