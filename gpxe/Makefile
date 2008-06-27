## -----------------------------------------------------------------------
##   
##   Copyright 2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
##   Boston MA 02110-1301, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Makefile for gpxe/gpxelinux.0
#
# Very simple, really...
#

TARGETS	= gpxelinux.0

all: $(TARGETS)

tidy:

clean: tidy
	rm -f $(TARGETS) src/bin/undionly.kpxe

dist:
	$(MAKE) -C src veryclean > /dev/null 2>&1

spotless: clean dist

installer:

src/bin/undionly.kpxe: ../core/pxelinux.0
	$(MAKE) -C src EMBEDDED_IMAGE=../$< bin/undionly.kpxe

gpxelinux.0: src/bin/undionly.kpxe
	cp -f $< $@
