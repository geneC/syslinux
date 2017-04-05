## -----------------------------------------------------------------------
##
##   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# SYSLINUX utilities
#

VPATH = $(SRC)
include $(MAKEDIR)/syslinux.mk

CFLAGS   = $(CFLAGS_FOR_BUILD) $(GCCWARN) -Os -fomit-frame-pointer -D_FILE_OFFSET_BITS=64 -I$(SRC)
LDFLAGS  = $(LDFLAGS_FOR_BUILD) -O2

C_TARGETS	 = isohybrid gethostip memdiskfind
SCRIPT_TARGETS	 = mkdiskimage
SCRIPT_TARGETS	+= isohybrid.pl  # about to be obsoleted
ASIS		 = $(addprefix $(SRC)/,keytab-lilo lss16toppm md5pass \
		   ppmtolss16 sha1pass syslinux2ansi pxelinux-options)

TARGETS = $(C_TARGETS) $(SCRIPT_TARGETS)

ISOHDPFX = $(addprefix $(OBJ)/,../mbr/isohdpfx.bin ../mbr/isohdpfx_f.bin \
	   ../mbr/isohdpfx_c.bin \
	   ../mbr/isohdppx.bin ../mbr/isohdppx_f.bin ../mbr/isohdppx_c.bin)

all: $(TARGETS)

%.o: %.c
	$(CC_FOR_BUILD) $(UMAKEDEPS) $(CFLAGS) -c -o $@ $<

mkdiskimage: mkdiskimage.in ../mbr/mbr.bin bin2hex.pl
	$(PERL) $(SRC)/bin2hex.pl < $(OBJ)/../mbr/mbr.bin | cat $(SRC)/mkdiskimage.in - > $@
	chmod a+x $@

# Works on anything with a Perl interpreter...
isohybrid.pl: isohybrid.in $(ISOHDPFX) bin2hex.pl
	cp -f $(SRC)/isohybrid.in $@
	for f in $(ISOHDPFX) ; do $(PERL) $(SRC)/bin2hex.pl < $$f >> $@ ; done
	chmod a+x $@

isohdpfx.c: $(ISOHDPFX) isohdpfxarray.pl
	$(PERL) $(SRC)/isohdpfxarray.pl $(ISOHDPFX) > $@

isohybrid: isohybrid.o isohdpfx.o
	$(CC_FOR_BUILD) $(LDFLAGS) -o $@ $^ -luuid

gethostip: gethostip.o
	$(CC_FOR_BUILD) $(LDFLAGS) -o $@ $^

memdiskfind: memdiskfind.o
	$(CC_FOR_BUILD) $(LDFLAGS) -o $@ $^

tidy dist:
	rm -f *.o .*.d isohdpfx.c

clean: tidy
	rm -f $(TARGETS)

spotless: clean

installer: all

install: installer
	mkdir -m 755 -p $(INSTALLROOT)$(BINDIR)
	install -m 755 $(TARGETS) $(ASIS) $(INSTALLROOT)$(BINDIR)

strip:
	$(STRIP) $(C_TARGETS)

-include .*.d
