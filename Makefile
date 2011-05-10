## -----------------------------------------------------------------------
##
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
# Main Makefile for SYSLINUX
#
topdir = .
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/syslinux.mk
-include $(topdir)/version.mk

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

# List of module objects that should be installed for all derivatives
MODULES = memdisk/memdisk memdump/memdump.com modules/*.com \
	com32/menu/*.c32 com32/modules/*.c32 com32/mboot/*.c32 \
	com32/hdt/*.c32 com32/rosh/*.c32 com32/gfxboot/*.c32 \
	com32/sysdump/*.c32 com32/lua/src/*.c32

# syslinux.exe is BTARGET so as to not require everyone to have the
# mingw suite installed
BTARGET  = version.gen version.h version.mk
BOBJECTS = $(BTARGET) \
	mbr/*.bin \
	core/pxelinux.0 core/isolinux.bin core/isolinux-debug.bin \
	gpxe/gpxelinux.0 dos/syslinux.com \
	win32/syslinux.exe win64/syslinux64.exe \
	dosutil/*.com dosutil/*.sys \
	$(MODULES)

# BSUBDIRs build the on-target binary components.
# ISUBDIRs build the installer (host) components.
#
# Note: libinstaller is both a BSUBDIR and an ISUBDIR.  It contains
# files that depend only on the B phase, but may have to be regenerated
# for "make installer".
BSUBDIRS = codepage com32 lzo core memdisk modules mbr memdump gpxe sample \
	   diag libinstaller dos win32 win64 dosutil
ITARGET  =
IOBJECTS = $(ITARGET) \
	utils/gethostip utils/isohybrid utils/mkdiskimage \
	mtools/syslinux linux/syslinux extlinux/extlinux
ISUBDIRS = libinstaller mtools linux extlinux utils

# Things to install in /usr/bin
INSTALL_BIN   =	mtools/syslinux
# Things to install in /sbin
INSTALL_SBIN  = extlinux/extlinux
# Things to install in /usr/lib/syslinux
INSTALL_AUX   =	core/pxelinux.0 gpxe/gpxelinux.0 gpxe/gpxelinuxk.0 \
		core/isolinux.bin core/isolinux-debug.bin \
		dos/syslinux.com \
		mbr/*.bin $(MODULES)
INSTALL_AUX_OPT = win32/syslinux.exe win64/syslinux64.exe
INSTALL_DIAG  =	diag/mbr/handoff.bin \
		diag/geodsp/geodsp1s.img.xz diag/geodsp/geodspms.img.xz

# These directories manage their own installables
INSTALLSUBDIRS = com32 utils dosutil

# Things to install in /boot/extlinux
EXTBOOTINSTALL = $(MODULES)

# Things to install in /tftpboot
NETINSTALLABLE = core/pxelinux.0 gpxe/gpxelinux.0 \
		 $(MODULES)

all:
	$(MAKE) all-local
	set -e ; for i in $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done
	-ls -l $(BOBJECTS) $(IOBJECTS)

all-local: $(BTARGET) $(ITARGET)

installer:
	$(MAKE) installer-local
	set -e ; for i in $(ISUBDIRS); do $(MAKE) -C $$i all ; done
	-ls -l $(BOBJECTS) $(IOBJECTS)

installer-local: $(ITARGET) $(BINFILES)

strip:
	$(MAKE) strip-local
	set -e ; for i in $(ISUBDIRS); do $(MAKE) -C $$i strip ; done
	-ls -l $(BOBJECTS) $(IOBJECTS)

strip-local:

version.gen: version version.pl
	$(PERL) version.pl $< $@ '%define < @'
version.h: version version.pl
	$(PERL) version.pl $< $@ '#define < @'
version.mk: version version.pl
	$(PERL) version.pl $< $@ '< := @'

local-install: installer
	mkdir -m 755 -p $(INSTALLROOT)$(BINDIR)
	install -m 755 -c $(INSTALL_BIN) $(INSTALLROOT)$(BINDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(SBINDIR)
	install -m 755 -c $(INSTALL_SBIN) $(INSTALLROOT)$(SBINDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)
	install -m 644 -c $(INSTALL_AUX) $(INSTALLROOT)$(AUXDIR)
	-install -m 644 -c $(INSTALL_AUX_OPT) $(INSTALLROOT)$(AUXDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(DIAGDIR)
	install -m 644 -c $(INSTALL_DIAG) $(INSTALLROOT)$(DIAGDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(MANDIR)/man1
	install -m 644 -c man/*.1 $(INSTALLROOT)$(MANDIR)/man1
	: mkdir -m 755 -p $(INSTALLROOT)$(MANDIR)/man8
	: install -m 644 -c man/*.8 $(INSTALLROOT)$(MANDIR)/man8

install: local-install
	set -e ; for i in $(INSTALLSUBDIRS) ; do $(MAKE) -C $$i $@ ; done

netinstall: installer
	mkdir -p $(INSTALLROOT)$(TFTPBOOT)
	install -m 644 $(NETINSTALLABLE) $(INSTALLROOT)$(TFTPBOOT)

extbootinstall: installer
	mkdir -m 755 -p $(INSTALLROOT)$(EXTLINUXDIR)
	install -m 644 $(EXTBOOTINSTALL) $(INSTALLROOT)$(EXTLINUXDIR)

install-all: install netinstall extbootinstall

local-tidy:
	rm -f *.o *.elf *_bin.c stupid.* patch.offset
	rm -f *.lsr *.lst *.map *.sec *.tmp
	rm -f $(OBSOLETE)

tidy: local-tidy
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-clean:
	rm -f $(ITARGET)

clean: local-tidy local-clean
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-dist:
	find . \( -name '*~' -o -name '#*' -o -name core \
		-o -name '.*.d' -o -name .depend \) -type f -print0 \
	| xargs -0rt rm -f

dist: local-dist local-tidy
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-spotless:
	rm -f $(BTARGET) .depend *.so.*

spotless: local-clean local-dist local-spotless
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

# Shortcut to build linux/syslinux using klibc
klibc:
	$(MAKE) clean
	$(MAKE) CC=klcc ITARGET= ISUBDIRS='linux extlinux' BSUBDIRS=

# Hook to add private Makefile targets for the maintainer.
-include Makefile.private
