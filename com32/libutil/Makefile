#ident "$Id$"
## -----------------------------------------------------------------------
##   
##   Copyright 2001-2004 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
##   USA; either version 2 of the License, or (at your option) any later
##   version; incorporated herein by reference.
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
CFLAGS     = -D__COM32__ -W -Wall -march=i386 -Os -fomit-frame-pointer -I../include
SFLAGS     = -D__COM32__ -march=i386
LDFLAGS    = -T ../lib/com32.ld
LNXCFLAGS  = -W -Wall -march=i386 -Os -g
LNXSFLAGS  = -march=i386
LNXLDFLAGS = -g
OBJCOPY    = objcopy
LIBOBJS	   = ansiline.o ansiraw.o
LNXLIBOBJS = $(patsubst %.o,%.lo,$(LIBOBJS))

.SUFFIXES: .lss .c .lo .o .elf .c32 .lnx

all: libutil_com.a libutil_lnx.a

libutil_com.a: $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $(LIBOBJS)
	$(RANLIB) $@

libutil_lnx.a: $(LNXLIBOBJS)
	rm -f $@
	$(AR) cq $@ $(LNXLIBOBJS)
	$(RANLIB) $@

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PRECIOUS: %.elf
%.elf: %.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBGCC)

.PRECIOUS: %.lo
%.lo: %.S
	$(CC) $(LNXSFLAGS) -c -o $@ $<

.PRECIOUS: %.lo
%.lo: %.c
	$(CC) $(LNXCFLAGS) -c -o $@ $<

.PRECIOUS: %.lnx
%.lnx: %.lo
	$(CC) $(LNXCFLAGS) -o $@ $^

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@

tidy:
	rm -f *.o *.lo *.lst *.elf

clean: tidy
	rm -f *.lss *.a *.c32 *.lnx *.com

spotless: clean
	rm -f *~ \#*
