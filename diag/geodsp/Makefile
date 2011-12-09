## -----------------------------------------------------------------------
##
##   Copyright 2010-2011 Gene Cumm
##
##   Portions from core/Makefile:
##   Copyright 1998-2009 H. Peter Anvin - All Rights Reserved
##   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Makefile for the SYSLINUX geometry display for diagnostics
#

topdir = ../..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/embedded.mk
coredir = $(topdir)/core

BTARGET = geodsp1s.bin geodspms.bin \
	geodsp1s.img.xz geodspms.img.xz

NASMOPT = -i $(coredir)/ -Ox -f bin
NASMOPT += -w+orphan-labels
CFLAGS = -g -O

all: $(BTARGET)

# Higher compression levels result in larger files
%.img.xz: %.bin mk-lba-img
	./mk-lba-img < $< | $(XZ) -0f > $@ || ( rm -f $@ ; false )

%.img.gz: %.bin mk-lba-img
	./mk-lba-img < $< | $(GZIP) -9 > $@ || ( rm -f $@ ; false )

%.bin: %.asm $(coredir)/writehex.inc $(coredir)/macros.inc $(coredir)/diskboot.inc
	$(NASM) $(NASMOPT) -o $@ -l $(@:.bin=.lst) $<

mk-lba-img: mk-lba-img.c
	$(CC) $(CFLAGS) -o $@ $<

tidy dist:
	rm -Rf *.lst *.img
	rm -f mk-lba-img

clean: tidy

spotless: clean
	rm -f $(BTARGET)
