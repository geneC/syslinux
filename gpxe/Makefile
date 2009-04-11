## -----------------------------------------------------------------------
##   
##   Copyright 2008-2009 H. Peter Anvin - All Rights Reserved
##   Copyright 2009 Intel Corporation; author: H. Peter Anvin
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

dist:
	$(MAKE) -C src veryclean > /dev/null 2>&1

spotless: clean dist
	rm -f $(TARGETS)

installer:

src/bin/undionly.kkpxe: pxelinux.gpxe ../core/pxelinux.0
	$(MAKE) -C src bin/undionly.kkpxe NO_WERROR=1 \
		EMBEDDED_IMAGE=../pxelinux.gpxe,../../core/pxelinux.0

gpxelinux.0: src/bin/undionly.kkpxe
	cp -f $< $@
