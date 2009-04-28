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
## MS-DOS FAT installer
##

topdir = ..
include $(topdir)/MCONFIG.embedded

LDFLAGS	 = -T com16.ld
OPTFLAGS = -g
INCLUDES = -include code16.h -nostdinc -iwithprefix include \
	   -I. -I.. -I../libfat -I ../libinstaller

SRCS     = syslinux.c \
	   ../libinstaller/syslxmod.c \
	   ../libinstaller/bootsect_bin.c \
	   ../libinstaller/ldlinux_bin.c \
	   ../libinstaller/mbr_bin.c \
           $(wildcard ../libfat/*.c)
OBJS	 = crt0.o $(patsubst %.c,%.o,$(notdir $(SRCS)))
LIBOBJS	 = conio.o memcpy.o memset.o  skipatou.o atou.o malloc.o free.o \
	   argv.o printf.o __divdi3.o __udivmoddi4.o

VPATH = .:../libfat:../libinstaller

TARGETS = syslinux.com copybs.com

all: $(TARGETS)

tidy dist:
	-rm -f *.o *.i *.s *.a .*.d *.tmp *.elf *.lst

clean: tidy

spotless: clean
	-rm -f *~ $(TARGETS)

installer:

syslinux.elf: $(OBJS) libcom.a
	$(LD) $(LDFLAGS) -o $@ $^

libcom.a: $(LIBOBJS)
	-rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

syslinux.com: syslinux.elf
	$(OBJCOPY) -O binary $< $@

%.com: %.asm
	( $(NASM) -M -DDEPEND $(NASMOPT) -o $@ -M $< && echo '' ) > .$@.d; true
	$(NASM) $(NASMOPT) -f bin -o $@ -l $*.lst $<

-include .*.d *.tmp
