##  $Id$
## -----------------------------------------------------------------------
##   
##   Copyright 1998-2004 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Main Makefile for SYSLINUX
#

OSTYPE   = $(shell uname -msr)
CC	 = gcc
INCLUDE  =
CFLAGS   = -W -Wall -Os -fomit-frame-pointer -D_FILE_OFFSET_BITS=64
PIC      = -fPIC
LDFLAGS  = -O2 -s
AR	 = ar
RANLIB   = ranlib

NASM	 = nasm -O99
NINCLUDE = 
BINDIR   = /usr/bin
LIBDIR   = /usr/lib
AUXDIR   = $(LIBDIR)/syslinux
INCDIR   = /usr/include

PERL     = perl

VERSION  = $(shell cat version)

.c.o:
	$(CC) $(INCLUDE) $(CFLAGS) -c $<

#
# The BTARGET refers to objects that are derived from ldlinux.asm; we
# like to keep those uniform for debugging reasons; however, distributors 
# want to recompile the installers (ITARGET).
# 
# BOBJECTS and IOBJECTS are the same thing, except used for
# installation, so they include objects that may be in subdirectories
# with their own Makefiles.  Finally, there is a list of those
# directories.
#
CSRC     = syslxmod.c gethostip.c
NASMSRC  = ldlinux.asm copybs.asm pxelinux.asm mbr.asm isolinux.asm
SOURCES = $(CSRC) *.h $(NASMSRC) *.inc

# _bin.c files required by both BTARGET and ITARGET installers
BINFILES = bootsect_bin.c ldlinux_bin.c mbr_bin.c \
	   extlinux_bss_bin.c extlinux_sys_bin.c 

# syslinux.exe is BTARGET so as to not require everyone to have the
# mingw suite installed
BTARGET  = kwdhash.gen version.gen ldlinux.bss ldlinux.sys ldlinux.bin \
	   pxelinux.0 mbr.bin isolinux.bin isolinux-debug.bin \
	   extlinux.bin extlinux.bss extlinux.sys
BOBJECTS = $(BTARGET) dos/syslinux.com win32/syslinux.exe memdisk/memdisk
BSUBDIRS = memdisk dos win32
ITARGET  = copybs.com gethostip mkdiskimage
IOBJECTS = $(ITARGET) mtools/syslinux unix/syslinux extlinux/extlinux
ISUBDIRS = mtools unix extlinux sample com32
DOCS     = COPYING NEWS README TODO BUGS *.doc sample menu com32
OTHER    = Makefile bin2c.pl now.pl genhash.pl keywords findpatch.pl \
	   keytab-lilo.pl version version.pl sys2ansi.pl \
	   ppmtolss16 lss16toppm memdisk bin2hex.pl mkdiskimage.in
OBSOLETE = pxelinux.bin

# Things to install in /usr/bin
INSTALL_BIN   =	mtools/syslinux gethostip ppmtolss16 lss16toppm
# Things to install in /usr/lib/syslinux
INSTALL_AUX   =	pxelinux.0 isolinux.bin isolinux-debug.bin \
		dos/syslinux.com win32/syslinux.exe \
		copybs.com memdisk/memdisk

# The DATE is set on the make command line when building binaries for
# official release.  Otherwise, substitute a hex string that is pretty much
# guaranteed to be unique to be unique from build to build.
ifndef HEXDATE
HEXDATE := $(shell $(PERL) now.pl ldlinux.asm pxelinux.asm isolinux.asm)
endif
ifndef DATE
DATE    := $(HEXDATE)
endif
MAKE    += DATE=$(DATE) HEXDATE=$(HEXDATE)

#
# NOTE: If you don't have the mingw compiler suite installed, you probably
# want to remove win32 from this list; otherwise you're going to get an
# error every time you try to build.
#

all:	all-local
	set -e ; for i in $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done
	$(MAKE) all-local
	-ls -l $(BOBJECTS) $(IOBJECTS)

all-local: $(BTARGET) $(ITARGET) $(BINFILES)

installer: installer-local
	set -e ; for i in $(ISUBDIRS); do $(MAKE) -C $$i all ; done
	-ls -l $(BOBJECTS) $(IOBJECTS)

installer-local: $(ITARGET) $(BINFILES)

version.gen: version version.pl
	$(PERL) version.pl version

kwdhash.gen: keywords genhash.pl
	$(PERL) genhash.pl < keywords > kwdhash.gen

ldlinux.bin: ldlinux.asm kwdhash.gen version.gen
	$(NASM) -f bin -DDATE_STR="'$(DATE)'" -DHEXDATE="$(HEXDATE)" \
		-DMAP=ldlinux.map -l ldlinux.lst -o ldlinux.bin ldlinux.asm

pxelinux.bin: pxelinux.asm kwdhash.gen version.gen
	$(NASM) -f bin -DDATE_STR="'$(DATE)'" -DHEXDATE="$(HEXDATE)" \
		-DMAP=pxelinux.map -l pxelinux.lst -o pxelinux.bin pxelinux.asm

