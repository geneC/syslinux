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

BINDIR  = /usr/bin

.c.o:
	$(CC) $(CFLAGS) -c $<

TARGETS=bootsect.bin ldlinux.sys syslinux.com syslinux

all:	$(TARGETS)
	ls -l $(TARGETS)

# The DATE is set on the make command line when building binaries for
# official release.  Otherwise, substitute a hex string that is pretty much
# guaranteed to be unique to be unique from build to build.
ifndef DATE
DATE = $(shell perl now.pl)
endif

ldlinux.bin: ldlinux.asm
	$(NASM) -f bin -dDATE_STR="'$(DATE)'" -l ldlinux.lst -o ldlinux.bin ldlinux.asm

bootsect.bin: ldlinux.bin
	dd if=ldlinux.bin of=bootsect.bin bs=512 count=1

ldlinux.sys: ldlinux.bin
	dd if=ldlinux.bin of=ldlinux.sys  bs=512 skip=1

syslinux.com: syslinux.asm bootsect.bin ldlinux.sys
	$(NASM) -f bin -l syslinux.lst -o syslinux.com syslinux.asm

bootsect_bin.c: bootsect.bin bin2c.pl
	perl bin2c.pl bootsect < bootsect.bin > bootsect_bin.c

ldlinux_bin.c: ldlinux.sys bin2c.pl
	perl bin2c.pl ldlinux < ldlinux.sys > ldlinux_bin.c

syslinux: syslinux.o bootsect_bin.o ldlinux_bin.o
	$(CC) $(LDFLAGS) -o syslinux syslinux.o bootsect_bin.o ldlinux_bin.o

install: all
	install -c syslinux $(BINDIR)

tidy:
	rm -f *.bin *.lst *.sys *.o *_bin.c

clean: tidy
	rm -f syslinux syslinux.com

dist: tidy
	rm -f *~ \#*

#
# This should only be used by the maintainer to generate official binaries
# for release.  Please do not "make official" and distribute the binaries,
# please.
#
official:
	$(MAKE) clean
	$(MAKE) all DATE=`date +'%Y-%m-%d'`
	$(MAKE) dist
