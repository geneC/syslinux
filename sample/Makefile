#ident "$Id$"
## -----------------------------------------------------------------------
##   
##   Copyright 2001 H. Peter Anvin - All Rights Reserved
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

CC         = gcc
LD         = ld
CFLAGS     = -march=i386 -O2 -fomit-frame-pointer -I../com32/include
LDFLAGS    = -s
OBJCOPY    = objcopy
PPMTOLSS16 = 	../ppmtolss16

.SUFFIXES: .lss .c .o .elf .c32

all: syslogo.lss hello.c32

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.elf.c32:
	$(OBJCOPY) -O binary $< $@

syslogo.lss:	syslogo.png $(PPMTOLSS16)
	pngtopnm syslogo.png | \
		$(PPMTOLSS16) \#000000=0 \#d0d0d0=7 \#f6f6f6=15	\
		> syslogo.lss

hello.elf: hello.o
	$(LD) -Ttext 0x101000 -e _start -o $@ $<

clean:
	rm -f *.lss *.o *.elf *.c32

spotless: clean
