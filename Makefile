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

VERSION = $(shell cat version)

.c.o:
	$(CC) $(CFLAGS) -c $<

SOURCES = ldlinux.asm syslinux.asm syslinux.c copybs.asm
TARGETS = bootsect.bin ldlinux.sys syslinux.com syslinux copybs.com
DOCS    = COPYING NEWS README TODO syslinux.doc keytab-lilo.doc
OTHER   = Makefile bin2c.pl now.pl genstupid.pl keytab-lilo.pl version

all:	$(TARGETS)
	ls -l $(TARGETS)

# The DATE is set on the make command line when building binaries for
# official release.  Otherwise, substitute a hex string that is pretty much
# guaranteed to be unique to be unique from build to build.
ifndef HEXDATE
HEXDATE = $(shell perl now.pl ldlinux.asm)
endif
ifndef DATE
DATE = $(HEXDATE)
endif

ldlinux.bin: ldlinux.asm genstupid.pl
	$(NASM) -f bin -dVERSION="'$(VERSION)'" -dDATE_STR="'$(DATE)'" -dHEXDATE="$(HEXDATE)" -l ldlinux.lst -o ldlinux.bin ldlinux.asm
	perl genstupid.pl < ldlinux.lst

bootsect.bin: ldlinux.bin
	dd if=ldlinux.bin of=bootsect.bin bs=512 count=1

ldlinux.sys: ldlinux.bin
	dd if=ldlinux.bin of=ldlinux.sys  bs=512 skip=1

syslinux.com: syslinux.asm bootsect.bin ldlinux.sys stupid.inc
	$(NASM) -f bin -l syslinux.lst -o syslinux.com syslinux.asm

copybs.com: copybs.asm
	$(NASM) -f bin -l copybs.lst -o copybs.com copybs.asm

bootsect_bin.c: bootsect.bin bin2c.pl
	perl bin2c.pl bootsect < bootsect.bin > bootsect_bin.c

ldlinux_bin.c: ldlinux.sys bin2c.pl
	perl bin2c.pl ldlinux < ldlinux.sys > ldlinux_bin.c

syslinux: syslinux.o bootsect_bin.o ldlinux_bin.o stupid.o
	$(CC) $(LDFLAGS) -o syslinux \
		syslinux.o bootsect_bin.o ldlinux_bin.o stupid.o

stupid.o: stupid.c

stupid.c: ldlinux.asm

stupid.inc: ldlinux.asm

install: all
	install -c syslinux $(BINDIR)

tidy:
	rm -f ldlinux.bin *.lst *.o *_bin.c stupid.*

clean: tidy
	rm -f $(TARGETS)

dist: tidy
	rm -f *~ \#*

#
# This should only be used by the maintainer to generate official binaries
# for release.  Please do not "make official" and distribute the binaries,
# please.
#
.PHONY: official release

official:
	$(MAKE) clean
	$(MAKE) all DATE=`date +'%Y-%m-%d'`
	$(MAKE) dist

release:
	-rm -rf release/syslinux-$(VERSION)
	-rm -f release/syslinux-$(VERSION).*
	mkdir -p release/syslinux-$(VERSION)
	cp $(SOURCES) $(DOCS) $(OTHER) release/syslinux-$(VERSION)
	make -C release/syslinux-$(VERSION) official
	cd release ; tar cvvf - syslinux-$(VERSION) | \
		gzip -9 > syslinux-$(VERSION).tar.gz
	cd release/syslinux-$(VERSION) ; \
		zip -9r ../syslinux-$(VERSION).zip *


PREREL    := syslinux-$(VERSION)-$(DATE)
PRERELDIR := release/syslinux-$(VERSION)-prerel
 
prerel:
	mkdir -p $(PRERELDIR)
	-rm -rf $(PRERELDIR)/$(PREREL)
	-rm -f $(PRERELDIR)/$(PREREL).*
	mkdir -p $(PRERELDIR)/$(PREREL)
	cp $(SOURCES) $(DOCS) $(OTHER) release/syslinux-$(VERSION)-$(DATE)
	make -C $(PRERELDIR)/$(PREREL) clean
	make -C $(PRERELDIR)/$(PREREL) HEXDATE="$(DATE)"
	make -C $(PRERELDIR)/$(PREREL) dist
	cd $(PRERELDIR) && tar cvvf - $(PREREL) | \
		gzip -9 > $(PREREL).tar.gz 
	cd $(PRERELDIR) && uuencode $(PREREL).tar.gz $(PREREL).tar.gz > $(PREREL).uu
	cd $(PRERELDIR)/$(PREREL) && \
		zip -9r ../$(PREREL).zip *
