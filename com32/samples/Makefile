#ident "$Id$"
## -----------------------------------------------------------------------
##   
##   Copyright 2001-2004 H. Peter Anvin - All Rights Reserved
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

gcc_ok   = $(shell if gcc $(1) -c -x c /dev/null -o /dev/null 2>/dev/null; \
	           then echo $(1); else echo $(2); fi)

M32     := $(call gcc_ok,-m32,)

CC         = gcc $(M32)
LD         = ld -m elf_i386
AR	   = ar
NASM	   = nasm
RANLIB	   = ranlib
CFLAGS     = -mregparm=3 -DREGPARM=3 -W -Wall -march=i386 -Os -fomit-frame-pointer -I../libutil/include -I../include  -D__COM32__
LNXCFLAGS  = -W -Wall -march=i386 -Os -g -I../libutil/include
LNXSFLAGS  = -march=i386
LNXLDFLAGS = -g
SFLAGS     = -D__COM32__ -march=i386
LDFLAGS    = -T ../lib/com32.ld
OBJCOPY    = objcopy
PPMTOLSS16 = 	../ppmtolss16
LIBGCC    := $(shell $(CC) --print-libgcc)
LIBS	   = ../libutil/libutil_com.a ../lib/libcom32.a $(LIBGCC)
LNXLIBS	   = ../libutil/libutil_lnx.a

.SUFFIXES: .lss .c .o .elf .c32 .lnx

all: 	hello.c32 cat.c32 resolv.c32 \
	fancyhello.c32 fancyhello.lnx \
	keytest.c32 keytest.lnx \

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

tidy:
	rm -f *.o *.lo *.a *.lst *.elf

clean: tidy
	rm -f *.lss *.c32 *.lnx *.com

spotless: clean
	rm -f *~ \#*

install:	# Don't install samples
