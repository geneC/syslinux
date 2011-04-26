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
# include $(topdir)/mk/embedded.mk
coredir = $(topdir)/core

BTARGET = geodsp1s.bin geodspms.bin \
	geodsp1s.img.xz geodspms.img.xz
#	lba-1s.img.xz lba-ms.img.xz
  # lba-1s.img lba-ms.img

NASMOPT = -i $(coredir)/ -Ox -f bin
NASMOPT += -w+orphan-labels

all: $(BTARGET)

.PRECIOUS: %.img
# .PRECIOUS: lba-%.img

# Higher compression levels result in larger files
%.img.xz: %.bin mk-lba-img
	./mk-lba-img < $< | xz -0f > $@ || ( rm -f $@ ; false )

%.img.gz: %.bin mk-lba-img
	./mk-lba-img < $< | gzip -9 > $@ || ( rm -f $@ ; false )

%.bin: %.asm $(coredir)/writehex.inc $(coredir)/macros.inc $(coredir)/diskboot.inc
	nasm $(NASMOPT) -o $@ -l $(@:.bin=.lst) $<

mk-lba-img: mk-lba-img.c
	gcc -g -O -o $@ $<

tidy dist:
	rm -Rf *.img

clean: tidy
	rm -f *.lst *.bin *_bin.c mk-lba-img

spotless: clean
	rm -f $(BTARGET)
