## -----------------------------------------------------------------------
##  $Id$
##
##   Copyright 1998 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
##   USA; either version 2 of the License, or (at your option) any later
##   version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Main Makefile for SYSLINUX
#

NASM	= nasm
CC	= gcc
CFLAGS	= -Wall -O2 -fomit-frame-pointer
LDFLAGS	= -O2 -s

.c.o:
	$(CC) $(CFLAGS) -c $<

all:	bootsect.bin ldlinux.sys syslinux.com syslinux

ldlinux.bin: ldlinux.asm
	$(NASM) -f bin -dHEX_TIME="`perl now.pl`" -l ldlinux.lst -o ldlinux.bin ldlinux.asm
	ls -l ldlinux.bin

bootsect.bin: ldlinux.bin
	dd if=ldlinux.bin of=bootsect.bin bs=512 count=1

ldlinux.sys: ldlinux.bin
	dd if=ldlinux.bin of=ldlinux.sys  bs=512 skip=1
	ls -l ldlinux.sys

syslinux.com: syslinux.asm bootsect.bin ldlinux.sys
	$(NASM) -f bin -l syslinux.lst -o syslinux.com syslinux.asm
	ls -l syslinux.com

bootsect_bin.c: bootsect.bin bin2c.pl
	perl bin2c.pl bootsect < bootsect.bin > bootsect_bin.c

ldlinux_bin.c: ldlinux.sys bin2c.pl
	perl bin2c.pl ldlinux < ldlinux.sys > ldlinux_bin.c

syslinux: syslinux.o bootsect_bin.o ldlinux_bin.o
	$(CC) $(LDFLAGS) -o syslinux syslinux.o bootsect_bin.o ldlinux_bin.o

tidy:
	rm -f *.bin *.lst *.sys *.o *_bin.c

clean: tidy
	rm -f syslinux syslinux.com

dist: tidy
	rm -f *~ \#*
