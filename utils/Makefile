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

topdir = ..
include $(topdir)/MCONFIG

CFLAGS   = -W -Wall -Os -fomit-frame-pointer -D_FILE_OFFSET_BITS=64
LDFLAGS  = -O2 -s

TARGETS	 = mkdiskimage isohybrid gethostip
ASIS     = keytab-lilo lss16toppm md5pass ppmtolss16 sha1pass syslinux2ansi

all: $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

mkdiskimage: mkdiskimage.in ../mbr/mbr.bin bin2hex.pl
	$(PERL) bin2hex.pl < ../mbr/mbr.bin | cat mkdiskimage.in - > $@
	chmod a+x $@

isohybrid: isohybrid.in ../mbr/isohdpfx.bin bin2hex.pl
	$(PERL) bin2hex.pl < ../mbr/isohdpfx.bin | cat isohybrid.in - > $@
	chmod a+x $@

gethostip: gethostip.o
	$(CC) $(LDFLAGS) -o $@ $^

tidy dist:
	rm -f *.o

clean: tidy
	rm -f $(TARGETS)

spotless: clean

installer: all

install: installer
	mkdir -m 755 -p $(INSTALLROOT)$(BINDIR)
	install -m 755 $(TARGETS) $(ASIS) $(INSTALLROOT)$(BINDIR)
