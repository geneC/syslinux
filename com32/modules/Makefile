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
CFLAGS     = -W -Wall -march=i386 -Os -fomit-frame-pointer -I../com32/include
SFLAGS     = -march=i386
LDFLAGS    = -s -T ../lib/com32.ld
OBJCOPY    = objcopy
PPMTOLSS16 = 	../ppmtolss16
LIB	   = ../lib/libcom32.a

.SUFFIXES: .lss .c .o .elf .c32

all: hello.c32

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PRECIOUS: %.elf
%.elf: %.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $^

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@

tidy:
	rm -f *.o *.a *.lst *.elf

clean: tidy
	rm -f *.lss *.o *.c32 *.com

spotless: clean
	rm -f *~ \#*