isolinux.bin: isolinux.asm kwdhash.gen version.gen checksumiso.pl
	$(NASM) -f bin -DDATE_STR="'$(DATE)'" -DHEXDATE="$(HEXDATE)" \
		-DMAP=isolinux.map -l isolinux.lst -o isolinux.bin isolinux.asm
	$(PERL) checksumiso.pl isolinux.bin

# Special verbose version of isolinux.bin
isolinux-debug.bin: isolinux.asm kwdhash.gen version.gen checksumiso.pl
	$(NASM) -f bin -DDATE_STR="'$(DATE)'" -DHEXDATE="$(HEXDATE)" \
		-DDEBUG_MESSAGES \
		-DMAP=isolinux-debug.map -l isolinux-debug.lst \
		-o isolinux-debug.bin isolinux.asm
	$(PERL) checksumiso.pl $@

extlinux.bin: extlinux.asm kwdhash.gen version.gen
	$(NASM) -f bin -DDATE_STR="'$(DATE)'" -DHEXDATE="$(HEXDATE)" \
		-DMAP=extlinux.map -l extlinux.lst -o extlinux.bin extlinux.asm

pxelinux.0: pxelinux.bin
	cp pxelinux.bin pxelinux.0

ldlinux.bss: ldlinux.bin
	dd if=$< of=$@ bs=512 count=1

ldlinux.sys: ldlinux.bin
	dd if=$< of=$@ bs=512 skip=1

extlinux.bss: extlinux.bin
	dd if=$< of=$@ bs=512 count=1

extlinux.sys: extlinux.bin
	dd if=$< of=$@ bs=512 skip=1

mbr.bin: mbr.asm
	$(NASM) -f bin -l mbr.lst -o mbr.bin mbr.asm

mbr_bin.c: mbr.bin bin2c.pl
	$(PERL) bin2c.pl syslinux_mbr < $< > $@

copybs.com: copybs.asm
	$(NASM) -f bin -l copybs.lst -o copybs.com copybs.asm

bootsect_bin.c: ldlinux.bss bin2c.pl
	$(PERL) bin2c.pl syslinux_bootsect < $< > $@

ldlinux_bin.c: ldlinux.sys bin2c.pl
	$(PERL) bin2c.pl syslinux_ldlinux < $< > $@

extlinux_bss_bin.c: extlinux.bss bin2c.pl
	$(PERL) bin2c.pl extlinux_bootsect < $< > $@

extlinux_sys_bin.c: extlinux.sys bin2c.pl
	$(PERL) bin2c.pl extlinux_image < $< > $@

libsyslinux.a: bootsect_bin.o ldlinux_bin.o mbr_bin.o syslxmod.o
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

$(LIB_SO): bootsect_bin.o ldlinux_bin.o syslxmod.o
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(LIB_SONAME) -o $@ $^

gethostip.o: gethostip.c

gethostip: gethostip.o

mkdiskimage: mkdiskimage.in mbr.bin bin2hex.pl
	$(PERL) bin2hex.pl < mbr.bin | cat mkdiskimage.in - > $@
	chmod a+x $@

install: installer
	mkdir -m 755 -p $(INSTALLROOT)$(BINDIR) $(INSTALLROOT)$(AUXDIR)
	install -m 755 -c $(INSTALL_BIN) $(INSTALLROOT)$(BINDIR)
	install -m 644 -c $(INSTALL_AUX) $(INSTALLROOT)$(AUXDIR)
	$(MAKE) -C com32 install

install-lib: installer

install-all: install install-lib

local-tidy:
	rm -f *.o *_bin.c stupid.* patch.offset
	rm -f *.lst *.map
	rm -f $(OBSOLETE)

tidy: local-tidy
	set -e ; for i in $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-clean:
	rm -f $(ITARGET)

clean: local-tidy local-clean
	set -e ; for i in $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

dist: tidy
	for dir in . sample memdisk ; do \
		( cd $$dir && rm -f *~ \#* core ) ; \
	done

local-spotless:
	rm -f $(BTARGET) .depend *.so.*

spotless: local-clean dist local-spotless
	set -e ; for i in $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

.depend:
	rm -f .depend
	for csrc in $(CSRC) ; do $(CC) $(INCLUDE) -MM $$csrc >> .depend ; done
	for nsrc in $(NASMSRC) ; do $(NASM) -DDEPEND $(NINCLUDE) -o `echo $$nsrc | sed -e 's/\.asm/\.bin/'` -M $$nsrc >> .depend ; done

local-depend:
	rm -f .depend
	$(MAKE) .depend

depend: local-depend
	$(MAKE) -C memdisk depend

# Hook to add private Makefile targets for the maintainer.
-include Makefile.private

# Include dependencies file
-include .depend
