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

VERSION := $(shell cat ../version)

TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)

CC	= gcc

gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) -c -x c /dev/null -o $$tmpf 2>/dev/null; \
	           then echo $(1); else echo $(2); fi; rm -f $$tmpf)

M32     := $(call gcc_ok,-m32,)
ALIGN   := $(call gcc_ok,-falign-functions=0 -falign-jumps=0 -falign-loops=0,-malign-functions=0 -malign-jumps=0 -malign-loops=0)
FREE    := $(call gcc_ok,-ffreestanding,) $(call gcc_ok,-fno-stack-protector,)

CFLAGS   =  $(M32) $(FREE) -g -W -Wall -Wno-sign-compare \
	   -Os -fomit-frame-pointer -march=i386 $(ALIGN) \
	   -DVERSION='"$(VERSION)"' -DDATE='"$(DATE)"'
SFLAGS	 = $(M32) -march=i386 -D__ASSEMBLY__
LDFLAGS  = $(M32) -g
INCLUDE  = -I../com32/include
LD	 = ld -m elf_i386
NASM     = nasm
NASMOPT  = -O9999
NFLAGS   = -dVERSION='"$(VERSION)"' -dDATE='"$(DATE)"' -dWITH_EDD
NINCLUDE =
OBJCOPY  = objcopy
PERL     = perl

# Important: init.o16 must be first!!
OBJS16   = init.o16 init32.o
OBJS32   = start32.o setup.o msetup.o e820func.o conio.o memcpy.o memset.o \
           unzip.o memdisk.o

CSRC     = setup.c msetup.c e820func.c conio.c unzip.c
SSRC     = start32.S memcpy.S memset.S
NASMSRC  = memdisk.asm memdisk16.asm

all: memdisk # e820test

# tidy, clean removes everything except the final binary
tidy dist:
	rm -f *.o *.s *.o16 *.s16 *.bin *.lst *.elf e820test

clean: tidy

# spotless also removes the product binary
spotless: clean
	rm -f memdisk .depend

%.o: %.asm
	$(NASM) $(NASMOPT) $(NFLAGS) $(NINCLUDE) -f elf -l $*.lst -o $@ $<

%.o: %.s
	$(CC) $(SFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(INCLUDE) $(SFLAGS) -c -o $@ $<

%.o16: %.s16
	$(CC) $(SFLAGS) -x assembler -c -o $@ $<

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) -c -o $@ $<

%.s16: %.s
	echo '.code16gcc' | cat - $< > $@

%.s: %.S
	$(CC) $(INCLUDE) $(SFLAGS) -E -o $@ $<

%.s16: %.S16
	$(CC) $(INCLUDE) $(SFLAGS) -x assembler-with-cpp -E -o $@ $<

%.s: %.c
	$(CC) $(INCLUDE) $(CFLAGS) -S -o $@ $<

%.i: %.c
	$(CC) $(INCLUDE) $(CFLAGS) -E -o $@ $<

# Cancel default rule
%.o: %.c

%.bin: %.asm
	$(NASM) -f bin $(NASMOPT) $(NFLAGS) $(NINCLUDE) -o $@ -l $*.lst $<

memdisk16.elf: $(OBJS16)
	$(LD) -Ttext 0 -o $@ $^

memdisk32.elf: $(OBJS32)
	$(LD) -Ttext 0x100000 -o $@ $^

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

memdisk: memdisk16.bin memdisk32.bin postprocess.pl
	$(PERL) postprocess.pl $@ memdisk16.bin memdisk32.bin

e820test: e820func.o msetup.o e820test.o memdisk.o
	$(CC) $(LDFLAGS) -o $@ $^

memdisk.o: memdisk.bin
	$(LD) -r -b binary -o $@ $<

.depend:
	rm -f .depend
	for csrc in *.c ; do $(CC) $(INCLUDE) $(CFLAGS) -MM $$csrc >> .depend ; done
	for ssrc in *.S ; do $(CC) $(INCLUDE) $(SFLAGS) -MM $$ssrc >> .depend ; done
	for nsrc in $(NASMSRC) ; do $(NASM) -DDEPEND $(NINCLUDE) -o `echo $$nsrc | sed -e 's/\.asm/\.bin/'` -M $$nsrc >> .depend ; done

depend:
	rm -f .depend
	$(MAKE) .depend

# This file contains the version number, so add a dependency for it
setup.s: ../version

# Include dependencies file
include .depend
