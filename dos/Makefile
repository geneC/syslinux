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
include $(topdir)/MCONFIG

GCCOPT    := $(call gcc_ok,-m32,) \
	     $(call gcc_ok,-ffreestanding,) \
	     $(call gcc_ok,-fno-stack-protector,) \
	     $(call gcc_ok,-falign-functions=0,-malign-functions=0) \
	     $(call gcc_ok,-falign-jumps=0,-malign-jumps=0) \
	     $(call gcc_ok,-falign-loops=0,-malign-loops=0) \
	     -march=i386 -Os -fomit-frame-pointer -mregparm=3 -DREGPARM=3 \
	     -msoft-float

LIBGCC  := $(shell $(CC) $(GCCOPT) --print-libgcc)

LDFLAGS	 = -m elf_i386 -T com16.ld
OPTFLAGS = -g
INCLUDES = -include code16.h -nostdinc -iwithprefix include \
	   -I. -I.. -I../libfat -I ../libinstaller
CFLAGS	 = $(GCCOPT) -W -Wall -msoft-float $(OPTFLAGS) $(INCLUDES)

SRCS     = syslinux.c \
	   ../libinstaller/syslxmod.c \
	   ../libinstaller/bootsect_bin.c \
	   ../libinstaller/ldlinux_bin.c \
	   ../libinstaller/mbr_bin.c \
           $(wildcard ../libfat/*.c)
OBJS	 = crt0.o $(patsubst %.c,%.o,$(notdir $(SRCS)))
LIBOBJS	 = conio.o memcpy.o memset.o  skipatou.o atou.o malloc.o free.o \
	   argv.o printf.o __divdi3.o __udivmoddi4.o

.SUFFIXES: .c .o .i .s .S .elf .com .asm .lst

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

%.o: %.c
	$(CC) -Wp,-MT,$@,-MD,.$@.d $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<
%.o: %.S
	$(CC) -Wp,-MT,$@,-MD,.$@.d $(CFLAGS) -D__ASSEMBLY__ -c -o $@ $<
%.s: %.S
	$(CC) $(CFLAGS) -E -o $@ $<
%.com: %.asm
	$(NASM) $(NASMOPT) -f bin -o $@ -l $*.lst $<

-include .*.d *.tmp
