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
## samples for syslinux users
##

topdir = ../..
include $(topdir)/MCONFIG.embedded

INCLUDES   = -I$(com32)/include

LIBGCC    := $(shell $(CC) $(GCCOPT) --print-libgcc)
LIB	   = liboldcom32.a


com32 = $(topdir)/com32
C_LIBS	   = $(com32)/libutil/libutil_com.a $(com32)/lib/libcom32.a $(LIBGCC)
LDFLAGS    = -m elf_i386 -T $(com32)/lib/com32.ld

all:  hdt.c32 $(LIB)

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	$(CC) $(CFLAGS) -std=gnu99  -D__COM32__ -I$(topdir)/menu/libmenu/ -c -o $@ $<

.PRECIOUS: %.elf
%.elf: c32entry.o %.o $(LIB)
	$(LD) -Ttext 0x101000 -e _start -o $@ $^

hdt.elf: hdt.o hdt-ata.o hdt-menu.o hdt-menu-pci.o hdt-menu-kernel.o \
	 hdt-menu-disk.o hdt-menu-dmi.o hdt-menu-processor.o hdt-menu-syslinux.o hdt-menu-about.o \
	 hdt-cli.o hdt-common.o hdt-cli-pci.o hdt-cli-dmi.o hdt-cli-cpu.o hdt-cli-pxe.o hdt-cli-kernel.o\
	$(com32)/modules/cpuid.o $(com32)/modules/dmi.o $(topdir)/menu/libmenu/libmenu.a $(LIB) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@

%.com: %.asm
	$(NASM) $(NASMOPT) -f bin -o $@ -l $*.lst $<

$(LIB):
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

tidy dist:
	rm -f *.o *.a *.lst *.elf

# Don't specify *.com since mdiskchk.com can't be built using Linux tools
clean: tidy
	rm -f *.o *.c32 *.c~ *.h~ Makefile~

spotless: clean
