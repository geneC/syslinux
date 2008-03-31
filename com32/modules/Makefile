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
## COM32 standard modules
##

TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)

gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) -c -x c /dev/null -o $$tmpf 2>/dev/null; \
	           then echo $(1); else echo $(2); fi; rm -f $$tmpf)

M32     := $(call gcc_ok,-m32,) $(call gcc_ok,-fno-stack-protector,)

CC         = gcc
LD         = ld -m elf_i386
AR	   = ar
NASM	   = nasm
NASMOPT	   = -O9999
RANLIB	   = ranlib
CFLAGS     = $(M32) -mregparm=3 -DREGPARM=3 -W -Wall -march=i386 -Os \
	     -fomit-frame-pointer -D__COM32__ \
	     -nostdinc -iwithprefix include \
	     -I../libutil/include -I../include \
	     -Wp,-MT,$@,-MD,$(dir $@).$(notdir $@).d
LNXCFLAGS  = -W -Wall -O -g -I../libutil/include
LNXSFLAGS  = -g
LNXLDFLAGS = -g
SFLAGS     = -D__COM32__ -march=i386
LDFLAGS    = -T ../lib/com32.ld
OBJCOPY    = objcopy
PPMTOLSS16 =	../ppmtolss16
LIBGCC    := $(shell $(CC) --print-libgcc)
LIBS	   = ../libutil/libutil_com.a ../lib/libcom32.a $(LIBGCC)
LNXLIBS	   = ../libutil/libutil_lnx.a

.SUFFIXES: .lss .c .o .elf .c32 .lnx

BINDIR   = /usr/bin
LIBDIR   = /usr/lib
AUXDIR   = $(LIBDIR)/syslinux
INCDIR   = /usr/include
COM32DIR = $(AUXDIR)/com32

MODULES	  = chain.c32 ethersel.c32 mboot.c32 dmitest.c32 cpuidtest.c32 \
	    pcitest.c32 elf.c32 linux.c32 reboot.c32 pmload.c32 meminfo.c32 \
	    sdi.c32
TESTFILES =

all: $(MODULES) $(TESTFILES)

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PRECIOUS: %.elf
%.elf: %.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

.PRECIOUS: %.lo
%.lo: %.S
	$(CC) $(LNXSFLAGS) -c -o $@ $<

.PRECIOUS: %.lo
%.lo: %.c
	$(CC) $(LNXCFLAGS) -c -o $@ $<

.PRECIOUS: %.lnx
%.lnx: %.lo $(LNXLIBS)
	$(CC) $(LNXLDFLAGS) -o $@ $^

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@

pcitest.elf : pcitest.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

cpuidtest.elf : cpuidtest.o cpuid.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

dmitest.elf : dmitest.o dmi_utils.o dmi.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

ethersel.elf : ethersel.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

tidy:
	rm -f *.o *.lo *.a *.lst *.elf .*.d

clean: tidy
	rm -f *.lss *.c32 *.lnx *.com

spotless: clean
	rm -f *~ \#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)
	install -m 644 $(MODULES) $(INSTALLROOT)$(AUXDIR)

-include .*.d
